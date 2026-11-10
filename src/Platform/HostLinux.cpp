#include <iiGUI/Host.h>
#include "DisplayPixelsPolicy.h"
#include <iiGUI/DispatchState.h>
#include <iiGUI/HostCommon.h>
#include <iiGUI/HostActivity.h>
#include "HostFailure.h"
#include "HostLegacyText.h"
#include "HostCallback.h"
#include "HostDriverPolicy.h"
#include "WindowForeground.h"
#include "X11Foreground.h"
#include <iiGUI/CanvasState.h>
#include <iiGUI/WidgetState.h>
#include <iiGUI/WidgetSurfaceNative.h>
#include <iiGUI/AtSpiHost.h>
#include <iiGUI/WidgetSurface.h>
#include <iiGUI/DesktopCommon.h>
#include <iiGUI/NativeDesktop_p.hpp>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#ifdef IIGUI_XRANDR
#include <X11/extensions/Xrandr.h>
#endif
#include <sys/eventfd.h>
#include <poll.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <thread>

namespace ii::gui {
namespace {
std::atomic_flag active_host = ATOMIC_FLAG_INIT;

std::runtime_error native_error(const char* operation) {
    return std::runtime_error(std::string(operation)+": "+std::strerror(errno));
}
struct WakeGate final {
    std::mutex mutex;
    int event = -1;
    void attach() {
        std::lock_guard lock(mutex);
        event = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (event < 0) throw native_error("Cannot create GUI wake event");
    }
    void signal() {
        std::lock_guard lock(mutex);
        if (event < 0) return;
        const std::uint64_t value = 1;
        while (write(event, &value, sizeof(value)) < 0 && errno == EINTR) {}
        // EAGAIN means the counter is already readable, so no wake is lost.
    }
    void consume() {
        std::uint64_t value = 0;
        while (read(event, &value, sizeof(value)) < 0 && errno == EINTR) {}
    }
    void detach() {
        std::lock_guard lock(mutex);
        if (event >= 0) { close(event); event = -1; }
    }
    ~WakeGate() { detach(); }
};
}

struct Host::Impl {
    enum class Phase { created, running, closing, closed };
    explicit Impl(HostOptions value) : options(detail::validate_options(std::move(value))),
        dispatch(std::make_shared<detail::DispatchState>(options.task_capacity, options.event_loop)),
        gate(std::make_shared<WakeGate>()) {

        state.size = options.size;
        state.title = options.title;
        state.text = "HelloGUI\n\nNative C++23 desktop host\niiSDKCore + iiSDKBuilder";
    }
    std::shared_ptr<detail::WidgetState> widgets;
    Widget legacy_text;
    std::shared_ptr<detail::AtSpiHost> accessibility;std::optional<std::uint64_t> accessible_window;
    bool mounting=false, application_driver=false;
    Impl* parent=nullptr;
    std::vector<Impl*> children;
    std::shared_ptr<detail::NativeDialog> dialog;
    std::shared_ptr<MotionPreferences> motion_preferences=std::make_shared<MotionPreferences>();
    std::unique_ptr<runtime::FrameLoop> motion_frames;
    runtime::TaskToken cadence;
    std::optional<detail::NativeMenu> app_menu;
    HostOptions options;
    std::shared_ptr<detail::DispatchState> dispatch;
    std::shared_ptr<PresentationStream> presentations=std::make_shared<PresentationStream>([this](runtime::Task task){return dispatch->loop.post(std::move(task));});
    std::shared_ptr<WakeGate> gate;
    const std::thread::id owner = std::this_thread::get_id();
    Phase phase = Phase::created;
    HostCallbacks callbacks;
    detail::HostActivity activity;
    detail::WindowForeground foreground;
    detail::X11WindowForeground window_foreground;
    WindowSnapshot state;
    HostStats statistics;
    detail::CanvasState canvas;
    detail::HostFailure failure;
    bool draining = false, opened_delivered = false, accessibility_layout_pending = false;
    Display* display = nullptr;
    Window window = 0;
    int screen = 0;
    Atom protocols = None, delete_window = None, utf8 = None, title_atom = None, resources = None, active_window_atom = None;
    bool text_dirty = true;

    bool on_owner() const noexcept { return owner == std::this_thread::get_id(); }
    core::Result<void> ready() const {
        if (!on_owner()) return core::fail(core::ErrorCode::wrong_executor, "GUI access requires its owning UI thread");
        if (phase != Phase::running) return core::fail(core::ErrorCode::closed, "GUI window is not open");
        return {};
    }
    void remember_failure(std::string message, std::exception_ptr cause = {}) {
        if (!failure) failure = core::Error{core::ErrorCode::failed, std::move(message), cause};
    }
    void update_snapshot() {
        if (!display || !window || phase == Phase::closed) return;
        XWindowAttributes attributes{};
        if (!XGetWindowAttributes(display, window, &attributes))
            throw std::runtime_error("Cannot read native X11 window state");
        state.size = {attributes.width/state.backing_scale, attributes.height/state.backing_scale};
        state.visible = attributes.map_state != IsUnmapped;
    }
    Impl* primary_host() noexcept {
        auto* host = this;
        while (host->parent) host = host->parent;
        return host;
    }
    std::optional<WidgetPoint> accessibility_origin()const noexcept{
        if(!display||!window||state.backing_scale!=1)return std::nullopt;
        int x=0,y=0;Window child=0;
        if(!XTranslateCoordinates(display,window,RootWindow(display,screen),0,0,&x,&y,&child))return std::nullopt;
        return WidgetPoint{double(x),double(y)};
    }
    void accessibility_geometry()noexcept{if(accessibility&&accessible_window)accessibility->geometry(*accessible_window,state.title,state.size,accessibility_origin());}
    void sync_accessibility()noexcept{if(accessibility&&accessible_window&&widgets)accessibility->update(*accessible_window,widgets);}
    void retire_foreground_window() noexcept {
        if (!parent) return;
        auto* primary = primary_host();
        if (primary->phase != Phase::running) return;
        primary->guard([&] {
            window_foreground.retire(primary->foreground, this);
            primary->gate->signal();
        });
    }
    void did_close() noexcept {
        if (phase != Phase::running) return;
        phase = Phase::closing;
        activity.close();
        if (parent) retire_foreground_window(); else foreground.close();
        dispatch->retire();
        if(!parent)dispatch->loop.request_stop();
        if(accessibility&&accessible_window)accessibility->close(*accessible_window);accessible_window.reset();
        if(widgets){widgets->close();widgets.reset();}
        dialog.reset();auto closing=children;for(auto* child:closing){child->force_close();child->cleanup();}
        canvas.clear();accessibility.reset();
        state.visible = false;
        state.closed = true;presentations->close();
        phase = Phase::closed;
        if (parent) {
            auto endpoint = parent->dispatch;
            try {
                detail::defer_host_closed(std::move(callbacks.closed), [endpoint, owner = parent](auto callback) {
                    if (!endpoint->accepting.load()) return;
                    auto sender = endpoint->loop.sender();
                    if (!endpoint->track(endpoint->tasks.post(sender, std::move(callback)))) {
                        owner->remember_failure("Cannot deliver secondary window close");
                        endpoint->loop.request_stop();
                    }
                });
            } catch (...) {
                parent->remember_failure("Cannot defer secondary window close", std::current_exception());
                endpoint->loop.request_stop();
            }
            return;
        }
        guard([&] { if (callbacks.closed) callbacks.closed(); });
    }
    void force_close() noexcept {
        if (phase != Phase::running) return;
        if (window) {
            XUnmapWindow(display, window);
            XFlush(display);
        }
        did_close();
    }
    template<class Work> void guard(Work&& work) noexcept {
        try { work(); }
        catch (...) {
            remember_failure("iiGUI native callback failed", std::current_exception());
            force_close();
        }
    }
    bool may_close() noexcept {
        bool accepted = true;
        try { accepted = !callbacks.close_requested || callbacks.close_requested(); }
        catch (...) { remember_failure("iiGUI close callback failed", std::current_exception()); }
        return accepted;
    }
    runtime::FrameLoop* frames(){return options.frame_loop?options.frame_loop:motion_frames.get();}
    void ensure_cadence(){
        if(parent){parent->ensure_cadence();return;}if(cadence.pending())return;
        auto weak=std::weak_ptr<detail::DispatchState>(dispatch);
        auto scheduled=dispatch->track(dispatch->loop.every(std::chrono::milliseconds(16),[this,weak]{auto endpoint=weak.lock();if(!endpoint||!endpoint->accepting.load())return;(void)endpoint->tasks.run([&]{guard([&]{if(frames())(void)frames()->step();if(widgets)widgets->present();auto active=children;for(auto* child:active)if(child->phase==Phase::running)child->drain();});});}));
        if(!scheduled)throw std::runtime_error("Cannot admit native frame cadence");cadence=scheduled.token;
    }
    detail::WidgetFrameHooks frame_hooks(){return {.frames=[this]()->core::Result<runtime::FrameLoop*>{if(!frames())motion_frames=std::make_unique<runtime::FrameLoop>(dispatch->loop);ensure_cadence();return frames();},.activity=[this](bool active){if(active)ensure_cadence();},.preferences=motion_preferences,.renderer=options.renderer,.events=[this]()->core::Result<runtime::EventLoop*>{return &dispatch->loop;},.presentations=presentations,.accessibility_changed=[this]{sync_accessibility();},.accessibility_value_changed=[this]{accessibility_layout_pending=true;if(gate)gate->signal();}};}
    void drain() noexcept {
        if (phase != Phase::running || draining) return;
        if(parent){guard([&]{if(!opened_delivered){opened_delivered=true;detail::deliver_host_opened(callbacks.opened);}for(int i=0;i<64&&phase==Phase::running&&display&&XPending(display);++i){XEvent event{};XNextEvent(display,&event);native_event(event);}if(widgets)widgets->present();if(std::exchange(accessibility_layout_pending,false))sync_accessibility();});return;}
        guard([&] {
            struct Reset final { bool& value; ~Reset() { value = false; } } reset{draining};
            draining = true;
            ++statistics.dispatches;
            if (!opened_delivered) {
                opened_delivered = true;
                detail::deliver_host_opened(callbacks.opened);
                if (phase == Phase::running) { activity.open(); foreground.open(); }
            }
            if (phase == Phase::running) activity.deliver();
            if (phase == Phase::running) foreground.deliver();
            if (phase == Phase::running) dispatch->loop.poll(options.budget);
            if (phase == Phase::running && dispatch->loop.state()==runtime::LoopState::stopped) force_close();
            if (phase == Phase::running) {
                canvas.flush(state.backing_scale,state.size.height);
                if (text_dirty) paint();if(std::exchange(accessibility_layout_pending,false))sync_accessibility();
            }
        });
    }
    void title_native() {
        XStoreName(display, window, state.title.c_str());
        XChangeProperty(display, window, title_atom, utf8, 8, PropModeReplace,
            reinterpret_cast<const unsigned char*>(state.title.data()), static_cast<int>(state.title.size()));
    }
    int pixels(double logical) const { return static_cast<int>(std::lround(logical*state.backing_scale)); }
    void discard_layout() { text_dirty=true; }
    void read_scale() {
        // Query the property instead of XResourceManagerString's cached value,
        // so desktop DPI changes invalidate existing text and GPU attachments.
        Atom type=None; int format=0; unsigned long count=0,remaining=0; unsigned char* data=nullptr;
        double scale=1;
        if (XGetWindowProperty(display,RootWindow(display,screen),resources,0,262144,False,XA_STRING,
            &type,&format,&count,&remaining,&data)==Success && data && format==8) {
            auto database=XrmGetStringDatabase(reinterpret_cast<const char*>(data));
            if (database) {
                XrmValue value{}; char* value_type=nullptr;
                if (XrmGetResource(database,"Xft.dpi","Xft.Dpi",&value_type,&value) && value.addr) {
                    char* end=nullptr; const double dpi=std::strtod(value.addr,&end);
                    if (end!=value.addr && std::isfinite(dpi) && dpi>=48 && dpi<=384) scale=dpi/96;
                }
                XrmDestroyDatabase(database);
            }
        }
        if (data) XFree(data);
        state.backing_scale=scale;
    }
    void size_hints() {
        XSizeHints limits{}; limits.flags=PMinSize;
        limits.min_width=pixels(options.minimum_size.width); limits.min_height=pixels(options.minimum_size.height);
        XSetWMNormalHints(display,window,&limits);
    }
    void paint() {
        text_dirty=false;
        if (widgets) widgets->present();
    }
    bool owns_window(Window candidate) const {
        if (window && window==candidate && phase==Phase::running) return true;
        return std::ranges::any_of(children,[&](const auto* child){return child && child->owns_window(candidate);});
    }
    void read_activity() {
        if (parent || !display || active_window_atom==None || phase!=Phase::running) return;
        Atom actual=None;int format=0;unsigned long count=0,remaining=0;unsigned char* bytes=nullptr;
        const auto status=XGetWindowProperty(display,RootWindow(display,screen),active_window_atom,0,1,False,
            XA_WINDOW,&actual,&format,&count,&remaining,&bytes);
        struct Release final { unsigned char* bytes; ~Release(){if(bytes)XFree(bytes);} } release{bytes};
        if(status!=Success || actual!=XA_WINDOW || format!=32 || count!=1 || !bytes) return;
        const auto active=*reinterpret_cast<const Window*>(bytes);
        activity.observe(owns_window(active));gate->signal();
    }
    void native_event(XEvent event) {
        if (phase == Phase::running && !application_driver &&
            (event.type == MapNotify || event.type == UnmapNotify)) {
            auto* primary = primary_host();
            if (primary->phase == Phase::running && window_foreground.apply(window, event, primary->foreground, this))
                primary->gate->signal();
        }
        if(event.type==PropertyNotify && event.xproperty.window==RootWindow(display,screen) && event.xproperty.atom==active_window_atom){read_activity();return;}
        if (canvas.native && canvas.native->native_event(&event)) return;
        if (event.type==PropertyNotify && event.xproperty.window==RootWindow(display,screen) && event.xproperty.atom==resources) {
            const auto previous=state.backing_scale; read_scale();
            if (previous!=state.backing_scale && phase==Phase::running) {
                update_snapshot(); size_hints(); discard_layout(); canvas.dirty=true;accessibility_geometry();accessibility_layout_pending=true;gate->signal();
                if (opened_delivered && callbacks.resized) callbacks.resized(state.size);
            }
            return;
        }
        if(detail::desktop_event(&event))return;
        if(app_menu&&app_menu->key&&app_menu->key(&event))return;
        if(widgets&&widgets->native_event(&event))return;
        if (event.xany.window != window || phase != Phase::running) return;
        switch (event.type) {
        case MapNotify:
            update_snapshot();
            break;
        case UnmapNotify:
            state.visible = false;
            break;
        case ConfigureNotify: {
            const Size size{event.xconfigure.width/state.backing_scale, event.xconfigure.height/state.backing_scale};
            const bool changed = size != state.size;
            state.size = size;
            accessibility_geometry();accessibility_layout_pending=true;
            if (opened_delivered && changed && callbacks.resized) callbacks.resized(size);
            if (changed) { discard_layout(); canvas.dirty=true; gate->signal(); }
            break;
        }
        case Expose: if (!event.xexpose.count) { text_dirty=true; gate->signal(); } break;
        case ClientMessage:
            if (event.xclient.message_type == protocols && event.xclient.format == 32 &&
                static_cast<Atom>(event.xclient.data.l[0]) == delete_window && may_close())
                force_close();
            break;
        case DestroyNotify: window = 0; did_close(); break;
        default: break;
        }
    }
    void open_native() {
        XInitThreads(); // EGL drivers may use internal Xlib threads; application Xlib remains UI-owned.
        XrmInitialize();
        display = XOpenDisplay(nullptr);
        if (!display) throw std::runtime_error("Cannot open X11 DISPLAY; Linux GUI requires a logged-in X11/XWayland desktop");
        screen = DefaultScreen(display);
        const auto root = RootWindow(display, screen);
        resources=XInternAtom(display,"RESOURCE_MANAGER",False);active_window_atom=XInternAtom(display,"_NET_ACTIVE_WINDOW",False);read_scale();
        if (pixels(options.size.width)>32767 || pixels(options.size.height)>32767)
            throw std::invalid_argument("Scaled X11 window exceeds native coordinates");
        XSelectInput(display,root,PropertyChangeMask);
        window = XCreateSimpleWindow(display, root, 0, 0,
            static_cast<unsigned int>(pixels(options.size.width)),
            static_cast<unsigned int>(pixels(options.size.height)), 0,
            BlackPixel(display, screen), WhitePixel(display, screen));
        if (!window) throw std::runtime_error("Cannot create native X11 window");
        XSelectInput(display, window, ExposureMask | StructureNotifyMask);
        protocols = XInternAtom(display, "WM_PROTOCOLS", False);
        delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
        utf8 = XInternAtom(display, "UTF8_STRING", False);
        title_atom = XInternAtom(display, "_NET_WM_NAME", False);
        XSetWMProtocols(display, window, &delete_window, 1);
        size_hints();
        XClassHint type{const_cast<char*>("iiGUI"), const_cast<char*>("iiGUI")};
        XSetClassHint(display, window, &type);
        const unsigned long process = static_cast<unsigned long>(getpid());
        XChangeProperty(display, window, XInternAtom(display, "_NET_WM_PID", False), XA_CARDINAL,
            32, PropModeReplace, reinterpret_cast<const unsigned char*>(&process), 1);
        title_native();
        gate->attach();
        if(!parent)dispatch->connect([wake=gate]() noexcept { wake->signal(); });
        phase = Phase::running;
        if(!parent)detail::bind_desktop(display,window);
        if(options.frame_loop)ensure_cadence();
        if (detail::show_native_window(application_driver)) XMapWindow(display, window);
        XFlush(display);
        read_activity();
        if (application_driver) gate->signal();
    }
    void execute() {
        while (phase == Phase::running) {
            // All application Xlib calls stay on this owner. Workers only touch eventfd.
            for (int count = 0; count < 64 && phase == Phase::running && XPending(display); ++count) {
                XEvent event{};
                XNextEvent(display, &event);
                guard([&] { native_event(event); });
            }
            if (phase != Phase::running) break;
            if (!opened_delivered && (state.visible || detail::deliver_opened_before_visible(application_driver))) drain();
            if (phase != Phase::running) break;
            auto timeout = opened_delivered ? detail::deadline_timeout(dispatch->loop.next_deadline()) : -1;
            if (!timeout) { ++statistics.timer_wakes; drain(); }
            if (phase != Phase::running) break;
            timeout = opened_delivered ? detail::deadline_timeout(dispatch->loop.next_deadline()) : -1;
            if (XPending(display)) timeout = 0;
            XFlush(display);
            pollfd events[]{{ConnectionNumber(display), POLLIN, 0}, {gate->event, POLLIN, 0}};
            const auto ready = poll(events, 2, timeout);
            if (ready < 0) {
                if (errno == EINTR) continue;
                throw native_error("Cannot wait for native GUI events");
            }
            if ((events[0].revents | events[1].revents) & (POLLERR | POLLHUP | POLLNVAL))
                throw std::runtime_error("Native GUI connection was closed");
            if (events[1].revents & POLLIN) {
                gate->consume();
                if (opened_delivered || state.visible || detail::deliver_opened_before_visible(application_driver)) drain();
            }
        }
    }
    void cleanup() {
        activity.close();
        if (parent) retire_foreground_window(); else foreground.close();
        dispatch->retire();
        if(!parent)dispatch->loop.request_stop();
        if(accessibility&&accessible_window)accessibility->close(*accessible_window);accessible_window.reset();
        if(widgets){widgets->close();widgets.reset();}dialog.reset();(void)cadence.cancel();accessibility.reset();
        if(parent){std::erase(parent->children,this);parent=nullptr;}
        if (auto detached=dispatch->disconnect(); !detached)
            remember_failure(detached.error().message,detached.error().cause);
        gate->detach();
        canvas.clear(); discard_layout();
        if (display) {
            if (window) { XDestroyWindow(display, window); window = 0; }
            detail::unbind_desktop(display);
            XCloseDisplay(display);
            display = nullptr;
        }
        app_menu.reset();
        callbacks = {};
        state.visible = false;
        state.closed = true;presentations->close();
        phase = Phase::closed;
    }
};

core::Result<std::shared_ptr<PresentationSource>> Host::presentation_source() const {
    if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Presentation source requires host owner");
    return std::static_pointer_cast<PresentationSource>(impl_->presentations);
}
Host::Host(HostOptions options) : impl_(std::make_unique<Impl>(std::move(options))) {}
Host::~Host() {
    if (!is_ui_thread() || impl_->phase == Impl::Phase::running || impl_->phase == Impl::Phase::closing)
        std::terminate();
    if (impl_->parent) impl_->cleanup();
    impl_->presentations->close();impl_->dispatch->retire();
    if (impl_->dispatch->owned_loop) impl_->dispatch->loop.request_stop();
    impl_->gate->detach();
}
UiExecutor Host::executor() const noexcept { return UiExecutor(impl_->dispatch); }
bool Host::is_ui_thread() const noexcept { return impl_->on_owner(); }
core::Result<void> Host::set_application_driver(bool enabled) {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Application driver requires the owner thread");
    if (impl_->phase != Impl::Phase::created) return core::fail(core::ErrorCode::busy, "Application driver must be configured before run");
    if (enabled && impl_->options.frame_loop) return core::fail(core::ErrorCode::invalid_argument, "A hidden application driver cannot own a window frame loop");
    impl_->application_driver = enabled;
    return {};
}
core::Result<void> Host::set_application_activity_handler(std::function<void(bool)> handler) {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor,"Application activity requires the host owner thread");
    if (impl_->phase != Impl::Phase::created) return core::fail(core::ErrorCode::busy,"Application activity must be configured before run");
    impl_->activity.set_handler(std::move(handler)); return {};
}
core::Result<void> Host::set_application_foreground_handler(std::function<void(bool)> handler) {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Application foreground requires the host owner thread");
    if (impl_->phase != Impl::Phase::created) return core::fail(core::ErrorCode::busy, "Application foreground must be configured before run");
    impl_->foreground.set_handler(std::move(handler)); return {};
}
core::Result<void> Host::set_failure_handler(std::function<void(const core::Error&)> handler) {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Host failure handling requires the owner thread");
    if (impl_->phase != Impl::Phase::created) return core::fail(core::ErrorCode::busy, "Host failure handling must be configured before run or show");
    impl_->failure.set_handler(std::move(handler));
    return {};
}
core::Result<void> Host::run(HostCallbacks callbacks) {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Host::run requires its owning UI thread");
    if (impl_->phase != Impl::Phase::created)
        return core::fail(impl_->phase == Impl::Phase::running ? core::ErrorCode::busy : core::ErrorCode::closed,
                          "Host::run is single use and cannot reenter");
    if (active_host.test_and_set()) return core::fail(core::ErrorCode::busy, "iiGUI requires a standalone application host");
    struct Release final { ~Release() { active_host.clear(); } } release;
    impl_->callbacks = std::move(callbacks);
    try { impl_->open_native();impl_->accessibility=detail::AtSpiHost::create(executor(),impl_->options.title);
        if(impl_->accessibility&&!impl_->application_driver)impl_->accessible_window=impl_->accessibility->open(impl_->state.title,impl_->state.size,impl_->accessibility_origin());
        impl_->execute(); }
    catch (const std::exception& error) {
        impl_->remember_failure(std::string("Cannot run Linux/X11 GUI host: ")+error.what(), std::current_exception());
        impl_->force_close();
    } catch (...) {
        impl_->remember_failure("Cannot run Linux/X11 GUI host", std::current_exception());
        impl_->force_close();
    }
    impl_->cleanup();
    if (impl_->failure) return std::unexpected(*impl_->failure);
    return {};
}
core::Result<void> Host::set_title(std::string_view title) {
    if (auto ready = impl_->ready(); !ready) return ready;
    if (!detail::valid_title(title) || title.size() > INT_MAX)
        return core::fail(core::ErrorCode::invalid_argument, "Title must be UTF-8 without NUL and fit native text limits");
    impl_->state.title = title;
    impl_->title_native();
    impl_->accessibility_geometry();
    XFlush(impl_->display);
    return {};
}
core::Result<void> Host::set_text(std::string_view text) {
    if (auto ready = impl_->ready(); !ready) return ready;
    if (!detail::valid_title(text) || text.size()>65536)
        return core::fail(core::ErrorCode::invalid_argument, "Display text must be valid UTF-8 without NUL, up to 64 KiB");
    if (impl_->widgets && !impl_->legacy_text.snapshot())
        return core::fail(core::ErrorCode::busy, "Use the mounted widget's text property");
    if (impl_->widgets) {
        if (impl_->state.text == text) return {};
        auto changed = impl_->legacy_text.set_text(std::string(text));
        if (!changed) return changed;
        impl_->state.text = text;
        return {};
    }
    Widget label;
    auto mounted = mount_widgets([&](WidgetBuilder& builder)->core::Result<void> {
        return detail::build_legacy_text(builder, text, label);
    });
    if (!mounted) return std::unexpected(mounted.error());
    impl_->legacy_text=label;
    impl_->state.text=text;
    return {};
}
core::Result<void> Host::set_content_size(Size size) {
    if (auto ready = impl_->ready(); !ready) return ready;
    if (!detail::valid_content_size(size, impl_->options))
        return core::fail(core::ErrorCode::invalid_argument, "Content size is outside native/minimum bounds");
    if (impl_->pixels(size.width)>32767 || impl_->pixels(size.height)>32767)
        return core::fail(core::ErrorCode::out_of_range,"Scaled X11 window exceeds native coordinates");
    XResizeWindow(impl_->display, impl_->window,
        static_cast<unsigned int>(impl_->pixels(size.width)), static_cast<unsigned int>(impl_->pixels(size.height)));
    XFlush(impl_->display);
    // ConfigureNotify is the authority for callback delivery. Do not consume
    // its new size here: doing so would suppress the resize notification.
    return {};
}
core::Result<void> Host::set_window_geometry(const WindowGeometry& update) {
    if(auto ready=impl_->ready();!ready)return ready;
    const WindowGeometryValue previous{impl_->state.title,impl_->state.size,impl_->options.minimum_size};
    auto next=resolve_window_geometry(previous,update);if(!next)return std::unexpected(next.error());
    for(auto dimension:{next->size.width,next->size.height,next->minimum_size.width,next->minimum_size.height})
        if(impl_->pixels(dimension)>32767)return core::fail(core::ErrorCode::out_of_range,"Scaled X11 window exceeds native coordinates");
    impl_->options.minimum_size=next->minimum_size;
    if(next->title!=previous.title){impl_->state.title=next->title;impl_->title_native();impl_->accessibility_geometry();}
    XSizeHints limits{};limits.flags=PMinSize;limits.min_width=impl_->pixels(next->minimum_size.width);limits.min_height=impl_->pixels(next->minimum_size.height);
    XSetWMNormalHints(impl_->display,impl_->window,&limits);
    if(next->size!=previous.size)XResizeWindow(impl_->display,impl_->window,static_cast<unsigned>(impl_->pixels(next->size.width)),static_cast<unsigned>(impl_->pixels(next->size.height)));
    XFlush(impl_->display);return {};
}
core::Result<void> Host::request_close() {
    if (auto ready = impl_->ready(); !ready) return ready;
    if (impl_->may_close()) impl_->force_close();
    return {};
}
core::Result<void> Host::set_canvas(CanvasFrame frame) {
    if (auto ready = impl_->ready(); !ready) return ready;
    auto result=impl_->canvas.set(std::move(frame),reinterpret_cast<void*>(static_cast<std::uintptr_t>(impl_->window)),
        impl_->display,impl_->state.backing_scale,impl_->presentations);
    if (result) impl_->gate->signal();
    return result;
}
core::Result<void> Host::clear_canvas() {
    if (auto ready = impl_->ready(); !ready) return ready;
    impl_->canvas.clear(); impl_->text_dirty=true; impl_->gate->signal();
    return {};
}
core::Result<GraphicsInfo> Host::graphics_info() const {
    if (auto ready = impl_->ready(); !ready) return std::unexpected(ready.error());
    if(impl_->widgets)return impl_->widgets->graphics_info();
    return impl_->canvas.info();
}
core::Result<PixelBuffer> Host::capture_canvas() {
    if (auto ready = impl_->ready(); !ready) return std::unexpected(ready.error());
    if(impl_->widgets)return impl_->widgets->capture();
    return impl_->canvas.capture(impl_->state.backing_scale,impl_->state.size.height);
}
core::Result<std::reference_wrapper<runtime::EventLoop>> Host::event_loop() {
    if (auto ready=impl_->ready(); !ready) return std::unexpected(ready.error());
    return std::ref(impl_->dispatch->loop);
}
core::Result<WindowSnapshot> Host::snapshot() const {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Snapshot requires its owning UI thread");
    if (impl_->phase == Impl::Phase::created) return core::fail(core::ErrorCode::closed, "Window has not opened");
    // Cached state follows native events, keeping callbacks and snapshots in
    // the same ordering even when X11 has queued a newer ConfigureNotify.
    return impl_->state;
}
core::Result<DisplayPixels> Host::display_pixels() const {
    if (auto ready=impl_->ready(); !ready) return std::unexpected(ready.error());
#ifdef IIGUI_XRANDR
    int major=0,minor=0;
    if (!XRRQueryVersion(impl_->display,&major,&minor) || major<1 || (major==1&&minor<5))
        return core::fail(core::ErrorCode::unsupported,"X11 RandR 1.5 monitor information is unavailable");
    const Window root=RootWindow(impl_->display,impl_->screen);
    XWindowAttributes window{};int x=0,y=0;Window child=0;
    if (!XGetWindowAttributes(impl_->display,impl_->window,&window) ||
        !XTranslateCoordinates(impl_->display,impl_->window,root,0,0,&x,&y,&child))
        return core::fail(core::ErrorCode::busy,"The X11 window position is unavailable");
    int count=0;XRRMonitorInfo* monitors=XRRGetMonitors(impl_->display,root,True,&count);
    if (!monitors || count<=0) { if(monitors)XRRFreeMonitors(monitors);return core::fail(core::ErrorCode::busy,"X11 displays are unavailable"); }
    struct Monitors final { XRRMonitorInfo* value;~Monitors(){XRRFreeMonitors(value);} } cleanup{monitors};
    const XRRMonitorInfo* selected=nullptr;std::int64_t largest=0;
    for(int i=0;i<count;++i){const auto& m=monitors[i];
        const auto w=std::max<std::int64_t>(0,std::min<std::int64_t>(std::int64_t(x)+window.width,std::int64_t(m.x)+m.width)-std::max<std::int64_t>(x,m.x));
        const auto h=std::max<std::int64_t>(0,std::min<std::int64_t>(std::int64_t(y)+window.height,std::int64_t(m.y)+m.height)-std::max<std::int64_t>(y,m.y));
        const auto area=w*h;if(area>largest||(area==largest&&area>0&&m.primary)){largest=area;selected=&m;}}
    if(!selected)return core::fail(core::ErrorCode::busy,"The X11 window is not on a display");
    XRRScreenResources* resources=XRRGetScreenResourcesCurrent(impl_->display,root);
    if(!resources)return core::fail(core::ErrorCode::busy,"The X11 display mode is unavailable");
    struct Resources final { XRRScreenResources* value;~Resources(){XRRFreeScreenResources(value);} } release{resources};
    // A single active output maps to one physical mode; tiled monitors are
    // rejected instead of guessing from a scaled virtual desktop rectangle.
    if(selected->noutput!=1)return core::fail(core::ErrorCode::unsupported,"Tiled X11 display modes are unavailable");
    XRROutputInfo* output=XRRGetOutputInfo(impl_->display,resources,selected->outputs[0]);
    if(!output)return core::fail(core::ErrorCode::busy,"The X11 output is unavailable");
    const RRCrtc crtc=output->crtc;XRRFreeOutputInfo(output);
    if(crtc==None)return core::fail(core::ErrorCode::busy,"The X11 output has no active mode");
    XRRCrtcInfo* mode=XRRGetCrtcInfo(impl_->display,resources,crtc);
    if(!mode)return core::fail(core::ErrorCode::busy,"The X11 display mode is unavailable");
    const RRMode mode_id=mode->mode;const Rotation rotation=mode->rotation;XRRFreeCrtcInfo(mode);
    for(int i=0;i<resources->nmode;++i)if(resources->modes[i].id==mode_id){
        auto width=std::uint64_t(resources->modes[i].width),height=std::uint64_t(resources->modes[i].height);
        if(rotation&(RR_Rotate_90|RR_Rotate_270))std::swap(width,height);
        if(auto pixels=detail::checked_display_pixels(width,height))return *pixels;
        break;
    }
    return core::fail(core::ErrorCode::failed,"The X11 display mode has invalid dimensions");
#else
    return core::fail(core::ErrorCode::unsupported,"X11 display modes require RandR support");
#endif
}
core::Result<HostStats> Host::stats() const {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Stats require their owning UI thread");
    auto result = impl_->statistics;
    result.loop = impl_->dispatch->loop.stats();
    result.host = impl_->dispatch->host_stats();
    return result;
}
} // namespace ii::gui

namespace ii::gui {

core::Result<void> Host::show_external(HostCallbacks){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"External host requires its owner thread");return core::fail(core::ErrorCode::unsupported,"External event-loop hosting is not implemented on X11");}
core::Result<void> Host::attach_external(void*,HostCallbacks){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"External host requires its owner thread");return core::fail(core::ErrorCode::unsupported,"External surface hosting is not implemented on X11");}
core::Result<void> Host::show(Host& parent,HostCallbacks callbacks){
    if(!is_ui_thread()||!parent.is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Native windows require the UI owner thread");
    if(this==&parent||impl_->application_driver)return core::fail(core::ErrorCode::invalid_argument,"A secondary window cannot be an application driver");
    if(impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"Window was already opened");
    if(impl_->activity.has_handler()||impl_->foreground.has_handler())return core::fail(core::ErrorCode::invalid_argument,"Application lifecycle belongs to the primary host");
    if(auto ready=parent.impl_->ready();!ready)return ready;
    if(impl_->options.event_loop&&impl_->options.event_loop!=&parent.impl_->dispatch->loop)return core::fail(core::ErrorCode::invalid_argument,"Secondary windows must share their parent's EventLoop");
    if(impl_->options.frame_loop)return core::fail(core::ErrorCode::invalid_argument,"A secondary window manages its own display cadence");
    try{impl_->parent=parent.impl_.get();impl_->options.event_loop=&parent.impl_->dispatch->loop;impl_->dispatch=std::make_shared<detail::DispatchState>(impl_->options.task_capacity,impl_->options.event_loop);impl_->callbacks=std::move(callbacks);impl_->accessibility=parent.impl_->primary_host()->accessibility;parent.impl_->children.push_back(impl_.get());impl_->open_native();
        if(impl_->accessibility)impl_->accessible_window=impl_->accessibility->open(impl_->state.title,impl_->state.size,impl_->accessibility_origin());parent.impl_->ensure_cadence();return {};}catch(...){impl_->cleanup();return core::fail(core::ErrorCode::failed,"Cannot open a native secondary window",std::current_exception());}
}
core::Result<WidgetMount> Host::mount_widgets(WidgetFactory factory){return mount_widgets(std::move(factory),WidgetAppearance{});}
core::Result<WidgetMount> Host::mount_widgets(WidgetFactory factory,const WidgetAppearance& appearance){
    if(auto ready=impl_->ready();!ready)return std::unexpected(ready.error());if(!factory)return core::fail(core::ErrorCode::invalid_argument,"Native widget factory is empty");if(impl_->mounting)return core::fail(core::ErrorCode::busy,"A widget factory is active");
    struct Reset final{bool& value;~Reset(){value=false;}}reset{impl_->mounting};impl_->mounting=true;
    try{
            auto hooks=impl_->frame_hooks();hooks.window_geometry=[this](const WindowGeometry& value){return set_window_geometry(value);};auto replacement=std::make_shared<detail::WidgetState>([host=impl_.get()](runtime::Task work){host->guard([&]{if(host->phase==Impl::Phase::running)work();});},appearance,std::move(hooks));if(auto built=replacement->construct(std::move(factory));!built)return std::unexpected(built.error());
        if(auto ready=impl_->ready();!ready)return std::unexpected(ready.error());
        detail::X11WidgetParent native{impl_->display,impl_->window,[host=impl_.get()]{return host->state.backing_scale;}};if(auto attached=replacement->attach(&native, detail::make_native_widget_surface);!attached)return std::unexpected(attached.error());
        if(auto committed=replacement->commit_window_properties();!committed)return std::unexpected(committed.error());
            impl_->canvas.clear();if(impl_->accessibility&&impl_->accessible_window)impl_->accessibility->clear(*impl_->accessible_window);if(impl_->widgets)impl_->widgets->close();impl_->widgets=std::move(replacement);auto published=impl_->widgets;published->commit_publication();if(auto alive=published->ready();!alive)return std::unexpected(alive.error());
            if(impl_->widgets==published&&impl_->accessibility&&impl_->accessible_window)impl_->accessibility->publish(*impl_->accessible_window,published,[host=impl_.get()](runtime::Task task){host->guard([&]{if(host->phase==Impl::Phase::running)task();});});return WidgetMount(published);
    }catch(...){return core::fail(core::ErrorCode::failed,"Native widget factory failed",std::current_exception());}
}
core::Result<void> Host::unmount_widgets(){if(auto ready=impl_->ready();!ready)return ready;if(impl_->mounting)return core::fail(core::ErrorCode::busy,"A widget factory is active");if(impl_->widgets){if(impl_->accessibility&&impl_->accessible_window)impl_->accessibility->clear(*impl_->accessible_window);impl_->widgets->close();impl_->widgets.reset();}return {};}
core::Result<void> Host::unmount_widgets(const WidgetMount& expected){auto owner=expected.state_.lock();if(!owner)return core::fail(core::ErrorCode::closed,"Widget generation is closed");if(owner!=impl_->widgets)return core::fail(core::ErrorCode::invalid_argument,"Widget generation belongs to another host");return unmount_widgets();}
core::Result<void> Host::choose_file(FileDialogOptions options,FileDialogCompletion completion){
    if(auto ready=impl_->ready();!ready)return ready;if(impl_->dialog)return core::fail(core::ErrorCode::busy,"A native file dialog is already open");
    auto weak=std::weak_ptr<detail::DispatchState>(impl_->dispatch);auto opened=detail::native_file_dialog(reinterpret_cast<void*>(impl_->window),std::move(options),executor(),[host=impl_.get(),weak,completion=std::move(completion)](auto result)mutable{auto endpoint=weak.lock();if(!endpoint||!endpoint->accepting.load())return;host->dialog.reset();if(completion)completion(std::move(result));});if(!opened)return std::unexpected(opened.error());impl_->dialog=std::move(*opened);return {};
}

}

namespace ii::gui {
core::Result<void> Host::window_action(WindowAction action){if(auto ready=impl_->ready();!ready)return ready;if(unsigned(action)>unsigned(WindowAction::raise))return core::fail(core::ErrorCode::invalid_argument,"Unknown native window action");auto* display=impl_->display;const auto window=impl_->window;const auto state=[&](long mode,const char* first,const char* second=nullptr){XEvent event{};event.xclient.type=ClientMessage;event.xclient.window=window;event.xclient.message_type=XInternAtom(display,"_NET_WM_STATE",False);event.xclient.format=32;event.xclient.data.l[0]=mode;event.xclient.data.l[1]=XInternAtom(display,first,False);event.xclient.data.l[2]=second?XInternAtom(display,second,False):0;event.xclient.data.l[3]=1;XSendEvent(display,RootWindow(display,impl_->screen),False,SubstructureRedirectMask|SubstructureNotifyMask,&event);};switch(action){case WindowAction::restore:XMapRaised(display,window);state(0,"_NET_WM_STATE_MAXIMIZED_VERT","_NET_WM_STATE_MAXIMIZED_HORZ");state(0,"_NET_WM_STATE_FULLSCREEN");break;case WindowAction::minimize:XIconifyWindow(display,window,impl_->screen);break;case WindowAction::maximize:state(1,"_NET_WM_STATE_MAXIMIZED_VERT","_NET_WM_STATE_MAXIMIZED_HORZ");break;case WindowAction::toggle_fullscreen:state(2,"_NET_WM_STATE_FULLSCREEN");break;case WindowAction::raise:XMapRaised(display,window);XRaiseWindow(display,window);break;}XFlush(display);return {};}
core::Result<void> Host::set_app_menu(std::vector<AppMenuItem> items){if(auto ready=impl_->ready();!ready)return ready;auto created=detail::native_app_menu(impl_->display,impl_->window,std::move(items),executor());if(!created)return std::unexpected(created.error());impl_->app_menu=std::move(*created);return {};}
}
