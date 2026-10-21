#include <iiGUI/Host.h>
#include "DispatchState.h"
#include "HostCommon.h"
#include "CanvasState.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <pango/pangocairo.h>
#include <cairo/cairo-xlib.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <unistd.h>
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
        dispatch(std::make_shared<detail::DispatchState>(options.task_capacity)),
        gate(std::make_shared<WakeGate>()) {

        state.size = options.size;
        state.title = options.title;
        state.text = "HelloGUI\n\nNative C++23 desktop host\niiSDKCore + iiSDKBuilder";
    }
    HostOptions options;
    std::shared_ptr<detail::DispatchState> dispatch;
    std::shared_ptr<WakeGate> gate;
    const std::thread::id owner = std::this_thread::get_id();
    Phase phase = Phase::created;
    HostCallbacks callbacks;
    WindowSnapshot state;
    HostStats statistics;
    detail::CanvasState canvas;
    std::optional<core::Error> failure;
    bool draining = false, opened_delivered = false;
    Display* display = nullptr;
    Window window = 0;
    int screen = 0;
    Atom protocols = None, delete_window = None, utf8 = None, title_atom = None, resources = None;
    cairo_surface_t* drawing = nullptr;
    PangoLayout* text_layout = nullptr;
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
    void did_close() noexcept {
        if (phase != Phase::running) return;
        phase = Phase::closing;
        dispatch->accepting.store(false, std::memory_order_release);
        dispatch->loop.request_stop();
        canvas.clear();
        state.visible = false;
        state.closed = true;
        phase = Phase::closed;
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
    void drain() noexcept {
        if (phase != Phase::running || draining) return;
        guard([&] {
            struct Reset final { bool& value; ~Reset() { value = false; } } reset{draining};
            draining = true;
            ++statistics.dispatches;
            if (!opened_delivered) {
                opened_delivered = true;
                if (callbacks.opened) callbacks.opened();
            }
            if (phase == Phase::running) dispatch->loop.poll(options.budget);
            if (phase == Phase::running && dispatch->loop.state()==runtime::LoopState::stopped) force_close();
            if (phase == Phase::running) {
                canvas.flush(state.backing_scale,state.size.height);
                if (text_dirty) paint();
            }
        });
    }
    void title_native() {
        XStoreName(display, window, state.title.c_str());
        XChangeProperty(display, window, title_atom, utf8, 8, PropModeReplace,
            reinterpret_cast<const unsigned char*>(state.title.data()), static_cast<int>(state.title.size()));
    }
    int pixels(double logical) const { return static_cast<int>(std::lround(logical*state.backing_scale)); }
    void discard_layout() { if (text_layout) { g_object_unref(text_layout); text_layout=nullptr; } text_dirty=true; }
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
        if (!drawing || !window || phase!=Phase::running) return;
        const double scale=state.backing_scale;
        const int width=std::max(1,pixels(state.size.width)), height=std::max(1,pixels(state.size.height));
        cairo_xlib_surface_set_size(drawing,width,height);
        auto* context=cairo_create(drawing);
        struct Release final { cairo_t* value; ~Release() { cairo_destroy(value); } } release{context};
        cairo_set_source_rgb(context,1,1,1); cairo_paint(context);
        if (!text_layout) {
            text_layout=pango_cairo_create_layout(context);
            if (!text_layout) throw std::runtime_error("Cannot allocate Pango text layout");
            auto* font=pango_font_description_new();
            pango_font_description_set_family(font,"sans-serif");
            pango_font_description_set_absolute_size(font,17*scale*PANGO_SCALE);
            pango_layout_set_font_description(text_layout,font); pango_font_description_free(font);
            pango_layout_set_text(text_layout,state.text.data(),static_cast<int>(state.text.size()));
            pango_layout_set_width(text_layout,std::max(1,width-pixels(56))*PANGO_SCALE);
            pango_layout_set_wrap(text_layout,PANGO_WRAP_WORD_CHAR);
            pango_layout_set_auto_dir(text_layout,TRUE);
        }
        cairo_rectangle(context,28*scale,28*scale,std::max(1,width-pixels(56)),std::max(1,height-pixels(56)));
        cairo_clip(context); cairo_move_to(context,28*scale,28*scale); cairo_set_source_rgb(context,0.13,0.13,0.13);
        pango_cairo_show_layout(context,text_layout);
        if (cairo_status(context)!=CAIRO_STATUS_SUCCESS || cairo_surface_status(drawing)!=CAIRO_STATUS_SUCCESS)
            throw std::runtime_error("Cannot render Pango/Cairo text");
        cairo_surface_flush(drawing); XFlush(display); text_dirty=false;
    }
    void native_event(const XEvent& event) {
        if (canvas.native && canvas.native->native_event(&event)) return;
        if (event.type==PropertyNotify && event.xproperty.window==RootWindow(display,screen) && event.xproperty.atom==resources) {
            const auto previous=state.backing_scale; read_scale();
            if (previous!=state.backing_scale && phase==Phase::running) {
                update_snapshot(); size_hints(); discard_layout(); canvas.dirty=true; gate->signal();
                if (opened_delivered && callbacks.resized) callbacks.resized(state.size);
            }
            return;
        }
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
        resources=XInternAtom(display,"RESOURCE_MANAGER",False); read_scale();
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
        drawing=cairo_xlib_surface_create(display,window,DefaultVisual(display,screen),pixels(options.size.width),pixels(options.size.height));
        if (cairo_surface_status(drawing)!=CAIRO_STATUS_SUCCESS) throw std::runtime_error("Cannot create Cairo X11 text surface");
        gate->attach();
        dispatch->connect([wake=gate]() noexcept { wake->signal(); });
        phase = Phase::running;
        XMapWindow(display, window);
        XFlush(display);
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
            if (!opened_delivered && state.visible) drain();
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
                if (opened_delivered || state.visible) drain();
            }
        }
    }
    void cleanup() {
        dispatch->accepting.store(false, std::memory_order_release);
        dispatch->loop.request_stop();
        if (auto detached=dispatch->disconnect(); !detached)
            remember_failure(detached.error().message,detached.error().cause);
        gate->detach();
        canvas.clear(); discard_layout();
        if (display) {
            if (drawing) { cairo_surface_destroy(drawing); drawing=nullptr; }
            if (window) { XDestroyWindow(display, window); window = 0; }
            XCloseDisplay(display);
            display = nullptr;
        }
        callbacks = {};
        state.visible = false;
        state.closed = true;
        phase = Phase::closed;
    }
};

Host::Host(HostOptions options) : impl_(std::make_unique<Impl>(std::move(options))) {}
Host::~Host() {
    if (!is_ui_thread() || impl_->phase == Impl::Phase::running || impl_->phase == Impl::Phase::closing)
        std::terminate();
    impl_->dispatch->accepting.store(false, std::memory_order_release);
    impl_->dispatch->loop.request_stop();
    impl_->gate->detach();
}
UiExecutor Host::executor() const noexcept { return UiExecutor(impl_->dispatch); }
bool Host::is_ui_thread() const noexcept { return impl_->on_owner(); }
core::Result<void> Host::run(HostCallbacks callbacks) {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Host::run requires its owning UI thread");
    if (impl_->phase != Impl::Phase::created)
        return core::fail(impl_->phase == Impl::Phase::running ? core::ErrorCode::busy : core::ErrorCode::closed,
                          "Host::run is single use and cannot reenter");
    if (active_host.test_and_set()) return core::fail(core::ErrorCode::busy, "iiGUI requires a standalone application host");
    struct Release final { ~Release() { active_host.clear(); } } release;
    impl_->callbacks = std::move(callbacks);
    try { impl_->open_native(); impl_->execute(); }
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
    XFlush(impl_->display);
    return {};
}
core::Result<void> Host::set_text(std::string_view text) {
    if (auto ready = impl_->ready(); !ready) return ready;
    if (!core::valid_utf8(text) || text.size() > INT_MAX)
        return core::fail(core::ErrorCode::invalid_argument, "Display text must be UTF-8 and fit native text limits");
    if (impl_->state.text == text) return {};
    impl_->state.text = text;
    impl_->discard_layout(); impl_->gate->signal();
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
core::Result<void> Host::request_close() {
    if (auto ready = impl_->ready(); !ready) return ready;
    if (impl_->may_close()) impl_->force_close();
    return {};
}
core::Result<void> Host::set_canvas(CanvasFrame frame) {
    if (auto ready = impl_->ready(); !ready) return ready;
    auto result=impl_->canvas.set(std::move(frame),reinterpret_cast<void*>(static_cast<std::uintptr_t>(impl_->window)),
        impl_->display,impl_->state.backing_scale);
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
    return impl_->canvas.info();
}
core::Result<PixelBuffer> Host::capture_canvas() {
    if (auto ready = impl_->ready(); !ready) return std::unexpected(ready.error());
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
core::Result<HostStats> Host::stats() const {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Stats require their owning UI thread");
    auto result = impl_->statistics;
    result.loop = impl_->dispatch->loop.stats();
    result.host = impl_->dispatch->host_stats();
    return result;
}
} // namespace ii::gui
