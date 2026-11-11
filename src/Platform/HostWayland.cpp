#include <iiGUI/HostCommon.h>
#include <iiGUI/HostActivity.h>
#include "HostFailure.h"
#include "HostCallback.h"
#include "WaylandChildren.h"
#include <iiGUI/DispatchState.h>
#include <iiGUI/CanvasState.h>
#include <iiGUI/WidgetState.h>
#include <iiGUI/WidgetSurfaceNative.h>
#include <iiGUI/AtSpiHost.h>
#include <iiGUI/MountTransaction.h>
#include "WaylandInputQueue.h"
#include <iiGUI/WaylandConnection.h>
#include <iiGUI/NativeDesktop_p.hpp>
#include <iiGUI/DesktopCommon.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <mutex>
#include <cerrno>
#include <vector>

namespace ii::gui {
namespace {
thread_local bool wayland_root_active=false;
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
    detail::WaylandChildren<Impl> children;
    Impl* parent=nullptr;
    const std::thread::id owner=std::this_thread::get_id();
    HostCallbacks callbacks;detail::HostActivity activity;WindowSnapshot state;HostStats statistics;Phase phase=Phase::created;
    detail::HostFailure failure;detail::CanvasState canvas;
    std::shared_ptr<detail::WidgetState> widgets;std::shared_ptr<detail::NativeDialog> dialog;
    std::shared_ptr<detail::AtSpiHost> accessibility;std::optional<std::uint64_t> accessible_window;
    std::unique_ptr<runtime::FrameLoop> motion_frames;
    std::shared_ptr<MotionPreferences> preferences=std::make_shared<MotionPreferences>();
    detail::WaylandInputQueue inputs;std::optional<std::uint32_t> current_pointer_serial;bool opened=false,draining=false,mounting=false,close_pending=false,configure_pending=false,resized_pending=false,frame_pending=false,native_close_pending=false;
    bool application_driver=false,owns_wake=true,focused=false,accessibility_layout_pending=false;std::size_t motion_clients=0;
    explicit Impl(HostOptions value):options(detail::validate_options(std::move(value))),dispatch(std::make_shared<detail::DispatchState>(options.task_capacity,options.event_loop)){state.title=options.title;state.size=options.size;}
    bool on_owner()const{return owner==std::this_thread::get_id();}
    core::Result<void> ready()const{if(!on_owner())return core::fail(core::ErrorCode::wrong_executor,"Wayland access requires the host owner");if(phase!=Phase::running||close_pending||!opened)return core::fail(core::ErrorCode::closed,"Wayland surface is not configured");return {};}
    core::Result<void> surface_ready()const{if(auto result=ready();!result)return result;if(!native)return core::fail(core::ErrorCode::unsupported,"The Wayland application driver has no window surface");return {};}
    template<class F>void guard(F&& work)noexcept{try{work();}catch(...){if(!failure)failure=core::Error{core::ErrorCode::failed,"Wayland host callback failed",std::current_exception()};defer_close();}}
    void defer_close()noexcept{if(close_pending)return;close_pending=true;dispatch->retire();if(!parent)dispatch->loop.request_stop();wake->signal();}
    void update_activity()noexcept{
        auto* root=parent?parent:this;
        if(root->phase!=Phase::running||root->close_pending)return;
        root->activity.observe(root->focused||root->children.any([](const Impl& child){return child.focused;}));
        root->wake->signal();
    }
    void observe_activity(bool value)noexcept{focused=value;update_activity();}
    void close()noexcept{
        if(phase!=Phase::running)return;
        phase=Phase::closed;close_pending=true;activity.close();dispatch->retire();
        if(!parent){
            dispatch->loop.request_stop();
            while(auto* child=children.back()){
                child->close();
                if(children.back()==child){children.remove(child);child->parent=nullptr;}
            }
        }
        if(accessibility&&accessible_window)accessibility->close(*accessible_window);accessible_window.reset();
        dialog.reset();if(native)native->cancel_frame();canvas.clear();
        if(widgets){widgets->close();widgets.reset();}
        state.visible=false;state.closed=true;presentations->close();native.reset();accessibility.reset();
        auto* owner=parent;
        if(owner){
            owner->children.remove(this);parent=nullptr;focused=false;owner->update_activity();
            auto endpoint=owner->dispatch;
            try{
                detail::defer_host_closed(std::move(callbacks.closed),[owner,endpoint](auto callback){
                    if(!endpoint->accepting.load())return;
                    auto sender=endpoint->loop.sender();
                    if(!endpoint->track(endpoint->tasks.post(sender,std::move(callback)))){
                        owner->failure=core::Error{core::ErrorCode::failed,"Cannot deliver Wayland child close"};
                        endpoint->loop.request_stop();
                    }
                });
            }catch(...){owner->failure=core::Error{core::ErrorCode::failed,"Cannot defer Wayland child close",std::current_exception()};endpoint->loop.request_stop();}
        }else if(callbacks.closed)try{auto closed=std::move(callbacks.closed);closed();}catch(...){if(!failure)failure=core::Error{core::ErrorCode::failed,"Wayland close callback failed",std::current_exception()};}
        wake->signal();
    }
    bool may_close(){return !callbacks.close_requested||callbacks.close_requested();}
    runtime::FrameLoop* frames(){return options.frame_loop?options.frame_loop:motion_frames.get();}
    void schedule_frame(){if(native&&phase==Phase::running&&!close_pending&&(motion_clients||options.frame_loop))native->request_frame();}
    void sync_accessibility()noexcept{if(accessibility&&accessible_window&&widgets)accessibility->update(*accessible_window,widgets);}
    void accessibility_geometry()noexcept{if(accessibility&&accessible_window)accessibility->geometry(*accessible_window,state.title,state.size);}
    detail::WidgetFrameHooks hooks(){return {
        .frames=[this]()->core::Result<runtime::FrameLoop*>{if(!frames())motion_frames=std::make_unique<runtime::FrameLoop>(dispatch->loop);return frames();},
        .activity=[this](bool active){if(active)++motion_clients;else if(motion_clients)--motion_clients;if(motion_clients||options.frame_loop)schedule_frame();else if(native)native->cancel_frame();},.preferences=preferences,.renderer=options.renderer,
        .events=[this]()->core::Result<runtime::EventLoop*>{return &dispatch->loop;},.presentations=presentations,.accessibility_changed=[this]{sync_accessibility();},
        .accessibility_value_changed=[this]{accessibility_layout_pending=true;wake->signal();}};}
    void drain(){
        if(phase!=Phase::running||draining)return;
        guard([&]{struct Reset{bool& value;~Reset(){value=false;}}reset{draining};draining=true;
            if(close_pending)return;
            if(native_close_pending){native_close_pending=false;if(may_close())defer_close();}
            if(close_pending)return;
            if(configure_pending){configure_pending=false;const bool resized=std::exchange(resized_pending,false);const bool first=!opened;
                if(first){opened=true;detail::deliver_host_opened(callbacks.opened);if(!parent&&!close_pending&&phase==Phase::running)activity.open();}
                if(close_pending)return;
                if(first||resized){if(widgets){const detail::WaylandSurfaceEvent event{detail::WaylandEventKind::resize};(void)widgets->native_event(&event);}
                    canvas.dirty=true;if(resized&&callbacks.resized)callbacks.resized(state.size);}
            }
            if(!opened||close_pending)return;
            if(!parent)activity.deliver();if(close_pending||phase!=Phase::running)return;
            while(!inputs.empty()&&!close_pending){auto input=*inputs.pop();if(widgets){auto owner=widgets;
                if(auto* pointer=std::get_if<detail::WaylandPointerEvent>(&input)){
                    struct SerialScope{std::optional<std::uint32_t>& current;~SerialScope(){current.reset();}} scope{current_pointer_serial};
                    current_pointer_serial=pointer->serial;
                    const detail::WaylandSurfaceEvent event{detail::WaylandEventKind::input,&pointer->input};(void)owner->native_event(&event);
                } else if(auto* widget=std::get_if<WidgetInput>(&input)){const detail::WaylandSurfaceEvent event{detail::WaylandEventKind::input,widget};(void)owner->native_event(&event);}
                else if(auto* ime=std::get_if<detail::WaylandImeEvent>(&input)){const detail::WaylandSurfaceEvent event{detail::WaylandEventKind::ime,nullptr,ime};(void)owner->native_event(&event);}}}
            if(close_pending)return;++statistics.dispatches;if(!parent)dispatch->loop.poll(options.budget);
            if(!parent&&dispatch->loop.state()==runtime::LoopState::stopped){defer_close();return;}
            if(std::exchange(frame_pending,false)&&frames())(void)frames()->step();
            if(close_pending)return;if(widgets)widgets->present();if(std::exchange(accessibility_layout_pending,false))sync_accessibility();canvas.flush(state.backing_scale,state.size.height);schedule_frame();
        });
        if(close_pending)close();
    }
    bool configure(Size size,double scale){const bool changed=size!=state.size||scale!=state.backing_scale;
        if(opened&&!changed&&state.visible)return configure_pending;
        resized_pending=resized_pending||changed;state.size=size;state.backing_scale=scale;state.visible=true;
        if(accessibility&&!accessible_window)accessible_window=accessibility->open(state.title,state.size);else if(changed)accessibility_geometry();
        accessibility_layout_pending=true;configure_pending=true;wake->signal();return true;}
    void push(WidgetInput input,std::optional<std::uint32_t> serial={})noexcept{try{if(!close_pending&&!inputs.push(std::move(input),serial))throw std::runtime_error("Wayland input queue is full");}catch(...){if(!failure)failure=core::Error{core::ErrorCode::full,"Wayland input admission failed",std::current_exception()};defer_close();}wake->signal();}
    void push(detail::WaylandImeEvent input)noexcept{try{if(!close_pending&&!inputs.push(std::move(input)))throw std::runtime_error("Wayland input queue is full");}catch(...){if(!failure)failure=core::Error{core::ErrorCode::full,"Wayland input admission failed",std::current_exception()};defer_close();}wake->signal();}
    void open_native(){
        native=std::make_unique<detail::WaylandConnection>();
        native->open(options,{
            .configure=[this](Size size,double scale){return configure(size,scale);},
            .close=[this]{native_close_pending=true;wake->signal();},
            .frame=[this]{++statistics.display_callbacks;statistics.display_synced=true;frame_pending=true;wake->signal();},
            .input=[this](WidgetInput value){push(std::move(value));},
            .ime=[this](detail::WaylandImeEvent value){push(std::move(value));},
            .activity=[this](bool value){observe_activity(value);},
            .pointer_press=[this](WidgetInput value,std::uint32_t serial){push(std::move(value),serial);}});
    }

};
core::Result<std::shared_ptr<PresentationSource>> Host::presentation_source() const {
    if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Presentation source requires host owner");
    return std::static_pointer_cast<PresentationSource>(impl_->presentations);
}
Host::Host(HostOptions options):impl_(std::make_unique<Impl>(std::move(options))){}
Host::~Host(){if(!is_ui_thread()||impl_->phase==Impl::Phase::running)std::terminate();if(impl_->parent)impl_->parent->children.remove(impl_.get());impl_->presentations->close();impl_->dispatch->retire();if(impl_->owns_wake)impl_->wake->close();}
UiExecutor Host::executor()const noexcept{return UiExecutor(impl_->dispatch);}
bool Host::is_ui_thread()const noexcept{return impl_->on_owner();}
core::Result<void> Host::set_application_driver(bool enabled){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Application driver requires the owner thread");if(impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"Application driver must be configured before run");if(enabled&&impl_->options.frame_loop)return core::fail(core::ErrorCode::invalid_argument,"A hidden application driver cannot own a window frame loop");impl_->application_driver=enabled;return {};}
core::Result<void> Host::set_application_activity_handler(std::function<void(bool)> handler){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Application activity requires the host owner thread");if(impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"Application activity must be configured before run");impl_->activity.set_handler(std::move(handler));return {};}
core::Result<void> Host::set_application_foreground_handler(std::function<void(bool)> handler) {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Application foreground requires the host owner thread");
    if (impl_->phase != Impl::Phase::created) return core::fail(core::ErrorCode::busy, "Application foreground must be configured before run");
    if (handler) return core::fail(core::ErrorCode::unsupported, "This host has no distinct application foreground signal");
    return {};
}
core::Result<void> Host::set_failure_handler(std::function<void(const core::Error&)> handler) {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Host failure handling requires the owner thread");
    if (impl_->phase != Impl::Phase::created) return core::fail(core::ErrorCode::busy, "Host failure handling must be configured before run or show");
    impl_->failure.set_handler(std::move(handler));
    return {};
}
core::Result<void> Host::run(HostCallbacks callbacks){
    if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Wayland host requires its owner");if(impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"Host is single use");
    if(wayland_root_active)return core::fail(core::ErrorCode::busy,"Wayland already has an application host on this thread");
    wayland_root_active=true;struct Release{~Release(){wayland_root_active=false;}}release;
    impl_->callbacks=std::move(callbacks);impl_->phase=Impl::Phase::running;
    try{impl_->wake->open();impl_->dispatch->connect([wake=impl_->wake]()noexcept{wake->signal();});
        impl_->accessibility=detail::AtSpiHost::create(executor(),impl_->options.title);
        if(impl_->application_driver){impl_->opened=true;detail::deliver_host_opened(impl_->callbacks.opened);if(!impl_->close_pending)impl_->activity.open();impl_->wake->signal();}
        else impl_->open_native();
        while(impl_->phase==Impl::Phase::running){
            if(impl_->native)impl_->guard([&]{impl_->native->dispatch();});
            impl_->children.for_each([](Impl& child){if(child.phase==Impl::Phase::running&&child.native)child.guard([&]{child.native->dispatch();});});
            impl_->drain();if(impl_->phase!=Impl::Phase::running)break;
            impl_->children.for_each([&](Impl& child){if(!impl_->close_pending&&child.phase==Impl::Phase::running)child.drain();});
            if(impl_->close_pending)impl_->drain();
            if(impl_->phase!=Impl::Phase::running)break;
            std::vector<detail::WaylandConnection*> connections;
            if(impl_->native)connections.push_back(impl_->native.get());
            impl_->children.for_each([&](Impl& child){if(child.phase==Impl::Phase::running&&child.native)connections.push_back(child.native.get());});
            const auto timeout=impl_->opened?detail::deadline_timeout(impl_->dispatch->loop.next_deadline()):-1;
            detail::WaylandConnection::wait_many(connections,impl_->wake->fd,timeout);impl_->wake->consume();
        }
    }catch(...){if(!impl_->failure)impl_->failure=core::Error{core::ErrorCode::failed,"Cannot run native Wayland host",std::current_exception()};}
    impl_->close();(void)impl_->dispatch->disconnect();impl_->native.reset();impl_->wake->close();if(impl_->failure)return std::unexpected(*impl_->failure);return {};
}
core::Result<void> Host::show_external(HostCallbacks){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"External host requires its owner thread");return core::fail(core::ErrorCode::unsupported,"External event-loop hosting is not implemented on Wayland");}
core::Result<void> Host::attach_external(void*,HostCallbacks){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"External host requires its owner thread");return core::fail(core::ErrorCode::unsupported,"External surface hosting is not implemented on Wayland");}
core::Result<void> Host::show(Host& parent,HostCallbacks callbacks){
    if(!is_ui_thread()||!parent.is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Wayland windows require the UI owner thread");
    if(this==&parent||impl_->application_driver)return core::fail(core::ErrorCode::invalid_argument,"A secondary window cannot be an application driver");
    if(auto ready=parent.impl_->ready();!ready)return ready;
    if(impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"Wayland window is single use");
    if(impl_->activity.has_handler())return core::fail(core::ErrorCode::invalid_argument,"Application activity belongs to the primary host");
    auto* root=parent.impl_->parent?parent.impl_->parent:parent.impl_.get();
    if(auto ready=root->ready();!ready)return ready;
    if(impl_->options.event_loop&&impl_->options.event_loop!=&root->dispatch->loop)return core::fail(core::ErrorCode::invalid_argument,"Wayland windows must share the application EventLoop");
    if(impl_->options.frame_loop)return core::fail(core::ErrorCode::invalid_argument,"A secondary window manages its own display cadence");
    try{
        impl_->options.event_loop=&root->dispatch->loop;
        impl_->dispatch=std::make_shared<detail::DispatchState>(impl_->options.task_capacity,impl_->options.event_loop);
        impl_->wake=root->wake;impl_->owns_wake=false;impl_->parent=root;impl_->callbacks=std::move(callbacks);impl_->accessibility=root->accessibility;
        root->children.add(impl_.get());impl_->phase=Impl::Phase::running;impl_->open_native();impl_->wake->signal();return {};
    }catch(...){
        auto cause=std::current_exception();root->children.remove(impl_.get());impl_->parent=nullptr;
        impl_->dispatch->retire();impl_->native.reset();impl_->accessibility.reset();impl_->phase=Impl::Phase::closed;impl_->state.closed=true;impl_->presentations->close();
        return core::fail(core::ErrorCode::failed,"Cannot open Wayland secondary window",cause);
    }
}
core::Result<void> Host::choose_file(FileDialogOptions options,FileDialogCompletion completion){if(auto r=impl_->ready();!r)return r;if(impl_->dialog)return core::fail(core::ErrorCode::busy,"File dialog already open");auto weak=std::weak_ptr<detail::DispatchState>(impl_->dispatch);auto opened=detail::portal_file_dialog("",std::move(options),executor(),[host=impl_.get(),weak,completion=std::move(completion)](auto result)mutable{auto endpoint=weak.lock();if(!endpoint||!endpoint->accepting.load())return;host->dialog.reset();if(completion)completion(std::move(result));});if(!opened)return std::unexpected(opened.error());impl_->dialog=std::move(*opened);return {};}
core::Result<void> Host::window_action(WindowAction action){if(auto r=impl_->surface_ready();!r)return r;try{impl_->native->action(action);return {};}catch(const std::invalid_argument& e){return core::fail(core::ErrorCode::unsupported,e.what());}}
core::Result<void> Host::begin_window_drag(){
    if(auto r=impl_->surface_ready();!r)return r;
    if(impl_->options.chrome!=WindowChrome::custom)return core::fail(core::ErrorCode::unsupported,"Window drag requires custom chrome");
    const auto serial=impl_->current_pointer_serial;
    if(!serial)return core::fail(core::ErrorCode::invalid_argument,"Window drag requires the initiating Wayland pointer press");
    if(auto widgets=impl_->widgets)if(auto canceled=widgets->input({.kind=WidgetInputKind::pointer_cancel});!canceled)return canceled;
    try{impl_->native->begin_move(*serial);return {};}
    catch(...){return core::fail(core::ErrorCode::failed,"Cannot start the Wayland window drag",std::current_exception());}
}
core::Result<void> Host::set_app_menu(std::vector<AppMenuItem> menu){if(!detail::valid_menu(menu))return core::fail(core::ErrorCode::invalid_argument,"Invalid menu");return core::fail(core::ErrorCode::unsupported,"Wayland has no standard global menu protocol; use an iiGUI menu");}
core::Result<void> Host::set_title(std::string_view title){if(auto r=impl_->surface_ready();!r)return r;if(!detail::valid_title(title)||title.size()>8192)return core::fail(core::ErrorCode::invalid_argument,"Invalid Wayland title");impl_->state.title=title;impl_->native->title(title);impl_->accessibility_geometry();return {};}
core::Result<void> Host::set_text(std::string_view text){if(auto r=impl_->surface_ready();!r)return r;if(!detail::valid_title(text))return core::fail(core::ErrorCode::invalid_argument,"Invalid text");auto mounted=mount_widgets([text=std::string(text)](WidgetBuilder& widgets)->core::Result<void>{auto r=widgets.label({},"host-text",{.text=text});if(!r)return std::unexpected(r.error());return {};});if(!mounted)return std::unexpected(mounted.error());impl_->state.text=text;return {};}
core::Result<void> Host::set_content_size(Size size){return set_window_geometry({.width=size.width,.height=size.height});}
core::Result<void> Host::set_window_geometry(const WindowGeometry& update){if(auto r=impl_->surface_ready();!r)return r;const WindowGeometryValue previous{impl_->state.title,impl_->state.size,impl_->options.minimum_size};auto next=resolve_window_geometry(previous,update);if(!next)return std::unexpected(next.error());impl_->options.minimum_size=next->minimum_size;impl_->native->minimum(next->minimum_size);if(next->title!=previous.title){impl_->state.title=next->title;impl_->native->title(next->title);impl_->accessibility_geometry();}if(next->size!=previous.size)impl_->native->resize(next->size);return {};}
core::Result<void> Host::request_close(){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Wayland close requires the host owner");if(impl_->phase!=Impl::Phase::running)return core::fail(core::ErrorCode::closed,"Wayland host is not running");if(impl_->close_pending)return {};impl_->guard([&]{if(impl_->may_close())impl_->defer_close();});return {};}
core::Result<void> Host::set_canvas(CanvasFrame frame){if(auto r=impl_->surface_ready();!r)return r;if(valid_canvas(frame)&&!detail::full_surface_canvas(frame,impl_->state.size))return core::fail(core::ErrorCode::unsupported,"This host requires a full-window raw canvas; compose subregions with C++ widgets");if(impl_->mounting)return core::fail(core::ErrorCode::busy,"Widget factory active");if(frame.renderer==Renderer::automatic)frame.renderer=impl_->options.renderer;auto result=impl_->canvas.set(std::move(frame),impl_->native.get(),nullptr,impl_->state.backing_scale,impl_->presentations);if(result){if(impl_->widgets){if(impl_->accessibility&&impl_->accessible_window)impl_->accessibility->clear(*impl_->accessible_window);impl_->widgets->close();impl_->widgets.reset();}impl_->wake->signal();}return result;}
core::Result<WidgetMount> Host::mount_widgets(WidgetFactory factory){return mount_widgets(std::move(factory),WidgetAppearance{});}
core::Result<WidgetMount> Host::mount_widgets(WidgetFactory factory,const WidgetAppearance& appearance){
    if(auto r=impl_->surface_ready();!r)return std::unexpected(r.error());if(!factory)return core::fail(core::ErrorCode::invalid_argument,"Empty widget factory");if(impl_->mounting)return core::fail(core::ErrorCode::busy,"Widget factory active");struct Reset{bool& value;~Reset(){value=false;}}reset{impl_->mounting};impl_->mounting=true;
    detail::MountTransaction transaction([this]()noexcept{if(impl_->phase!=Impl::Phase::running)return;impl_->guard([&]{if(impl_->widgets){const detail::WaylandSurfaceEvent event{detail::WaylandEventKind::resize};(void)impl_->widgets->native_event(&event);impl_->widgets->present();}else if(impl_->canvas.frame){impl_->canvas.dirty=true;impl_->canvas.flush(impl_->state.backing_scale,impl_->state.size.height);}});});
    try{auto hooks=impl_->hooks();hooks.window_geometry=[this](const WindowGeometry& value){return set_window_geometry(value);};auto replacement=std::make_shared<detail::WidgetState>([host=impl_.get()](runtime::Task task){host->guard([&]{if(host->phase==Impl::Phase::running&&!host->close_pending)task();});},appearance,std::move(hooks));if(auto r=replacement->construct(std::move(factory));!r)return std::unexpected(r.error());if(auto r=impl_->ready();!r)return std::unexpected(r.error());if(auto r=replacement->attach(impl_->native.get(), detail::make_native_widget_surface);!r)return std::unexpected(r.error());if(auto r=replacement->commit_window_properties();!r)return std::unexpected(r.error());if(auto r=impl_->ready();!r)return std::unexpected(r.error());impl_->canvas.clear();if(impl_->accessibility&&impl_->accessible_window)impl_->accessibility->clear(*impl_->accessible_window);if(impl_->widgets)impl_->widgets->close();impl_->widgets=std::move(replacement);transaction.commit();auto published=impl_->widgets;published->commit_publication();if(auto alive=published->ready();!alive)return std::unexpected(alive.error());
        if(impl_->widgets==published&&impl_->accessibility&&impl_->accessible_window)impl_->accessibility->publish(*impl_->accessible_window,published,[host=impl_.get()](runtime::Task task){host->guard([&]{if(host->phase==Impl::Phase::running&&!host->close_pending)task();});});
        return WidgetMount(published);
    }catch(...){return core::fail(core::ErrorCode::failed,"Wayland widget mount failed",std::current_exception());}}
core::Result<void> Host::unmount_widgets(){if(auto r=impl_->surface_ready();!r)return r;if(impl_->mounting)return core::fail(core::ErrorCode::busy,"Widget factory active");if(impl_->widgets){if(impl_->accessibility&&impl_->accessible_window)impl_->accessibility->clear(*impl_->accessible_window);impl_->widgets->close();impl_->widgets.reset();}return {};}
core::Result<void> Host::unmount_widgets(const WidgetMount& expected){auto owner=expected.state_.lock();if(!owner)return core::fail(core::ErrorCode::closed,"Widget generation retired");if(owner!=impl_->widgets)return core::fail(core::ErrorCode::invalid_argument,"Widget generation belongs to another host");return unmount_widgets();}
core::Result<void> Host::clear_canvas(){if(auto r=impl_->surface_ready();!r)return r;impl_->canvas.clear();return {};}
core::Result<GraphicsInfo> Host::graphics_info()const{if(auto r=impl_->surface_ready();!r)return std::unexpected(r.error());return impl_->widgets?impl_->widgets->graphics_info():core::Result<GraphicsInfo>{impl_->canvas.info()};}
core::Result<PixelBuffer> Host::capture_canvas(){if(auto r=impl_->surface_ready();!r)return std::unexpected(r.error());return impl_->widgets?impl_->widgets->capture():impl_->canvas.capture(impl_->state.backing_scale,impl_->state.size.height);}
core::Result<std::reference_wrapper<runtime::EventLoop>> Host::event_loop(){if(auto r=impl_->ready();!r)return std::unexpected(r.error());return std::ref(impl_->dispatch->loop);}
core::Result<WindowSnapshot> Host::snapshot()const{if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Snapshot requires owner");if(impl_->phase==Impl::Phase::created)return core::fail(core::ErrorCode::closed,"Host not started");return impl_->state;}
core::Result<DisplayPixels> Host::display_pixels()const{if(auto ready=impl_->surface_ready();!ready)return std::unexpected(ready.error());const auto pixels=impl_->native->display_pixels();if(!pixels)return core::fail(core::ErrorCode::busy,"The Wayland surface has not entered a display");return *pixels;}
core::Result<HostStats> Host::stats()const{if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Stats require owner");auto result=impl_->statistics;result.loop=impl_->dispatch->loop.stats();result.host=impl_->dispatch->host_stats();return result;}
}
