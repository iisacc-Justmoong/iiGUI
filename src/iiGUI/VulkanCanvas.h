#pragma once
#include "NativeCanvas.h"

namespace ii::gui::detail {
class VulkanCanvas final {
public:
    VulkanCanvas(void* parent,void* display,Acceleration);
    ~VulkanCanvas();
    VulkanCanvas(const VulkanCanvas&)=delete;
    VulkanCanvas& operator=(const VulkanCanvas&)=delete;
    void draw(const CanvasFrame&,double scale,double parent_height);
    PixelBuffer capture();
    GraphicsInfo info() const;
    bool native_event(const void*);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace ii::gui::detail
