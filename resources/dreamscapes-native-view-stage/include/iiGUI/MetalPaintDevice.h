#pragma once

#include <iiGUI/Export.h>
#include <iiGUI/Painter.h>

#include <memory>

namespace ii::gui {

#if defined(__APPLE__)
// The host provides a live CAMetalLayer pointer and must not render to it
// concurrently. The device retains the layer but never creates a native view.
// All methods and destruction use the thread that created the device.
class IIGUI_EXPORT MetalPaintDevice final : public FramePaintDevice {
public:
    [[nodiscard]] static core::Result<std::shared_ptr<MetalPaintDevice>> create(void* metal_layer);
    ~MetalPaintDevice() override;
    [[nodiscard]] PaintCapabilities capabilities() const noexcept override;
    core::Result<void> paint_frame(const CanvasFrame&, const PaintFrameInfo&, std::stop_token) override;

private:
    struct Impl;
    explicit MetalPaintDevice(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl_;
};
#endif
} // namespace ii::gui
