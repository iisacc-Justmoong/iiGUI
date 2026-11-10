#pragma once
#include <iiGUI/NativeCanvas.h>

namespace ii::gui { struct VulkanSurfaceProvider; }
namespace ii::gui::detail {
struct VulkanSurfaceFailure final { core::Error error; };
class VulkanCanvas final {
public:
    VulkanCanvas(void* parent,void* display,Acceleration);
    VulkanCanvas(const VulkanSurfaceProvider&, Acceleration);
    ~VulkanCanvas();
    VulkanCanvas(const VulkanCanvas&)=delete;
    VulkanCanvas& operator=(const VulkanCanvas&)=delete;
    void draw(const CanvasFrame&,double scale,double parent_height);
    bool draw_external(const CanvasFrame&,double scale,std::stop_token);
    PixelBuffer capture();
    GraphicsInfo info() const;
    bool native_event(const void*);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace ii::gui::detail
