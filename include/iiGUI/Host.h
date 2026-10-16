#pragma once

#include <iiGUI/Export.h>
#include <iiGUI/Graphics.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <iiSDKCore/Runtime/EventLoop.h>
#include <iiSDKCore/Runtime/HostLoop.h>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace ii::gui {
namespace detail { struct DispatchState; }

struct Size final {
    double width = 0;
    double height = 0;
    friend bool operator==(const Size&, const Size&) = default;
};
struct HostOptions final {
    std::string title = "iiGUI";
    Size size{640, 420};
    Size minimum_size{320, 220};
    std::size_t task_capacity = 1024;
    runtime::PumpBudget budget{};
};
struct HostCallbacks final {
    runtime::Task opened;
    std::function<void(Size)> resized;
    std::function<bool()> close_requested;
    runtime::Task closed;
};
struct WindowSnapshot final {
    Size size;
    double backing_scale = 1;
    bool visible = false;
    bool closed = false;
    std::string title;
    std::string text;
};
struct HostStats final {
    std::uint64_t dispatches = 0;
    std::uint64_t timer_wakes = 0;
    runtime::LoopStats loop;
    runtime::HostLoopStats host;
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
    [[nodiscard]] core::Result<void> set_title(std::string_view title);
    [[nodiscard]] core::Result<void> set_text(std::string_view text);
    [[nodiscard]] core::Result<void> set_content_size(Size size);
    [[nodiscard]] core::Result<void> request_close();
    // Borrow only while Host is alive. Core enforces the bound UI dispatch
    // thread; use executor() for lifetime-safe cross-thread delivery.
    [[nodiscard]] core::Result<std::reference_wrapper<runtime::EventLoop>> event_loop();
    [[nodiscard]] core::Result<void> set_canvas(CanvasFrame frame);
    [[nodiscard]] core::Result<void> clear_canvas();
    [[nodiscard]] core::Result<GraphicsInfo> graphics_info() const;
    // Explicit synchronous GPU readback; normal presentation never reads back.
    [[nodiscard]] core::Result<PixelBuffer> capture_canvas();
    [[nodiscard]] core::Result<WindowSnapshot> snapshot() const;
    [[nodiscard]] core::Result<HostStats> stats() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace ii::gui
