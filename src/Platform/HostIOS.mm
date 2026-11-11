#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <QuartzCore/QuartzCore.h>
#include <iiGUI/Host.h>
#include "DisplayPixelsPolicy.h"
#include <iiGUI/HostCommon.h>
#include <iiGUI/HostActivity.h>
#include "HostFailure.h"
#include "HostFileRequests.h"
#include "FrameDemand.h"
#include "HostCallback.h"
#include "UIKitSceneBinding.h"
#include "UIKitSceneRouter.h"
#include "UIKitWindowGeometry.h"
#include "UIKitDisplayScale.h"
#include <iiGUI/DesktopCommon.h>
#include <iiGUI/DispatchState.h>
#include <iiGUI/CanvasState.h>
#include <iiGUI/WidgetState.h>
#include <iiGUI/WidgetSurfaceNative.h>
#include <algorithm>
#include <mutex>
#include <optional>
#include <stdexcept>

namespace {
std::function<void(UIWindowScene*, UISceneSession*, NSUserActivity*)> launch_scene;
std::function<void(UIWindowScene*, bool)> activate_scene;
std::function<void(UIWindowScene*)> disconnect_scene;
std::function<void(UISceneSession*)> discard_session;
std::function<void(bool)> foreground_application;
std::function<void(NSURL*)> open_document;
std::string native_error(NSException* e){const auto* message=e.reason.UTF8String;return message?message:"UIKit exception";}
NSString* native(std::string_view s) { return [[NSString alloc] initWithBytes:s.data() length:s.size() encoding:NSUTF8StringEncoding]; }
std::string session_identifier(UISceneSession* session) {
    NSData* bytes=[session.persistentIdentifier dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:NO];
    if(!bytes||!bytes.length||bytes.length>4096)throw std::invalid_argument("Invalid UIKit scene session identifier");
    return {static_cast<const char*>(bytes.bytes),bytes.length};
}
}
@interface IIGUIIOSController : UIViewController <UIDocumentPickerDelegate> {
@public
    std::function<void()> resize;
    std::function<void()> appeared;
    std::function<void(CADisplayLink*)> frame;
    std::function<void(NSArray<NSURL*>*)> picked;
    std::vector<ii::gui::AppMenuItem> menu_rows;
    ii::gui::UiExecutor menu_ui;
}
- (void)displayFrame:(CADisplayLink*)link;
@end
namespace {
UIMenu* native_menu(const ii::gui::AppMenuItem& row, ii::gui::UiExecutor ui) {
    NSMutableArray<UIMenuElement*>* children=[NSMutableArray new];
    for(const auto& item:row.children) {
        if(item.separator)continue;
        if(!item.children.empty()){[children addObject:native_menu(item,ui)];continue;}
        UIMenuElement* element=nil;
        if(item.shortcut){const auto& k=*item.shortcut;auto flags=(k.meta?UIKeyModifierCommand:0)|(k.control?UIKeyModifierControl:0)|(k.alt?UIKeyModifierAlternate:0)|(k.shift?UIKeyModifierShift:0);element=[UIKeyCommand commandWithTitle:native(item.title) image:nil action:@selector(iiguiCommand:) input:native(k.key) modifierFlags:flags propertyList:native(item.id)];}
        else {auto work=item.triggered;element=[UIAction actionWithTitle:native(item.title) image:nil identifier:native(item.id) handler:^(UIAction*){if(work)(void)ui.post(work);}];}
        if([element isKindOfClass:UICommand.class])((UICommand*)element).attributes=item.enabled?0:UIMenuElementAttributesDisabled;else ((UIAction*)element).attributes=item.enabled?0:UIMenuElementAttributesDisabled;
        if([element isKindOfClass:UICommand.class])((UICommand*)element).state=item.checked?UIMenuElementStateOn:UIMenuElementStateOff;
        else ((UIAction*)element).state=item.checked?UIMenuElementStateOn:UIMenuElementStateOff;
        [children addObject:element];
    }
    auto id=row.title=="File"?UIMenuFile:row.title=="Edit"?UIMenuEdit:native(row.id);
    return [UIMenu menuWithTitle:native(row.title) image:nil identifier:id options:0 children:children];
}
}
@implementation IIGUIIOSController
- (void)viewDidLayoutSubviews { [super viewDidLayoutSubviews]; if(resize) resize(); }
- (void)traitCollectionDidChange:(UITraitCollection*)previous {
    [super traitCollectionDidChange:previous];
    if(ii::gui::detail::uikit_display_scale_changed(previous.displayScale,self.traitCollection.displayScale) && resize)resize();
}
- (void)viewDidAppear:(BOOL)animated { [super viewDidAppear:animated]; if(appeared) appeared(); }
- (void)displayFrame:(CADisplayLink*)link { if(frame) frame(link); }
- (void)documentPicker:(UIDocumentPickerViewController*)picker didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls { (void)picker; auto call=picked;picked={};if(call)call(urls); }
- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)picker { (void)picker;auto call=picked;picked={};if(call)call(nil); }
- (BOOL)prefersStatusBarHidden { return YES; }
- (void)buildMenuWithBuilder:(id<UIMenuBuilder>)builder { [super buildMenuWithBuilder:builder];if(builder.system!=UIMenuSystem.mainSystem)return;for(const auto& row:menu_rows){auto menu=native_menu(row,menu_ui);if(row.title=="File"||row.title=="Edit")[builder replaceMenuForIdentifier:menu.identifier withMenu:menu];else {[builder removeMenuForIdentifier:menu.identifier];[builder insertChildMenu:menu atEndOfMenuForIdentifier:UIMenuRoot];}} }
- (void)iiguiCommand:(UICommand*)command { const auto* text=((NSString*)command.propertyList).UTF8String;const auto id=std::string(text?text:"");const auto ui=menu_ui;const auto find=[&](auto&& self,const std::vector<ii::gui::AppMenuItem>& rows)->void {for(const auto& row:rows){if(row.id==id&&row.enabled&&row.triggered){(void)ui.post(row.triggered);return;}self(self,row.children);}};find(find,menu_rows); }
@end
@interface IIGUIIOSScene : UIResponder <UIWindowSceneDelegate>
@property(nonatomic,strong) UIWindow* window;
@end
@implementation IIGUIIOSScene
- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)options {
    NSUserActivity* activity=nil;
    for(NSUserActivity* candidate in options.userActivities)if([candidate.activityType isEqualToString:@"com.iisacc.iigui.scene"]){activity=candidate;break;}
    if(!activity)activity=session.stateRestorationActivity;
    if([scene isKindOfClass:UIWindowScene.class]&&launch_scene)launch_scene((UIWindowScene*)scene,session,activity);
    for(UIOpenURLContext* c in options.URLContexts)if(open_document)open_document(c.URL);
}
- (void)scene:(UIScene*)scene openURLContexts:(NSSet<UIOpenURLContext*>*)contexts {(void)scene;for(UIOpenURLContext* c in contexts)if(open_document)open_document(c.URL);}
- (void)sceneDidBecomeActive:(UIScene*)scene { if([scene isKindOfClass:UIWindowScene.class]&&activate_scene)activate_scene((UIWindowScene*)scene,true); }
- (void)sceneWillResignActive:(UIScene*)scene { if([scene isKindOfClass:UIWindowScene.class]&&activate_scene)activate_scene((UIWindowScene*)scene,false); }
- (void)sceneDidDisconnect:(UIScene*)scene { if([scene isKindOfClass:UIWindowScene.class]&&disconnect_scene)disconnect_scene((UIWindowScene*)scene); }
@end
@interface IIGUIIOSApplication : UIResponder <UIApplicationDelegate> {
    id foregroundObserver;
    id backgroundObserver;
}
@end
@implementation IIGUIIOSApplication
- (BOOL)application:(UIApplication*)app didFinishLaunchingWithOptions:(NSDictionary*)options {
    (void)options;
    auto center=NSNotificationCenter.defaultCenter;
    foregroundObserver=[center addObserverForName:UIApplicationWillEnterForegroundNotification object:app queue:nil usingBlock:^(NSNotification*) {
        auto callback=foreground_application;if(callback)callback(true);
    }];
    backgroundObserver=[center addObserverForName:UIApplicationDidEnterBackgroundNotification object:app queue:nil usingBlock:^(NSNotification*) {
        auto callback=foreground_application;if(callback)callback(false);
    }];
    return YES;
}
- (void)dealloc {
    auto center=NSNotificationCenter.defaultCenter;
    if(foregroundObserver)[center removeObserver:foregroundObserver];
    if(backgroundObserver)[center removeObserver:backgroundObserver];
}
- (UISceneConfiguration*)application:(UIApplication*)app configurationForConnectingSceneSession:(UISceneSession*)session options:(UISceneConnectionOptions*)options { (void)app;(void)options;auto config=[[UISceneConfiguration alloc] initWithName:@"iiSDK" sessionRole:session.role];config.delegateClass=IIGUIIOSScene.class;return config; }
- (void)application:(UIApplication*)app didDiscardSceneSessions:(NSSet<UISceneSession*>*)sessions { (void)app;for(UISceneSession* session in sessions)if(discard_session)discard_session(session); }
@end

namespace ii::gui {
namespace {
struct WakeGate final {
    std::mutex mutex; CFRunLoopSourceRef source=nullptr;
    void attach(CFRunLoopSourceRef s){std::lock_guard lock(mutex);source=s;CFRetain(s);}
    void signal(){std::lock_guard lock(mutex);if(source){CFRunLoopSourceSignal(source);CFRunLoopWakeUp(CFRunLoopGetMain());}}
    void detach(){std::lock_guard lock(mutex);if(source){CFRelease(source);source=nullptr;}}
    ~WakeGate(){detach();}
};
HostOptions validate(HostOptions o){if(!NSThread.isMainThread)throw std::invalid_argument("iiGUI UIKit host requires the main thread");return detail::validate_options(std::move(o));}
}
struct Host::Impl final {
    enum class Phase {created,pending,running,closing,closed};
    struct Lifetime final { Impl* owner=nullptr; };
    HostOptions options;
    std::shared_ptr<detail::DispatchState> dispatch;
    std::shared_ptr<PresentationStream> presentations=std::make_shared<PresentationStream>([this](runtime::Task task){return dispatch->loop.post(std::move(task));});
    std::shared_ptr<WakeGate> gate=std::make_shared<WakeGate>();
    std::unique_ptr<runtime::FrameLoop> motion_frames;
    std::shared_ptr<MotionPreferences> motion_preferences=std::make_shared<MotionPreferences>();
    detail::FrameDemand frame_demand;std::optional<runtime::TickToken> present_tick;
    std::optional<double> display_origin;runtime::Clock::time_point frame_origin{};
    std::optional<Size> disconnected_size;
    HostCallbacks callbacks;detail::HostActivity activity,foreground;detail::HostFileRequests pending_files;
    detail::UIKitSceneBinding scene_binding;detail::UIKitSceneRouter scene_router;
    detail::UIKitSceneRouter::Token scene_request=0,initial_request=0;
    detail::HostCallbackGate callback_gate;WindowSnapshot state;HostStats statistics;
    Phase phase=Phase::created;detail::HostFailure failure;
    const std::thread::id owner=std::this_thread::get_id();
    detail::CanvasState canvas;std::shared_ptr<detail::WidgetState> widgets,legacy;
    Widget legacy_text;bool mounting=false,draining=false,opened=false,appearance_ready=false,secondary=false,active=true;
    bool application_driver=false,independent_scene=false,close_pending=false;
    Impl* parent=nullptr;std::vector<Impl*> children;
    std::shared_ptr<Lifetime> lifetime=std::make_shared<Lifetime>();
    CFRunLoopSourceRef source=nullptr;CFRunLoopTimerRef timer=nullptr;
    UIWindow* window=nil;IIGUIIOSController* controller=nil;UIView* content=nil;
    UIWindowScene* initial_scene=nil;UISceneSession* initial_session=nil;UISceneSession* native_session=nil;
    NSString* scene_nonce=nil;
    CADisplayLink* display_link=nil;UIDocumentPickerViewController* picker=nil;
    NSMutableArray<NSURL*>* scopes=[NSMutableArray new];std::vector<AppMenuItem> menu;
    explicit Impl(HostOptions o):options(validate(std::move(o))),dispatch(std::make_shared<detail::DispatchState>(options.task_capacity,options.event_loop)){lifetime->owner=this;state.size=options.size;state.title=options.title;}
    bool on_owner()const{return owner==std::this_thread::get_id();}
    core::Result<void> ready()const{if(!on_owner())return core::fail(core::ErrorCode::wrong_executor,"UIKit access requires the owner");if(phase!=Phase::running)return core::fail(core::ErrorCode::closed,"GUI window is not open");return {};}
    void remember(std::string text,std::exception_ptr cause={}){NSLog(@"iiGUI UIKit error: %s",text.c_str());if(cause)try{std::rethrow_exception(cause);}catch(const std::exception& e){NSLog(@"iiGUI cause: %s",e.what());}catch(...){}if(!failure)failure=core::Error{core::ErrorCode::failed,std::move(text),cause};}
    void deliver_primary_closed(runtime::Task call)noexcept{if(call)try{call();}catch(...){remember("UIKit close callback failed",std::current_exception());}}
    template<class F> void guard(F&& f)noexcept{
        callback_gate.enter();
        @try{try{f();}catch(...){remember("UIKit callback failed",std::current_exception());close();}}@catch(NSException* e){remember(native_error(e));close();}
        auto closed=callback_gate.leave();
        if(closed)deliver_primary_closed(std::move(closed));
    }
    void setup_source(){
        if(source)return;
        CFRunLoopSourceContext context{};context.info=this;
        context.perform=[](void* value){static_cast<Impl*>(value)->drain();};
        source=CFRunLoopSourceCreate(kCFAllocatorDefault,0,&context);
        if(!source)throw std::runtime_error("Cannot create UIKit event source");
        CFRunLoopAddSource(CFRunLoopGetMain(),source,kCFRunLoopCommonModes);
        gate->attach(source);
        if(!secondary)dispatch->connect([wake=gate]() noexcept {wake->signal();});
    }
    Impl* scene_child(detail::UIKitSceneRouter::Token token)const noexcept{
        for(auto* child:children)if(child->independent_scene&&child->scene_request==token)return child;
        return nullptr;
    }
    void update_driver_activity(){
        if(!application_driver||phase!=Phase::running)return;
        bool any=false;
        for(auto* child:children)if(child->independent_scene&&child->phase==Phase::running&&child->active&&child->window.windowScene){any=true;break;}
        activity.observe(any);gate->signal();
    }
    void open_driver(UIWindowScene* scene,UISceneSession* session){
        initial_scene=scene;initial_session=session;appearance_ready=true;active=false;
        phase=Phase::running;setup_source();
        switch(UIApplication.sharedApplication.applicationState){
        case UIApplicationStateActive:case UIApplicationStateInactive:foreground.observe(true);break;
        case UIApplicationStateBackground:foreground.observe(false);break;
        default:break;
        }
        update_driver_activity();gate->signal();
    }
    detail::UIKitSceneRouter::Token activity_request(NSUserActivity* activity)const{
        if(!activity||![activity.activityType isEqualToString:@"com.iisacc.iigui.scene"])return 0;
        id nonce=activity.userInfo[@"iiGUI.instance"];
        id number=activity.userInfo[@"iiGUI.request"];
        if(![nonce isKindOfClass:NSString.class]||![nonce isEqualToString:scene_nonce]||![number isKindOfClass:NSNumber.class])return 0;
        return [(NSNumber*)number unsignedLongLongValue];
    }
    void scene_destruction_failed(detail::UIKitSceneRouter::Token token,NSString* message){
        auto* child=scene_child(token);
        if(!child){NSLog(@"iiGUI UIKit scene cleanup failed: %@",message);return;}
        child->close_pending=false;
        const char* text=message.UTF8String;
        child->remember(text?std::string("UIKit scene destruction failed: ")+text:"UIKit scene destruction failed");
        gate->signal();
    }
    void destroy_session(UISceneSession* session,detail::UIKitSceneRouter::Token token){
        if(!session)return;
        auto weak=std::weak_ptr<Lifetime>(lifetime);
        [UIApplication.sharedApplication requestSceneSessionDestruction:session options:nil errorHandler:^(NSError* error){
            NSString* message=[error.localizedDescription copy];
            dispatch_async(dispatch_get_main_queue(), ^{
                if(auto life=weak.lock();life&&life->owner){auto* owner=life->owner;owner->guard([&]{owner->scene_destruction_failed(token,message);});}
            });
        }];
    }
    void scene_activation_failed(detail::UIKitSceneRouter::Token token,NSString* message){
        const auto result=scene_router.fail(token);
        if(result.status!=detail::UIKitSceneRouter::Status::failed)return;
        if(auto* child=scene_child(token)){
            auto weak=std::weak_ptr<Lifetime>(child->lifetime);
            const char* text=message.UTF8String;
            child->remember(text?std::string("UIKit scene activation failed: ")+text:"UIKit scene activation failed");
            if(auto life=weak.lock();life&&life->owner==child)child->close();
        }
        gate->signal();
    }
    void request_scene(detail::UIKitSceneRouter::Token token){
        NSUserActivity* activity=[[NSUserActivity alloc] initWithActivityType:@"com.iisacc.iigui.scene"];
        activity.eligibleForHandoff=NO;activity.eligibleForSearch=NO;activity.eligibleForPublicIndexing=NO;
        activity.userInfo=@{@"iiGUI.instance":scene_nonce,@"iiGUI.request":@(token)};
        auto weak=std::weak_ptr<Lifetime>(lifetime);
        [UIApplication.sharedApplication requestSceneSessionActivation:nil userActivity:activity options:nil errorHandler:^(NSError* error){
            NSString* message=[error.localizedDescription copy];
            dispatch_async(dispatch_get_main_queue(), ^{
                if(auto life=weak.lock();life&&life->owner){auto* owner=life->owner;owner->guard([&]{owner->scene_activation_failed(token,message);});}
            });
        }];
    }
    void connect_independent(UIWindowScene* scene,UISceneSession* session,NSUserActivity* activity){
        const auto id=session_identifier(session);
        auto token=activity_request(activity);
        if(initial_session&&[initial_session.persistentIdentifier isEqualToString:session.persistentIdentifier]){
            if(!initial_request){initial_scene=scene;return;}
            token=initial_request;initial_scene=nil;
        }
        const auto route=scene_router.connect(id,(__bridge const void*)scene,token);
        if(route.status==detail::UIKitSceneRouter::Status::cancelled){destroy_session(session,route.token);return;}
        if(route.status!=detail::UIKitSceneRouter::Status::connected&&route.status!=detail::UIKitSceneRouter::Status::reconnected)return;
        auto* child=scene_child(route.token);
        if(!child){(void)scene_router.cancel(route.token);destroy_session(session,route.token);return;}
        child->native_session=session;
        const auto binding=child->scene_binding.connect(id,(__bridge const void*)scene);
        if(binding!=detail::UIKitSceneBinding::Connection::first&&binding!=detail::UIKitSceneBinding::Connection::reconnected)
            throw std::logic_error("UIKit independent scene routing and binding disagree");
        child->open(scene);
    }
    void update_snapshot(){if(content){auto r=content.bounds;state.size={r.size.width,r.size.height};if(window.windowScene)if(UIScreen* screen=window.screen)state.backing_scale=screen.scale;state.visible=window.windowScene&&!window.hidden&&phase==Phase::running;}}
    runtime::FrameLoop* frames(){return options.frame_loop?options.frame_loop:motion_frames.get();}
    detail::WidgetFrameHooks frame_hooks(){return {
        .frames=[this]()->core::Result<runtime::FrameLoop*>{if(!frames())motion_frames=std::make_unique<runtime::FrameLoop>(dispatch->loop);return frames();},
        .activity=[this](bool value){frame_demand.motion(value);refresh_frames();},
        .preferences=motion_preferences,.renderer=options.renderer,.events=[this]()->core::Result<runtime::EventLoop*>{return &dispatch->loop;},.presentations=presentations};}
    void clear_legacy(){if(legacy){legacy->close();legacy.reset();legacy_text={};}}
    core::Result<void> show_legacy_text(){if(!legacy){auto next=std::make_shared<detail::WidgetState>([this](runtime::Task t){guard([&]{if(phase==Phase::running)t();});},WidgetAppearance{},frame_hooks());auto root=next->add(WidgetKind::vertical_stack,{},"iiGUI.legacy.root",{.padding=28},{},{},{});if(!root)return std::unexpected(root.error());auto label=next->add(WidgetKind::label,*root,"iiGUI.legacy.text",{},{.text=state.text,.font_size=17},{},{});if(!label)return std::unexpected(label.error());if(auto r=next->attach((__bridge void*)content, detail::make_native_widget_surface);!r)return r;legacy_text=*label;legacy=std::move(next);}return legacy_text.set_text(state.text);}
    void stop_frames(){if(display_link){[display_link invalidate];display_link=nil;if(widgets)widgets->set_frame_paced(false);if(legacy)legacy->set_frame_paced(false);}display_origin.reset();statistics.display_synced=false;}
    void start_frames(){if(display_link||!frame_demand.needed()||!frames())return;display_link=[CADisplayLink displayLinkWithTarget:controller selector:@selector(displayFrame:)];display_link.preferredFrameRateRange=CAFrameRateRangeMake(30,120,60);[display_link addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];statistics.display_synced=true;if(widgets)widgets->set_frame_paced(true);if(legacy)legacy->set_frame_paced(true);}
    void refresh_frames(){frame_demand.foreground(active&&phase==Phase::running);frame_demand.continuous(options.frame_loop!=nullptr);if(frame_demand.needed())start_frames();else stop_frames();}
    void frame(CADisplayLink* link){if(phase!=Phase::running||draining||!active||!opened)return;guard([&]{if(!secondary)dispatch->loop.poll(options.budget);if(phase!=Phase::running)return;++statistics.display_callbacks;const double target=link.targetTimestamp;statistics.display_interval=std::chrono::duration_cast<runtime::Clock::duration>(std::chrono::duration<double>(target-link.timestamp));if(frames()){if(!display_origin){display_origin=target;frame_origin=runtime::Clock::now();if(!options.frame_loop)frames()->reset_clock(frame_origin);}frames()->step(frame_origin+std::chrono::duration_cast<runtime::Clock::duration>(std::chrono::duration<double>(target-*display_origin)));}if(active){if(widgets)widgets->present();if(legacy)legacy->present();canvas.flush(state.backing_scale,state.size.height);}arm_timer();});}
    void arm_timer(){if(timer){CFRunLoopTimerInvalidate(timer);CFRelease(timer);timer=nullptr;}if(secondary||phase!=Phase::running)return;auto next=dispatch->loop.next_deadline();if(!next)return;double delay=std::chrono::duration<double>(*next-runtime::Clock::now()).count();if(delay<=0){gate->signal();return;}CFRunLoopTimerContext c{};c.info=this;timer=CFRunLoopTimerCreate(kCFAllocatorDefault,CFAbsoluteTimeGetCurrent()+delay,0,0,0,[](CFRunLoopTimerRef fired,void* p){auto& s=*static_cast<Impl*>(p);if(s.timer==fired){s.timer=nullptr;CFRelease(fired);}++s.statistics.timer_wakes;s.drain();},&c);CFRunLoopAddTimer(CFRunLoopGetMain(),timer,kCFRunLoopCommonModes);}
    void drain(){
        if(phase!=Phase::running||draining||!appearance_ready)return;
        guard([&]{
            struct Reset{bool& x;~Reset(){x=false;}} reset{draining};draining=true;++statistics.dispatches;
            if(!opened){opened=true;auto call=callbacks.opened;if(call)call();if(phase==Phase::running){foreground.open();activity.open();}}
            if(phase!=Phase::running)return;
            foreground.deliver();
            if(phase!=Phase::running)return;
            activity.deliver();
            if(phase!=Phase::running)return;
            if(!pending_files.empty()){
                auto call=callbacks.files_opened;
                if(call)pending_files.deliver(std::move(call),[this]{if(phase==Phase::running)gate->signal();});
                else pending_files.clear();
            }
            if(phase!=Phase::running)return;
            if(!secondary)dispatch->loop.poll(options.budget);
            if(phase!=Phase::running)return;
            if(dispatch->loop.state()==runtime::LoopState::stopped){close();return;}
            if(active){if(widgets)widgets->present();if(legacy)legacy->present();canvas.flush(state.backing_scale,state.size.height);}
            arm_timer();
        });
    }
    void activation(bool value){
        active=value;
        if(independent_scene&&parent)parent->update_driver_activity();
        else if(!secondary&&!application_driver&&phase==Phase::running)activity.observe(value);
        display_origin.reset();if(frames())frames()->reset_clock();refresh_frames();gate->signal();
        if(!application_driver){const auto list=children;for(auto* child:list)if(std::find(children.begin(),children.end(),child)!=children.end())child->activation(value);}
    }
    void application_foreground(bool value){
        if(secondary||phase==Phase::closing||phase==Phase::closed)return;
        foreground.observe(value);
        if(phase==Phase::running&&opened)foreground.deliver();
        else gate->signal();
    }
    bool place_window(UIWindowScene* scene,bool restoring=false,const detail::UIKitSceneBinding* binding=nullptr,std::uint64_t revision=0){
        const CGRect previous=window.frame;
        window.windowScene=scene;
        if(binding&&binding->revision()!=revision)return false;
        const CGRect available=scene.coordinateSpace.bounds;
        if(secondary&&!independent_scene){
            const double width=restoring?(disconnected_size?disconnected_size->width:double(previous.size.width)):options.size.width;
            const double height=restoring?(disconnected_size?disconnected_size->height:double(previous.size.height)):options.size.height;
            const double w=std::clamp(width,1.0,std::max(1.0,double(available.size.width)-24));
            const double h=std::clamp(height,1.0,std::max(1.0,double(available.size.height)-48));
            window.frame=CGRectMake((available.size.width-w)/2,(available.size.height-h)/2,w,h);
            if(binding&&binding->revision()!=revision)return false;
            if(parent&&parent->window)window.windowLevel=parent->window.windowLevel+1;
        }else window.frame=available;
        if(binding&&binding->revision()!=revision)return false;
        disconnected_size.reset();
        return true;
    }
    void detach_scene_windows(const detail::UIKitSceneBinding& binding,std::uint64_t revision){
        if(binding.revision()!=revision)return;
        const auto list=children;
        for(auto* child:list){
            if(binding.revision()!=revision)return;
            if(std::find(children.begin(),children.end(),child)!=children.end())child->detach_scene_windows(binding,revision);
        }
        if(binding.revision()!=revision)return;
        if(secondary&&window)disconnected_size=Size{window.frame.size.width,window.frame.size.height};
        if(window){window.hidden=YES;if(binding.revision()!=revision)return;window.windowScene=nil;if(binding.revision()!=revision)return;}
        state.visible=false;
    }
    void reattach_scene_windows(UIWindowScene* scene,const detail::UIKitSceneBinding& binding,std::uint64_t revision){
        if(phase!=Phase::running||!window||binding.revision()!=revision)return;
        if(!place_window(scene,true,&binding,revision))return;
        if(phase!=Phase::running||binding.revision()!=revision)return;
        [window makeKeyAndVisible];
        if(phase!=Phase::running||binding.revision()!=revision)return;
        [controller.view setNeedsLayout];
        if(phase!=Phase::running||binding.revision()!=revision)return;
        update_snapshot();
        const auto list=children;
        for(auto* child:list){
            if(phase!=Phase::running||binding.revision()!=revision)break;
            if(std::find(children.begin(),children.end(),child)!=children.end())child->reattach_scene_windows(scene,binding,revision);
        }
    }
    void suspend_scene(std::uint64_t revision){
        if(phase!=Phase::running||scene_binding.revision()!=revision)return;
        activation(false);
        if(phase==Phase::running&&scene_binding.revision()==revision)detach_scene_windows(scene_binding,revision);
    }
    void open(UIWindowScene* scene){
        if(phase==Phase::running&&window){const auto revision=scene_binding.revision();reattach_scene_windows(scene,scene_binding,revision);if(phase==Phase::running&&scene_binding.revision()==revision&&scene_binding.accepts((__bridge const void*)scene))activation(scene.activationState==UISceneActivationStateForegroundActive);return;}
        controller=[IIGUIIOSController new];controller.view.backgroundColor=UIColor.blackColor;
        window=[[UIWindow alloc] initWithWindowScene:scene];window.rootViewController=controller;
        (void)place_window(scene);
        content=[[UIView alloc] initWithFrame:controller.view.bounds];content.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;content.backgroundColor=UIColor.blackColor;[controller.view addSubview:content];
        controller->resize=[this]{if(phase!=Phase::running)return;guard([&]{
            const auto previous_size=state.size;const auto previous_scale=state.backing_scale;
            const CGRect previous_frame=content.frame;
            content.frame=controller.view.bounds;update_snapshot();
            if(state.size==previous_size&&state.backing_scale==previous_scale&&CGRectEqualToRect(previous_frame,content.frame))return;
            canvas.dirty=true;gate->signal();if(opened&&callbacks.resized)callbacks.resized(state.size);
        });};
        controller->frame=[this](CADisplayLink* value){frame(value);};
        controller->appeared=[this]{appearance_ready=true;gate->signal();};
        setup_source();
        phase=Phase::running;active=scene.activationState==UISceneActivationStateForegroundActive;if(!secondary){
            activity.observe(active);
            switch(UIApplication.sharedApplication.applicationState){
            case UIApplicationStateActive:case UIApplicationStateInactive:foreground.observe(true);break;
            case UIApplicationStateBackground:foreground.observe(false);break;
            default:break;
            }
        }[window makeKeyAndVisible];[controller.view layoutIfNeeded];update_snapshot();refresh_frames();gate->signal();
        if(independent_scene&&parent)parent->update_driver_activity();
    }
    void cleanup(){scene_binding.reset();scene_router.clear();scene_request=0;lifetime->owner=nullptr;initial_scene=nil;initial_session=nil;native_session=nil;gate->detach();if(timer){CFRunLoopTimerInvalidate(timer);CFRelease(timer);timer=nullptr;}if(source){CFRunLoopSourceInvalidate(source);CFRelease(source);source=nullptr;}if(controller){controller->resize={};controller->appeared={};controller->frame={};controller->picked={};}for(NSURL* url in scopes)[url stopAccessingSecurityScopedResource];[scopes removeAllObjects];picker=nil;content=nil;window=nil;controller=nil;if(parent){std::erase(parent->children,this);parent=nullptr;}}
    void close()noexcept{
        if(phase!=Phase::running&&phase!=Phase::pending)return;
        phase=Phase::closing;activity.close();foreground.close();pending_files.clear();dispatch->retire();
        auto* owner=parent;
        if(independent_scene&&owner){
            (void)owner->scene_router.cancel(scene_request);
            if(native_session&&!close_pending)@try{owner->destroy_session(native_session,scene_request);}@catch(NSException* error){remember(native_error(error));}
        }
        auto list=children;
        for(auto* child:list)if(std::find(children.begin(),children.end(),child)!=children.end())child->close();
        children.clear();stop_frames();
        canvas.clear();clear_legacy();if(widgets){widgets->close();widgets.reset();}
        [picker dismissViewControllerAnimated:NO completion:nil];window.hidden=YES;state.visible=false;state.closed=true;presentations->close();phase=Phase::closed;
        auto call=std::move(callbacks.closed);auto endpoint=owner?owner->dispatch:nullptr;cleanup();
        if(owner&&independent_scene)owner->update_driver_activity();
        if(secondary){
            if(call&&endpoint&&endpoint->accepting.load()){
                try{
                    const bool submitted=detail::try_defer_host_closed(std::move(call),[endpoint](auto callback){
                        auto sender=endpoint->loop.sender();
                        return bool(endpoint->track(endpoint->tasks.post(sender,std::move(callback))));
                    });
                    if(!submitted){owner->remember("Cannot deliver UIKit child close");endpoint->loop.request_stop();owner->gate->signal();}
                }catch(...){owner->remember("Cannot defer UIKit child close",std::current_exception());endpoint->loop.request_stop();owner->gate->signal();}
            }
        }
        else{dispatch->loop.request_stop();if(call&&!callback_gate.defer(call))deliver_primary_closed(std::move(call));}
    }
};
core::Result<std::shared_ptr<PresentationSource>> Host::presentation_source() const {
    if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Presentation source requires host owner");
    return std::static_pointer_cast<PresentationSource>(impl_->presentations);
}
Host::Host(HostOptions o):impl_(std::make_unique<Impl>(std::move(o))){}
Host::~Host(){if(!is_ui_thread())std::terminate();if(impl_->phase==Impl::Phase::running||impl_->phase==Impl::Phase::pending)impl_->close();impl_->lifetime->owner=nullptr;impl_->presentations->close();impl_->dispatch->retire();impl_->gate->detach();}
UiExecutor Host::executor()const noexcept{return UiExecutor(impl_->dispatch);}
bool Host::is_ui_thread()const noexcept{return impl_->on_owner();}
core::Result<void> Host::set_application_driver(bool enabled){
    if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Application driver requires the owner thread");
    if(impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"Application driver must be configured before run");
    if(enabled&&impl_->options.frame_loop)return core::fail(core::ErrorCode::invalid_argument,"A hidden application driver cannot own a window frame loop");
    impl_->application_driver=enabled;return {};
}
core::Result<void> Host::set_application_activity_handler(std::function<void(bool)> handler){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Application activity requires the host owner thread");if(impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"Application activity must be configured before run");impl_->activity.set_handler(std::move(handler));return {};}
core::Result<void> Host::set_application_foreground_handler(std::function<void(bool)> handler){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Application foreground requires the host owner thread");if(impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"Application foreground must be configured before run");impl_->foreground.set_handler(std::move(handler));return {};}
core::Result<void> Host::set_failure_handler(std::function<void(const core::Error&)> handler) {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Host failure handling requires the owner thread");
    if (impl_->phase != Impl::Phase::created) return core::fail(core::ErrorCode::busy, "Host failure handling must be configured before run or show");
    impl_->failure.set_handler(std::move(handler));
    return {};
}
core::Result<void> Host::run(HostCallbacks callbacks){
    if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"UIKit host requires main thread");
    if(impl_->phase!=Impl::Phase::created||launch_scene)return core::fail(core::ErrorCode::busy,"UIKit application host is single use");
    if(impl_->application_driver)impl_->scene_nonce=NSUUID.UUID.UUIDString;
    else{
        const auto request=impl_->scene_router.reserve();
        if(!request)return core::fail(core::ErrorCode::full,"UIKit scene request capacity is exhausted");
        impl_->scene_request=*request;
    }
    impl_->callbacks=std::move(callbacks);
    launch_scene=[this](UIWindowScene* scene,UISceneSession* session,NSUserActivity* activity){impl_->guard([&]{
        if(impl_->application_driver){
            if(impl_->phase==Impl::Phase::created)impl_->open_driver(scene,session);
            else if(impl_->phase==Impl::Phase::running)impl_->connect_independent(scene,session,activity);
            else if(impl_->phase==Impl::Phase::closed){const auto token=impl_->activity_request(activity);if(token)impl_->destroy_session(session,token);}
            return;
        }
        if(impl_->phase!=Impl::Phase::created&&impl_->phase!=Impl::Phase::running)return;
        const auto id=session_identifier(session);
        const auto routed=impl_->scene_router.connect(id,(__bridge const void*)scene,impl_->scene_request);
        if(routed.status==detail::UIKitSceneRouter::Status::connected||routed.status==detail::UIKitSceneRouter::Status::reconnected){
            const auto connection=impl_->scene_binding.connect(id,(__bridge const void*)scene);
            if(connection!=detail::UIKitSceneBinding::Connection::first&&connection!=detail::UIKitSceneBinding::Connection::reconnected)
                throw std::logic_error("UIKit scene routing and binding disagree");
            impl_->open(scene);
        }
    });};
    activate_scene=[this](UIWindowScene* scene,bool active){impl_->guard([&]{
        const auto routed=impl_->scene_router.find((__bridge const void*)scene);
        if(impl_->application_driver){
            if(routed.status==detail::UIKitSceneRouter::Status::connected)if(auto* child=impl_->scene_child(routed.token))child->activation(active);
            return;
        }
        if(routed.status==detail::UIKitSceneRouter::Status::connected&&routed.token==impl_->scene_request&&
           impl_->scene_binding.accepts((__bridge const void*)scene))impl_->activation(active);
    });};
    disconnect_scene=[this](UIWindowScene* scene){impl_->guard([&]{
        if(impl_->application_driver){
            if(impl_->initial_scene==scene)impl_->initial_scene=nil;
            const auto routed=impl_->scene_router.disconnect((__bridge const void*)scene);
            if(routed.status==detail::UIKitSceneRouter::Status::disconnected)if(auto* child=impl_->scene_child(routed.token)){
                if(child->scene_binding.disconnect((__bridge const void*)scene))child->suspend_scene(child->scene_binding.revision());
            }
            impl_->update_driver_activity();return;
        }
        if(!impl_->scene_binding.accepts((__bridge const void*)scene))return;
        const auto routed=impl_->scene_router.disconnect((__bridge const void*)scene);
        if(routed.status==detail::UIKitSceneRouter::Status::disconnected&&routed.token==impl_->scene_request&&
           impl_->scene_binding.disconnect((__bridge const void*)scene))impl_->suspend_scene(impl_->scene_binding.revision());
    });};
    discard_session=[this](UISceneSession* session){impl_->guard([&]{
        const auto id=session_identifier(session);
        if(impl_->application_driver){
            const bool initial=impl_->initial_session&&[impl_->initial_session.persistentIdentifier isEqualToString:session.persistentIdentifier];
            const auto routed=impl_->scene_router.discard(id);
            if(routed.status==detail::UIKitSceneRouter::Status::discarded){
                if(auto* child=impl_->scene_child(routed.token)){child->native_session=nil;child->close_pending=false;child->close();}
            }else if(initial&&impl_->initial_request){
                impl_->scene_activation_failed(impl_->initial_request,@"The initial UIKit session was discarded before connecting");
            }
            if(initial){impl_->initial_session=nil;impl_->initial_scene=nil;impl_->initial_request=0;}
            impl_->update_driver_activity();return;
        }
        if(!impl_->scene_binding.session_is(id))return;
        const auto routed=impl_->scene_router.discard(id);
        if(routed.status==detail::UIKitSceneRouter::Status::discarded&&routed.token==impl_->scene_request&&impl_->scene_binding.discard(id)){
            const auto next=impl_->scene_router.reserve();
            if(!next)throw std::runtime_error("UIKit scene request capacity is exhausted");
            impl_->scene_request=*next;
            impl_->suspend_scene(impl_->scene_binding.revision());
        }
    });};
    foreground_application=[this](bool value){impl_->guard([&]{impl_->application_foreground(value);});};
    open_document=[this](NSURL* url){
        if(!url.fileURL||!impl_->callbacks.files_opened)return;
        impl_->guard([&]{
            if(impl_->phase!=Impl::Phase::running)return;
            NSData* bytes=[url.path dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:NO];
            if(!bytes||!bytes.length||bytes.length>detail::HostFileRequests::maximum_path_bytes)
                throw std::invalid_argument("Invalid UIKit file path");
            std::vector<std::string> paths;
            paths.emplace_back(static_cast<const char*>(bytes.bytes),bytes.length);
            if(!impl_->pending_files.admit(std::move(paths)))
                throw std::runtime_error("UIKit file request was invalid or the queue is full");
            if([url startAccessingSecurityScopedResource])[impl_->scopes addObject:url];
            impl_->gate->signal();
        });
    };
    @autoreleasepool{char name[]="iiSDK";char* args[]={name,nullptr};UIApplicationMain(1,args,nil,NSStringFromClass(IIGUIIOSApplication.class));}
    launch_scene={};activate_scene={};disconnect_scene={};discard_session={};foreground_application={};open_document={};
    if(impl_->failure)return std::unexpected(*impl_->failure);
    return {};
}
core::Result<void> Host::show_external(HostCallbacks){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"External host requires its owner thread");return core::fail(core::ErrorCode::unsupported,"External event-loop hosting is not implemented on UIKit");}
core::Result<void> Host::attach_external(void*,HostCallbacks){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"External host requires its owner thread");return core::fail(core::ErrorCode::unsupported,"External surface hosting is not implemented on UIKit");}
core::Result<void> Host::show(Host& parent,HostCallbacks callbacks){
    if(!is_ui_thread()||!parent.is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"UIKit child requires main thread");
    if(this==&parent||impl_->application_driver)return core::fail(core::ErrorCode::invalid_argument,"An application driver cannot be a secondary window");
    if(impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"UIKit child is single use");
    if(impl_->activity.has_handler()||impl_->foreground.has_handler())return core::fail(core::ErrorCode::invalid_argument,"Application lifecycle belongs to the primary host");
    if(auto r=parent.impl_->ready();!r)return r;
    const bool independent=parent.impl_->application_driver;
    if(independent&&!UIApplication.sharedApplication.supportsMultipleScenes)
        return core::fail(core::ErrorCode::unsupported,"UIKit multiple scenes are unavailable in this app or device");
    if(!independent&&!parent.impl_->window.windowScene)return core::fail(core::ErrorCode::busy,"UIKit scene is disconnected");
    std::optional<detail::UIKitSceneRouter::Token> request;
    bool submitted=false,registered=false;
    const auto rollback=[&]{
        const bool had_session=impl_->native_session!=nil;
        impl_->callbacks.closed={};
        if(impl_->phase==Impl::Phase::running||impl_->phase==Impl::Phase::pending)impl_->close();
        else if(registered){std::erase(parent.impl_->children,impl_.get());impl_->parent=nullptr;}
        if(request&&!submitted&&!had_session){
            (void)parent.impl_->scene_router.fail(*request);
            if(parent.impl_->initial_request==*request)parent.impl_->initial_request=0;
        }else if(request&&had_session&&parent.impl_->initial_request==*request){
            parent.impl_->initial_request=0;parent.impl_->initial_session=nil;parent.impl_->initial_scene=nil;
        }
    };
    @try {
        try {
            if(independent){request=parent.impl_->scene_router.reserve();if(!request)return core::fail(core::ErrorCode::full,"UIKit scene request capacity is exhausted");}
            auto dispatch=std::make_shared<detail::DispatchState>(impl_->options.task_capacity,&parent.impl_->dispatch->loop);
            parent.impl_->children.push_back(impl_.get());registered=true;
            impl_->dispatch->retire();impl_->dispatch=std::move(dispatch);
            impl_->secondary=true;impl_->independent_scene=independent;impl_->parent=parent.impl_.get();impl_->callbacks=std::move(callbacks);
            if(request)impl_->scene_request=*request;
            if(independent){
                impl_->phase=Impl::Phase::pending;
                if(parent.impl_->initial_session&&!parent.impl_->initial_request){
                    parent.impl_->initial_request=*request;
                    if(parent.impl_->initial_scene)parent.impl_->connect_independent(parent.impl_->initial_scene,parent.impl_->initial_session,nil);
                }else{submitted=true;parent.impl_->request_scene(*request);}
            }else impl_->open(parent.impl_->window.windowScene);
            return {};
        }catch(...){
            rollback();
            return core::fail(core::ErrorCode::failed,"Cannot open UIKit child",std::current_exception());
        }
    }@catch(NSException* error){
        rollback();
        return core::fail(core::ErrorCode::failed,native_error(error));
    }
}
core::Result<void> Host::choose_file(FileDialogOptions options,FileDialogCompletion completion){if(auto r=impl_->ready();!r)return r;if(!completion||!detail::valid_dialog(options))return core::fail(core::ErrorCode::invalid_argument,"Invalid document picker options");if(impl_->picker)return core::fail(core::ErrorCode::busy,"Document picker is open");NSMutableArray<UTType*>* types=[NSMutableArray new];if(options.mode==FileDialogMode::directory||options.mode==FileDialogMode::save)[types addObject:UTTypeFolder];else{for(const auto& filter:options.filters)for(const auto& ext:filter.extensions){auto type=[UTType typeWithFilenameExtension:native(ext)];if(type)[types addObject:type];}if(!types.count)[types addObject:UTTypeData];}auto picker=[[UIDocumentPickerViewController alloc] initForOpeningContentTypes:types asCopy:options.mode==FileDialogMode::open];picker.allowsMultipleSelection=options.multiple;picker.delegate=impl_->controller;picker.title=native(options.title);impl_->picker=picker;auto weak=std::weak_ptr(impl_->dispatch);auto host=impl_.get();auto ui=executor();impl_->controller->picked=[weak,host,ui,completion,options](NSArray<NSURL*>* urls){auto endpoint=weak.lock();if(!endpoint||!endpoint->accepting.load())return;FileDialogSelection result;result.canceled=urls==nil;for(NSURL* url in urls){if([url startAccessingSecurityScopedResource])[host->scopes addObject:url];auto selected=options.mode==FileDialogMode::save?[url URLByAppendingPathComponent:native(options.filename)]:url;result.paths.emplace_back(selected.path.UTF8String);}host->picker=nil;(void)ui.post([completion,result=std::move(result)]()mutable{completion(std::move(result));});};[impl_->controller presentViewController:picker animated:YES completion:nil];return {};}
core::Result<void> Host::window_action(WindowAction action){if(auto r=impl_->ready();!r)return r;if(action==WindowAction::raise||action==WindowAction::restore){[impl_->window makeKeyAndVisible];return {};}if(unsigned(action)>unsigned(WindowAction::toggle_maximize))return core::fail(core::ErrorCode::invalid_argument,"Invalid UIKit window action");return core::fail(core::ErrorCode::unsupported,"UIKit scene geometry is managed by iPadOS");}
core::Result<void> Host::set_app_menu(std::vector<AppMenuItem> menu){if(auto r=impl_->ready();!r)return r;if(!detail::valid_menu(menu))return core::fail(core::ErrorCode::invalid_argument,"Invalid application menu");impl_->menu=std::move(menu);impl_->controller->menu_rows=impl_->menu;impl_->controller->menu_ui=executor();[UIMenuSystem.mainSystem setNeedsRebuild];return {};}
core::Result<void> Host::set_title(std::string_view text){if(auto r=impl_->ready();!r)return r;if(!detail::valid_title(text))return core::fail(core::ErrorCode::invalid_argument,"Invalid title");impl_->state.title=text;impl_->controller.title=native(text);return {};}
core::Result<void> Host::set_text(std::string_view text) {
    if (auto ready = impl_->ready(); !ready) return ready;
    if (impl_->widgets) return core::fail(core::ErrorCode::busy, "Use the mounted widget's text property");
    if (!detail::valid_title(text) || text.size()>65536) return core::fail(core::ErrorCode::invalid_argument, "Display text must be valid UTF-8 without NUL, up to 64 KiB");
    std::string value(text);
    if (value == impl_->state.text && impl_->legacy) return {};
    auto previous=std::exchange(impl_->state.text,std::move(value));
    if (auto result=impl_->show_legacy_text(); !result) { impl_->state.text=std::move(previous); return result; }
    return {};
}
core::Result<void> Host::set_content_size(Size size){if(auto r=impl_->ready();!r)return r;if(!detail::valid_size(size))return core::fail(core::ErrorCode::invalid_argument,"Invalid content size");if(!impl_->secondary||impl_->independent_scene)return core::fail(core::ErrorCode::unsupported,"UIKit scene size belongs to the operating system");auto r=impl_->window.frame;r.size=CGSizeMake(size.width,size.height);impl_->window.frame=r;if(!impl_->window.windowScene)impl_->disconnected_size=size;[impl_->controller.view layoutIfNeeded];impl_->update_snapshot();return {};}
core::Result<void> Host::set_window_geometry(const WindowGeometry& update){
    if(auto r=impl_->ready();!r)return r;
    impl_->update_snapshot();
    auto next=resolve_window_geometry({impl_->state.title,impl_->state.size,impl_->options.minimum_size},update);
    if(!next)return std::unexpected(next.error());
    if((!impl_->secondary||impl_->independent_scene)&&(update.width||update.height||update.minimum_width||update.minimum_height))
        return core::fail(core::ErrorCode::unsupported,"UIKit scene geometry belongs to the operating system");
    if(impl_->secondary&&!impl_->independent_scene){auto r=impl_->window.frame;r.size=CGSizeMake(next->size.width,next->size.height);impl_->window.frame=r;if(!impl_->window.windowScene)impl_->disconnected_size=next->size;[impl_->controller.view layoutIfNeeded];}
    impl_->options.minimum_size=next->minimum_size;impl_->state.title=next->title;impl_->controller.title=native(next->title);impl_->update_snapshot();return {};
}
core::Result<void> Host::request_close(){
    if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"UIKit close requires the host owner");
    if(impl_->phase!=Impl::Phase::running&&impl_->phase!=Impl::Phase::pending)return core::fail(core::ErrorCode::closed,"GUI window is not open");
    if(impl_->close_pending)return {};
    @try {
        try {
            if(impl_->callbacks.close_requested&&!impl_->callbacks.close_requested())return {};
            if(impl_->phase!=Impl::Phase::running&&impl_->phase!=Impl::Phase::pending)return {};
            if(impl_->independent_scene&&impl_->native_session&&impl_->parent){
                impl_->close_pending=true;
                impl_->parent->destroy_session(impl_->native_session,impl_->scene_request);
            }else impl_->close();
            return {};
        }catch(...){impl_->close_pending=false;return core::fail(core::ErrorCode::failed,"UIKit close request failed",std::current_exception());}
    }@catch(NSException* error){impl_->close_pending=false;return core::fail(core::ErrorCode::failed,native_error(error));}
}
core::Result<void> Host::set_canvas(CanvasFrame frame) {
    if(frame.renderer==Renderer::automatic)frame.renderer=impl_->options.renderer;
    if (auto ready = impl_->ready(); !ready) return ready;
    if (impl_->mounting) return core::fail(core::ErrorCode::busy, "A widget factory is active");
    auto result = impl_->canvas.set(std::move(frame), (__bridge void*)impl_->content,
                                   nullptr, impl_->state.backing_scale,impl_->presentations);
    if (result) {
        if (impl_->widgets) { impl_->widgets->close(); impl_->widgets.reset(); }
        impl_->clear_legacy();
        impl_->gate->signal();
    }
    return result;
}
core::Result<WidgetMount> Host::mount_widgets(WidgetFactory factory) {
    return mount_widgets(std::move(factory), WidgetAppearance{});
}
core::Result<WidgetMount> Host::mount_widgets(WidgetFactory factory, const WidgetAppearance& appearance) {
    if (auto ready = impl_->ready(); !ready) return std::unexpected(ready.error());
    if (!factory) return core::fail(core::ErrorCode::invalid_argument, "Native widget factory is empty");
    if (impl_->mounting) return core::fail(core::ErrorCode::busy, "A widget factory is already active");
    struct Reset final { bool& value; ~Reset() { value = false; } } reset{impl_->mounting};
    impl_->mounting = true;
    @try {
            auto geometry=std::make_shared<detail::UIKitWindowGeometry>(!impl_->secondary||impl_->independent_scene);
            auto hooks=impl_->frame_hooks();
            hooks.window_geometry=[this,geometry](const WindowGeometry& value){return set_window_geometry(geometry->prepare(value));};
        try {
            auto replacement = std::make_shared<detail::WidgetState>([host=impl_.get()](runtime::Task work) {
                host->guard([&] { if (host->phase == Impl::Phase::running) work(); });
            }, appearance,std::move(hooks));
            if (auto built = replacement->construct(std::move(factory)); !built) return std::unexpected(built.error());
            if (auto ready = impl_->ready(); !ready) return std::unexpected(ready.error());
            if (auto attached = replacement->attach((__bridge void*)impl_->content, detail::make_native_widget_surface); !attached)
                return std::unexpected(attached.error());
            if(auto committed=replacement->commit_window_properties();!committed)return std::unexpected(committed.error());
            geometry->finish_initial();
            impl_->canvas.clear();
            if (impl_->widgets) impl_->widgets->close();
            impl_->widgets = std::move(replacement);
            impl_->clear_legacy();
            impl_->widgets->set_frame_paced(impl_->display_link!=nil || impl_->options.frame_loop!=nullptr);
            auto published=impl_->widgets;published->commit_publication();
            if(auto alive=published->ready();!alive)return std::unexpected(alive.error());
            return WidgetMount(published);
        } catch (...) {
            return core::fail(core::ErrorCode::failed, "Native widget factory failed", std::current_exception());
        }
    } @catch (NSException* error) {
        return core::fail(core::ErrorCode::failed, native_error(error));
    }
}
core::Result<void> Host::unmount_widgets() {
    if (auto ready = impl_->ready(); !ready) return ready;
    if (impl_->mounting) return core::fail(core::ErrorCode::busy, "A widget factory is active");
    if (impl_->widgets) { impl_->widgets->close(); impl_->widgets.reset(); }
    return {};
}
core::Result<void> Host::unmount_widgets(const WidgetMount& expected) {
    if (auto ready=impl_->ready(); !ready) return ready;
    auto owner=expected.state_.lock();
    if (!owner) return core::fail(core::ErrorCode::closed,"Native widget generation is closed");
    if (owner!=impl_->widgets) return core::fail(core::ErrorCode::invalid_argument,"Native widget belongs to another host or generation");
    return unmount_widgets();
}
core::Result<void> Host::clear_canvas() {
    if (auto ready = impl_->ready(); !ready) return ready;
    impl_->canvas.clear();
    return {};
}
core::Result<GraphicsInfo> Host::graphics_info() const {
    if (auto ready = impl_->ready(); !ready) return std::unexpected(ready.error());
    if (impl_->widgets) return impl_->widgets->graphics_info();
    if (impl_->legacy) return impl_->legacy->graphics_info();
    return impl_->canvas.info();
}
core::Result<PixelBuffer> Host::capture_canvas() {
    if (auto ready = impl_->ready(); !ready) return std::unexpected(ready.error());
    if (impl_->widgets) return impl_->widgets->capture();
    if (impl_->legacy) return impl_->legacy->capture();
    return impl_->canvas.capture(impl_->state.backing_scale, impl_->state.size.height);
}
core::Result<std::reference_wrapper<runtime::EventLoop>> Host::event_loop() {
    if (auto ready=impl_->ready(); !ready) return std::unexpected(ready.error());
    return std::ref(impl_->dispatch->loop);
}
core::Result<WindowSnapshot> Host::snapshot() const {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Snapshot requires the main thread");
    if (impl_->phase == Impl::Phase::created) return core::fail(core::ErrorCode::closed, "Window has not opened");
    impl_->update_snapshot();
    return impl_->state;
}
core::Result<DisplayPixels> Host::display_pixels() const {
    if (auto ready=impl_->ready(); !ready) return std::unexpected(ready.error());
    UIWindowScene* scene=impl_->window.windowScene;
    UIScreen* screen=scene.screen;
    if (!scene || !screen) return core::fail(core::ErrorCode::busy,"The window display is not yet available");
    // A scene can occupy only part of the screen (for example in Split View),
    // so its aspect ratio cannot be used to infer the display's orientation.
    const auto native=screen.nativeBounds.size,oriented=screen.coordinateSpace.bounds.size;
    if (!std::isfinite(native.width) || !std::isfinite(native.height) || native.width<=0 || native.height<=0 ||
        native.width>double(std::numeric_limits<int>::max()) || native.height>double(std::numeric_limits<int>::max()))
        return core::fail(core::ErrorCode::failed,"The window display mode has invalid dimensions");
    const auto pixels=detail::oriented_display_pixels(std::uint64_t(std::llround(native.width)),
        std::uint64_t(std::llround(native.height)),oriented.width,oriented.height);
    if (!pixels) return core::fail(core::ErrorCode::busy,"The window display orientation is unavailable");
    return *pixels;
}
core::Result<HostStats> Host::stats() const {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Stats require the main thread");
    auto result = impl_->statistics;
    result.loop = impl_->dispatch->loop.stats();
    result.host = impl_->dispatch->host_stats();
    return result;
}
} // namespace ii::gui
