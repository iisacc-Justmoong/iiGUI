#define VK_USE_PLATFORM_WAYLAND_KHR
#include <iiGUI/VulkanWindow.h>
#include <iiGUI/WaylandConnection.h>
#include <stdexcept>
namespace ii::gui::detail {
struct VulkanWindow::Impl {WaylandConnection& parent;explicit Impl(WaylandConnection& p):parent(p){}};
VulkanWindow::VulkanWindow(void* parent,void*):impl_(std::make_unique<Impl>(*static_cast<WaylandConnection*>(parent))){}
VulkanWindow::~VulkanWindow()=default;
const char* VulkanWindow::extension(){return VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;}
VkSurfaceKHR VulkanWindow::surface(VkInstance instance){VkWaylandSurfaceCreateInfoKHR info{};info.sType=VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;info.display=impl_->parent.display();info.surface=impl_->parent.surface();VkSurfaceKHR result=VK_NULL_HANDLE;if(vkCreateWaylandSurfaceKHR(instance,&info,nullptr,&result)!=VK_SUCCESS)throw std::runtime_error("Cannot create native Wayland Vulkan surface");return result;}
void VulkanWindow::resize(Rect,double,double){}
VulkanWindowEvent VulkanWindow::event(const void*){return VulkanWindowEvent::ignored;}
}
