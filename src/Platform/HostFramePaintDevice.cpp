#include <iiGUI/HostFramePaintDevice.h>
#include <iiGUI/Host.h>

#include <cmath>
#include <utility>

namespace ii::gui {
HostFramePaintDevice::HostFramePaintDevice(std::weak_ptr<Host> host, PaintCapabilities capabilities)
    : host_(std::move(host)), capabilities_(capabilities) {}
PaintCapabilities HostFramePaintDevice::capabilities() const noexcept { return capabilities_; }
core::Result<void> HostFramePaintDevice::paint_frame(const CanvasFrame& frame, const PaintFrameInfo& info, std::stop_token stop) {
    if (stop.stop_requested()) return core::fail(core::ErrorCode::cancelled,"GPU frame was cancelled");
    auto host=host_.lock();
    if (!host) return core::fail(core::ErrorCode::closed,"GPU host has retired");
    auto snapshot=host->snapshot();
    if (!snapshot) return std::unexpected(snapshot.error());
    if (snapshot->closed) return core::fail(core::ErrorCode::closed,"GPU host has closed");
    if (!std::isfinite(snapshot->backing_scale) || std::abs(snapshot->backing_scale-info.scale)>1e-6)
        return core::fail(core::ErrorCode::invalid_argument,"Paint scale must match the host's current backing scale");
    if (stop.stop_requested()) return core::fail(core::ErrorCode::cancelled,"GPU frame was cancelled");
    return host->set_canvas(frame);
}
} // namespace ii::gui
