#include "FontDatabase.h"
#import <UIKit/UIKit.h>
#import <CoreText/CoreText.h>
#include "WidgetSurface.h"
#include "NativeCanvas.h"
#include "MobileInput.h"
#include "Desktop.h"
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
    !(n.accessibility_semantics&&(n.accessibility_semantics->role==AccessibilityRole::tab_group||n.accessibility_semantics->role==AccessibilityRole::menu));}
- (NSString*)accessibilityIdentifier{return native(identity);}
- (NSString*)accessibilityLabel {auto n=read?read():WidgetSnapshot{};return native(!n.accessibility_name.empty()?n.accessibility_name:n.text.empty()?n.icon_name:n.text);}
- (NSString*)accessibilityValue {auto n=read?read():WidgetSnapshot{};if(n.password)return @"";if(n.accessibility_semantics&&(n.accessibility_semantics->role==AccessibilityRole::progress||n.accessibility_semantics->role==AccessibilityRole::slider)){if(!n.accessibility_semantics->range)return nil;return [NSNumberFormatter localizedStringFromNumber:@((n.value-n.minimum)/(n.maximum-n.minimum)) numberStyle:NSNumberFormatterPercentStyle];}if(n.kind==WidgetKind::action_surface){if(n.action_role==ActionRole::toggle_switch)return NSLocalizedString(n.checked?@"On":@"Off",nil);if(n.action_role==ActionRole::check_box||n.action_role==ActionRole::radio_button||n.action_role==ActionRole::tab||n.action_role==ActionRole::menu_item)return nil;}return native(n.text);}
- (UIAccessibilityTraits)accessibilityTraits {auto n=read?read():WidgetSnapshot{};UIAccessibilityTraits t=n.kind==WidgetKind::button||n.kind==WidgetKind::action_surface||n.kind==WidgetKind::menu_item?UIAccessibilityTraitButton:n.kind==WidgetKind::slider?UIAccessibilityTraitAdjustable:UIAccessibilityTraitStaticText;if(n.accessibility_semantics&&n.accessibility_semantics->role==AccessibilityRole::image)t=UIAccessibilityTraitImage;else if(n.accessibility_semantics&&n.accessibility_semantics->role==AccessibilityRole::progress)t=UIAccessibilityTraitUpdatesFrequently;else if(n.accessibility_semantics&&(n.accessibility_semantics->role==AccessibilityRole::slider||n.accessibility_semantics->role==AccessibilityRole::stepper))t=UIAccessibilityTraitAdjustable;if(!n.enabled)t|=UIAccessibilityTraitNotEnabled;if(n.selected||n.checked)t|=UIAccessibilityTraitSelected;return t;}
- (CGRect)accessibilityFrameInContainerSpace {auto n=read?read():WidgetSnapshot{};auto r=n.visual_bounds;if(n.visual_clip_bounds){auto c=*n.visual_clip_bounds;float right=std::min(r.x+r.width,c.x+c.width),bottom=std::min(r.y+r.height,c.y+c.height);r.x=std::max(r.x,c.x);r.y=std::max(r.y,c.y);r.width=std::max(0.f,right-r.x);r.height=std::max(0.f,bottom-r.y);}return CGRectMake(r.x,r.y,r.width,r.height);}
- (BOOL)accessibilityActivate {auto s=send;if(!s)return NO;s({.kind=WidgetInputKind::activate,.target=identity});return YES;}
- (void)accessibilityIncrement {if(send)send({.kind=WidgetInputKind::increment,.target=identity});}
- (void)accessibilityDecrement {if(send)send({.kind=WidgetInputKind::decrement,.target=identity});}
@end

@interface IIGUIIOSSurface : UIView <UIKeyInput, UITextInputTraits> {
@public ii::gui::detail::WidgetSurfaceCallbacks callbacks;std::function<void()> paint;
    __weak UITouch* pointer;NSMutableDictionary<NSString*,IIGUIIOSElement*>* peers;
    NSString* editing;UIView* hiddenKeyboard;CGPoint panOrigin;CGFloat pinchScale;
}
@end
@implementation IIGUIIOSSurface
- (BOOL)canBecomeFirstResponder{return YES;}
- (UIView*)inputView{return [self editor]?nil:hiddenKeyboard;}
- (std::optional<WidgetSnapshot>)editor {if(callbacks.accessibility)for(auto& n:callbacks.accessibility())if(n.visible&&n.focused&&n.text_input&&!n.input_blocked)return n;return {};}
- (UIKeyboardType)keyboardType {auto e=[self editor];if(e&&(e->accessibility_name=="Width"||e->accessibility_name=="Height"||e->accessibility_name=="Print resolution (PPI)"))return UIKeyboardTypeDecimalPad;return UIKeyboardTypeDefault;}
- (UITextAutocorrectionType)autocorrectionType{return UITextAutocorrectionTypeNo;}
- (BOOL)isSecureTextEntry {auto e=[self editor];return e&&e->password;}
- (UITextAutocapitalizationType)autocapitalizationType{return UITextAutocapitalizationTypeNone;}
- (BOOL)hasText {auto e=[self editor];return e&&e->text_length>0;}
- (void)insertText:(NSString*)text {if(!text.UTF8String)return;if([text isEqualToString:@"\n"])[self deliver:WidgetInput{.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}];else [self deliver:WidgetInput{.kind=WidgetInputKind::text,.text=text.UTF8String}];}
- (void)deleteBackward {[self deliver:WidgetInput{.kind=WidgetInputKind::key_down,.key=WidgetKey::backspace}];}
- (void)deliver:(WidgetInput)value {try{if(callbacks.input)callbacks.input(std::move(value));auto e=[self editor];auto next=e?native(e->id):@"";if(![next isEqualToString:editing]){editing=next;[self reloadInputViews];}if(!self.isFirstResponder)[self becomeFirstResponder];}catch(...){if(callbacks.failure)callbacks.failure(std::current_exception());}}
- (void)drawRect:(CGRect)r {(void)r;try{if(paint)paint();}catch(...){if(callbacks.failure)callbacks.failure(std::current_exception());}}
- (void)layoutSubviews {[super layoutSubviews];[self setNeedsDisplay];}
- (void)sample:(UITouch*)touch kind:(WidgetInputKind)kind {auto p=[touch locationInView:self];auto active=pointer?:touch;auto e=ii::gui::detail::touch_input(kind,{p.x,p.y,touch.force,touch.maximumPossibleForce,touch.altitudeAngle,[touch azimuthAngleInView:self],touch.timestamp,std::uint64_t((__bridge void*)active),touch.type==UITouchTypePencil});[self deliver:std::move(e)];}
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {if(event.allTouches.count>1){if(pointer)[self deliver:WidgetInput{.kind=WidgetInputKind::focus_lost}];pointer=nil;return;}if(!pointer){pointer=touches.anyObject;[self sample:pointer kind:WidgetInputKind::pointer_down];}}
- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {if(!pointer||![touches containsObject:pointer])return;auto samples=[event coalescedTouchesForTouch:pointer];if(samples.count)for(UITouch* t in samples)[self sample:t kind:WidgetInputKind::pointer_move];else [self sample:pointer kind:WidgetInputKind::pointer_move];}
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {(void)event;if(pointer&&[touches containsObject:pointer]){[self sample:pointer kind:WidgetInputKind::pointer_up];pointer=nil;}}
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {(void)touches;(void)event;[self deliver:WidgetInput{.kind=WidgetInputKind::focus_lost}];pointer=nil;}
- (void)pan:(UIPanGestureRecognizer*)g {CGPoint p=[g locationInView:self],translation=[g translationInView:self];if(g.state==UIGestureRecognizerStateBegan){panOrigin=translation;[self deliver:WidgetInput{.kind=WidgetInputKind::focus_lost}];pointer=nil;}else if(g.state==UIGestureRecognizerStateChanged){[self deliver:WidgetInput{.kind=WidgetInputKind::scroll,.x=p.x,.y=p.y,.scroll_y=double(panOrigin.y-translation.y),.scroll_x=double(panOrigin.x-translation.x),.precise_scroll=true,.device=PointerDevice::touch}];panOrigin=translation;}}
- (void)pinch:(UIPinchGestureRecognizer*)g {if(g.state==UIGestureRecognizerStateBegan){pinchScale=g.scale;[self deliver:WidgetInput{.kind=WidgetInputKind::focus_lost}];pointer=nil;}else if(g.state==UIGestureRecognizerStateChanged&&pinchScale>0){auto p=[g locationInView:self];[self deliver:WidgetInput{.kind=WidgetInputKind::scroll,.x=p.x,.y=p.y,.scroll_y=-std::log(double(g.scale/pinchScale))*240,.control=true,.precise_scroll=true,.device=PointerDevice::touch}];pinchScale=g.scale;}}
- (void)hover:(UIHoverGestureRecognizer*)g {auto p=[g locationInView:self];[self deliver:WidgetInput{.kind=g.state==UIGestureRecognizerStateEnded?WidgetInputKind::pointer_leave:WidgetInputKind::pointer_move,.x=p.x,.y=p.y,.device=PointerDevice::mouse,.pressure=0}];}
- (void)pressesBegan:(NSSet<UIPress*>*)presses withEvent:(UIPressesEvent*)event {bool handled=false;for(UIPress* press in presses){auto k=press.key;if(!k)continue;auto modifiers=k.modifierFlags;WidgetInput e{.kind=WidgetInputKind::key_down,.key=key(k.keyCode),.shift=bool(modifiers&UIKeyModifierShift),.text=k.charactersIgnoringModifiers.UTF8String?:"",.control=bool(modifiers&UIKeyModifierControl),.alt=bool(modifiers&UIKeyModifierAlternate),.meta=bool(modifiers&UIKeyModifierCommand)};const bool has_editor=bool([self editor]);if((e.meta||e.control)&&e.text=="a"&&has_editor)e.key=WidgetKey::select_all;if(ii::gui::detail::mobile_text_service_key(e,has_editor))continue;if(e.key!=WidgetKey::character||e.meta||e.control){[self deliver:std::move(e)];handled=true;}}if(!handled)[super pressesBegan:presses withEvent:event];}
- (void)resignForBackground:(NSNotification*)notification {(void)notification;try{if(callbacks.input)callbacks.input({.kind=WidgetInputKind::focus_lost});}catch(...){if(callbacks.failure)callbacks.failure(std::current_exception());}pointer=nil;}
- (NSArray*)accessibilityElements {if(!callbacks.accessibility)return @[];NSMutableArray* result=[NSMutableArray new];auto snapshot=callbacks.accessibility();for(auto& n:snapshot){if(!n.visible||n.input_blocked)continue;auto id=native(n.id);auto item=peers[id];if(!item){item=[[IIGUIIOSElement alloc] initWithAccessibilityContainer:self];item->identity=n.id;auto read=callbacks.lookup;auto key=n.id;item->read=[read,key]{return read?read(key):WidgetSnapshot{};};item->send=callbacks.input;peers[id]=item;}[result addObject:item];}return result;}
@end

namespace ii::gui::detail {
struct WidgetSurface::Impl final {
    IIGUIIOSSurface* view=nil;std::unique_ptr<NativeCanvas> canvas;bool paced=false,dirty=true;
    Size rendered;double scale=0;
    void draw(){if(!view||!view->callbacks.frame||UIApplication.sharedApplication.applicationState==UIApplicationStateBackground)return;auto s=view.bounds.size;if(s.width<=0||s.height<=0)return;auto frame=view->callbacks.frame({s.width,s.height});if(!valid_canvas(frame))throw std::runtime_error("Invalid UIKit native widget draw list");auto dpi=view.window.screen.scale;canvas->draw(frame,dpi,s.height);rendered={s.width,s.height};scale=dpi;dirty=false;}
    ~Impl(){if(view){[NSNotificationCenter.defaultCenter removeObserver:view];view->paint={};view->callbacks={};for(IIGUIIOSElement* item in view->peers.allValues){item->read={};item->send={};}[view->peers removeAllObjects];canvas.reset();[view removeFromSuperview];}}
};
WidgetSurface::WidgetSurface(void* value,WidgetSurfaceCallbacks callbacks):impl_(std::make_unique<Impl>()){
    fonts();auto parent=(__bridge UIView*)value;if(!parent)throw std::invalid_argument("UIKit surface requires a parent");auto view=[[IIGUIIOSSurface alloc] initWithFrame:parent.bounds];impl_->view=view;view->callbacks=std::move(callbacks);view->peers=[NSMutableDictionary new];view->editing=@"";view->hiddenKeyboard=[[UIView alloc] initWithFrame:CGRectZero];view.multipleTouchEnabled=YES;view.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;view.opaque=NO;view.accessibilityIdentifier=@"iiGUI.widget.surface";[parent addSubview:view];auto pan=[[UIPanGestureRecognizer alloc] initWithTarget:view action:@selector(pan:)];pan.minimumNumberOfTouches=2;[view addGestureRecognizer:pan];[view addGestureRecognizer:[[UIPinchGestureRecognizer alloc] initWithTarget:view action:@selector(pinch:)]];[view addGestureRecognizer:[[UIHoverGestureRecognizer alloc] initWithTarget:view action:@selector(hover:)]];[NSNotificationCenter.defaultCenter addObserver:view selector:@selector(resignForBackground:) name:UIApplicationWillResignActiveNotification object:nil];impl_->canvas=std::make_unique<NativeCanvas>((__bridge void*)view,nullptr,Acceleration::prefer_hardware,view->callbacks.renderer,view->callbacks.presentations);auto owner=impl_.get();view->paint=[owner]{owner->dirty=true;if(!owner->paced)owner->draw();};[view becomeFirstResponder];invalidate();
}
WidgetSurface::~WidgetSurface()=default;
void WidgetSurface::invalidate(){impl_->dirty=true;if(!impl_->paced)[impl_->view setNeedsDisplay];}
void WidgetSurface::accessibility_changed(){UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,nil);}
void WidgetSurface::accessibility_value_changed(std::string_view){UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,nil);}
void WidgetSurface::set_frame_paced(bool v){impl_->paced=v;if(!v)invalidate();}
void WidgetSurface::flush(){if(impl_->dirty||size()!=impl_->rendered||backing_scale()!=impl_->scale)impl_->draw();}
Size WidgetSurface::size()const{auto s=impl_->view.bounds.size;return {s.width,s.height};}
double WidgetSurface::backing_scale()const{return impl_->view.window.screen.scale;}
std::string WidgetSurface::resolve_font_family(std::string_view family)const{return FontDatabase::application()->resolve_family(family);}
GraphicsInfo WidgetSurface::info(){return impl_->canvas->info();}
PixelBuffer WidgetSurface::capture(){impl_->draw();return impl_->canvas->capture();}
bool WidgetSurface::native_event(const void*){return false;}
}
