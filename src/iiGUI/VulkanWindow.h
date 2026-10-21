#pragma once
#include <iiGUI/Graphics.h>
#include <vulkan/vulkan.h>
#include <memory>

namespace ii::gui::detail {
enum class VulkanWindowEvent { ignored, handled, exposed };
// Owns only the native child surface. Instance/device/swapchain belong to the renderer.
class VulkanWindow final {
public:
    VulkanWindow(void* parent,void* display);
    ~VulkanWindow();
    VulkanWindow(const VulkanWindow&)=delete;
    VulkanWindow& operator=(const VulkanWindow&)=delete;
    static const char* extension();
    VkSurfaceKHR surface(VkInstance);
    void resize(Rect viewport,double scale,double parent_height);
    VulkanWindowEvent event(const void*);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace ii::gui::detail
