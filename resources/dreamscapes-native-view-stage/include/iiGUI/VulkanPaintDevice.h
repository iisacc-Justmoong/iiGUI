#pragma once

#include <iiGUI/Export.h>
#include <iiGUI/Painter.h>
#include <vulkan/vulkan.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ii::gui {
// The host creates a surface for the instance supplied by iiGUI. Successful
// creation transfers that surface to iiGUI; the native window/display and any
// captured state must remain alive until the device is destroyed. No platform
// window or input type appears in this contract.
struct VulkanSurfaceProvider final {
    std::vector<std::string> extensions;
    std::function<core::Result<VkSurfaceKHR>(VkInstance)> create;
};

// A host-provided surface transferred to the renderer, which also owns the
// Vulkan instance, device, swapchain and command resources. Create, paint and
// destroy on one thread.
class IIGUI_EXPORT VulkanPaintDevice final : public FramePaintDevice {
public:
    [[nodiscard]] static core::Result<std::shared_ptr<VulkanPaintDevice>> create(
        VulkanSurfaceProvider, Acceleration = Acceleration::require_hardware);
    ~VulkanPaintDevice() override;
    [[nodiscard]] PaintCapabilities capabilities() const noexcept override;
    core::Result<void> paint_frame(const CanvasFrame&, const PaintFrameInfo&, std::stop_token) override;

private:
    struct Impl;
    explicit VulkanPaintDevice(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl_;
};
} // namespace ii::gui
