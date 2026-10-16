#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <iiGUI/Host.h>
#include "DispatchState.h"
#include "HostCommon.h"
#include "CanvasState.h"
#include <algorithm>
#include <atomic>
#include <mutex>
#include <optional>
#include <thread>

namespace ii::gui {
namespace {
constexpr UINT wake_message = WM_APP + 0x149;
constexpr UINT_PTR deadline_timer = 0x49494755;
std::atomic_flag active_host = ATOMIC_FLAG_INIT;
using Microsoft::WRL::ComPtr;
void checked(HRESULT value, const char* operation) {
    if (FAILED(value)) throw std::runtime_error(std::string(operation)+" (HRESULT "+std::to_string(value)+")");
}

std::runtime_error native_error(const char* operation) {
    return std::runtime_error(std::string(operation)+" (Win32 error "+std::to_string(GetLastError())+")");
}
std::wstring native_text(std::string_view text) {
    if (text.empty()) return {};
    if (text.size() > INT_MAX) throw std::invalid_argument("Native display text is too large");
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                       static_cast<int>(text.size()), nullptr, 0);
    if (!size) throw native_error("Cannot convert UTF-8 text");
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                            static_cast<int>(text.size()), result.data(), size))
        throw native_error("Cannot convert UTF-8 text");
    return result;
}

// Workers signal an event, plus a coalesced window message for Win32's nested
// move/size loops. Detach and signal share the same lock: no recycled HWND or
// closed event handle can be used by a producer racing native teardown.
struct WakeGate final {
    std::mutex mutex;
    HANDLE event = nullptr;
    HWND window = nullptr;
    bool queued = false;
    void attach(HWND value) {
        std::lock_guard lock(mutex);
        event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!event) throw native_error("Cannot create GUI wake event");
        window = value;
    }
    void signal() {
        std::lock_guard lock(mutex);
        if (!event) return;
        SetEvent(event);
        if (window && !queued) queued = PostMessageW(window, wake_message, 0, 0) != FALSE;
    }
    void consumed() {
        std::lock_guard lock(mutex);
        queued = false;
    }
    void retire_window() {
        std::lock_guard lock(mutex);
        window = nullptr;
        queued = false;
    }
    void detach() {
        std::lock_guard lock(mutex);
        window = nullptr;
        queued = false;
        if (event) { CloseHandle(event); event = nullptr; }
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
        text = native_text(state.text);
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
    HWND window = nullptr;
    ComPtr<ID2D1Factory> drawing_factory;
    ComPtr<IDWriteFactory> text_factory;
    ComPtr<IDWriteTextFormat> text_format;
    ComPtr<IDWriteTextLayout> text_layout;
    ComPtr<ID2D1HwndRenderTarget> drawing;
    ComPtr<ID2D1SolidColorBrush> text_brush;
    HINSTANCE instance = GetModuleHandleW(nullptr);
    std::wstring class_name, text;
    bool registered = false;
    UINT dpi = 96;
    DPI_AWARENESS_CONTEXT previous_dpi = nullptr;

    bool on_owner() const noexcept { return owner == std::this_thread::get_id(); }
    core::Result<void> ready() const {
        if (!on_owner()) return core::fail(core::ErrorCode::wrong_executor, "GUI access requires its owning UI thread");
        if (phase != Phase::running) return core::fail(core::ErrorCode::closed, "GUI window is not open");
        return {};
    }
    void remember_failure(std::string message, std::exception_ptr cause = {}) {
        if (!failure) failure = core::Error{core::ErrorCode::failed, std::move(message), cause};
    }
    int pixels(double value) const { return static_cast<int>(std::lround(value*dpi/96.0)); }
    RECT frame(Size size) const {
        RECT rectangle{0, 0, pixels(size.width), pixels(size.height)};
        if (!AdjustWindowRectExForDpi(&rectangle, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi))
            throw native_error("Cannot calculate window frame");
        return rectangle;
    }
    void update_snapshot() {
        if (!window || phase == Phase::closed) return;
        RECT content{};
        if (!GetClientRect(window, &content)) throw native_error("Cannot read window size");
        const UINT current = GetDpiForWindow(window);
        if (current) dpi = current;
        state.backing_scale = dpi/96.0;
        state.size = {(content.right-content.left)/state.backing_scale,
                      (content.bottom-content.top)/state.backing_scale};
        state.visible = IsWindowVisible(window) != FALSE;
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
        if (phase == Phase::running) {
            canvas.clear();
            if (window) DestroyWindow(window);
            did_close();
        }
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
    void arm_timer() {
        if (window) KillTimer(window, deadline_timer);
        if (phase != Phase::running) return;
        const auto delay = detail::deadline_timeout(dispatch->loop.next_deadline());
        if (delay < 0) return;
        if (!delay) { gate->signal(); return; }
        if (!SetTimer(window, deadline_timer, static_cast<UINT>(delay), nullptr))
            throw native_error("Cannot arm GUI deadline timer");
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
            if (phase == Phase::running) canvas.flush(state.backing_scale, state.size.height);
            arm_timer();
        });
    }
    void make_font() {
        if (!drawing_factory) checked(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,drawing_factory.GetAddressOf()), "Cannot create Direct2D factory");
        if (!text_factory) checked(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(text_factory.GetAddressOf())), "Cannot create DirectWrite factory");
        if (!text_format) checked(text_factory->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,17,L"",&text_format),"Cannot create native text format");
        text_layout.Reset();
    }
    void paint() {
        PAINTSTRUCT paint_state{};
        if (!BeginPaint(window, &paint_state)) return;
        struct End final { HWND window; PAINTSTRUCT& state; ~End() { EndPaint(window,&state); } } end{window,paint_state};
        make_font_if_needed();
        RECT content{};
        GetClientRect(window, &content);
        const auto size=D2D1::SizeU(std::max<LONG>(1,content.right),std::max<LONG>(1,content.bottom));
        if (!drawing) {
            checked(drawing_factory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
                D2D1::HwndRenderTargetProperties(window,size),&drawing),"Cannot create Direct2D text surface");
            checked(drawing->CreateSolidColorBrush(D2D1::ColorF(0.13f,0.13f,0.13f),&text_brush),"Cannot create text brush");
        }
        if (drawing->GetPixelSize().width!=size.width || drawing->GetPixelSize().height!=size.height)
            checked(drawing->Resize(size),"Cannot resize Direct2D text surface");
        drawing->SetDpi(float(dpi),float(dpi));
        if (!text_layout) checked(text_factory->CreateTextLayout(text.data(),static_cast<UINT32>(text.size()),
            text_format.Get(),float(std::max(1.0,state.size.width-56)),float(std::max(1.0,state.size.height-56)),
            &text_layout),"Cannot shape DirectWrite text layout");
        drawing->BeginDraw();
        drawing->Clear(D2D1::ColorF(D2D1::ColorF::White));
        drawing->DrawTextLayout(D2D1::Point2F(28,28),text_layout.Get(),text_brush.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
        const auto result=drawing->EndDraw();
        if (result==D2DERR_RECREATE_TARGET) {
            text_brush.Reset(); drawing.Reset(); InvalidateRect(window,nullptr,FALSE);
        } else checked(result,"Cannot paint DirectWrite text");
    }
    void make_font_if_needed() { if (!text_format) make_font(); }
    static LRESULT CALLBACK procedure(HWND handle, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(handle, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
            SetWindowLongPtrW(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->window = handle;
        }
        if (!self) return DefWindowProcW(handle, message, wparam, lparam);
        bool handled = true;
        LRESULT result = 0;
        self->guard([&] {
            switch (message) {
            case wake_message: self->gate->consumed(); self->drain(); break;
            case WM_TIMER:
                if (wparam != deadline_timer) { handled = false; break; }
                KillTimer(handle, deadline_timer);
                ++self->statistics.timer_wakes;
                self->drain(); break;
            case WM_PAINT: self->paint(); break;
            case WM_SIZE:
                if (self->phase == Phase::running) {
                    self->update_snapshot();
                    self->text_layout.Reset();
                    self->canvas.dirty = true;
                    self->gate->signal();
                    InvalidateRect(handle,nullptr,FALSE);
                    if (self->opened_delivered && self->callbacks.resized)
                        self->callbacks.resized(self->state.size);
                }
                break;
            case WM_GETMINMAXINFO: {
                const auto rectangle = self->frame(self->options.minimum_size);
                auto& limits = *reinterpret_cast<MINMAXINFO*>(lparam);
                limits.ptMinTrackSize = {rectangle.right-rectangle.left, rectangle.bottom-rectangle.top};
                break;
            }
            case WM_DPICHANGED: {
                self->dpi = HIWORD(wparam);
                self->make_font();
                const auto& rectangle = *reinterpret_cast<RECT*>(lparam);
                if (!SetWindowPos(handle, nullptr, rectangle.left, rectangle.top,
                    rectangle.right-rectangle.left, rectangle.bottom-rectangle.top,
                    SWP_NOZORDER | SWP_NOACTIVATE)) throw native_error("Cannot update DPI frame");
                InvalidateRect(handle, nullptr, TRUE);
                break;
            }
            case WM_CLOSE: if (self->may_close()) self->force_close(); break;
            case WM_DESTROY: self->gate->retire_window(); self->did_close(); break;
            case WM_NCDESTROY:
                SetWindowLongPtrW(handle, GWLP_USERDATA, 0);
                self->window = nullptr;
                handled = false; break;
            default: handled = false; break;
            }
        });
        return handled ? result : DefWindowProcW(handle, message, wparam, lparam);
    }
    void open_native() {
        previous_dpi = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        if (!previous_dpi) throw native_error("Cannot set UI thread DPI awareness");
        dpi = GetDpiForSystem();
        if (!dpi) dpi = 96;
        class_name = L"iiGUI.Host."+std::to_wstring(reinterpret_cast<std::uintptr_t>(this));
        WNDCLASSEXW type{};
        type.cbSize = sizeof(type);
        type.style = CS_HREDRAW | CS_VREDRAW;
        type.lpfnWndProc = procedure;
        type.hInstance = instance;
        type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        type.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        type.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
        type.lpszClassName = class_name.c_str();
        if (!RegisterClassExW(&type)) throw native_error("Cannot register native window class");
        registered = true;
        const auto rectangle = frame(options.size);
        const auto title = native_text(state.title);
        window = CreateWindowExW(0, class_name.c_str(), title.c_str(), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT, CW_USEDEFAULT, rectangle.right-rectangle.left, rectangle.bottom-rectangle.top,
            nullptr, nullptr, instance, this);
        if (!window) throw native_error("Cannot create native GUI window");
        if (failure) throw std::runtime_error(failure->message);
        dpi = GetDpiForWindow(window);
        if (!dpi) dpi = 96;
        make_font();
        const auto actual_frame = frame(options.size);
        if (!SetWindowPos(window, nullptr, 0, 0, actual_frame.right-actual_frame.left,
            actual_frame.bottom-actual_frame.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE))
            throw native_error("Cannot size native GUI window");
        gate->attach(window);
        dispatch->connect([wake=gate]() noexcept { wake->signal(); });
        phase = Phase::running;
        ShowWindow(window, SW_SHOWNORMAL);
        UpdateWindow(window);
        update_snapshot();
        gate->signal();
    }
    void execute() {
        while (phase == Phase::running) {
            const HANDLE event = gate->event; // Only the owner detaches, after this loop.
            const auto wake = MsgWaitForMultipleObjectsEx(1, &event, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            if (wake == WAIT_FAILED) throw native_error("Cannot wait for native GUI events");
            // Bound each native turn, so an input flood cannot starve Core tasks.
            MSG message{};
            for (int count = 0; count < 64 && phase == Phase::running &&
                 PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE); ++count) {
                if (message.message == WM_QUIT) {
                    remember_failure("Native thread quit before its iiGUI window closed");
                    force_close();
                    break;
                }
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if (wake == WAIT_OBJECT_0) drain();
        }
    }
    void cleanup() {
        dispatch->accepting.store(false, std::memory_order_release);
        dispatch->loop.request_stop();
        if (auto detached=dispatch->disconnect(); !detached)
            remember_failure(detached.error().message,detached.error().cause);
        gate->detach();
        canvas.clear();
        text_brush.Reset(); drawing.Reset(); text_layout.Reset();
        text_format.Reset(); text_factory.Reset(); drawing_factory.Reset();
        if (window) {
            KillTimer(window, deadline_timer);
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            DestroyWindow(window);
            window = nullptr;
        }
        if (registered) { UnregisterClassW(class_name.c_str(), instance); registered = false; }
        if (previous_dpi) { SetThreadDpiAwarenessContext(previous_dpi); previous_dpi = nullptr; }
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
        impl_->remember_failure(std::string("Cannot run Win32 GUI host: ")+error.what(), std::current_exception());
        impl_->force_close();
    } catch (...) {
        impl_->remember_failure("Cannot run Win32 GUI host", std::current_exception());
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
    const auto native = native_text(title);
    if (!SetWindowTextW(impl_->window, native.c_str())) return core::fail(core::ErrorCode::failed, "Cannot set native window title");
    impl_->state.title = title;
    return {};
}
core::Result<void> Host::set_text(std::string_view text) {
    if (auto ready = impl_->ready(); !ready) return ready;
    if (!core::valid_utf8(text) || text.size() > INT_MAX)
        return core::fail(core::ErrorCode::invalid_argument, "Display text must be UTF-8 and fit native text limits");
    if (impl_->state.text == text) return {};
    impl_->text = native_text(text);
    impl_->state.text = text;
    impl_->text_layout.Reset();
    InvalidateRect(impl_->window, nullptr, FALSE);
    return {};
}
core::Result<void> Host::set_content_size(Size size) {
    if (auto ready = impl_->ready(); !ready) return ready;
    if (!detail::valid_content_size(size, impl_->options))
        return core::fail(core::ErrorCode::invalid_argument, "Content size is outside native/minimum bounds");
    const auto rectangle = impl_->frame(size);
    if (!SetWindowPos(impl_->window, nullptr, 0, 0, rectangle.right-rectangle.left,
        rectangle.bottom-rectangle.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE))
        return core::fail(core::ErrorCode::failed, "Cannot resize native content");
    impl_->update_snapshot();
    return {};
}
core::Result<void> Host::request_close() {
    if (auto ready = impl_->ready(); !ready) return ready;
    SendMessageW(impl_->window, WM_CLOSE, 0, 0);
    return {};
}
core::Result<void> Host::set_canvas(CanvasFrame frame) {
    if (auto ready = impl_->ready(); !ready) return ready;
    auto result = impl_->canvas.set(std::move(frame), impl_->window, nullptr, impl_->state.backing_scale);
    if (result) impl_->gate->signal();
    return result;
}
core::Result<void> Host::clear_canvas() {
    if (auto ready = impl_->ready(); !ready) return ready;
    impl_->canvas.clear();
    InvalidateRect(impl_->window,nullptr,FALSE);
    return {};
}
core::Result<GraphicsInfo> Host::graphics_info() const {
    if (auto ready = impl_->ready(); !ready) return std::unexpected(ready.error());
    return impl_->canvas.info();
}
core::Result<PixelBuffer> Host::capture_canvas() {
    if (auto ready = impl_->ready(); !ready) return std::unexpected(ready.error());
    return impl_->canvas.capture(impl_->state.backing_scale, impl_->state.size.height);
}
core::Result<std::reference_wrapper<runtime::EventLoop>> Host::event_loop() {
    if (auto ready=impl_->ready(); !ready) return std::unexpected(ready.error());
    return std::ref(impl_->dispatch->loop);
}
core::Result<WindowSnapshot> Host::snapshot() const {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Snapshot requires its owning UI thread");
    if (impl_->phase == Impl::Phase::created) return core::fail(core::ErrorCode::closed, "Window has not opened");
    impl_->update_snapshot();
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
