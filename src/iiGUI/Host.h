#pragma once

#include <iiGUI/Export.h>
#include <iiGUI/Graphics.h>
#include <iiGUI/Presentation.h>
#include <iiGUI/Desktop.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <iiSDKCore/Runtime/EventLoop.h>
#include <iiSDKCore/Runtime/HostLoop.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace ii::gui {
class WidgetBuilder;
class WidgetMount;
struct WidgetAppearance;
namespace detail { struct DispatchState; }

struct Size final {
    double width = 0;
    double height = 0;
    friend bool operator==(const Size&, const Size&) = default;
};
// A declaration update owns only present fields. Unchanged declarations never
// restore an old size after a user resizes the window.
struct WindowGeometry final {
    std::optional<std::string> title;
    std::optional<double> width,height,minimum_width,minimum_height;
    friend bool operator==(const WindowGeometry&,const WindowGeometry&)=default;
};
struct HostOptions final {
    std::string title = "iiGUI";
    Size size{640, 420};
    Size minimum_size{320, 220};
    std::size_t task_capacity = 1024;
    runtime::PumpBudget budget{};
    // Borrowed for this Host's entire lifetime. Null keeps a standalone loop.
    runtime::EventLoop* event_loop = nullptr;
    // Opt-in display cadence; borrows the same EventLoop and outlives Host.
    // macOS 14+ uses the window's CADisplayLink, never a nominal 16 ms timer.
    runtime::FrameLoop* frame_loop = nullptr;
    Renderer renderer=Renderer::automatic;
};
struct HostCallbacks final {
    runtime::Task opened;
    std::function<void(Size)> resized;
    std::function<bool()> close_requested;
    runtime::Task closed;
    std::function<void(std::vector<std::string>)> files_opened;
};
struct WindowSnapshot final {
    Size size;
    double backing_scale = 1;
    bool visible = false;
    bool closed = false;
    std::string title;
    std::string text;
};
// Reports the compiled host contract; OS permissions/session services may still
// deny an individual request. Core targets without a window system have no GUI host.
enum class HostBackend { cocoa, win32, x11, wayland, android, uikit, web, none };
struct HostCapabilities final {
    HostBackend backend=HostBackend::none;
    bool clipboard_text=false,external_urls=false,file_dialogs=false;
    bool secondary_windows=false,display_frames=false;
    bool shaped_text=false,text_input=false,accessibility=false;
};
[[nodiscard]] IIGUI_EXPORT HostCapabilities host_capabilities() noexcept;

struct HostStats final {
    std::uint64_t dispatches = 0;
    std::uint64_t timer_wakes = 0;
    runtime::LoopStats loop;
    runtime::HostLoopStats host;
    std::uint64_t display_callbacks=0;
    runtime::Clock::duration display_interval{};
    bool display_synced=false;
};

// Copyable weak endpoint. Submissions wake the native UI host. It never owns a window.
class IIGUI_EXPORT UiExecutor final {
public:
    UiExecutor() = default;
    [[nodiscard]] runtime::Submission post(runtime::Task task,
        runtime::Priority priority = runtime::Priority::normal) const;
    [[nodiscard]] runtime::Submission at(runtime::Clock::time_point time, runtime::Task task,
        runtime::Priority priority = runtime::Priority::normal) const;
    [[nodiscard]] runtime::Submission after(runtime::Clock::duration delay, runtime::Task task,
        runtime::Priority priority = runtime::Priority::normal) const;
    [[nodiscard]] runtime::Submission every(runtime::Clock::duration interval, runtime::Task task,
        runtime::Priority priority = runtime::Priority::normal) const;
    bool cancel(runtime::TaskToken token) const;
    [[nodiscard]] runtime::Submission watch(io::Channel channel, io::Interest interest,
        runtime::IOReady ready, runtime::IOWatchOptions options = {}) const;
    [[nodiscard]] runtime::Submission read(io::Channel channel, io::Read request,
        runtime::IOCompletion completion, runtime::Priority priority = runtime::Priority::normal) const;
    [[nodiscard]] runtime::Submission write(io::Channel channel, io::Write request,
        runtime::IOCompletion completion, runtime::Priority priority = runtime::Priority::normal) const;
private:
    friend class Host;
    explicit UiExecutor(const std::shared_ptr<detail::DispatchState>& state) : state_(state) {}
    std::weak_ptr<detail::DispatchState> state_;
};

// One native desktop window. Construct/run/destroy on the same UI thread;
// use the process main thread for portable applications (required by AppKit).
// run() is single use. Sizes are logical content units, not framebuffer pixels.
// Runtime workers own background work; UiExecutor delivers its results here.
class IIGUI_EXPORT Host final {
public:
    explicit Host(HostOptions options = {});
    ~Host();
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;
    Host(Host&&) = delete;
    Host& operator=(Host&&) = delete;

    [[nodiscard]] UiExecutor executor() const noexcept;
    [[nodiscard]] bool is_ui_thread() const noexcept;
    [[nodiscard]] core::Result<void> run(HostCallbacks callbacks = {});
    // Nonblocking secondary window. It shares the parent's Core pump and closes
    // with its parent; callbacks/UiExecutor retire independently of that parent.
    [[nodiscard]] core::Result<void> show(Host& parent,HostCallbacks callbacks = {});
    [[nodiscard]] core::Result<void> choose_file(FileDialogOptions,FileDialogCompletion);
    [[nodiscard]] core::Result<void> window_action(WindowAction);
    [[nodiscard]] core::Result<void> set_app_menu(std::vector<AppMenuItem>);
    [[nodiscard]] core::Result<void> set_title(std::string_view title);
    [[nodiscard]] core::Result<void> set_text(std::string_view text);
    [[nodiscard]] core::Result<void> set_content_size(Size size);
    [[nodiscard]] core::Result<void> set_window_geometry(const WindowGeometry&);
    [[nodiscard]] core::Result<void> request_close();
    // Borrow only while Host is alive. Core enforces the bound UI dispatch
    // thread; use executor() for lifetime-safe cross-thread delivery.
    [[nodiscard]] core::Result<std::reference_wrapper<runtime::EventLoop>> event_loop();
    [[nodiscard]] core::Result<void> set_canvas(CanvasFrame frame);
    // Include NativeWidgets.h for self-rendered C++ widgets. Failed factories
    // preserve previous content. The OS adapter provides one input/GPU surface.
    [[nodiscard]] core::Result<WidgetMount> mount_widgets(std::function<core::Result<void>(WidgetBuilder&)> factory);
    [[nodiscard]] core::Result<WidgetMount> mount_widgets(std::function<core::Result<void>(WidgetBuilder&)> factory, const WidgetAppearance&);
    [[nodiscard]] core::Result<void> unmount_widgets();
    [[nodiscard]] core::Result<void> unmount_widgets(const WidgetMount& expected);
    // A higher-level view supplies render_view(host, view, renderer) through
    // argument-dependent lookup. The host does not depend on that view type.
    template<class View>
        requires requires(Host& host, const View& view, Renderer renderer) {
            { render_view(host, view, renderer) } -> std::same_as<core::Result<void>>;
        }
    [[nodiscard]] core::Result<void> set_view(const View& view, Renderer renderer = Renderer::automatic) {
        return render_view(*this, view, renderer);
    }
    [[nodiscard]] core::Result<void> clear_canvas();
    [[nodiscard]] core::Result<GraphicsInfo> graphics_info() const;
    // Explicit synchronous GPU readback; normal presentation never reads back.
    [[nodiscard]] core::Result<PixelBuffer> capture_canvas();
    [[nodiscard]] core::Result<WindowSnapshot> snapshot() const;
    [[nodiscard]] core::Result<HostStats> stats() const;
    [[nodiscard]] core::Result<std::shared_ptr<PresentationSource>> presentation_source() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace ii::gui
