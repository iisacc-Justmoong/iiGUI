#import "UIKitTextInputIOS.h"
#include <iiSDKCore/Core/Text/Text.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

using namespace ii::gui;
using namespace ii::gui::detail;

@interface IIGUIIOSTextPosition : UITextPosition {
@public std::uint64_t generation;std::size_t offset;
}
@end
@implementation IIGUIIOSTextPosition
@end

@interface IIGUIIOSTextRange : UITextRange <NSCopying> {
@public IIGUIIOSTextPosition* first;IIGUIIOSTextPosition* last;
}
@end
@implementation IIGUIIOSTextRange
- (UITextPosition*)start{return first;}
- (UITextPosition*)end{return last;}
- (BOOL)isEmpty{return first->offset==last->offset;}
- (id)copyWithZone:(NSZone*)zone{(void)zone;return self;}
@end

@interface IIGUIIOSTextSelectionRect : UITextSelectionRect {
@public CGRect bounds;BOOL hasStart,hasEnd;
}
@end
@implementation IIGUIIOSTextSelectionRect
- (CGRect)rect{return bounds;}
- (NSWritingDirection)writingDirection{return NSWritingDirectionNatural;}
- (BOOL)containsStart{return hasStart;}
- (BOOL)containsEnd{return hasEnd;}
- (BOOL)isVertical{return NO;}
@end

namespace {
struct UIKitHost final {
    WidgetSurfaceCallbacks callbacks;
    std::optional<WidgetSnapshot> focus(){
        if(!callbacks.accessibility)return {};
        for(auto& node:callbacks.accessibility())
            if(node.visible&&node.focused&&node.text_input&&node.enabled&&!node.input_blocked)return node;
        return {};
    }
    ii::core::Result<TextContent> read(std::string_view id,TextRange range){
        return callbacks.text_content?callbacks.text_content(id,range):ii::core::fail(ii::core::ErrorCode::closed,"UIKit text reader unavailable");
    }
    ii::core::Result<TextInputGeometry> geometry(std::string_view id,TextRange range){
        return callbacks.text_geometry?callbacks.text_geometry(id,range):ii::core::fail(ii::core::ErrorCode::unsupported,"UIKit text geometry unavailable");
    }
    ii::core::Result<std::size_t> position(std::string_view id,double x,double y){
        return callbacks.text_position?callbacks.text_position(id,x,y):ii::core::fail(ii::core::ErrorCode::unsupported,"UIKit text hit testing unavailable");
    }
    ii::core::Result<void> send(WidgetInput value){
        return callbacks.text_service?callbacks.text_service(std::move(value)):ii::core::fail(ii::core::ErrorCode::closed,"UIKit text editor unavailable");
    }
};

template<class Result,class Function>
Result guarded(IIGUIIOSSurface* view,Result fallback,Function&& operation) noexcept {
    IIGUIIOSSurface* lifetime __attribute__((objc_precise_lifetime))=view;
    try{return operation();}catch(...){
        try{auto failure=lifetime->callbacks.failure;if(failure)failure(std::current_exception());}catch(...){}
        return fallback;
    }
}
template<class Function>
void guarded_action(IIGUIIOSSurface* view,Function&& operation) noexcept {
    IIGUIIOSSurface* lifetime __attribute__((objc_precise_lifetime))=view;
    try{operation();}catch(...){try{auto failure=lifetime->callbacks.failure;if(failure)failure(std::current_exception());}catch(...){}}
}
IIGUIIOSTextPosition* make_position(std::uint64_t generation,std::size_t offset){
    auto value=[IIGUIIOSTextPosition new];value->generation=generation;value->offset=offset;return value;
}
IIGUIIOSTextRange* make_range(std::uint64_t generation,TextRange range){
    auto value=[IIGUIIOSTextRange new];value->first=make_position(generation,range.location);
    value->last=make_position(generation,range.location+range.length);return value;
}
std::optional<std::size_t> position(UITextPosition* value,const UIKitTextInputState& state){
    if(![value isKindOfClass:IIGUIIOSTextPosition.class])return {};
    auto item=(IIGUIIOSTextPosition*)value;
    if(!item->generation||item->generation!=state.generation||item->offset>state.length)return {};
    return item->offset;
}
std::optional<TextRange> range(UITextRange* value,const UIKitTextInputState& state){
    if(![value isKindOfClass:IIGUIIOSTextRange.class])return {};
    auto item=(IIGUIIOSTextRange*)value;auto first=position(item->first,state),last=position(item->last,state);
    if(!first||!last||*last<*first)return {};return TextRange{*first,*last-*first};
}
std::optional<std::string> utf8(NSString* value){
    if(!value)return std::string{};
    if(value.length>65536)return {};
    NSData* bytes=[value dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:NO];
    if(!bytes||bytes.length>65536)return {};
    if(!bytes.length)return std::string{};
    return std::string(static_cast<const char*>(bytes.bytes),bytes.length);
}
CGRect rectangle(ii::gui::Rect value){return CGRectMake(value.x,value.y,value.width,value.height);}
bool contains_caret(ii::gui::Rect span,ii::gui::Rect caret){
    constexpr float tolerance=1.5f;
    return caret.width>0&&caret.height>0&&caret.x>=span.x-tolerance&&caret.x<=span.x+span.width+tolerance&&
        caret.y+caret.height>span.y&&caret.y<span.y+span.height;
}
std::optional<std::size_t> shifted(std::size_t value,NSInteger delta,std::size_t length){
    if(delta>=0){const auto count=std::size_t(delta);if(count>length-value)return {};return value+count;}
    const auto count=std::uint64_t(-(delta+1))+1;if(count>value)return {};return value-std::size_t(count);
}
struct SystemEdit final {
    IIGUIIOSSurface* view;
    explicit SystemEdit(IIGUIIOSSurface* owner):view(owner){++view->nativeTextDepth;}
    ~SystemEdit(){--view->nativeTextDepth;}
};
}

namespace ii::gui::detail {
void initialize_uikit_text_input(IIGUIIOSSurface* view){view->textProtocol=std::make_shared<UIKitTextInputProtocol>();}
runtime::Task begin_uikit_text_input_change(IIGUIIOSSurface* view,bool value_changed){
    if(!view||(!view->textNoticeDepth&&!view->textDelegate))return {};
    if(view->textNoticeDepth==(std::numeric_limits<unsigned int>::max)())throw std::runtime_error("UIKit text notification depth exhausted");
    IIGUIIOSSurface* retained=view;
    const bool first=view->textNoticeDepth++==0;
    if(first){view->textNoticeDelegate=view->textDelegate;view->textNoticeValue=NO;}
    auto observer=view->textNoticeDelegate;
    @try {
        if(first)[observer selectionWillChange:view];
        if(value_changed&&!view->textNoticeValue){view->textNoticeValue=YES;[observer textWillChange:view];}
    } @catch(NSException* exception) {
        --view->textNoticeDepth;if(!view->textNoticeDepth){view->textNoticeDelegate=nil;view->textNoticeValue=NO;}
        const char* reason=exception.reason.UTF8String;throw std::runtime_error(reason?reason:"UIKit text notification failed");
    }
    try{return [retained]{
        if(!retained->textNoticeDepth)return;
        if(--retained->textNoticeDepth)return;
        auto observer=retained->textNoticeDelegate;const bool text=retained->textNoticeValue;
        retained->textNoticeDelegate=nil;retained->textNoticeValue=NO;
        @try{if(text)[observer textDidChange:retained];[observer selectionDidChange:retained];}
        @catch(NSException*){}
    };}catch(...){
        if(--view->textNoticeDepth==0){view->textNoticeDelegate=nil;view->textNoticeValue=NO;
            @try{if(value_changed)[observer textDidChange:view];[observer selectionDidChange:view];}@catch(NSException*){}}
        throw;
    }
}
}

@implementation IIGUIIOSSurface (UIKitTextInput)
- (id<UITextInputDelegate>)inputDelegate{return textDelegate;}
- (void)setInputDelegate:(id<UITextInputDelegate>)value{textDelegate=value;}
- (NSDictionary<NSAttributedStringKey,id>*)markedTextStyle{return markedStyle;}
- (void)setMarkedTextStyle:(NSDictionary<NSAttributedStringKey,id>*)value{markedStyle=[value copy];}
- (id<UITextInputTokenizer>)tokenizer{
    if(!textTokenizer)textTokenizer=[[UITextInputStringTokenizer alloc] initWithTextInput:self];return textTokenizer;
}
- (UIView*)textInputView{return self;}
- (BOOL)isEditable{auto value=[self editor];return value&&!value->read_only;}
- (BOOL)hasText{auto value=[self editor];return value&&value->text_length>0;}
- (void)insertText:(NSString*)text{
    guarded_action(self,[&]{
        auto data=utf8(text);if(!data||!textProtocol)return;UIKitHost host{callbacks};auto state=textProtocol->state(host);
        if(!state){auto legacy=[self editor];if(legacy&&!legacy->text_revision){SystemEdit edit(self);
            if(*data=="\n")[self deliver:WidgetInput{.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}];
            else [self deliver:WidgetInput{.kind=WidgetInputKind::text,.text=std::move(*data)}];}return;}
        SystemEdit edit(self);
        if(*data=="\n"&&!state->multiline&&!state->marked){[self deliver:WidgetInput{.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}];return;}
        (void)textProtocol->insert(host,state->generation,std::move(*data));
    });
}
- (void)deleteBackward{
    guarded_action(self,[&]{if(!textProtocol)return;UIKitHost host{callbacks};auto state=textProtocol->state(host);
        if(!state){auto legacy=[self editor];if(legacy&&!legacy->text_revision){SystemEdit edit(self);[self deliver:WidgetInput{.kind=WidgetInputKind::key_down,.key=WidgetKey::backspace}];}return;}
        SystemEdit edit(self);(void)textProtocol->delete_backward(host,state->generation);});
}
- (NSString*)textInRange:(UITextRange*)value{
    return guarded<NSString*>(self,nil,[&]()->NSString*{if(!textProtocol)return nil;UIKitHost host{callbacks};auto state=textProtocol->state(host);if(!state)return nil;
        auto selected=range(value,*state);if(!selected)return nil;auto text=textProtocol->read(host,state->generation,*selected);if(!text)return nil;
        return [[NSString alloc] initWithCharacters:reinterpret_cast<const unichar*>(text->data()) length:text->size()];});
}
- (void)replaceRange:(UITextRange*)value withText:(NSString*)text{
    guarded_action(self,[&]{if(!textProtocol)return;auto data=utf8(text);if(!data)return;UIKitHost host{callbacks};auto state=textProtocol->state(host);if(!state)return;
        auto selected=range(value,*state);if(!selected)return;SystemEdit edit(self);(void)textProtocol->replace(host,state->generation,*selected,std::move(*data));});
}
- (UITextRange*)selectedTextRange{
    return guarded<UITextRange*>(self,nil,[&]()->UITextRange*{if(!textProtocol)return nil;UIKitHost host{callbacks};auto state=textProtocol->state(host);
        return state?make_range(state->generation,state->selection):nil;});
}
- (void)setSelectedTextRange:(UITextRange*)value{
    guarded_action(self,[&]{if(!textProtocol||!value)return;UIKitHost host{callbacks};auto state=textProtocol->state(host);if(!state)return;
        auto selected=range(value,*state);if(!selected)return;SystemEdit edit(self);(void)textProtocol->select(host,state->generation,*selected);});
}
- (UITextRange*)markedTextRange{
    return guarded<UITextRange*>(self,nil,[&]()->UITextRange*{if(!textProtocol)return nil;UIKitHost host{callbacks};auto state=textProtocol->state(host);
        return state&&state->marked?make_range(state->generation,*state->marked):nil;});
}
- (void)setMarkedText:(NSString*)value selectedRange:(NSRange)selected{
    guarded_action(self,[&]{if(!textProtocol||selected.location==NSNotFound)return;auto data=utf8(value);if(!data)return;UIKitHost host{callbacks};
        auto state=textProtocol->state(host);if(!state)return;SystemEdit edit(self);
        (void)textProtocol->compose(host,state->generation,std::move(*data),{selected.location,selected.length});});
}
- (void)unmarkText{
    guarded_action(self,[&]{if(!textProtocol)return;UIKitHost host{callbacks};auto state=textProtocol->state(host);if(!state)return;
        SystemEdit edit(self);(void)textProtocol->finish(host,state->generation);});
}
- (UITextPosition*)beginningOfDocument{
    return guarded<UITextPosition*>(self,make_position(0,0),[&]()->UITextPosition*{if(!textProtocol)return make_position(0,0);UIKitHost host{callbacks};
        auto state=textProtocol->state(host);return state?make_position(state->generation,0):make_position(0,0);});
}
- (UITextPosition*)endOfDocument{
    return guarded<UITextPosition*>(self,make_position(0,0),[&]()->UITextPosition*{if(!textProtocol)return make_position(0,0);UIKitHost host{callbacks};
        auto state=textProtocol->state(host);return state?make_position(state->generation,state->length):make_position(0,0);});
}
- (UITextRange*)textRangeFromPosition:(UITextPosition*)from toPosition:(UITextPosition*)to{
    return guarded<UITextRange*>(self,nil,[&]()->UITextRange*{if(!textProtocol)return nil;UIKitHost host{callbacks};auto state=textProtocol->state(host);if(!state)return nil;
        auto first=position(from,*state),last=position(to,*state);if(!first||!last)return nil;
        return make_range(state->generation,{std::min(*first,*last),std::max(*first,*last)-std::min(*first,*last)});});
}
- (UITextPosition*)positionFromPosition:(UITextPosition*)value offset:(NSInteger)amount{
    return guarded<UITextPosition*>(self,nil,[&]()->UITextPosition*{if(!textProtocol)return nil;UIKitHost host{callbacks};auto state=textProtocol->state(host);if(!state)return nil;
        auto at=position(value,*state);if(!at)return nil;auto next=shifted(*at,amount,state->length);return next?make_position(state->generation,*next):nil;});
}
- (UITextPosition*)positionFromPosition:(UITextPosition*)value inDirection:(UITextLayoutDirection)direction offset:(NSInteger)amount{
    return guarded<UITextPosition*>(self,nil,[&]()->UITextPosition*{if(!textProtocol)return nil;UIKitHost host{callbacks};auto state=textProtocol->state(host);if(!state)return nil;
        auto at=position(value,*state);if(!at)return nil;UIKitTextDirection movement;
        switch(direction){case UITextLayoutDirectionLeft:movement=UIKitTextDirection::left;break;case UITextLayoutDirectionRight:movement=UIKitTextDirection::right;break;
            case UITextLayoutDirectionUp:movement=UIKitTextDirection::up;break;case UITextLayoutDirectionDown:movement=UIKitTextDirection::down;break;default:return nil;}
        const auto count=amount<0?std::uint64_t(-(amount+1))+1:std::uint64_t(amount);
        if(amount<0){switch(movement){case UIKitTextDirection::left:movement=UIKitTextDirection::right;break;case UIKitTextDirection::right:movement=UIKitTextDirection::left;break;
            case UIKitTextDirection::up:movement=UIKitTextDirection::down;break;case UIKitTextDirection::down:movement=UIKitTextDirection::up;break;}}
        auto next=textProtocol->move(host,state->generation,*at,movement,count);return next?make_position(state->generation,*next):nil;});
}
- (NSComparisonResult)comparePosition:(UITextPosition*)value toPosition:(UITextPosition*)other{
    return guarded<NSComparisonResult>(self,NSOrderedSame,[&]{if(!textProtocol)return NSOrderedSame;UIKitHost host{callbacks};auto state=textProtocol->state(host);if(!state)return NSOrderedSame;
        auto first=position(value,*state),last=position(other,*state);if(!first||!last)return NSOrderedSame;
        return *first<*last?NSOrderedAscending:*first>*last?NSOrderedDescending:NSOrderedSame;});
}
- (NSInteger)offsetFromPosition:(UITextPosition*)from toPosition:(UITextPosition*)to{
    return guarded<NSInteger>(self,0,[&]()->NSInteger{if(!textProtocol)return 0;UIKitHost host{callbacks};auto state=textProtocol->state(host);if(!state)return 0;
        auto first=position(from,*state),last=position(to,*state);if(!first||!last)return 0;
        const auto difference=*first>*last?*first-*last:*last-*first;if(difference>std::size_t(NSIntegerMax))return 0;
        return *first>*last?-NSInteger(difference):NSInteger(difference);});
}
- (UITextPosition*)positionWithinRange:(UITextRange*)value farthestInDirection:(UITextLayoutDirection)direction{
    return guarded<UITextPosition*>(self,nil,[&]()->UITextPosition*{if(!textProtocol)return nil;UIKitHost host{callbacks};auto state=textProtocol->state(host);if(!state)return nil;
        auto selected=range(value,*state);if(!selected)return nil;auto geometry=textProtocol->geometry(host,state->generation,*selected);
        bool end=direction==UITextLayoutDirectionRight||direction==UITextLayoutDirectionDown;
        if(geometry&&geometry->start_caret.width>0&&geometry->end_caret.width>0){
            const auto a=geometry->start_caret,b=geometry->end_caret;
            if(direction==UITextLayoutDirectionLeft||direction==UITextLayoutDirectionRight)end=direction==UITextLayoutDirectionRight?b.x>a.x:b.x<a.x;
            else if(direction==UITextLayoutDirectionUp||direction==UITextLayoutDirectionDown)end=direction==UITextLayoutDirectionDown?b.y>a.y:b.y<a.y;
        }
        return make_position(state->generation,selected->location+(end?selected->length:0));});
}
- (UITextRange*)characterRangeByExtendingPosition:(UITextPosition*)value inDirection:(UITextLayoutDirection)direction{
    return guarded<UITextRange*>(self,nil,[&]()->UITextRange*{if(!textProtocol)return nil;UIKitHost host{callbacks};auto state=textProtocol->state(host);if(!state)return nil;
        auto at=position(value,*state);if(!at)return nil;const bool forward=direction==UITextLayoutDirectionRight||direction==UITextLayoutDirectionDown;
        auto character=textProtocol->character(host,state->generation,*at,forward);return character&&character->length?make_range(state->generation,*character):nil;});
}
- (NSWritingDirection)baseWritingDirectionForPosition:(UITextPosition*)value inDirection:(UITextStorageDirection)direction{(void)value;(void)direction;return NSWritingDirectionNatural;}
- (void)setBaseWritingDirection:(NSWritingDirection)direction forRange:(UITextRange*)value{(void)direction;(void)value;}
- (CGRect)firstRectForRange:(UITextRange*)value{
    return guarded<CGRect>(self,CGRectNull,[&]{if(!textProtocol)return CGRectNull;UIKitHost host{callbacks};auto state=textProtocol->state(host);if(!state)return CGRectNull;
        auto selected=range(value,*state);if(!selected)return CGRectNull;auto geometry=textProtocol->geometry(host,state->generation,*selected);if(!geometry)return CGRectNull;
        return rectangle(geometry->selection.empty()?geometry->start_caret:geometry->selection.front());});
}
- (CGRect)caretRectForPosition:(UITextPosition*)value{
    return guarded<CGRect>(self,CGRectNull,[&]{if(!textProtocol)return CGRectNull;UIKitHost host{callbacks};auto state=textProtocol->state(host);if(!state)return CGRectNull;
        auto at=position(value,*state);if(!at)return CGRectNull;auto geometry=textProtocol->geometry(host,state->generation,{*at,0});return geometry?rectangle(geometry->start_caret):CGRectNull;});
}
- (NSArray<UITextSelectionRect*>*)selectionRectsForRange:(UITextRange*)value{
    return guarded<NSArray<UITextSelectionRect*>*>(self,@[],[&]()->NSArray<UITextSelectionRect*>*{if(!textProtocol)return @[];UIKitHost host{callbacks};auto state=textProtocol->state(host);if(!state)return @[];
        auto selected=range(value,*state);if(!selected)return @[];auto geometry=textProtocol->geometry(host,state->generation,*selected);if(!geometry)return @[];
        NSMutableArray<UITextSelectionRect*>* result=[NSMutableArray new];bool start=false,end=false;
        for(const auto& span:geometry->selection){auto item=[IIGUIIOSTextSelectionRect new];item->bounds=rectangle(span);
            item->hasStart=!start&&contains_caret(span,geometry->start_caret);item->hasEnd=!end&&contains_caret(span,geometry->end_caret);
            start|=item->hasStart;end|=item->hasEnd;[result addObject:item];}return result;});
}
- (UITextPosition*)closestPositionToPoint:(CGPoint)point{
    return guarded<UITextPosition*>(self,nil,[&]()->UITextPosition*{if(!textProtocol)return nil;UIKitHost host{callbacks};auto state=textProtocol->state(host);if(!state)return nil;
        auto at=textProtocol->position(host,state->generation,point.x,point.y);return at?make_position(state->generation,*at):nil;});
}
- (UITextPosition*)closestPositionToPoint:(CGPoint)point withinRange:(UITextRange*)value{
    return guarded<UITextPosition*>(self,nil,[&]()->UITextPosition*{if(!textProtocol)return nil;UIKitHost host{callbacks};auto state=textProtocol->state(host);if(!state)return nil;
        auto selected=range(value,*state);if(!selected)return nil;auto at=textProtocol->position(host,state->generation,point.x,point.y);if(!at)return nil;
        return make_position(state->generation,std::clamp(*at,selected->location,selected->location+selected->length));});
}
- (UITextRange*)characterRangeAtPoint:(CGPoint)point{
    return guarded<UITextRange*>(self,nil,[&]()->UITextRange*{if(!textProtocol)return nil;UIKitHost host{callbacks};auto state=textProtocol->state(host);if(!state)return nil;
        auto at=textProtocol->position(host,state->generation,point.x,point.y);if(!at)return nil;
        auto character=textProtocol->character(host,state->generation,*at,*at<state->length);return character&&character->length?make_range(state->generation,*character):nil;});
}
@end
