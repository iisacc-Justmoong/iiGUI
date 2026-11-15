#pragma once

#include <iiGUI/Export.h>
#include <iiGUI/Painter.h>

#include <memory>

#if defined(_WIN32)
struct IDXGISwapChain;
#endif

namespace ii::gui {
#if defined(_WIN32)
// The host supplies a D3D11 swapchain and owns its resize and surface lifetime.
// The device retains the COM interface. The host must exclusively delegate the
// immediate context and swapchain while painting and until destruction.
class IIGUI_EXPORT Direct3DPaintDevice final : public FramePaintDevice {
public:
    [[nodiscard]] static core::Result<std::shared_ptr<Direct3DPaintDevice>> create(IDXGISwapChain*);
    ~Direct3DPaintDevice() override;
    [[nodiscard]] PaintCapabilities capabilities() const noexcept override;
    core::Result<void> paint_frame(const CanvasFrame&,const PaintFrameInfo&,std::stop_token) override;

private:
    struct Impl;
    explicit Direct3DPaintDevice(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl_;
};
#endif
} // namespace ii::gui
