#import <AppKit/AppKit.h>
#import <CoreText/CoreText.h>
#include "WidgetSurface.h"
#include "NativeCanvas.h"
#include "DesktopCommon.h"
#include "Desktop.h"
#include <mutex>

namespace {
using namespace ii::gui;
NSString* native(std::string_view v) { return [[NSString alloc] initWithBytes:v.data() length:v.size() encoding:NSUTF8StringEncoding]; }
void register_fonts() {
    static std::once_flag once;
    std::call_once(once,[] {
        for(NSString* face in @[@"Regular",@"Medium",@"SemiBold",@"Bold"]) {
            auto url=[NSBundle.mainBundle URLForResource:[@"Pretendard-" stringByAppendingString:face] withExtension:@"ttf"];
            if(url) CTFontManagerRegisterFontsForURL((__bridge CFURLRef)url,kCTFontManagerScopeProcess,nullptr);
        }
        auto inter=[NSBundle.mainBundle URLForResource:@"Inter-Regular" withExtension:@"ttf"];
        if(inter) CTFontManagerRegisterFontsForURL((__bridge CFURLRef)inter,kCTFontManagerScopeProcess,nullptr);
    });
}
WidgetKey key(unsigned short code) {
    switch(code) {
        case 48:return WidgetKey::tab; case 36:case 76:return WidgetKey::enter; case 49:return WidgetKey::space;
        case 53:return WidgetKey::escape; case 123:return WidgetKey::left; case 124:return WidgetKey::right;
        case 125:return WidgetKey::down; case 126:return WidgetKey::up; case 115:return WidgetKey::home;
        case 119:return WidgetKey::end; case 51:return WidgetKey::backspace; case 117:return WidgetKey::delete_forward;
        default:return WidgetKey::none;
    }
}
WidgetInput native_input(NSEvent* event,WidgetInputKind kind){
    WidgetInput value;value.kind=kind;value.shift=(event.modifierFlags&NSEventModifierFlagShift)!=0;
    value.control=(event.modifierFlags&NSEventModifierFlagControl)!=0;value.alt=(event.modifierFlags&NSEventModifierFlagOption)!=0;
    value.meta=(event.modifierFlags&NSEventModifierFlagCommand)!=0;value.timestamp=event.timestamp;value.buttons=std::uint32_t(NSEvent.pressedMouseButtons);
    const auto type=event.type;
    const bool mouse=type==NSEventTypeLeftMouseDown||type==NSEventTypeLeftMouseUp||type==NSEventTypeLeftMouseDragged||type==NSEventTypeRightMouseDown||type==NSEventTypeRightMouseUp||type==NSEventTypeRightMouseDragged||type==NSEventTypeOtherMouseDown||type==NSEventTypeOtherMouseUp||type==NSEventTypeOtherMouseDragged||type==NSEventTypeMouseMoved;
    if(type==NSEventTypeTabletPoint||(mouse&&event.subtype==NSEventSubtypeTabletPoint)){
        value.device=event.pointingDeviceType==NSPointingDeviceTypeEraser?PointerDevice::eraser:PointerDevice::pen;
        value.device_id=event.deviceID;value.pressure=std::clamp(double(event.pressure),0.,1.);
        value.tilt_x=std::clamp(event.tilt.x*90.,-90.,90.);value.tilt_y=std::clamp(event.tilt.y*90.,-90.,90.);value.rotation=event.rotation;
    }
    return value;
}
}
// Virtual accessibility peers, never OS views or controls. All values and
// geometry are queried from the C++ widget tree.
@interface IIGUIWidgetElement : NSAccessibilityElement {
@public
    std::string identity;
    __weak NSView* surface;
    std::function<WidgetSnapshot()> read;
    std::function<void(WidgetInput)> send;
    NSMutableArray* entries;
}
@end
@implementation IIGUIWidgetElement
- (BOOL)isAccessibilityElement { auto n=read?read():WidgetSnapshot{};return n.attached && n.visible && !n.input_blocked; }
- (NSArray*)accessibilityChildren { return entries?[entries copy]:@[]; }
- (NSString*)accessibilityIdentifier { return native(identity); }
- (NSString*)accessibilityRole {
    auto n=read?read():WidgetSnapshot{};
    switch(n.kind) {
        case WidgetKind::menu:return NSAccessibilityMenuRole;
        case WidgetKind::popover:return NSAccessibilityGroupRole;
        case WidgetKind::scroll_view:return NSAccessibilityScrollAreaRole;
        case WidgetKind::menu_item:return NSAccessibilityMenuItemRole;
        case WidgetKind::button:case WidgetKind::combo_box:return NSAccessibilityButtonRole;
        case WidgetKind::action_surface:return n.action_role==ActionRole::link?NSAccessibilityLinkRole:NSAccessibilityButtonRole;
        case WidgetKind::stepper:return NSAccessibilityIncrementorRole;
        case WidgetKind::check_box:case WidgetKind::toggle_switch:return NSAccessibilityCheckBoxRole;
        case WidgetKind::radio_button:return NSAccessibilityRadioButtonRole;
        case WidgetKind::tab:return NSAccessibilityRadioButtonRole;
        case WidgetKind::input_field:return NSAccessibilityTextFieldRole;
        case WidgetKind::slider:return NSAccessibilitySliderRole;
        case WidgetKind::progress:return NSAccessibilityProgressIndicatorRole;
        case WidgetKind::color_surface:return NSAccessibilityColorWellRole;
        case WidgetKind::image:return NSAccessibilityImageRole;
        default:return NSAccessibilityStaticTextRole;
    }
}
- (NSString*)accessibilityLabel {
    auto n=read?read():WidgetSnapshot{};
    if(!n.accessibility_name.empty()) return native(n.accessibility_name);
    if(n.kind==WidgetKind::menu) return native(n.id);
    return native(n.kind==WidgetKind::input_field?(n.placeholder.empty()?n.id:n.placeholder):n.text.empty()?n.icon_name:n.text);
}
- (NSString*)accessibilityHelp { auto n=read?read():WidgetSnapshot{};return n.accessibility_description.empty()?nil:native(n.accessibility_description); }
- (BOOL)isAccessibilitySelected { return read && read().selected; }
- (id)accessibilityValue {
    if(read && read().password)return @"";
    auto n=read?read():WidgetSnapshot{};
    if(n.kind==WidgetKind::tab) return @(n.selected);
    if(n.kind==WidgetKind::check_box || n.kind==WidgetKind::radio_button || n.kind==WidgetKind::toggle_switch) return @(n.checked);
    if(n.kind==WidgetKind::slider || n.kind==WidgetKind::progress) return @(n.value);
    if(n.color_state)return native("#"+n.color_state->hex);
    return native(n.text);
}
- (BOOL)isAccessibilityEnabled { auto n=read?read():WidgetSnapshot{};return n.attached && n.enabled && n.visible && !n.input_blocked; }
- (BOOL)isAccessibilityFocused { return read && read().focused; }
- (BOOL)isAccessibilitySelectorAllowed:(SEL)selector {
    auto n=read?read():WidgetSnapshot{};
    if(!n.attached || !n.visible || n.input_blocked) return NO;
    if(selector==@selector(accessibilityPerformShowMenu)) return n.enabled && n.has_context_menu;
    if(selector==@selector(accessibilityPerformCancel)) return (n.kind==WidgetKind::menu||n.kind==WidgetKind::popover)&&n.opened;
    if(selector==@selector(setAccessibilityValue:)) return n.kind==WidgetKind::input_field && n.enabled && !n.read_only;
    if(selector==@selector(accessibilityPerformIncrement)) return n.enabled && ((n.kind==WidgetKind::stepper && n.arrow!=StepperArrow::down) || (n.color_state && n.color_field>=ColorField::hue_rail));
    if(selector==@selector(accessibilityPerformDecrement)) return n.enabled && ((n.kind==WidgetKind::stepper && n.arrow!=StepperArrow::up) || (n.color_state && n.color_field>=ColorField::hue_rail));
    if(selector==@selector(accessibilityPerformPress) || selector==@selector(setAccessibilityFocused:))
        return n.enabled && (n.kind==WidgetKind::button || n.kind==WidgetKind::action_surface || n.kind==WidgetKind::check_box || n.kind==WidgetKind::radio_button || n.kind==WidgetKind::toggle_switch || n.kind==WidgetKind::input_field || n.kind==WidgetKind::slider || n.kind==WidgetKind::tab || n.kind==WidgetKind::stepper || n.kind==WidgetKind::combo_box || n.kind==WidgetKind::menu_item || n.kind==WidgetKind::color_surface);
    return [super isAccessibilitySelectorAllowed:selector];
}
- (void)setAccessibilityFocused:(BOOL)value {
    auto n=read?read():WidgetSnapshot{};if(!n.attached || !n.enabled || !n.visible || n.input_blocked) return;
    auto action=send; if(value && action) action({.kind=WidgetInputKind::focus,.target=identity});
    if(value && surface) NSAccessibilityPostNotification(surface,NSAccessibilityFocusedUIElementChangedNotification);
}
- (NSRect)accessibilityFrame {
    if(!surface || !read) return NSZeroRect;
    auto n=read();auto r=n.visual_bounds;
    if(n.visual_clip_bounds) {auto c=*n.visual_clip_bounds;const auto right=std::min(r.x+r.width,c.x+c.width),bottom=std::min(r.y+r.height,c.y+c.height);r.x=std::max(r.x,c.x);r.y=std::max(r.y,c.y);r.width=std::max(0.f,right-r.x);r.height=std::max(0.f,bottom-r.y);}
    return [surface.window convertRectToScreen:[surface convertRect:NSMakeRect(r.x,r.y,r.width,r.height) toView:nil]];
}
- (BOOL)accessibilityPerformPress {
    auto action=send; auto id=identity;
    auto n=read?read():WidgetSnapshot{};
    if(!action || !n.enabled || !n.attached || !n.visible || n.input_blocked) return NO;
    if(n.kind==WidgetKind::input_field || n.kind==WidgetKind::slider || n.kind==WidgetKind::color_surface) {
        action({.kind=WidgetInputKind::focus,.target=std::move(id)});
        if(surface) NSAccessibilityPostNotification(surface,NSAccessibilityFocusedUIElementChangedNotification);
        return YES;
    }
    action({.kind=WidgetInputKind::activate,.target=std::move(id)}); return YES;
}
- (BOOL)accessibilityPerformCancel {
    auto n=read?read():WidgetSnapshot{};auto action=send;
    if(!action || (n.kind!=WidgetKind::menu&&n.kind!=WidgetKind::popover) || !n.opened) return NO;
    action({.kind=WidgetInputKind::close_popup,.target=identity});return YES;
}
- (BOOL)accessibilityPerformShowMenu {
    auto n=read?read():WidgetSnapshot{};auto action=send;
    if(!action || !n.attached || !n.visible || n.input_blocked || !n.enabled || !n.has_context_menu) return NO;
    action({.kind=WidgetInputKind::context_request,.x=n.visual_bounds.x+n.visual_bounds.width/2,.y=n.visual_bounds.y+n.visual_bounds.height/2,.target=identity});return YES;
}
- (BOOL)accessibilityPerformIncrement {
    auto n=read?read():WidgetSnapshot{};auto action=send;
    if(!action || !n.attached || !n.visible || n.input_blocked || !n.enabled || !((n.kind==WidgetKind::stepper && n.arrow!=StepperArrow::down) || (n.color_state && n.color_field>=ColorField::hue_rail))) return NO;
    action({.kind=WidgetInputKind::increment,.target=identity});return YES;
}
- (BOOL)accessibilityPerformDecrement {
    auto n=read?read():WidgetSnapshot{};auto action=send;
    if(!action || !n.attached || !n.visible || n.input_blocked || !n.enabled || !((n.kind==WidgetKind::stepper && n.arrow!=StepperArrow::up) || (n.color_state && n.color_field>=ColorField::hue_rail))) return NO;
    action({.kind=WidgetInputKind::decrement,.target=identity});return YES;
}
- (void)setAccessibilityValue:(id)value {
    auto n=read?read():WidgetSnapshot{}; auto action=send;
    if(!action || !n.attached || !n.visible || n.input_blocked || !n.enabled || n.read_only || n.kind!=WidgetKind::input_field || ![value isKindOfClass:NSString.class]) return;
    auto id=identity;
    action({.kind=WidgetInputKind::focus,.target=id});
    action({.kind=WidgetInputKind::key_down,.key=WidgetKey::select_all});
    action({.kind=WidgetInputKind::text,.text=std::string([(NSString*)value UTF8String])});
    if(surface) NSAccessibilityPostNotification(surface,NSAccessibilityFocusedUIElementChangedNotification);
}
@end

// One OS input/presentation surface for the entire C++ widget tree. It creates
// no OS control or layout. Drawing goes through iiGUI's Metal/Vulkan renderer.
@interface IIGUIWidgetSurface : NSView <NSTextInputClient> {
@public
    std::function<void()> paint;
    std::function<void(WidgetInput)> send;
    std::function<std::vector<WidgetSnapshot>()> snapshots;
    std::function<WidgetSnapshot(std::string_view)> lookup;
    std::function<void(std::exception_ptr)> failure;
    NSMutableDictionary<NSString*,IIGUIWidgetElement*>* peers;
    NSString* marked;
    NSRange selection;
    NSTrackingArea* tracking;
    NSTextInputContext* textContext;
}
@end
@implementation IIGUIWidgetSurface
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent*)event { (void)event; return YES; }
- (NSView*)hitTest:(NSPoint)point { return NSPointInRect(point,self.frame)?self:nil; }
- (NSTextInputContext*)inputContext {
    if(!textContext) textContext=[[NSTextInputContext alloc] initWithClient:self];
    return textContext;
}
- (void)drawRect:(NSRect)dirty {
    (void)dirty; auto draw=paint;
    try { if(draw) draw(); } catch(...) { auto handler=failure; if(handler) handler(std::current_exception()); }
}
- (void)setFrameSize:(NSSize)size { [super setFrameSize:size]; self.needsDisplay=YES; }
- (void)updateTrackingAreas {
    [super updateTrackingAreas]; if(tracking) [self removeTrackingArea:tracking];
    tracking=[[NSTrackingArea alloc] initWithRect:NSZeroRect options:NSTrackingMouseMoved|NSTrackingMouseEnteredAndExited|NSTrackingActiveInActiveApp|NSTrackingInVisibleRect owner:self userInfo:nil];
    [self addTrackingArea:tracking];
}
- (void)pointer:(NSEvent*)event kind:(WidgetInputKind)kind {
    auto p=[self convertPoint:event.locationInWindow fromView:nil]; auto action=send;
    auto value=native_input(event,kind);value.x=p.x;value.y=p.y;if(action)action(std::move(value));
}
- (void)mouseDown:(NSEvent*)event { [self.window makeFirstResponder:self]; [self pointer:event kind:WidgetInputKind::pointer_down]; }
- (void)rightMouseDown:(NSEvent*)event { [self pointer:event kind:WidgetInputKind::context_request]; }
- (void)scrollWheel:(NSEvent*)event {
    auto p=[self convertPoint:event.locationInWindow fromView:nil];auto action=send;
    auto value=native_input(event,WidgetInputKind::scroll);value.x=p.x;value.y=p.y;value.precise_scroll=event.hasPreciseScrollingDeltas;
    value.scroll_y=-event.scrollingDeltaY*(value.precise_scroll?1:20);value.scroll_x=-event.scrollingDeltaX*(value.precise_scroll?1:20);if(action)action(std::move(value));
}
- (void)mouseUp:(NSEvent*)event { [self pointer:event kind:WidgetInputKind::pointer_up]; }
- (void)mouseMoved:(NSEvent*)event { [self pointer:event kind:WidgetInputKind::pointer_move]; }
- (void)mouseDragged:(NSEvent*)event { [self pointer:event kind:WidgetInputKind::pointer_move]; }
- (void)rightMouseDragged:(NSEvent*)event { [self pointer:event kind:WidgetInputKind::pointer_move]; }
- (void)rightMouseUp:(NSEvent*)event { [self pointer:event kind:WidgetInputKind::pointer_up]; }
- (void)otherMouseDown:(NSEvent*)event { [self.window makeFirstResponder:self];[self pointer:event kind:WidgetInputKind::pointer_down]; }
- (void)otherMouseDragged:(NSEvent*)event { [self pointer:event kind:WidgetInputKind::pointer_move]; }
- (void)otherMouseUp:(NSEvent*)event { [self pointer:event kind:WidgetInputKind::pointer_up]; }
- (void)tabletPoint:(NSEvent*)event { [self pointer:event kind:WidgetInputKind::pointer_move]; }
- (void)mouseExited:(NSEvent*)event { [self pointer:event kind:WidgetInputKind::pointer_leave]; }
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender { (void)sender;return NSDragOperationCopy; }
- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    auto content=ii::gui::detail::read_pasteboard((__bridge void*)sender.draggingPasteboard);if(!content)return NO;
    auto point=[self convertPoint:sender.draggingLocation fromView:nil];auto action=send;
    if(!action)return NO;WidgetInput event{.kind=WidgetInputKind::drop,.x=point.x,.y=point.y};event.content=std::make_shared<ClipboardContent>(std::move(*content));action(std::move(event));return YES;
}
- (BOOL)resignFirstResponder {
    marked=@""; auto action=send;
    if(action) { action({.kind=WidgetInputKind::composition}); action({.kind=WidgetInputKind::focus_lost}); }
    return [super resignFirstResponder];
}
- (void)keyDown:(NSEvent*)event {
    auto code=key(event.keyCode);
    bool editing=false;
    for(const auto& n:snapshots?snapshots():std::vector<WidgetSnapshot>{}) if(n.focused && !n.input_blocked && n.kind==WidgetKind::input_field) {editing=true;break;}
    if(event.modifierFlags&NSEventModifierFlagCommand){
        if(event.keyCode==0){[self selectAll:nil];return;}if(event.keyCode==9){[self paste:nil];return;}
        if(event.keyCode==8){[self copy:nil];return;}if(event.keyCode==7){[self cut:nil];return;}
    }
    if((code!=WidgetKey::none||!editing||(event.modifierFlags&(NSEventModifierFlagCommand|NSEventModifierFlagControl))) && (!self.hasMarkedText || !editing)) {
        auto value=native_input(event,WidgetInputKind::key_down);value.key=code==WidgetKey::none?WidgetKey::character:code;
        value.text=event.charactersIgnoringModifiers.lowercaseString.UTF8String?:"";auto action=send;if(action)action(std::move(value));
    } else [self interpretKeyEvents:@[event]];
}
- (void)keyUp:(NSEvent*)event {
    auto code=key(event.keyCode); auto action=send;
    auto value=native_input(event,WidgetInputKind::key_up);value.key=code==WidgetKey::none?WidgetKey::character:code;
    value.text=event.charactersIgnoringModifiers.lowercaseString.UTF8String?:"";if(action)action(std::move(value));
}
- (void)selectAll:(id)sender { (void)sender; auto action=send; if(action) action({.kind=WidgetInputKind::key_down,.key=WidgetKey::select_all}); }
- (void)paste:(id)sender {
    (void)sender;
    for(const auto& item:snapshots?snapshots():std::vector<WidgetSnapshot>{})if(item.focused&&item.kind==WidgetKind::input_field){
        auto text=[NSPasteboard.generalPasteboard stringForType:NSPasteboardTypeString];if(text)[self insertText:text replacementRange:NSMakeRange(NSNotFound,0)];return;
    }
    auto content=ii::gui::read_clipboard();if(!content)throw std::runtime_error(content.error().message);
    auto action=send;if(action)action({.kind=WidgetInputKind::text,.content=std::make_shared<const ClipboardContent>(std::move(*content))});
}
- (void)copy:(id)sender {
    (void)sender;for(const auto& item:snapshots?snapshots():std::vector<WidgetSnapshot>{})if(item.focused&&item.kind==WidgetKind::input_field){
        if(item.password)return;NSString* text=native(item.text);auto range=item.text_selection;
        if(range.location<=text.length&&range.length<=text.length-range.location){
            [NSPasteboard.generalPasteboard clearContents];[NSPasteboard.generalPasteboard setString:[text substringWithRange:NSMakeRange(range.location,range.length)] forType:NSPasteboardTypeString];
        }return;
    }
    auto action=send;if(action)action({.kind=WidgetInputKind::key_down,.key=WidgetKey::copy,.meta=true});
}
- (void)cut:(id)sender { [self copy:sender];auto action=send;if(action)action({.kind=WidgetInputKind::key_down,.key=WidgetKey::cut}); }
- (void)insertText:(id)value replacementRange:(NSRange)range {
    NSString* text=[value isKindOfClass:NSAttributedString.class]?[(NSAttributedString*)value string]:(NSString*)value;
    WidgetInput event{.kind=WidgetInputKind::text,.text=std::string(text.UTF8String)};
    if(range.location!=NSNotFound){auto current=[self markedRange];if(current.location==NSNotFound||!NSEqualRanges(current,range))event.replacement_range=ii::gui::TextRange{range.location,range.length};}
    marked=@"";auto action=send;if(action)action(std::move(event));
}
- (void)setMarkedText:(id)value selectedRange:(NSRange)selected replacementRange:(NSRange)range {
    auto current=[self markedRange];marked=[value isKindOfClass:NSAttributedString.class]?[(NSAttributedString*)value string]:(NSString*)value;selection=selected;
    WidgetInput event{.kind=WidgetInputKind::composition,.text=std::string(marked.UTF8String)};
    event.composition_selection=ii::gui::TextRange{selected.location,selected.length};
    if(range.location!=NSNotFound&&(current.location==NSNotFound||!NSEqualRanges(current,range)))event.replacement_range=ii::gui::TextRange{range.location,range.length};
    auto action=send;if(action)action(std::move(event));
}
- (void)unmarkText { marked=@""; auto action=send; if(action) action({.kind=WidgetInputKind::composition}); }
- (BOOL)hasMarkedText { return marked.length>0; }
- (NSRange)markedRange { for(const auto& n:snapshots?snapshots():std::vector<WidgetSnapshot>{})if(n.focused&&n.marked_range)return NSMakeRange(n.marked_range->location,n.marked_range->length);return NSMakeRange(NSNotFound,0); }
- (NSRange)selectedRange { for(const auto& n:snapshots?snapshots():std::vector<WidgetSnapshot>{})if(n.focused&&n.kind==WidgetKind::input_field)return NSMakeRange(n.text_selection.location,n.text_selection.length);return NSMakeRange(NSNotFound,0); }
- (NSArray<NSAttributedStringKey>*)validAttributesForMarkedText { return @[]; }
- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actual {
    (void)range; if(actual) *actual=NSMakeRange(NSNotFound,0); return nil;
}
- (NSUInteger)characterIndexForPoint:(NSPoint)point { (void)point; return 0; }
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actual {
    if(actual) *actual=range; auto list=snapshots?snapshots():std::vector<WidgetSnapshot>{};
    for(auto& item:list) if(item.focused) { auto r=item.visual_bounds; return [self.window convertRectToScreen:[self convertRect:NSMakeRect(r.x,r.y+r.height,1,16) toView:nil]]; }
    return [self.window convertRectToScreen:self.frame];
}
- (void)doCommandBySelector:(SEL)selector {
    if(selector==@selector(paste:)) { [self paste:nil]; return; }
    WidgetKey code=selector==@selector(deleteBackward:)?WidgetKey::backspace:selector==@selector(deleteForward:)?WidgetKey::delete_forward:
        selector==@selector(moveLeft:)?WidgetKey::left:selector==@selector(moveRight:)?WidgetKey::right:selector==@selector(insertTab:)?WidgetKey::tab:
        selector==@selector(insertNewline:)?WidgetKey::enter:WidgetKey::none;
    auto action=send; if(code!=WidgetKey::none && action) action({.kind=WidgetInputKind::key_down,.key=code});
}
- (BOOL)isAccessibilityElement { return NO; }
- (NSString*)accessibilityRole { return NSAccessibilityGroupRole; }
- (NSArray*)accessibilityChildren {
    if(!peers) peers=[NSMutableDictionary new];
    auto current=snapshots?snapshots():std::vector<WidgetSnapshot>{}; auto result=[NSMutableArray new];
    // Hidden static widgets keep their identity. Removed activation generations
    // leave the snapshot entirely and must not accumulate in the native cache.
    auto live=[NSMutableSet new];for(const auto& item:current)[live addObject:native(item.id)];
    for(NSString* key in peers.allKeys)if(![live containsObject:key]){
        auto peer=peers[key];peer->read={};peer->send={};peer->surface=nil;[peer->entries removeAllObjects];peer.accessibilityParent=nil;[peers removeObjectForKey:key];
    }
    for(IIGUIWidgetElement* peer in peers.allValues) {[peer->entries removeAllObjects];peer.accessibilityParent=nil;}
    for(auto& item:current) {
        if(!item.visible || item.input_blocked) continue;
        if(item.kind==WidgetKind::vertical_stack || item.kind==WidgetKind::horizontal_stack || item.kind==WidgetKind::overlay_stack || item.kind==WidgetKind::absolute_stack || item.kind==WidgetKind::spacer || item.kind==WidgetKind::tab_bar || item.kind==WidgetKind::segmented_control || item.kind==WidgetKind::menu_divider) continue;
        auto id=native(item.id); auto peer=peers[id];
        if(!peer) {
            peer=[IIGUIWidgetElement new];peer->entries=[NSMutableArray new]; peer->identity=item.id; peer->surface=self;
            __weak IIGUIWidgetSurface* weak=self; auto identity=item.id;
            peer->read=[weak,identity] {
                IIGUIWidgetSurface* view=weak; if(view && view->lookup) return view->lookup(identity);
                return WidgetSnapshot{};
            };
            peer->send=send;peers[id]=peer;
        }
        peer.accessibilityParent=self;
    }
    for(auto& item:current) {
        auto peer=peers[native(item.id)];if(!peer || !item.visible || item.input_blocked || !peer.accessibilityParent) continue;
        if(!item.popup_id.empty() && item.popup_id!=item.id) {
            auto parent=peers[native(item.popup_id)];if(parent) {[parent->entries addObject:peer];peer.accessibilityParent=parent;}
        } else [result addObject:peer];
    }
    return result;
}
- (id)accessibilityFocusedUIElement {
    for(IIGUIWidgetElement* peer in self.accessibilityChildren) {
        if(peer.isAccessibilityFocused) return peer;
        for(IIGUIWidgetElement* child in peer->entries) if(child.isAccessibilityFocused) return child;
    }
    return self;
}
@end

namespace ii::gui::detail {
struct WidgetSurface::Impl final {
    IIGUIWidgetSurface* view=nil;
    std::unique_ptr<NativeCanvas> canvas;
    WidgetSurfaceCallbacks callbacks;
    bool paced=false,dirty=true;
    Size rendered_size;
    double rendered_scale=0;
    void draw() {
        if(!view || !callbacks.frame) return;
        auto size=view.bounds.size; if(size.width<1 || size.height<1) return;
        auto frame=callbacks.frame({size.width,size.height});
        if(!valid_canvas(frame)) throw std::runtime_error("Invalid iiGUI widget draw list");
        canvas->draw(frame,view.window.backingScaleFactor,size.height);
        dirty=false;rendered_size={size.width,size.height};rendered_scale=view.window.backingScaleFactor;
    }
    ~Impl() {
        if(view) {
            view->paint={}; view->send={}; view->snapshots={}; view->lookup={}; view->failure={};
            for(IIGUIWidgetElement* peer in view->peers.allValues) { peer->read={}; peer->send={};[peer->entries removeAllObjects]; peer.accessibilityParent=nil; }
            [view->peers removeAllObjects]; view->textContext=nil;
            canvas.reset(); [view removeFromSuperview]; view=nil;
        }
    }
};
WidgetSurface::WidgetSurface(void* value,WidgetSurfaceCallbacks callbacks):impl_(std::make_unique<Impl>()) {
    register_fonts(); auto parent=(__bridge NSView*)value;
    if(!parent) throw std::invalid_argument("Widget surface requires a native parent");
    impl_->callbacks=std::move(callbacks);
    impl_->view=[[IIGUIWidgetSurface alloc] initWithFrame:parent.bounds];
    auto view=impl_->view; view.identifier=@"iiGUI.widget.surface";
    [view registerForDraggedTypes:@[NSPasteboardTypeFileURL,NSPasteboardTypeURL,NSPasteboardTypePNG,NSPasteboardTypeTIFF,NSPasteboardTypeString]];
    view.autoresizingMask=NSViewWidthSizable|NSViewHeightSizable; view.wantsLayer=YES;
    view->marked=@""; view->selection=NSMakeRange(0,0); view->send=impl_->callbacks.input;
    view->snapshots=impl_->callbacks.accessibility; view->lookup=impl_->callbacks.lookup; view->failure=impl_->callbacks.failure;
    [parent addSubview:view];
    impl_->canvas=std::make_unique<NativeCanvas>((__bridge void*)view,nullptr,Acceleration::prefer_hardware,impl_->callbacks.renderer);
    auto state=impl_.get(); view->paint=[state] { state->dirty=true;if(!state->paced) state->draw(); };
    [view.window makeFirstResponder:view]; invalidate();
}
WidgetSurface::~WidgetSurface()=default;
void WidgetSurface::invalidate() { impl_->dirty=true;if(!impl_->paced) impl_->view.needsDisplay=YES; }
void WidgetSurface::accessibility_changed() {
    NSAccessibilityPostNotification(impl_->view,NSAccessibilityLayoutChangedNotification);
    NSAccessibilityPostNotification(impl_->view,NSAccessibilityFocusedUIElementChangedNotification);
}
void WidgetSurface::accessibility_value_changed(std::string_view id) {
    auto peer=impl_->view->peers[native(id)];if(peer)NSAccessibilityPostNotification(peer,NSAccessibilityValueChangedNotification);
}
void WidgetSurface::set_frame_paced(bool value) { impl_->paced=value;if(value) impl_->view.needsDisplay=NO;else invalidate(); }
void WidgetSurface::flush() { if(!impl_->paced || impl_->dirty || size()!=impl_->rendered_size || impl_->rendered_scale!=impl_->view.window.backingScaleFactor) impl_->draw(); }
Size WidgetSurface::size() const { auto s=impl_->view.bounds.size; return {s.width,s.height}; }
double WidgetSurface::backing_scale() const {return impl_->view.window.backingScaleFactor;}
std::string WidgetSurface::resolve_font_family(std::string_view requested) const {
    auto family=native(requested);
    NSFont* font=[NSFont fontWithName:[family stringByAppendingString:@"-Regular"] size:13];
    if(!font) font=[NSFont fontWithName:family size:13];
    if(!font) font=[NSFont systemFontOfSize:13];
    return std::string(font.familyName.UTF8String);
}
GraphicsInfo WidgetSurface::info() { return impl_->canvas->info(); }
PixelBuffer WidgetSurface::capture() { return impl_->canvas->capture(); }
} // namespace ii::gui::detail

namespace ii::gui::detail { bool WidgetSurface::native_event(const void*){return false;} }
