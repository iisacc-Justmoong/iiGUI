#pragma once

#include <iiGUI/NativeWidgets.h>
#include <iiGUI/Painter.h>
#include <iiSDKCore/Runtime/FrameLoop.h>

#include <memory>
#include <string_view>

namespace ii::gui {

// Optional host-owned services. Event/frame loops must outlive the view. The
// invalidation callback is delivered on events, never inline with a mutation.
// The host supplies display frames only while frame_activity is true.
struct PaintViewServices final {
    runtime::EventLoop* events = nullptr;
    runtime::FrameLoop* frames = nullptr;
    std::function<void()> invalidated;
    std::function<void(bool)> frame_activity;
    std::function<core::Result<void>(const WindowGeometry&)> window_geometry;
};

// A surface-independent retained widget tree. The application supplies both
// the paint device and input; it decides when events or queries require paint.
// Create, operate and retire the view on the same owner thread.
class IIGUI_EXPORT PaintView final : public std::enable_shared_from_this<PaintView> {
public:
    [[nodiscard]] static core::Result<std::shared_ptr<PaintView>> create(
        std::shared_ptr<PaintDevice>, WidgetFactory, WidgetAppearance = {});
    [[nodiscard]] static core::Result<std::shared_ptr<PaintView>> create(
        std::shared_ptr<PaintDevice>, WidgetFactory, WidgetAppearance, PaintViewServices);
    ~PaintView();
    PaintView(const PaintView&) = delete;
    PaintView& operator=(const PaintView&) = delete;

    [[nodiscard]] core::Result<void> paint(Size logical_size, double scale = 1);
    [[nodiscard]] core::Result<bool> needs_paint() const;
    [[nodiscard]] core::Result<void> input(const WidgetInput&);
    [[nodiscard]] core::Result<Widget> find(std::string_view id) const;
    [[nodiscard]] core::Result<void> close();

private:
    struct Impl;
    explicit PaintView(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl_;
};
} // namespace ii::gui
