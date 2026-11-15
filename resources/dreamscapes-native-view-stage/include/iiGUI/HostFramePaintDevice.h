#pragma once
#include <iiGUI/HostBridgeExport.h>
#include <iiGUI/Painter.h>

#include <memory>

namespace ii::gui {
class Host;

// Adapter owned by the app-side host. A successful paint queues the frame on
// the host's existing native GPU renderer; no window or worker is created.
// The caller supplies capabilities matching its chosen renderer.
class IIGUI_HOST_BRIDGE_EXPORT HostFramePaintDevice final : public FramePaintDevice {
public:
    HostFramePaintDevice(std::weak_ptr<Host>, PaintCapabilities);
    [[nodiscard]] PaintCapabilities capabilities() const noexcept override;
    core::Result<void> paint_frame(const CanvasFrame&, const PaintFrameInfo&, std::stop_token) override;
private:
    std::weak_ptr<Host> host_;
    PaintCapabilities capabilities_;
};
} // namespace ii::gui
