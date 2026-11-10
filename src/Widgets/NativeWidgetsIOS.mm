#include <iiGUI/FontDatabase.h>
#import <UIKit/UIKit.h>
#import <CoreText/CoreText.h>
#include <iiGUI/WidgetSurfaceNative.h>
#include <iiGUI/NativeCanvas.h>
#include <iiGUI/MobileInput.h>
#include <iiGUI/Desktop.h>
#include "../Platform/UIKitDisplayScale.h"
#include "../Platform/TouchInputBridge.h"
#import "UIKitTextInputIOS.h"
#include <algorithm>
#include <limits>
#include <mutex>

namespace {
using namespace ii::gui;
NSString* native(std::string_view text){return [[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding];}
void fonts(){const auto loaded=FontDatabase::application()->load_bundled_fonts();if(!loaded)throw std::runtime_error(loaded.error().message);}
WidgetKey key(UIKeyboardHIDUsage code){switch(code){case UIKeyboardHIDUsageKeyboardTab:return WidgetKey::tab;case UIKeyboardHIDUsageKeyboardReturnOrEnter:case UIKeyboardHIDUsageKeypadEnter:return WidgetKey::enter;case UIKeyboardHIDUsageKeyboardEscape:return WidgetKey::escape;case UIKeyboardHIDUsageKeyboardSpacebar:return WidgetKey::space;case UIKeyboardHIDUsageKeyboardDeleteOrBackspace:return WidgetKey::backspace;case UIKeyboardHIDUsageKeyboardDeleteForward:return WidgetKey::delete_forward;case UIKeyboardHIDUsageKeyboardLeftArrow:return WidgetKey::left;case UIKeyboardHIDUsageKeyboardRightArrow:return WidgetKey::right;case UIKeyboardHIDUsageKeyboardUpArrow:return WidgetKey::up;case UIKeyboardHIDUsageKeyboardDownArrow:return WidgetKey::down;case UIKeyboardHIDUsageKeyboardHome:return WidgetKey::home;case UIKeyboardHIDUsageKeyboardEnd:return WidgetKey::end;case UIKeyboardHIDUsageKeyboardPageUp:return WidgetKey::page_up;case UIKeyboardHIDUsageKeyboardPageDown:return WidgetKey::page_down;default:return WidgetKey::character;}}
}
@interface IIGUIIOSElement : UIAccessibilityElement {
@public std::function<WidgetSnapshot()> read;std::function<void(WidgetInput)> send;std::string identity;
}
@end
@implementation IIGUIIOSElement
- (BOOL)isAccessibilityElement {auto n=read?read():WidgetSnapshot{};return n.attached&&n.visible&&!n.input_blocked&&
    !(n.accessibility_semantics&&(n.accessibility_semantics->role==AccessibilityRole::tab_group||n.accessibility_semantics->role==AccessibilityRole::menu||n.accessibility_semantics->role==AccessibilityRole::table||n.accessibility_semantics->role==AccessibilityRole::column_header_group));}
- (NSString*)accessibilityIdentifier{return native(identity);}
- (NSString*)accessibilityLabel {auto n=read?read():WidgetSnapshot{};return native(!n.accessibility_name.empty()?n.accessibility_name:n.text.empty()?n.icon_name:n.text);}
- (NSString*)accessibilityValue {auto n=read?read():WidgetSnapshot{};if(n.password)return @"";if(n.accessibility_semantics&&n.accessibility_semantics->role==AccessibilityRole::column_header)return n.accessibility_description.empty()?nil:native(n.accessibility_description);if(n.accessibility_semantics&&(n.accessibility_semantics->role==AccessibilityRole::progress||n.accessibility_semantics->role==AccessibilityRole::slider)){if(!n.accessibility_semantics->range)return nil;return [NSNumberFormatter localizedStringFromNumber:@((n.value-n.minimum)/(n.maximum-n.minimum)) numberStyle:NSNumberFormatterPercentStyle];}if(n.kind==WidgetKind::action_surface){if(n.action_role==ActionRole::toggle_switch)return NSLocalizedString(n.checked?@"On":@"Off",nil);if(n.action_role==ActionRole::check_box||n.action_role==ActionRole::radio_button||n.action_role==ActionRole::tab||n.action_role==ActionRole::menu_item)return nil;}return native(n.text);}
- (UIAccessibilityTraits)accessibilityTraits {auto n=read?read():WidgetSnapshot{};UIAccessibilityTraits t=n.kind==WidgetKind::button||n.kind==WidgetKind::action_surface||n.kind==WidgetKind::menu_item?UIAccessibilityTraitButton:n.kind==WidgetKind::slider?UIAccessibilityTraitAdjustable:UIAccessibilityTraitStaticText;if(n.accessibility_semantics&&n.accessibility_semantics->role==AccessibilityRole::image)t=UIAccessibilityTraitImage;else if(n.accessibility_semantics&&n.accessibility_semantics->role==AccessibilityRole::progress)t=UIAccessibilityTraitUpdatesFrequently;else if(n.accessibility_semantics&&(n.accessibility_semantics->role==AccessibilityRole::slider||n.accessibility_semantics->role==AccessibilityRole::stepper))t=UIAccessibilityTraitAdjustable;else if(n.accessibility_semantics&&n.accessibility_semantics->role==AccessibilityRole::column_header)t=UIAccessibilityTraitHeader|(n.accessibility_actionable?UIAccessibilityTraitButton:UIAccessibilityTraitStaticText);if(!n.enabled)t|=UIAccessibilityTraitNotEnabled;if(n.selected||n.checked)t|=UIAccessibilityTraitSelected;return t;}
- (CGRect)accessibilityFrameInContainerSpace {auto n=read?read():WidgetSnapshot{};auto r=n.visual_bounds;if(n.visual_clip_bounds){auto c=*n.visual_clip_bounds;float right=std::min(r.x+r.width,c.x+c.width),bottom=std::min(r.y+r.height,c.y+c.height);r.x=std::max(r.x,c.x);r.y=std::max(r.y,c.y);r.width=std::max(0.f,right-r.x);r.height=std::max(0.f,bottom-r.y);}return CGRectMake(r.x,r.y,r.width,r.height);}
- (BOOL)accessibilityActivate {auto s=send;auto n=read?read():WidgetSnapshot{};if(!s)return NO;if(n.accessibility_semantics&&n.accessibility_semantics->role==AccessibilityRole::column_header&&(!n.attached||!n.visible||n.input_blocked||!n.enabled||!n.accessibility_actionable))return NO;s({.kind=WidgetInputKind::activate,.target=identity});return YES;}
- (void)accessibilityIncrement {if(send)send({.kind=WidgetInputKind::increment,.target=identity});}
- (void)accessibilityDecrement {if(send)send({.kind=WidgetInputKind::decrement,.target=identity});}
@end

@implementation IIGUIIOSSurface
- (BOOL)canBecomeFirstResponder{return YES;}
- (UIView*)inputView{auto value=[self editor];return value&&value->enabled&&!value->read_only?nil:hiddenKeyboard;}
- (std::optional<WidgetSnapshot>)editor {try{if(auto focused=callbacks.focused_editor)return focused();auto snapshot=callbacks.accessibility;if(snapshot)for(auto& n:snapshot())if(n.visible&&n.focused&&n.text_input&&!n.input_blocked)return n;}catch(...){try{auto failure=callbacks.failure;if(failure)failure(std::current_exception());}catch(...){}}return {};}
- (void)synchronizeEditor {
    IIGUIIOSSurface* lifetime __attribute__((objc_precise_lifetime))=self;
    if(syncingInputViews){pendingInputViews=true;return;}
    syncingInputViews=true;struct Reset{bool& flag;~Reset(){flag=false;}}reset{syncingInputViews};
    try{
        for(unsigned pass=0;pass<8;++pass){
            pendingInputViews=false;auto value=[self editor];NSString* next=value?native(value->id):@"";
            auto identity=value&&value->text_revision?value->text_revision->identity:nullptr;
            const auto focus=value&&value->text_revision?value->text_revision->focus:0;
            const bool read_only=value&&value->read_only,secure=value&&value->password,multiline=value&&value->multiline;
            if(![next isEqualToString:editing]||identity!=editingIdentity||focus!=editingFocus||
               read_only!=editingReadOnly||secure!=editingSecure||multiline!=editingMultiline){
                editing=next;editingIdentity=std::move(identity);editingFocus=focus;
                editingReadOnly=read_only;editingSecure=secure;editingMultiline=multiline;[self reloadInputViews];
            }
            if(!pendingInputViews)return;
        }
        throw std::runtime_error("UIKit text focus updates did not settle");
    }catch(...){try{auto failure=lifetime->callbacks.failure;if(failure)failure(std::current_exception());}catch(...){}}
}
- (UIKeyboardType)keyboardType {auto e=[self editor];if(e&&(e->accessibility_name=="Width"||e->accessibility_name=="Height"||e->accessibility_name=="Print resolution (PPI)"))return UIKeyboardTypeDecimalPad;return UIKeyboardTypeDefault;}
- (UITextAutocorrectionType)autocorrectionType{return UITextAutocorrectionTypeNo;}
- (BOOL)isSecureTextEntry {auto e=[self editor];return e&&e->password;}
- (UITextAutocapitalizationType)autocapitalizationType{return UITextAutocapitalizationTypeNone;}
- (void)deliver:(WidgetInput)value {
    IIGUIIOSSurface* lifetime __attribute__((objc_precise_lifetime))=self;
    try{
        const bool key=value.kind==WidgetInputKind::key_down;
        const bool text=value.kind==WidgetInputKind::text||value.kind==WidgetInputKind::composition||
            (key&&(value.key==WidgetKey::backspace||value.key==WidgetKey::delete_forward||value.key==WidgetKey::cut||
                  value.key==WidgetKey::undo||value.key==WidgetKey::redo||value.key==WidgetKey::enter));
        const bool selection=text||key||value.kind==WidgetInputKind::pointer_down||value.kind==WidgetInputKind::pointer_up||
            (value.kind==WidgetInputKind::pointer_move&&pointer)||value.kind==WidgetInputKind::focus_lost;
        auto editor=[self editor];
        TextInputChange notice{selection&&editor&&!nativeTextDepth?ii::gui::detail::begin_uikit_text_input_change(self,text):ii::runtime::Task{}};
        auto input=callbacks.input;if(input)input(std::move(value));[self synchronizeEditor];if(!self.isFirstResponder)[self becomeFirstResponder];
    }catch(...){try{auto failure=lifetime->callbacks.failure;if(failure)failure(std::current_exception());}catch(...){}}
}
- (void)drawRect:(CGRect)r {(void)r;try{if(paint)paint();}catch(...){if(callbacks.failure)callbacks.failure(std::current_exception());}}
- (void)layoutSubviews {[super layoutSubviews];[self setNeedsDisplay];}
- (void)traitCollectionDidChange:(UITraitCollection*)previous {
    [super traitCollectionDidChange:previous];
    if(ii::gui::detail::uikit_display_scale_changed(previous.displayScale,self.traitCollection.displayScale))[self setNeedsDisplay];
}
- (ii::runtime::touch::TouchSample)runtimeSample:(UITouch*)touch identity:(UITouch*)identity {
    auto p=[touch locationInView:self];const auto count=std::uint32_t(std::min<NSUInteger>(touch.tapCount,std::numeric_limits<std::uint32_t>::max()));
    auto native=ii::gui::detail::touch_input(WidgetInputKind::pointer_move,{p.x,p.y,touch.force,touch.maximumPossibleForce,
        touch.altitudeAngle,[touch azimuthAngleInView:self],touch.timestamp,std::uint64_t((__bridge void*)identity),touch.type==UITouchTypePencil,count});
    native.native_click_count=count;return ii::gui::detail::runtime_touch_sample(native);
}
- (void)emitTouchBatch:(const std::optional<ii::runtime::touch::TouchBatch>&)batch {
    if(!batch)return;const auto revision=touchRouter.revision();
    for(std::size_t i=0;i<batch->size;++i)if(auto event=ii::gui::detail::widget_touch_event((*batch)[i])) {
        if(event->kind==WidgetInputKind::pointer_down)pointer=(__bridge UITouch*)reinterpret_cast<void*>(std::uintptr_t(event->device_id));
        const bool release=event->kind==WidgetInputKind::pointer_up||event->kind==WidgetInputKind::pointer_cancel;
        [self deliver:std::move(*event)];if(touchRouter.revision()!=revision)return;if(release)pointer=nil;
    }
}
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {(void)event;try {
    for(UITouch* touch in touches){auto sample=[self runtimeSample:touch identity:touch];
        if(callbacks.touch_begin)callbacks.touch_begin(sample.timestamp);
        const bool eligible=touch.type==UITouchTypeDirect||touch.type==UITouchTypePencil;
        const bool scrollable=eligible&&callbacks.touch_scrollable&&callbacks.touch_scrollable(sample.x,sample.y);
        [self emitTouchBatch:touchRouter.down(sample,scrollable)];}
    }catch(...){if(callbacks.failure)callbacks.failure(std::current_exception());}}
- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {try {
    for(UITouch* changed in touches){UITouch* other=nil;
        if(changed.type!=UITouchTypePencil)for(UITouch* touch in event.allTouches)
            if(touch!=changed&&touch.view==self&&touch.phase!=UITouchPhaseEnded&&touch.phase!=UITouchPhaseCancelled){other=touch;break;}
        if(other){auto sample=[self runtimeSample:changed identity:changed],second=[self runtimeSample:other identity:other];
            [self emitTouchBatch:touchRouter.move(sample,second)];continue;}
        auto samples=[event coalescedTouchesForTouch:changed];
        if(samples.count)for(UITouch* touch in samples)[self emitTouchBatch:touchRouter.move([self runtimeSample:touch identity:changed])];
        else [self emitTouchBatch:touchRouter.move([self runtimeSample:changed identity:changed])];
    }
    }catch(...){if(callbacks.failure)callbacks.failure(std::current_exception());}}
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {(void)event;try {
    double time=0;for(UITouch* touch in touches){auto sample=[self runtimeSample:touch identity:touch];time=std::max(time,sample.timestamp);[self emitTouchBatch:touchRouter.up(sample)];}
    if(touchRouter.empty()&&callbacks.touch_end)callbacks.touch_end(time);
    }catch(...){if(callbacks.failure)callbacks.failure(std::current_exception());}}
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {(void)event;try {
    if(callbacks.touch_cancel)callbacks.touch_cancel();
    for(UITouch* touch in touches)[self emitTouchBatch:touchRouter.cancel(std::uint64_t((__bridge void*)touch))];
    }catch(...){if(callbacks.failure)callbacks.failure(std::current_exception());}}
- (void)hover:(UIHoverGestureRecognizer*)g {auto p=[g locationInView:self];[self deliver:WidgetInput{.kind=g.state==UIGestureRecognizerStateEnded?WidgetInputKind::pointer_leave:WidgetInputKind::pointer_move,.x=p.x,.y=p.y,.device=PointerDevice::mouse,.pressure=0}];}
- (void)pressesBegan:(NSSet<UIPress*>*)presses withEvent:(UIPressesEvent*)event {bool handled=false;for(UIPress* press in presses){auto k=press.key;if(!k)continue;auto modifiers=k.modifierFlags;WidgetInput e{.kind=WidgetInputKind::key_down,.key=key(k.keyCode),.shift=bool(modifiers&UIKeyModifierShift),.text=k.charactersIgnoringModifiers.UTF8String?:"",.control=bool(modifiers&UIKeyModifierControl),.alt=bool(modifiers&UIKeyModifierAlternate),.meta=bool(modifiers&UIKeyModifierCommand)};const bool has_editor=bool([self editor]);if((e.meta||e.control)&&e.text=="a"&&has_editor)e.key=WidgetKey::select_all;if(ii::gui::detail::mobile_text_service_key(e,has_editor))continue;if(e.key!=WidgetKey::character||e.meta||e.control){[self deliver:std::move(e)];handled=true;}}if(!handled)[super pressesBegan:presses withEvent:event];}
- (void)resignForBackground:(NSNotification*)notification {(void)notification;(void)touchRouter.cancel();try{if(callbacks.input)callbacks.input({.kind=WidgetInputKind::focus_lost});}catch(...){if(callbacks.failure)callbacks.failure(std::current_exception());}pointer=nil;}
- (NSArray*)accessibilityElements {if(!callbacks.accessibility)return @[];NSMutableArray* result=[NSMutableArray new];auto snapshot=callbacks.accessibility();
    auto live=[NSMutableSet new];for(const auto& n:snapshot)[live addObject:native(n.id)];
    for(NSString* key in peers.allKeys)if(![live containsObject:key]){auto item=peers[key];item->read={};item->send={};[peers removeObjectForKey:key];}
    for(auto& n:snapshot){if(!n.visible||n.input_blocked)continue;auto id=native(n.id);auto item=peers[id];if(!item){item=[[IIGUIIOSElement alloc] initWithAccessibilityContainer:self];item->identity=n.id;auto read=callbacks.lookup;auto key=n.id;item->read=[read,key]{return read?read(key):WidgetSnapshot{};};item->send=callbacks.input;peers[id]=item;}[result addObject:item];}return result;}
@end

namespace ii::gui::detail {
struct NativeWidgetSurface::Impl final {
    IIGUIIOSSurface* view=nil;std::unique_ptr<NativeCanvas> canvas;bool paced=false,dirty=true;
    Size rendered;double scale=0;
    void draw(){if(!view||!view->callbacks.frame||UIApplication.sharedApplication.applicationState==UIApplicationStateBackground)return;auto s=view.bounds.size;if(s.width<=0||s.height<=0)return;auto frame=view->callbacks.frame({s.width,s.height});if(!valid_canvas(frame))throw std::runtime_error("Invalid UIKit native widget draw list");auto dpi=view.window.screen.scale;canvas->draw(frame,dpi,s.height);rendered={s.width,s.height};scale=dpi;dirty=false;}
    ~Impl(){if(view){[NSNotificationCenter.defaultCenter removeObserver:view];view->paint={};view->callbacks={};view->textTokenizer=nil;view->textDelegate=nil;view->markedStyle=nil;for(IIGUIIOSElement* item in view->peers.allValues){item->read={};item->send={};}[view->peers removeAllObjects];canvas.reset();[view removeFromSuperview];}}
};
NativeWidgetSurface::NativeWidgetSurface(void* value,WidgetSurfaceCallbacks callbacks):impl_(std::make_unique<Impl>()){
    fonts();auto parent=(__bridge UIView*)value;if(!parent)throw std::invalid_argument("UIKit surface requires a parent");auto view=[[IIGUIIOSSurface alloc] initWithFrame:parent.bounds];impl_->view=view;view->callbacks=std::move(callbacks);initialize_uikit_text_input(view);view->peers=[NSMutableDictionary new];view->editing=@"";view->hiddenKeyboard=[[UIView alloc] initWithFrame:CGRectZero];view.multipleTouchEnabled=YES;view.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;view.opaque=NO;view.accessibilityIdentifier=@"iiGUI.widget.surface";[parent addSubview:view];[view addGestureRecognizer:[[UIHoverGestureRecognizer alloc] initWithTarget:view action:@selector(hover:)]];[NSNotificationCenter.defaultCenter addObserver:view selector:@selector(resignForBackground:) name:UIApplicationWillResignActiveNotification object:nil];impl_->canvas=std::make_unique<NativeCanvas>((__bridge void*)view,nullptr,Acceleration::prefer_hardware,view->callbacks.renderer,view->callbacks.presentations);auto owner=impl_.get();view->paint=[owner]{owner->dirty=true;if(!owner->paced)owner->draw();};[view becomeFirstResponder];invalidate();
}
NativeWidgetSurface::~NativeWidgetSurface()=default;
void NativeWidgetSurface::invalidate(){impl_->dirty=true;if(!impl_->paced)[impl_->view setNeedsDisplay];}
void NativeWidgetSurface::accessibility_changed(){auto held=impl_;if(held&&held->view)[held->view synchronizeEditor];UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,nil);}
void NativeWidgetSurface::accessibility_value_changed(std::string_view){auto held=impl_;if(held&&held->view)[held->view synchronizeEditor];UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,nil);}
runtime::Task NativeWidgetSurface::begin_text_input_change(bool value_changed){auto held=impl_;return held&&held->view?begin_uikit_text_input_change(held->view,value_changed):runtime::Task{};}
bool NativeWidgetSurface::reduced_motion()const{return UIAccessibilityIsReduceMotionEnabled();}
void NativeWidgetSurface::set_frame_paced(bool v){impl_->paced=v;if(!v)invalidate();}
void NativeWidgetSurface::flush(){if(impl_->dirty||size()!=impl_->rendered||backing_scale()!=impl_->scale)impl_->draw();}
Size NativeWidgetSurface::size()const{auto s=impl_->view.bounds.size;return {s.width,s.height};}
double NativeWidgetSurface::backing_scale()const{return impl_->view.window.screen.scale;}
std::string NativeWidgetSurface::resolve_font_family(std::string_view family)const{return FontDatabase::application()->resolve_family(family);}
GraphicsInfo NativeWidgetSurface::info(){return impl_->canvas->info();}
PixelBuffer NativeWidgetSurface::capture(){impl_->draw();return impl_->canvas->capture();}
bool NativeWidgetSurface::native_event(const void*){return false;}
}
