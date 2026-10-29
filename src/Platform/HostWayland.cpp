#include <iiGUI/HostCommon.h>
#include <iiGUI/DispatchState.h>
#include <iiGUI/CanvasState.h>
#include <iiGUI/WidgetState.h>
#include <iiGUI/MountTransaction.h>
#include <iiGUI/HostInputQueue.h>
#include <iiGUI/WaylandConnection.h>
#include <iiGUI/NativeDesktop_p.hpp>
#include <iiGUI/DesktopCommon.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <mutex>
#include <cerrno>

namespace ii::gui {
namespace {
struct WaylandWake final {
    std::mutex mutex;int fd=-1;
    void open(){std::lock_guard lock(mutex);fd=eventfd(0,EFD_NONBLOCK|EFD_CLOEXEC);if(fd<0)throw std::runtime_error("Cannot create Wayland wake event");}
    void signal(){std::lock_guard lock(mutex);if(fd<0)return;const std::uint64_t value=1;while(::write(fd,&value,sizeof(value))<0&&errno==EINTR){}}
    void consume(){std::uint64_t value;while(::read(fd,&value,sizeof(value))<0&&errno==EINTR){}}
    void close(){std::lock_guard lock(mutex);if(fd>=0){::close(fd);fd=-1;}}
    ~WaylandWake(){close();}
};
}
struct Host::Impl final {
    enum class Phase {created,running,closed};
    HostOptions options;std::shared_ptr<detail::DispatchState> dispatch;
    std::shared_ptr<PresentationStream> presentations=std::make_shared<PresentationStream>([this](runtime::Task task){return dispatch->loop.post(std::move(task));});
    std::shared_ptr<WaylandWake> wake=std::make_shared<WaylandWake>();
    std::unique_ptr<detail::WaylandConnection> native;
    const std::thread::id owner=std::this_thread::get_id();
    HostCallbacks callbacks;WindowSnapshot state;HostStats statistics;Phase phase=Phase::created;
    std::optional<core::Error> failure;detail::CanvasState canvas;
    std::shared_ptr<detail::WidgetState> widgets;std::shared_ptr<detail::NativeDialog> dialog;
    std::unique_ptr<runtime::FrameLoop> motion_frames;
    std::shared_ptr<MotionPreferences> preferences=std::make_shared<MotionPreferences>();
    detail::HostInputQueue inputs;bool opened=false,draining=false,mounting=false,close_pending=false,configure_pending=false,resized_pending=false,frame_pending=false,native_close_pending=false;std::size_t motion_clients=0;
    explicit Impl(HostOptions value):options(detail::validate_options(std::move(value))),dispatch(std::make_shared<detail::DispatchState>(options.task_capacity,options.event_loop)){state.title=options.title;state.size=options.size;}
    bool on_owner()const{return owner==std::this_thread::get_id();}
    core::Result<void> ready()const{if(!on_owner())return core::fail(core::ErrorCode::wrong_executor,"Wayland access requires the host owner");if(phase!=Phase::running||close_pending||!opened)return core::fail(core::ErrorCode::closed,"Wayland surface is not configured");return {};}
    template<class F>void guard(F&& work)noexcept{try{work();}catch(...){if(!failure)failure=core::Error{core::ErrorCode::failed,"Wayland host callback failed",std::current_exception()};defer_close();}}
    void defer_close()noexcept{close_pending=true;dispatch->retire();dispatch->loop.request_stop();wake->signal();}
    void close()noexcept{if(phase!=Phase::running)return;phase=Phase::closed;dispatch->retire();dialog.reset();if(native)native->cancel_frame();canvas.clear();if(widgets){widgets->close();widgets.reset();}state.visible=false;state.closed=true;presentations->close();dispatch->loop.request_stop();if(callbacks.closed)try{callbacks.closed();}catch(...){if(!failure)failure=core::Error{core::ErrorCode::failed,"Wayland close callback failed",std::current_exception()};}wake->signal();}
    bool may_close(){return !callbacks.close_requested||callbacks.close_requested();}
    runtime::FrameLoop* frames(){return options.frame_loop?options.frame_loop:motion_frames.get();}
    void schedule_frame(){if(native&&phase==Phase::running&&!close_pending&&(motion_clients||options.frame_loop))native->request_frame();}
    detail::WidgetFrameHooks hooks(){return {
        .frames=[this]()->core::Result<runtime::FrameLoop*>{if(!frames())motion_frames=std::make_unique<runtime::FrameLoop>(dispatch->loop);return frames();},
        .activity=[this](bool active){if(active)++motion_clients;else if(motion_clients)--motion_clients;if(motion_clients||options.frame_loop)schedule_frame();else if(native)native->cancel_frame();},.preferences=preferences,.renderer=options.renderer,
        .events=[this]()->core::Result<runtime::EventLoop*>{return &dispatch->loop;},.presentations=presentations};}
    void drain(){
        if(phase!=Phase::running||draining)return;
        guard([&]{struct Reset{bool& value;~Reset(){value=false;}}reset{draining};draining=true;
            if(native_close_pending){native_close_pending=false;if(may_close())defer_close();}
            if(close_pending)return;
            if(configure_pending){configure_pending=false;const bool resized=std::exchange(resized_pending,false);
                if(!opened){opened=true;if(callbacks.opened)callbacks.opened();}
                if(close_pending)return;
                if(widgets){const detail::WaylandSurfaceEvent event{detail::WaylandEventKind::resize};(void)widgets->native_event(&event);}
                canvas.dirty=true;if(resized&&callbacks.resized)callbacks.resized(state.size);
            }
            if(!opened||close_pending)return;
            while(!inputs.empty()&&!close_pending){auto input=*inputs.pop();if(widgets){auto owner=widgets;const detail::WaylandSurfaceEvent event{detail::WaylandEventKind::input,&input};(void)owner->native_event(&event);}}
            if(close_pending)return;++statistics.dispatches;dispatch->loop.poll(options.budget);
            if(dispatch->loop.state()==runtime::LoopState::stopped){defer_close();return;}
            if(std::exchange(frame_pending,false)&&frames())(void)frames()->step();
            if(close_pending)return;if(widgets)widgets->present();canvas.flush(state.backing_scale,state.size.height);schedule_frame();
        });
        if(close_pending)close();
    }
    void configure(Size size,double scale){resized_pending=resized_pending||size!=state.size||scale!=state.backing_scale;state.size=size;state.backing_scale=scale;state.visible=true;configure_pending=true;wake->signal();}
    void push(WidgetInput input)noexcept{try{if(!close_pending&&!inputs.push(std::move(input)))throw std::runtime_error("Wayland input queue is full");}catch(...){if(!failure)failure=core::Error{core::ErrorCode::full,"Wayland input admission failed",std::current_exception()};defer_close();}wake->signal();}

};
core::Result<std::shared_ptr<PresentationSource>> Host::presentation_source() const {
    if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Presentation source requires host owner");
    return std::static_pointer_cast<PresentationSource>(impl_->presentations);
}
Host::Host(HostOptions options):impl_(std::make_unique<Impl>(std::move(options))){}
Host::~Host(){if(!is_ui_thread()||impl_->phase==Impl::Phase::running)std::terminate();impl_->presentations->close();impl_->dispatch->retire();impl_->wake->close();}
UiExecutor Host::executor()const noexcept{return UiExecutor(impl_->dispatch);}
bool Host::is_ui_thread()const noexcept{return impl_->on_owner();}
core::Result<void> Host::run(HostCallbacks callbacks){
    if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Wayland host requires its owner");if(impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"Host is single use");
    impl_->callbacks=std::move(callbacks);impl_->phase=Impl::Phase::running;
    try{impl_->wake->open();impl_->dispatch->connect([wake=impl_->wake]()noexcept{wake->signal();});impl_->native=std::make_unique<detail::WaylandConnection>();
        impl_->native->open(impl_->options,{
            .configure=[this](Size size,double scale){impl_->configure(size,scale);},
            .close=[this]{impl_->native_close_pending=true;impl_->wake->signal();},
            .frame=[this]{++impl_->statistics.display_callbacks;impl_->statistics.display_synced=true;impl_->frame_pending=true;impl_->wake->signal();},
            .input=[this](WidgetInput value){impl_->push(std::move(value));}});
        while(impl_->phase==Impl::Phase::running){impl_->native->dispatch();impl_->drain();if(impl_->phase!=Impl::Phase::running)break;const auto timeout=impl_->opened?detail::deadline_timeout(impl_->dispatch->loop.next_deadline()):-1;impl_->native->wait(impl_->wake->fd,timeout);impl_->wake->consume();}
    }catch(...){if(!impl_->failure)impl_->failure=core::Error{core::ErrorCode::failed,"Cannot run native Wayland host",std::current_exception()};}
    impl_->close();(void)impl_->dispatch->disconnect();impl_->native.reset();impl_->wake->close();if(impl_->failure)return std::unexpected(*impl_->failure);return {};
}
core::Result<void> Host::show(Host&,HostCallbacks){return core::fail(core::ErrorCode::unsupported,"Wayland secondary windows require a separate surface owner");}
core::Result<void> Host::choose_file(FileDialogOptions options,FileDialogCompletion completion){if(auto r=impl_->ready();!r)return r;if(impl_->dialog)return core::fail(core::ErrorCode::busy,"File dialog already open");auto opened=detail::portal_file_dialog("",std::move(options),executor(),[host=impl_.get(),completion=std::move(completion)](auto result)mutable{host->dialog.reset();if(completion)completion(std::move(result));});if(!opened)return std::unexpected(opened.error());impl_->dialog=std::move(*opened);return {};}
core::Result<void> Host::window_action(WindowAction action){if(auto r=impl_->ready();!r)return r;try{impl_->native->action(action);return {};}catch(const std::invalid_argument& e){return core::fail(core::ErrorCode::unsupported,e.what());}}
core::Result<void> Host::set_app_menu(std::vector<AppMenuItem> menu){if(!detail::valid_menu(menu))return core::fail(core::ErrorCode::invalid_argument,"Invalid menu");return core::fail(core::ErrorCode::unsupported,"Wayland has no standard global menu protocol; use an iiGUI menu");}
core::Result<void> Host::set_title(std::string_view title){if(auto r=impl_->ready();!r)return r;if(!detail::valid_title(title)||title.size()>8192)return core::fail(core::ErrorCode::invalid_argument,"Invalid Wayland title");impl_->state.title=title;impl_->native->title(title);return {};}
core::Result<void> Host::set_text(std::string_view text){if(auto r=impl_->ready();!r)return r;if(!detail::valid_title(text))return core::fail(core::ErrorCode::invalid_argument,"Invalid text");auto mounted=mount_widgets([text=std::string(text)](WidgetBuilder& widgets)->core::Result<void>{auto r=widgets.label({},"host-text",{.text=text});if(!r)return std::unexpected(r.error());return {};});if(!mounted)return std::unexpected(mounted.error());impl_->state.text=text;return {};}
core::Result<void> Host::set_content_size(Size size){return set_window_geometry({.width=size.width,.height=size.height});}
core::Result<void> Host::set_window_geometry(const WindowGeometry& update){if(auto r=impl_->ready();!r)return r;const WindowGeometryValue previous{impl_->state.title,impl_->state.size,impl_->options.minimum_size};auto next=resolve_window_geometry(previous,update);if(!next)return std::unexpected(next.error());impl_->options.minimum_size=next->minimum_size;impl_->native->minimum(next->minimum_size);if(next->title!=previous.title){impl_->state.title=next->title;impl_->native->title(next->title);}if(next->size!=previous.size)impl_->native->resize(next->size);return {};}
core::Result<void> Host::request_close(){if(auto r=impl_->ready();!r)return r;impl_->guard([&]{if(impl_->may_close())impl_->defer_close();});return {};}
core::Result<void> Host::set_canvas(CanvasFrame frame){if(auto r=impl_->ready();!r)return r;if(valid_canvas(frame)&&!detail::full_surface_canvas(frame,impl_->state.size))return core::fail(core::ErrorCode::unsupported,"This host requires a full-window raw canvas; compose subregions with C++ widgets");if(impl_->mounting)return core::fail(core::ErrorCode::busy,"Widget factory active");if(frame.renderer==Renderer::automatic)frame.renderer=impl_->options.renderer;auto result=impl_->canvas.set(std::move(frame),impl_->native.get(),nullptr,impl_->state.backing_scale,impl_->presentations);if(result){if(impl_->widgets){impl_->widgets->close();impl_->widgets.reset();}impl_->wake->signal();}return result;}
core::Result<WidgetMount> Host::mount_widgets(WidgetFactory factory){return mount_widgets(std::move(factory),WidgetAppearance{});}
core::Result<WidgetMount> Host::mount_widgets(WidgetFactory factory,const WidgetAppearance& appearance){
    if(auto r=impl_->ready();!r)return std::unexpected(r.error());if(!factory)return core::fail(core::ErrorCode::invalid_argument,"Empty widget factory");if(impl_->mounting)return core::fail(core::ErrorCode::busy,"Widget factory active");struct Reset{bool& value;~Reset(){value=false;}}reset{impl_->mounting};impl_->mounting=true;
    detail::MountTransaction transaction([this]()noexcept{if(impl_->phase!=Impl::Phase::running)return;impl_->guard([&]{if(impl_->widgets){const detail::WaylandSurfaceEvent event{detail::WaylandEventKind::resize};(void)impl_->widgets->native_event(&event);impl_->widgets->present();}else if(impl_->canvas.frame){impl_->canvas.dirty=true;impl_->canvas.flush(impl_->state.backing_scale,impl_->state.size.height);}});});
    try{auto hooks=impl_->hooks();hooks.window_geometry=[this](const WindowGeometry& value){return set_window_geometry(value);};auto replacement=std::make_shared<detail::WidgetState>([host=impl_.get()](runtime::Task task){host->guard([&]{if(host->phase==Impl::Phase::running&&!host->close_pending)task();});},appearance,std::move(hooks));if(auto r=replacement->construct(std::move(factory));!r)return std::unexpected(r.error());if(auto r=impl_->ready();!r)return std::unexpected(r.error());if(auto r=replacement->attach(impl_->native.get());!r)return std::unexpected(r.error());if(auto r=replacement->commit_window_properties();!r)return std::unexpected(r.error());if(auto r=impl_->ready();!r)return std::unexpected(r.error());impl_->canvas.clear();if(impl_->widgets)impl_->widgets->close();impl_->widgets=std::move(replacement);transaction.commit();auto published=impl_->widgets;published->commit_publication();if(auto alive=published->ready();!alive)return std::unexpected(alive.error());return WidgetMount(published);
    }catch(...){return core::fail(core::ErrorCode::failed,"Wayland widget mount failed",std::current_exception());}}
core::Result<void> Host::unmount_widgets(){if(auto r=impl_->ready();!r)return r;if(impl_->mounting)return core::fail(core::ErrorCode::busy,"Widget factory active");if(impl_->widgets){impl_->widgets->close();impl_->widgets.reset();}return {};}
core::Result<void> Host::unmount_widgets(const WidgetMount& expected){auto owner=expected.state_.lock();if(!owner)return core::fail(core::ErrorCode::closed,"Widget generation retired");if(owner!=impl_->widgets)return core::fail(core::ErrorCode::invalid_argument,"Widget generation belongs to another host");return unmount_widgets();}
core::Result<void> Host::clear_canvas(){if(auto r=impl_->ready();!r)return r;impl_->canvas.clear();return {};}
core::Result<GraphicsInfo> Host::graphics_info()const{if(auto r=impl_->ready();!r)return std::unexpected(r.error());return impl_->widgets?impl_->widgets->graphics_info():core::Result<GraphicsInfo>{impl_->canvas.info()};}
core::Result<PixelBuffer> Host::capture_canvas(){if(auto r=impl_->ready();!r)return std::unexpected(r.error());return impl_->widgets?impl_->widgets->capture():impl_->canvas.capture(impl_->state.backing_scale,impl_->state.size.height);}
core::Result<std::reference_wrapper<runtime::EventLoop>> Host::event_loop(){if(auto r=impl_->ready();!r)return std::unexpected(r.error());return std::ref(impl_->dispatch->loop);}
core::Result<WindowSnapshot> Host::snapshot()const{if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Snapshot requires owner");if(impl_->phase==Impl::Phase::created)return core::fail(core::ErrorCode::closed,"Host not started");return impl_->state;}
core::Result<HostStats> Host::stats()const{if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Stats require owner");auto result=impl_->statistics;result.loop=impl_->dispatch->loop.stats();result.host=impl_->dispatch->host_stats();return result;}
}
