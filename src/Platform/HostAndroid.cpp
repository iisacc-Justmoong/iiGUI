#include <iiGUI/Android.h>
#include <iiGUI/AndroidSurface.h>
#include <iiGUI/HostCommon.h>
#include <iiGUI/DispatchState.h>
#include <iiGUI/CanvasState.h>
#include <iiGUI/WidgetState.h>
#include <iiGUI/MountTransaction.h>
#include <iiGUI/PendingCallbacks.h>
#include <android/configuration.h>
#include <android/choreographer.h>
#include <android/log.h>
#include <android/window.h>
#include <mutex>

namespace ii::gui {
namespace {
thread_local android_app* current_app=nullptr;
struct WakeGate final {
    std::mutex mutex;
    ALooper* looper=nullptr;
    void attach(ALooper* value){std::lock_guard lock(mutex);looper=value;ALooper_acquire(looper);}
    void signal(){std::lock_guard lock(mutex);if(looper)ALooper_wake(looper);}
    void detach(){std::lock_guard lock(mutex);if(looper){ALooper_release(looper);looper=nullptr;}}
    ~WakeGate(){detach();}
};
}
namespace detail {android_app* android_application()noexcept{return current_app;}}

core::Result<int> run_android_application(android_app* native,std::function<core::Result<void>(runtime::Application&)> configure){
    if(!native||!native->activity||!native->looper||!configure)
        return core::fail(core::ErrorCode::invalid_argument,"Android application requires native_app_glue and a configuration callback");
    if(current_app)return core::fail(core::ErrorCode::busy,"An Android application is already bound to this thread");
    struct Binding final{~Binding(){current_app=nullptr;}} binding;
    current_app=native;
    core::Result<int> result=core::fail(core::ErrorCode::failed,"Android application did not start");
    try{
        runtime::Application application({"AndroidApplication"});
        if(auto configured=configure(application);!configured)result=std::unexpected(configured.error());
        else result=application.exec();
    }catch(...){result=core::fail(core::ErrorCode::failed,"Android application failed",std::current_exception());}
    // Core has retired its scopes before native_app_glue releases its activity.
    if(!native->destroyRequested)ANativeActivity_finish(native->activity);
    while(!native->destroyRequested){android_poll_source* source=nullptr;const auto event=ALooper_pollOnce(-1,nullptr,nullptr,reinterpret_cast<void**>(&source));if(event==ALOOPER_POLL_ERROR)break;if(source)source->process(native,source);}
    return result;
}

struct Host::Impl final {
    enum class Phase {created,running,closing,closed};
    HostOptions options;
    std::shared_ptr<detail::DispatchState> dispatch;
    std::shared_ptr<PresentationStream> presentations=std::make_shared<PresentationStream>([this](runtime::Task task){return dispatch->loop.post(std::move(task));});
    std::shared_ptr<WakeGate> gate=std::make_shared<WakeGate>();
    const std::thread::id owner=std::this_thread::get_id();
    android_app* app;
    detail::AndroidSurfaceParent surface;
    HostCallbacks callbacks;
    WindowSnapshot state;
    HostStats statistics;
    Phase phase=Phase::created;
    std::optional<core::Error> failure;
    detail::CanvasState canvas;
    std::shared_ptr<detail::WidgetState> widgets;
    std::unique_ptr<runtime::FrameLoop> motion_frames;
    std::shared_ptr<MotionPreferences> preferences=std::make_shared<MotionPreferences>();
    struct FrameGate final {Impl* owner=nullptr;bool pending=false;};
    std::shared_ptr<FrameGate> frame_gate=std::make_shared<FrameGate>();
    static detail::PendingCallbacks<FrameGate>& pending_frames(){static thread_local detail::PendingCallbacks<FrameGate> pending(128);return pending;}
    bool opened=false,draining=false,mounting=false,resumed=false,close_pending=false;
    std::size_t motion_clients=0;std::int64_t last_frame=0;
    explicit Impl(HostOptions value):options(detail::validate_options(std::move(value))),
        dispatch(std::make_shared<detail::DispatchState>(options.task_capacity,options.event_loop)),app(current_app){
        if(!app)throw std::invalid_argument("Android Host must be constructed inside run_android_application");
        surface.app=app;state.title=options.title;state.size=options.size;frame_gate->owner=this;
    }
    bool on_owner()const{return owner==std::this_thread::get_id();}
    core::Result<void> ready()const{if(!on_owner())return core::fail(core::ErrorCode::wrong_executor,"Android GUI access requires its native Looper thread");if(phase!=Phase::running||close_pending||!surface.window)return core::fail(core::ErrorCode::closed,"Android window is not available");return {};}
    void remember(std::string message,std::exception_ptr cause={}){
        if(cause)try{std::rethrow_exception(cause);}catch(const std::exception& e){message+=": ";message+=e.what();}catch(...){}
        __android_log_write(ANDROID_LOG_ERROR,"iiGUI",message.c_str());
        if(!failure)failure=core::Error{core::ErrorCode::failed,std::move(message),cause};
    }
    template<class F>void guard(F&& work)noexcept{try{work();}catch(...){remember("Android GUI callback failed",std::current_exception());defer_close();}}
    void update(){
        const auto density=AConfiguration_getDensity(app->config);
        surface.scale=density>0&&density<1000?density/160.:1.;state.backing_scale=surface.scale;
        if(surface.window)state.size={ANativeWindow_getWidth(surface.window)/surface.scale,ANativeWindow_getHeight(surface.window)/surface.scale};
        surface.active=resumed&&surface.window;state.visible=surface.active&&phase==Phase::running;
    }
    void notify(detail::AndroidSurfaceEventKind kind){if(widgets){const detail::AndroidSurfaceEvent e{kind};(void)widgets->native_event(&e);}}
    runtime::FrameLoop* frames(){return options.frame_loop?options.frame_loop:motion_frames.get();}
    void ensure_cadence(){
        if(close_pending||frame_gate->pending||!surface.active||(!motion_clients&&!options.frame_loop))return;
        auto* choreographer=AChoreographer_getInstance();if(!choreographer)throw std::runtime_error("Android display choreographer is unavailable");
        if(!pending_frames().hold(frame_gate))throw std::runtime_error("Android frame callback ownership is full");
        frame_gate->pending=true;
        AChoreographer_postFrameCallback64(choreographer,[](std::int64_t timestamp,void* raw){
            auto gate=pending_frames().take(static_cast<FrameGate*>(raw));if(!gate)return;
            gate->pending=false;auto* host=gate->owner;
            if(!host||host->phase!=Phase::running||!host->surface.active)return;
            host->guard([&]{
                ++host->statistics.display_callbacks;host->statistics.display_synced=true;
                if(host->last_frame>0)host->statistics.display_interval=std::chrono::duration_cast<runtime::Clock::duration>(std::chrono::nanoseconds(timestamp-host->last_frame));host->last_frame=timestamp;
                if(host->frames())(void)host->frames()->step(runtime::Clock::time_point(std::chrono::duration_cast<runtime::Clock::duration>(std::chrono::nanoseconds(timestamp))));
                if(host->widgets)host->widgets->present();host->ensure_cadence();
            });
        },frame_gate.get());
    }
    detail::WidgetFrameHooks hooks(){return {
        .frames=[this]()->core::Result<runtime::FrameLoop*>{if(!frames())motion_frames=std::make_unique<runtime::FrameLoop>(dispatch->loop);ensure_cadence();return frames();},
        .activity=[this](bool active){if(active)++motion_clients;else if(motion_clients)--motion_clients;if(motion_clients||options.frame_loop)ensure_cadence();},.preferences=preferences,.renderer=options.renderer,
        .events=[this]()->core::Result<runtime::EventLoop*>{return &dispatch->loop;},.presentations=presentations};}
    void defer_close()noexcept{close_pending=true;dispatch->retire();dispatch->loop.request_stop();gate->signal();}
    void close()noexcept{
        if(phase!=Phase::running)return;phase=Phase::closing;frame_gate->owner=nullptr;dispatch->retire();
        canvas.clear();if(widgets){widgets->close();widgets.reset();}
        state.visible=false;state.closed=true;presentations->close();surface.active=false;phase=Phase::closed;dispatch->loop.request_stop();
        if(callbacks.closed)try{callbacks.closed();}catch(...){remember("Android close callback failed",std::current_exception());}
        gate->signal();
    }
    bool may_close(){return !callbacks.close_requested||callbacks.close_requested();}
    void command(std::int32_t command){guard([&]{
        switch(command){
        case APP_CMD_INIT_WINDOW:surface.window=app->window;update();notify(detail::AndroidSurfaceEventKind::window);ensure_cadence();break;
        case APP_CMD_TERM_WINDOW:surface.window=nullptr;update();canvas.clear();notify(detail::AndroidSurfaceEventKind::window);break;
        case APP_CMD_RESUME:last_frame=0;resumed=true;update();if(frames())frames()->reset_clock();notify(detail::AndroidSurfaceEventKind::activation);ensure_cadence();break;
        case APP_CMD_PAUSE:resumed=false;update();notify(detail::AndroidSurfaceEventKind::activation);break;
        case APP_CMD_CONFIG_CHANGED:case APP_CMD_WINDOW_RESIZED:case APP_CMD_CONTENT_RECT_CHANGED:
            update();canvas.dirty=true;notify(detail::AndroidSurfaceEventKind::resize);if(opened&&callbacks.resized)callbacks.resized(state.size);break;
        case APP_CMD_DESTROY:defer_close();break;
        default:break;
        }
    });}
    int input(AInputEvent* input){int handled=0;guard([&]{
        if(AInputEvent_getType(input)==AINPUT_EVENT_TYPE_KEY&&AKeyEvent_getKeyCode(input)==AKEYCODE_BACK){handled=1;if(AKeyEvent_getAction(input)==AKEY_EVENT_ACTION_UP&&may_close())defer_close();return;}
        if(widgets){const detail::AndroidSurfaceEvent event{detail::AndroidSurfaceEventKind::input,input};handled=widgets->native_event(&event)?1:0;}
    });return handled;}
    void drain(){if(phase!=Phase::running||draining)return;if(close_pending){close();return;}guard([&]{
        struct Reset final{bool& value;~Reset(){value=false;}} reset{draining};draining=true;++statistics.dispatches;update();
        if(!opened&&surface.active){opened=true;if(callbacks.opened)callbacks.opened();}
        if(!opened||phase!=Phase::running)return;
        dispatch->loop.poll(options.budget);
        if(dispatch->loop.state()==runtime::LoopState::stopped){defer_close();return;}
        if(surface.active&&phase==Phase::running&&!close_pending){if(widgets)widgets->present();canvas.flush(surface.scale,state.size.height);}
    });if(close_pending)close();}
};

core::Result<std::shared_ptr<PresentationSource>> Host::presentation_source() const {
    if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Presentation source requires host owner");
    return std::static_pointer_cast<PresentationSource>(impl_->presentations);
}
Host::Host(HostOptions options):impl_(std::make_unique<Impl>(std::move(options))){}
Host::~Host(){if(!is_ui_thread())std::terminate();impl_->close();impl_->frame_gate->owner=nullptr;impl_->presentations->close();impl_->dispatch->retire();impl_->gate->detach();}
UiExecutor Host::executor()const noexcept{return UiExecutor(impl_->dispatch);}
bool Host::is_ui_thread()const noexcept{return impl_->on_owner();}
core::Result<void> Host::run(HostCallbacks callbacks){
    if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Android host requires its native Looper thread");
    if(impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"Android host is single use");
    auto* app=impl_->app;auto* previous=app->userData;const auto old_command=app->onAppCmd;const auto old_input=app->onInputEvent;
    struct Restore final{android_app* app;void* data;decltype(android_app::onAppCmd) command;decltype(android_app::onInputEvent) input;~Restore(){app->userData=data;app->onAppCmd=command;app->onInputEvent=input;}} restore{app,previous,old_command,old_input};
    impl_->callbacks=std::move(callbacks);app->userData=impl_.get();
    app->onAppCmd=[](android_app* app,std::int32_t value){static_cast<Impl*>(app->userData)->command(value);};
    app->onInputEvent=[](android_app* app,AInputEvent* value)->std::int32_t{return static_cast<Impl*>(app->userData)->input(value);};
    impl_->gate->attach(app->looper);impl_->dispatch->connect([wake=impl_->gate]()noexcept{wake->signal();});
    impl_->phase=Impl::Phase::running;ANativeActivity_setWindowFlags(app->activity,AWINDOW_FLAG_FULLSCREEN,0);
    while(impl_->phase==Impl::Phase::running&&!app->destroyRequested){
        android_poll_source* source=nullptr;
        // Before the first surface, only native lifecycle events can start the
        // app. Afterwards Core deadlines remain dispatchable while GPU work is
        // suspended, so an expired background timer cannot cause a busy loop.
        const auto timeout=impl_->opened?detail::deadline_timeout(impl_->dispatch->loop.next_deadline()):-1;
        const auto event=ALooper_pollOnce(timeout,nullptr,nullptr,reinterpret_cast<void**>(&source));
        if(event==ALOOPER_POLL_ERROR){impl_->remember("Android Looper poll failed");impl_->close();break;}
        if(source)source->process(app,source);
        impl_->drain();
    }
    impl_->close();(void)impl_->dispatch->disconnect();impl_->gate->detach();
    if(impl_->failure)return std::unexpected(*impl_->failure);return {};
}
core::Result<void> Host::show(Host&,HostCallbacks){return core::fail(core::ErrorCode::unsupported,"Android secondary windows are not implemented");}
core::Result<void> Host::choose_file(FileDialogOptions,FileDialogCompletion){return core::fail(core::ErrorCode::unsupported,"Android document picker is not implemented");}
core::Result<void> Host::window_action(WindowAction){return core::fail(core::ErrorCode::unsupported,"Android owns activity geometry");}
core::Result<void> Host::set_app_menu(std::vector<AppMenuItem>){return core::fail(core::ErrorCode::unsupported,"Android native menus are not implemented");}
core::Result<void> Host::set_title(std::string_view title){if(auto r=impl_->ready();!r)return r;if(!detail::valid_title(title))return core::fail(core::ErrorCode::invalid_argument,"Invalid Android window title");impl_->state.title=title;return {};}
core::Result<void> Host::set_text(std::string_view){return core::fail(core::ErrorCode::unsupported,"Use an LVUI or iiGUI native label on Android");}
core::Result<void> Host::set_content_size(Size){return core::fail(core::ErrorCode::unsupported,"Android owns activity size");}
core::Result<void> Host::set_window_geometry(const WindowGeometry& update){if(auto r=impl_->ready();!r)return r;if(update.width||update.height||update.minimum_width||update.minimum_height)return core::fail(core::ErrorCode::unsupported,"Android owns activity size");return update.title?set_title(*update.title):core::Result<void>{};}
core::Result<void> Host::request_close(){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Android close requires its owner");if(impl_->phase!=Impl::Phase::running)return core::fail(core::ErrorCode::closed,"Android host is closed");impl_->guard([&]{if(impl_->may_close())impl_->defer_close();});return {};}
core::Result<void> Host::set_canvas(CanvasFrame frame){if(auto r=impl_->ready();!r)return r;if(valid_canvas(frame)&&!detail::full_surface_canvas(frame,impl_->state.size))return core::fail(core::ErrorCode::unsupported,"This host requires a full-window raw canvas; compose subregions with C++ widgets");if(impl_->mounting)return core::fail(core::ErrorCode::busy,"Widget factory is active");if(frame.renderer==Renderer::automatic)frame.renderer=impl_->options.renderer;auto result=impl_->canvas.set(std::move(frame),impl_->surface.window,nullptr,impl_->surface.scale,impl_->presentations);if(result){if(impl_->widgets){impl_->widgets->close();impl_->widgets.reset();}impl_->gate->signal();}return result;}
core::Result<WidgetMount> Host::mount_widgets(WidgetFactory factory){return mount_widgets(std::move(factory),WidgetAppearance{});}
core::Result<WidgetMount> Host::mount_widgets(WidgetFactory factory,const WidgetAppearance& appearance){
    if(auto r=impl_->ready();!r)return std::unexpected(r.error());if(!factory)return core::fail(core::ErrorCode::invalid_argument,"Empty native widget factory");if(impl_->mounting)return core::fail(core::ErrorCode::busy,"Widget factory is active");
    struct Reset final{bool& value;~Reset(){value=false;}} reset{impl_->mounting};impl_->mounting=true;
    detail::MountTransaction transaction([this]()noexcept{if(impl_->phase!=Impl::Phase::running)return;impl_->guard([&]{if(impl_->widgets){const detail::AndroidSurfaceEvent event{detail::AndroidSurfaceEventKind::resize};(void)impl_->widgets->native_event(&event);impl_->widgets->present();}else if(impl_->canvas.frame){impl_->canvas.dirty=true;impl_->canvas.flush(impl_->surface.scale,impl_->state.size.height);}});});
    try{
        auto hooks=impl_->hooks();hooks.window_geometry=[this](const WindowGeometry& value){return set_window_geometry(value);};
        auto replacement=std::make_shared<detail::WidgetState>([host=impl_.get()](runtime::Task task){host->guard([&]{if(host->phase==Impl::Phase::running&&!host->close_pending)task();});},appearance,std::move(hooks));
        if(auto result=replacement->construct(std::move(factory));!result)return std::unexpected(result.error());
        if(auto result=impl_->ready();!result)return std::unexpected(result.error());
        if(auto result=replacement->attach(&impl_->surface);!result)return std::unexpected(result.error());
        if(auto result=replacement->commit_window_properties();!result)return std::unexpected(result.error());
        if(auto result=impl_->ready();!result)return std::unexpected(result.error());
        impl_->canvas.clear();if(impl_->widgets)impl_->widgets->close();impl_->widgets=std::move(replacement);transaction.commit();auto published=impl_->widgets;published->commit_publication();if(auto alive=published->ready();!alive)return std::unexpected(alive.error());return WidgetMount(published);
    }catch(...){return core::fail(core::ErrorCode::failed,"Android widget mounting failed",std::current_exception());}
}
core::Result<void> Host::unmount_widgets(){if(auto r=impl_->ready();!r)return r;if(impl_->mounting)return core::fail(core::ErrorCode::busy,"Widget factory is active");if(impl_->widgets){impl_->widgets->close();impl_->widgets.reset();}return {};}
core::Result<void> Host::unmount_widgets(const WidgetMount& expected){if(auto r=impl_->ready();!r)return r;auto owner=expected.state_.lock();if(!owner)return core::fail(core::ErrorCode::closed,"Native widget generation is closed");if(owner!=impl_->widgets)return core::fail(core::ErrorCode::invalid_argument,"Widget generation belongs to another host");return unmount_widgets();}
core::Result<void> Host::clear_canvas(){if(auto r=impl_->ready();!r)return r;impl_->canvas.clear();return {};}
core::Result<GraphicsInfo> Host::graphics_info()const{if(auto r=impl_->ready();!r)return std::unexpected(r.error());return impl_->widgets?impl_->widgets->graphics_info():core::Result<GraphicsInfo>{impl_->canvas.info()};}
core::Result<PixelBuffer> Host::capture_canvas(){if(auto r=impl_->ready();!r)return std::unexpected(r.error());return impl_->widgets?impl_->widgets->capture():impl_->canvas.capture(impl_->surface.scale,impl_->state.size.height);}
core::Result<std::reference_wrapper<runtime::EventLoop>> Host::event_loop(){if(auto r=impl_->ready();!r)return std::unexpected(r.error());return std::ref(impl_->dispatch->loop);}
core::Result<WindowSnapshot> Host::snapshot()const{if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Android snapshot requires its owner");if(impl_->phase==Impl::Phase::created)return core::fail(core::ErrorCode::closed,"Android host has not opened");impl_->update();return impl_->state;}
core::Result<HostStats> Host::stats()const{if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Android stats require its owner");auto value=impl_->statistics;value.loop=impl_->dispatch->loop.stats();value.host=impl_->dispatch->host_stats();return value;}
}
