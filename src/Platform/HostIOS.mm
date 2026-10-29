#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <QuartzCore/QuartzCore.h>
#include <iiGUI/Host.h>
#include <iiGUI/HostCommon.h>
#include <iiGUI/DesktopCommon.h>
#include <iiGUI/DispatchState.h>
#include <iiGUI/CanvasState.h>
#include <iiGUI/WidgetState.h>
#include <mutex>
#include <optional>

namespace {
std::function<void(UIWindowScene*)> launch_scene;
std::function<void(bool)> activate_scene;
std::function<void(NSURL*)> open_document;
std::string native_error(NSException* e){return e.reason.UTF8String?:"UIKit exception";}
NSString* native(std::string_view s) { return [[NSString alloc] initWithBytes:s.data() length:s.size() encoding:NSUTF8StringEncoding]; }
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
- (void)viewDidAppear:(BOOL)animated { [super viewDidAppear:animated]; if(appeared) appeared(); }
- (void)displayFrame:(CADisplayLink*)link { if(frame) frame(link); }
- (void)documentPicker:(UIDocumentPickerViewController*)picker didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls { (void)picker; auto call=picked;picked={};if(call)call(urls); }
- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)picker { (void)picker;auto call=picked;picked={};if(call)call(nil); }
- (BOOL)prefersStatusBarHidden { return YES; }
- (void)buildMenuWithBuilder:(id<UIMenuBuilder>)builder { [super buildMenuWithBuilder:builder];if(builder.system!=UIMenuSystem.mainSystem)return;for(const auto& row:menu_rows){auto menu=native_menu(row,menu_ui);if(row.title=="File"||row.title=="Edit")[builder replaceMenuForIdentifier:menu.identifier withMenu:menu];else {[builder removeMenuForIdentifier:menu.identifier];[builder insertChildMenu:menu atEndOfMenuForIdentifier:UIMenuRoot];}} }
- (void)iiguiCommand:(UICommand*)command { const auto id=std::string(((NSString*)command.propertyList).UTF8String?:"");const auto ui=menu_ui;const auto find=[&](auto&& self,const std::vector<ii::gui::AppMenuItem>& rows)->void {for(const auto& row:rows){if(row.id==id&&row.enabled&&row.triggered){(void)ui.post(row.triggered);return;}self(self,row.children);}};find(find,menu_rows); }
@end
@interface IIGUIIOSScene : UIResponder <UIWindowSceneDelegate>
@property(nonatomic,strong) UIWindow* window;
@end
@implementation IIGUIIOSScene
- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)options { (void)session;(void)options;if(launch_scene)launch_scene((UIWindowScene*)scene);for(UIOpenURLContext* c in options.URLContexts)if(open_document)open_document(c.URL); }
- (void)scene:(UIScene*)scene openURLContexts:(NSSet<UIOpenURLContext*>*)contexts {(void)scene;for(UIOpenURLContext* c in contexts)if(open_document)open_document(c.URL);}
- (void)sceneDidBecomeActive:(UIScene*)scene { (void)scene;if(activate_scene)activate_scene(true); }
- (void)sceneWillResignActive:(UIScene*)scene { (void)scene;if(activate_scene)activate_scene(false); }
@end
@interface IIGUIIOSApplication : UIResponder <UIApplicationDelegate>
@end
@implementation IIGUIIOSApplication
- (UISceneConfiguration*)application:(UIApplication*)app configurationForConnectingSceneSession:(UISceneSession*)session options:(UISceneConnectionOptions*)options { (void)app;(void)options;auto config=[[UISceneConfiguration alloc] initWithName:@"iiSDK" sessionRole:session.role];config.delegateClass=IIGUIIOSScene.class;return config; }
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
    enum class Phase {created,running,closing,closed};
    HostOptions options;
    std::shared_ptr<detail::DispatchState> dispatch;
    std::shared_ptr<PresentationStream> presentations=std::make_shared<PresentationStream>([this](runtime::Task task){return dispatch->loop.post(std::move(task));});
    std::shared_ptr<WakeGate> gate=std::make_shared<WakeGate>();
    std::unique_ptr<runtime::FrameLoop> motion_frames;
    std::shared_ptr<MotionPreferences> motion_preferences=std::make_shared<MotionPreferences>();
    std::size_t motion_clients=0;std::optional<runtime::TickToken> present_tick;
    std::optional<double> display_origin;runtime::Clock::time_point frame_origin{};
    HostCallbacks callbacks;WindowSnapshot state;HostStats statistics;
    Phase phase=Phase::created;std::optional<core::Error> failure;
    const std::thread::id owner=std::this_thread::get_id();
    detail::CanvasState canvas;std::shared_ptr<detail::WidgetState> widgets,legacy;
    Widget legacy_text;bool mounting=false,draining=false,opened=false,appearance_ready=false,secondary=false,active=true;
    Impl* parent=nullptr;std::vector<Impl*> children;
    CFRunLoopSourceRef source=nullptr;CFRunLoopTimerRef timer=nullptr;
    UIWindow* window=nil;IIGUIIOSController* controller=nil;UIView* content=nil;
    CADisplayLink* display_link=nil;UIDocumentPickerViewController* picker=nil;
    NSMutableArray<NSURL*>* scopes=[NSMutableArray new];std::vector<AppMenuItem> menu;
    explicit Impl(HostOptions o):options(validate(std::move(o))),dispatch(std::make_shared<detail::DispatchState>(options.task_capacity,options.event_loop)){state.size=options.size;state.title=options.title;}
    bool on_owner()const{return owner==std::this_thread::get_id();}
    core::Result<void> ready()const{if(!on_owner())return core::fail(core::ErrorCode::wrong_executor,"UIKit access requires the owner");if(phase!=Phase::running)return core::fail(core::ErrorCode::closed,"GUI window is not open");return {};}
    void remember(std::string text,std::exception_ptr cause={}){NSLog(@"iiGUI UIKit error: %s",text.c_str());if(cause)try{std::rethrow_exception(cause);}catch(const std::exception& e){NSLog(@"iiGUI cause: %s",e.what());}catch(...){}if(!failure)failure=core::Error{core::ErrorCode::failed,std::move(text),cause};}
    template<class F> void guard(F&& f)noexcept{@try{try{f();}catch(...){remember("UIKit callback failed",std::current_exception());close();}}@catch(NSException* e){remember(e.reason.UTF8String?:"UIKit exception");close();}}
    void update_snapshot(){if(content){auto r=content.bounds;state.size={r.size.width,r.size.height};state.backing_scale=window.screen.scale;state.visible=!window.hidden&&phase==Phase::running;}}
    runtime::FrameLoop* frames(){return options.frame_loop?options.frame_loop:motion_frames.get();}
    detail::WidgetFrameHooks frame_hooks(){return {
        .frames=[this]()->core::Result<runtime::FrameLoop*>{if(!frames())motion_frames=std::make_unique<runtime::FrameLoop>(dispatch->loop);return frames();},
        .activity=[this](bool value){if(value)++motion_clients;else if(motion_clients)--motion_clients;if(phase==Phase::running)start_frames();},
        .preferences=motion_preferences,.renderer=options.renderer,.events=[this]()->core::Result<runtime::EventLoop*>{return &dispatch->loop;},.presentations=presentations};}
    void clear_legacy(){if(legacy){legacy->close();legacy.reset();legacy_text={};}}
    core::Result<void> show_legacy_text(){if(!legacy){auto next=std::make_shared<detail::WidgetState>([this](runtime::Task t){guard([&]{if(phase==Phase::running)t();});},WidgetAppearance{},frame_hooks());auto root=next->add(WidgetKind::vertical_stack,{},"iiGUI.legacy.root",{.padding=28},{},{},{});if(!root)return std::unexpected(root.error());auto label=next->add(WidgetKind::label,*root,"iiGUI.legacy.text",{},{.text=state.text,.font_size=17},{},{});if(!label)return std::unexpected(label.error());if(auto r=next->attach((__bridge void*)content);!r)return r;legacy_text=*label;legacy=std::move(next);}return legacy_text.set_text(state.text);}
    void stop_frames(){[display_link invalidate];display_link=nil;statistics.display_synced=false;}
    void start_frames(){if(display_link||!active||phase!=Phase::running)return;display_link=[CADisplayLink displayLinkWithTarget:controller selector:@selector(displayFrame:)];display_link.preferredFrameRateRange=CAFrameRateRangeMake(30,120,60);[display_link addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];statistics.display_synced=true;if(widgets)widgets->set_frame_paced(true);}
    void frame(CADisplayLink* link){if(phase!=Phase::running||draining||!active||!opened)return;guard([&]{if(!secondary)dispatch->loop.poll(options.budget);if(phase!=Phase::running)return;++statistics.display_callbacks;const double target=link.targetTimestamp;statistics.display_interval=std::chrono::duration_cast<runtime::Clock::duration>(std::chrono::duration<double>(target-link.timestamp));if(frames()){if(!display_origin){display_origin=target;frame_origin=runtime::Clock::now();}frames()->step(frame_origin+std::chrono::duration_cast<runtime::Clock::duration>(std::chrono::duration<double>(target-*display_origin)));}if(active){if(widgets)widgets->present();if(legacy)legacy->present();canvas.flush(state.backing_scale,state.size.height);}arm_timer();});}
    void arm_timer(){if(timer){CFRunLoopTimerInvalidate(timer);CFRelease(timer);timer=nullptr;}if(secondary||phase!=Phase::running)return;auto next=dispatch->loop.next_deadline();if(!next)return;double delay=std::chrono::duration<double>(*next-runtime::Clock::now()).count();if(delay<=0){gate->signal();return;}CFRunLoopTimerContext c{};c.info=this;timer=CFRunLoopTimerCreate(kCFAllocatorDefault,CFAbsoluteTimeGetCurrent()+delay,0,0,0,[](CFRunLoopTimerRef fired,void* p){auto& s=*static_cast<Impl*>(p);if(s.timer==fired){s.timer=nullptr;CFRelease(fired);}++s.statistics.timer_wakes;s.drain();},&c);CFRunLoopAddTimer(CFRunLoopGetMain(),timer,kCFRunLoopCommonModes);}
    void drain(){if(phase!=Phase::running||draining||!appearance_ready)return;guard([&]{struct Reset{bool& x;~Reset(){x=false;}} reset{draining};draining=true;++statistics.dispatches;if(!opened){opened=true;auto call=callbacks.opened;if(call)call();}if(phase!=Phase::running)return;if(!secondary)dispatch->loop.poll(options.budget);if(phase!=Phase::running)return;if(dispatch->loop.state()==runtime::LoopState::stopped){close();return;}if(active){if(widgets)widgets->present();if(legacy)legacy->present();canvas.flush(state.backing_scale,state.size.height);}arm_timer();});}
    void activation(bool value){active=value;display_origin.reset();if(frames())frames()->reset_clock();if(value){start_frames();gate->signal();}else stop_frames();for(auto* child:children)child->activation(value);}
    void open(UIWindowScene* scene){
        if(phase==Phase::running&&window){window.windowScene=scene;window.frame=scene.coordinateSpace.bounds;[window makeKeyAndVisible];activation(true);return;}
        controller=[IIGUIIOSController new];controller.view.backgroundColor=UIColor.blackColor;
        window=[[UIWindow alloc] initWithWindowScene:scene];window.rootViewController=controller;
        CGRect available=scene.coordinateSpace.bounds;
        if(secondary){double w=std::min(options.size.width,double(available.size.width)-24),h=std::min(options.size.height,double(available.size.height)-48);window.frame=CGRectMake((available.size.width-w)/2,(available.size.height-h)/2,w,h);window.windowLevel=parent->window.windowLevel+1;}else window.frame=available;
        content=[[UIView alloc] initWithFrame:controller.view.bounds];content.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;content.backgroundColor=UIColor.blackColor;[controller.view addSubview:content];
        controller->resize=[this]{if(phase!=Phase::running)return;guard([&]{content.frame=controller.view.safeAreaLayoutGuide.layoutFrame;update_snapshot();canvas.dirty=true;gate->signal();if(opened&&callbacks.resized)callbacks.resized(state.size);});};
        controller->frame=[this](CADisplayLink* value){frame(value);};
        controller->appeared=[this]{appearance_ready=true;gate->signal();};
        CFRunLoopSourceContext c{};c.info=this;c.perform=[](void* p){static_cast<Impl*>(p)->drain();};source=CFRunLoopSourceCreate(kCFAllocatorDefault,0,&c);CFRunLoopAddSource(CFRunLoopGetMain(),source,kCFRunLoopCommonModes);gate->attach(source);if(!secondary)dispatch->connect([wake=gate]() noexcept {wake->signal();});
        phase=Phase::running;[window makeKeyAndVisible];[controller.view layoutIfNeeded];update_snapshot();start_frames();gate->signal();
    }
    void cleanup(){gate->detach();if(timer){CFRunLoopTimerInvalidate(timer);CFRelease(timer);timer=nullptr;}if(source){CFRunLoopSourceInvalidate(source);CFRelease(source);source=nullptr;}if(controller){controller->resize={};controller->appeared={};controller->frame={};controller->picked={};}for(NSURL* url in scopes)[url stopAccessingSecurityScopedResource];[scopes removeAllObjects];picker=nil;content=nil;window=nil;controller=nil;if(parent){std::erase(parent->children,this);parent=nullptr;}}
    void close()noexcept{if(phase!=Phase::running)return;phase=Phase::closing;auto list=children;for(auto* child:list)child->close();children.clear();stop_frames();dispatch->retire();canvas.clear();clear_legacy();if(widgets){widgets->close();widgets.reset();}[picker dismissViewControllerAnimated:NO completion:nil];window.hidden=YES;state.visible=false;state.closed=true;presentations->close();phase=Phase::closed;auto call=std::move(callbacks.closed);auto endpoint=parent?parent->dispatch:nullptr;cleanup();if(secondary){if(call&&endpoint&&endpoint->accepting.load()){auto sender=endpoint->loop.sender();(void)endpoint->track(endpoint->tasks.post(sender,std::move(call)));}}else{dispatch->loop.request_stop();if(call)try{call();}catch(...){remember("UIKit close callback failed",std::current_exception());}}}
};
core::Result<std::shared_ptr<PresentationSource>> Host::presentation_source() const {
    if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Presentation source requires host owner");
    return std::static_pointer_cast<PresentationSource>(impl_->presentations);
}
Host::Host(HostOptions o):impl_(std::make_unique<Impl>(std::move(o))){}
Host::~Host(){if(!is_ui_thread())std::terminate();if(impl_->phase==Impl::Phase::running)impl_->close();impl_->presentations->close();impl_->dispatch->retire();impl_->gate->detach();}
UiExecutor Host::executor()const noexcept{return UiExecutor(impl_->dispatch);}
bool Host::is_ui_thread()const noexcept{return impl_->on_owner();}
core::Result<void> Host::run(HostCallbacks callbacks){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"UIKit host requires main thread");if(impl_->phase!=Impl::Phase::created||launch_scene)return core::fail(core::ErrorCode::busy,"UIKit application host is single use");impl_->callbacks=std::move(callbacks);launch_scene=[this](UIWindowScene* scene){impl_->guard([&]{impl_->open(scene);});};activate_scene=[this](bool active){impl_->activation(active);};open_document=[this](NSURL* url){if(!url.fileURL)return;if([url startAccessingSecurityScopedResource])[impl_->scopes addObject:url];auto path=std::string(url.path.UTF8String);auto call=impl_->callbacks.files_opened;if(call)(void)executor().post([call,path]{call({path});});};@autoreleasepool{char name[]="iiSDK";char* args[]={name,nullptr};UIApplicationMain(1,args,nil,NSStringFromClass(IIGUIIOSApplication.class));}launch_scene={};activate_scene={};open_document={};if(impl_->failure)return std::unexpected(*impl_->failure);return {};}
core::Result<void> Host::show(Host& parent,HostCallbacks callbacks){if(!is_ui_thread()||!parent.is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"UIKit child requires main thread");if(impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"UIKit child is single use");if(auto r=parent.impl_->ready();!r)return r;impl_->dispatch->retire();impl_->dispatch=std::make_shared<detail::DispatchState>(impl_->options.task_capacity,&parent.impl_->dispatch->loop);impl_->secondary=true;impl_->parent=parent.impl_.get();impl_->callbacks=std::move(callbacks);parent.impl_->children.push_back(impl_.get());try{impl_->open(parent.impl_->window.windowScene);return {};}catch(...){return core::fail(core::ErrorCode::failed,"Cannot open UIKit child",std::current_exception());}}
core::Result<void> Host::choose_file(FileDialogOptions options,FileDialogCompletion completion){if(auto r=impl_->ready();!r)return r;if(!completion||!detail::valid_dialog(options))return core::fail(core::ErrorCode::invalid_argument,"Invalid document picker options");if(impl_->picker)return core::fail(core::ErrorCode::busy,"Document picker is open");NSMutableArray<UTType*>* types=[NSMutableArray new];if(options.mode==FileDialogMode::directory||options.mode==FileDialogMode::save)[types addObject:UTTypeFolder];else{for(const auto& filter:options.filters)for(const auto& ext:filter.extensions){auto type=[UTType typeWithFilenameExtension:native(ext)];if(type)[types addObject:type];}if(!types.count)[types addObject:UTTypeData];}auto picker=[[UIDocumentPickerViewController alloc] initForOpeningContentTypes:types asCopy:options.mode==FileDialogMode::open];picker.allowsMultipleSelection=options.multiple;picker.delegate=impl_->controller;picker.title=native(options.title);impl_->picker=picker;auto weak=std::weak_ptr(impl_->dispatch);auto host=impl_.get();auto ui=executor();impl_->controller->picked=[weak,host,ui,completion,options](NSArray<NSURL*>* urls){auto endpoint=weak.lock();if(!endpoint||!endpoint->accepting.load())return;FileDialogSelection result;result.canceled=urls==nil;for(NSURL* url in urls){if([url startAccessingSecurityScopedResource])[host->scopes addObject:url];auto selected=options.mode==FileDialogMode::save?[url URLByAppendingPathComponent:native(options.filename)]:url;result.paths.emplace_back(selected.path.UTF8String);}host->picker=nil;(void)ui.post([completion,result=std::move(result)]()mutable{completion(std::move(result));});};[impl_->controller presentViewController:picker animated:YES completion:nil];return {};}
core::Result<void> Host::window_action(WindowAction action){if(auto r=impl_->ready();!r)return r;if(action==WindowAction::raise||action==WindowAction::restore){[impl_->window makeKeyAndVisible];return {};}if(unsigned(action)>unsigned(WindowAction::raise))return core::fail(core::ErrorCode::invalid_argument,"Invalid UIKit window action");return core::fail(core::ErrorCode::unsupported,"UIKit scene geometry is managed by iPadOS");}
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
core::Result<void> Host::set_content_size(Size size){if(auto r=impl_->ready();!r)return r;if(!detail::valid_size(size))return core::fail(core::ErrorCode::invalid_argument,"Invalid content size");if(!impl_->secondary)return core::fail(core::ErrorCode::unsupported,"UIKit scene size belongs to the operating system");auto r=impl_->window.frame;r.size=CGSizeMake(size.width,size.height);impl_->window.frame=r;[impl_->controller.view layoutIfNeeded];impl_->update_snapshot();return {};}
core::Result<void> Host::set_window_geometry(const WindowGeometry& update){
    if(auto r=impl_->ready();!r)return r;
    impl_->update_snapshot();
    auto next=resolve_window_geometry({impl_->state.title,impl_->state.size,impl_->options.minimum_size},update);
    if(!next)return std::unexpected(next.error());
    if(!impl_->secondary&&(update.width||update.height||update.minimum_width||update.minimum_height))
        return core::fail(core::ErrorCode::unsupported,"UIKit scene geometry belongs to the operating system");
    if(impl_->secondary){auto r=impl_->window.frame;r.size=CGSizeMake(next->size.width,next->size.height);impl_->window.frame=r;[impl_->controller.view layoutIfNeeded];}
    impl_->options.minimum_size=next->minimum_size;impl_->state.title=next->title;impl_->controller.title=native(next->title);impl_->update_snapshot();return {};
}
core::Result<void> Host::request_close(){if(auto r=impl_->ready();!r)return r;if(!impl_->callbacks.close_requested||impl_->callbacks.close_requested())impl_->close();return {};}
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
            auto hooks=impl_->frame_hooks();hooks.window_geometry=[this](const WindowGeometry& value){return set_window_geometry(value);};
        try {
            auto replacement = std::make_shared<detail::WidgetState>([host=impl_.get()](runtime::Task work) {
                host->guard([&] { if (host->phase == Impl::Phase::running) work(); });
            }, appearance,std::move(hooks));
            if (auto built = replacement->construct(std::move(factory)); !built) return std::unexpected(built.error());
            if (auto ready = impl_->ready(); !ready) return std::unexpected(ready.error());
            if (auto attached = replacement->attach((__bridge void*)impl_->content); !attached)
                return std::unexpected(attached.error());
            if(auto committed=replacement->commit_window_properties();!committed)return std::unexpected(committed.error());
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
core::Result<HostStats> Host::stats() const {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Stats require the main thread");
    auto result = impl_->statistics;
    result.loop = impl_->dispatch->loop.stats();
    result.host = impl_->dispatch->host_stats();
    return result;
}
} // namespace ii::gui
