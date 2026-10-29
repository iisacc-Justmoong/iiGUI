#define VK_USE_PLATFORM_ANDROID_KHR
#include <android/native_window.h>
#include <iiGUI/VulkanWindow.h>
#include <stdexcept>

namespace ii::gui::detail {
struct VulkanWindow::Impl final {
    ANativeWindow* window;
    explicit Impl(ANativeWindow* value):window(value) {
        if(!window)throw std::invalid_argument("Android surface is not available");
        ANativeWindow_acquire(window);
    }
    ~Impl(){ANativeWindow_release(window);}
};
VulkanWindow::VulkanWindow(void* parent,void*):impl_(std::make_unique<Impl>(static_cast<ANativeWindow*>(parent))){}
VulkanWindow::~VulkanWindow()=default;
const char* VulkanWindow::extension(){return VK_KHR_ANDROID_SURFACE_EXTENSION_NAME;}
VkSurfaceKHR VulkanWindow::surface(VkInstance instance){
    VkAndroidSurfaceCreateInfoKHR info{};info.sType=VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;info.window=impl_->window;
    VkSurfaceKHR surface=VK_NULL_HANDLE;
    if(const auto result=vkCreateAndroidSurfaceKHR(instance,&info,nullptr,&surface);result!=VK_SUCCESS)
        throw std::runtime_error("Cannot create Android Vulkan surface: "+std::to_string(result));
    return surface;
}
void VulkanWindow::resize(Rect,double,double){} // Android owns the native surface extent.
VulkanWindowEvent VulkanWindow::event(const void*){return VulkanWindowEvent::ignored;}
}
