#include <iiGUI/Android.h>
#include <iiGUI/AndroidSurface.h>
#include "AndroidJni.h"
#include "DisplayPixelsPolicy.h"
#include <iiGUI/HostCommon.h>
#include <iiGUI/HostActivity.h>
#include "HostFailure.h"
#include "HostCallback.h"
#include "AndroidFrameCadence.h"
#include "AndroidBackInput.h"
#include <iiGUI/DispatchState.h>
#include <iiGUI/CanvasState.h>
#include <iiGUI/WidgetState.h>
#include <iiGUI/WidgetSurfaceNative.h>
#include <iiGUI/MountTransaction.h>
#include <iiGUI/PendingCallbacks.h>
#include <android/configuration.h>
#include <android/choreographer.h>
#include <android/log.h>
#include <android/window.h>
#include <limits>
#include <mutex>
#include <time.h>

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
    detail::HostActivity activity,foreground;
    WindowSnapshot state;
    HostStats statistics;
    Phase phase=Phase::created;
    detail::HostFailure failure;
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
        surface.app=app;surface.sender=dispatch->loop.sender();state.title=options.title;state.size=options.size;frame_gate->owner=this;
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
    static std::optional<std::int64_t> monotonic_frame_time()noexcept{
        timespec now{};
        constexpr std::int64_t billion=1000000000;
        if(clock_gettime(CLOCK_MONOTONIC,&now)!=0||now.tv_sec<0||now.tv_nsec<0||now.tv_nsec>=billion||
           now.tv_sec>(std::numeric_limits<std::int64_t>::max()-now.tv_nsec)/billion)
            return std::nullopt;
        return static_cast<std::int64_t>(now.tv_sec)*billion+now.tv_nsec;
    }
    static std::int64_t legacy_frame_time(long frame){
        const auto reconstructed=detail::android_legacy_frame_time(frame,[]{return monotonic_frame_time();});
        if(!reconstructed)throw std::runtime_error("Android legacy frame clock or timestamp is invalid");
        return *reconstructed;
    }
    template<class Timestamp>static void dispatch_frame(void* raw,Timestamp timestamp){
        auto gate=pending_frames().take(static_cast<FrameGate*>(raw));if(!gate)return;
        gate->pending=false;auto* host=gate->owner;
        if(!host||host->phase!=Phase::running||!host->surface.active)return;
        host->guard([&]{
            const auto time=timestamp();
            ++host->statistics.display_callbacks;host->statistics.display_synced=true;
            if(host->last_frame>0)host->statistics.display_interval=std::chrono::duration_cast<runtime::Clock::duration>(std::chrono::nanoseconds(time-host->last_frame));host->last_frame=time;
            if(host->frames())(void)host->frames()->step(runtime::Clock::time_point(std::chrono::duration_cast<runtime::Clock::duration>(std::chrono::nanoseconds(time))));
            if(host->widgets)host->widgets->present();host->ensure_cadence();
        });
    }
    static void legacy_frame(long timestamp,void* raw){dispatch_frame(raw,[timestamp]{return legacy_frame_time(timestamp);});}
    static void modern_frame(std::int64_t timestamp,void* raw){dispatch_frame(raw,[timestamp]{return timestamp;});}
    void ensure_cadence(){
        if(close_pending||frame_gate->pending||!surface.active||(!motion_clients&&!options.frame_loop))return;
        auto* choreographer=AChoreographer_getInstance();if(!choreographer)throw std::runtime_error("Android display choreographer is unavailable");
        if(!pending_frames().hold(frame_gate))throw std::runtime_error("Android frame callback ownership is full");
        frame_gate->pending=true;
        detail::post_android_frame<&Impl::legacy_frame,&Impl::modern_frame>(choreographer,frame_gate.get());
    }
    detail::WidgetFrameHooks hooks(){return {
        .frames=[this]()->core::Result<runtime::FrameLoop*>{if(!frames())motion_frames=std::make_unique<runtime::FrameLoop>(dispatch->loop);ensure_cadence();return frames();},
        .activity=[this](bool active){if(active)++motion_clients;else if(motion_clients)--motion_clients;if(motion_clients||options.frame_loop)ensure_cadence();},.preferences=preferences,.renderer=options.renderer,
        .events=[this]()->core::Result<runtime::EventLoop*>{return &dispatch->loop;},.presentations=presentations};}
    void defer_close()noexcept{close_pending=true;dispatch->retire();dispatch->loop.request_stop();gate->signal();}
    void close()noexcept{
        if(phase!=Phase::running)return;phase=Phase::closing;activity.close();foreground.close();frame_gate->owner=nullptr;dispatch->retire();
        canvas.clear();if(widgets){widgets->close();widgets.reset();}
        state.visible=false;state.closed=true;presentations->close();surface.active=false;phase=Phase::closed;dispatch->loop.request_stop();
        if(callbacks.closed)try{callbacks.closed();}catch(...){remember("Android close callback failed",std::current_exception());}
        gate->signal();
    }
    bool may_close(){return !callbacks.close_requested||callbacks.close_requested();}
    void command(std::int32_t command){guard([&]{
        switch(command){
        case APP_CMD_START:foreground.observe(true);break;
        case APP_CMD_STOP:foreground.observe(false);break;
        case APP_CMD_INIT_WINDOW:surface.window=app->window;update();notify(detail::AndroidSurfaceEventKind::window);ensure_cadence();break;
        case APP_CMD_TERM_WINDOW:surface.window=nullptr;update();canvas.clear();notify(detail::AndroidSurfaceEventKind::window);break;
        case APP_CMD_RESUME:last_frame=0;resumed=true;activity.observe(true);update();if(frames())frames()->reset_clock();notify(detail::AndroidSurfaceEventKind::activation);ensure_cadence();break;
        case APP_CMD_PAUSE:resumed=false;activity.observe(false);update();notify(detail::AndroidSurfaceEventKind::activation);break;
        case APP_CMD_LOST_FOCUS:notify(detail::AndroidSurfaceEventKind::focus_lost);break;
        case APP_CMD_CONFIG_CHANGED:case APP_CMD_WINDOW_RESIZED:case APP_CMD_CONTENT_RECT_CHANGED:
            update();canvas.dirty=true;notify(detail::AndroidSurfaceEventKind::resize);if(opened&&callbacks.resized)callbacks.resized(state.size);break;
        case APP_CMD_DESTROY:defer_close();break;
        default:break;
        }
    });}
    int input(AInputEvent* input){int handled=0;guard([&]{
        if(AInputEvent_getType(input)==AINPUT_EVENT_TYPE_KEY){
            if(auto back=detail::android_back_close(AInputEvent_getType(input),AKeyEvent_getKeyCode(input),AKeyEvent_getAction(input),AKeyEvent_getFlags(input))){
                handled=1;if(*back&&may_close())defer_close();return;
            }
        }
        if(widgets){const detail::AndroidSurfaceEvent event{detail::AndroidSurfaceEventKind::input,input};handled=widgets->native_event(&event)?1:0;}
    });return handled;}
    void drain(){if(phase!=Phase::running||draining)return;if(close_pending){close();return;}guard([&]{
        struct Reset final{bool& value;~Reset(){value=false;}} reset{draining};draining=true;++statistics.dispatches;update();
        if(!opened&&surface.active){opened=true;detail::deliver_host_opened(callbacks.opened);if(phase==Phase::running&&!close_pending){foreground.open();activity.open();}}
        if(!opened||phase!=Phase::running)return;
        if(!close_pending)foreground.deliver();
        if(!close_pending)activity.deliver();
        if(close_pending||phase!=Phase::running)return;
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
core::Result<void> Host::set_application_driver(bool enabled){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Application driver requires the owner thread");if(impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"Application driver must be configured before run");if(enabled)return core::fail(core::ErrorCode::unsupported,"Android does not support an independent application driver");return {};}
core::Result<void> Host::set_application_activity_handler(std::function<void(bool)> handler){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Application activity requires the host owner thread");if(impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"Application activity must be configured before run");impl_->activity.set_handler(std::move(handler));return {};}
core::Result<void> Host::set_application_foreground_handler(std::function<void(bool)> handler){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Application foreground requires the host owner thread");if(impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"Application foreground must be configured before run");impl_->foreground.set_handler(std::move(handler));return {};}
core::Result<void> Host::set_failure_handler(std::function<void(const core::Error&)> handler) {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Host failure handling requires the owner thread");
    if (impl_->phase != Impl::Phase::created) return core::fail(core::ErrorCode::busy, "Host failure handling must be configured before run or show");
    impl_->failure.set_handler(std::move(handler));
    return {};
}
core::Result<void> Host::run(HostCallbacks callbacks){
    if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Android host requires its native Looper thread");
    if(impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"Android host is single use");
    auto* app=impl_->app;auto* previous=app->userData;const auto old_command=app->onAppCmd;const auto old_input=app->onInputEvent;
    if(app->activityState==APP_CMD_RESUME)impl_->activity.observe(true);else if(app->activityState==APP_CMD_PAUSE)impl_->activity.observe(false);
    if(app->activityState==APP_CMD_START||app->activityState==APP_CMD_RESUME||app->activityState==APP_CMD_PAUSE)impl_->foreground.observe(true);
    else if(app->activityState==APP_CMD_STOP)impl_->foreground.observe(false);
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
core::Result<void> Host::show_external(HostCallbacks){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"External host requires its owner thread");return core::fail(core::ErrorCode::unsupported,"External event-loop hosting is not implemented on Android");}
core::Result<void> Host::attach_external(void*,HostCallbacks){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"External host requires its owner thread");return core::fail(core::ErrorCode::unsupported,"External surface hosting is not implemented on Android");}
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
        if(auto result=replacement->attach(&impl_->surface, detail::make_native_widget_surface);!result)return std::unexpected(result.error());
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
core::Result<DisplayPixels> Host::display_pixels()const{
    if(auto ready=impl_->ready();!ready)return std::unexpected(ready.error());
    try {
        detail::AndroidJni j;
        const auto activity=j.type("android/app/Activity"), manager_type=j.type("android/view/WindowManager");
        auto manager=j.env->CallObjectMethod(j.activity,j.method(activity,"getWindowManager","()Landroid/view/WindowManager;"));j.check();
        if(!manager)return core::fail(core::ErrorCode::busy,"Android window manager is unavailable");
        auto display=j.env->CallObjectMethod(manager,j.method(manager_type,"getDefaultDisplay","()Landroid/view/Display;"));j.check();
        if(!display)return core::fail(core::ErrorCode::busy,"Android display is unavailable");
        const auto display_type=j.type("android/view/Display"),mode_type=j.type("android/view/Display$Mode");
        auto mode=j.env->CallObjectMethod(display,j.method(display_type,"getMode","()Landroid/view/Display$Mode;"));j.check();
        if(!mode)return core::fail(core::ErrorCode::busy,"Android display mode is unavailable");
        int width=j.env->CallIntMethod(mode,j.method(mode_type,"getPhysicalWidth","()I"));j.check();
        int height=j.env->CallIntMethod(mode,j.method(mode_type,"getPhysicalHeight","()I"));j.check();
        const int rotation=j.env->CallIntMethod(display,j.method(display_type,"getRotation","()I"));j.check();
        if(rotation==1||rotation==3)std::swap(width,height);
        if(width<=0||height<=0)return core::fail(core::ErrorCode::failed,"Android display dimensions are invalid");
        const auto result=detail::checked_display_pixels(std::uint64_t(width),std::uint64_t(height));
        if(!result)return core::fail(core::ErrorCode::failed,"Android display dimensions are invalid");
        return *result;
    }catch(...){return core::fail(core::ErrorCode::unsupported,"Android display mode is unavailable",std::current_exception());}
}
core::Result<HostStats> Host::stats()const{if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Android stats require its owner");auto value=impl_->statistics;value.loop=impl_->dispatch->loop.stats();value.host=impl_->dispatch->host_stats();return value;}
}
