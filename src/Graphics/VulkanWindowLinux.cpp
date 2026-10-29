#include <X11/Xlib.h>
#define VK_USE_PLATFORM_XLIB_KHR
#include <iiGUI/VulkanWindow.h>
#include <cmath>
#include <stdexcept>

namespace ii::gui::detail {
struct VulkanWindow::Impl final {
    Display* display=nullptr;
    Window window=0;
    ~Impl() { if (window) XDestroyWindow(display,window); }
};
VulkanWindow::VulkanWindow(void* parent,void* display) : impl_(std::make_unique<Impl>()) {
    impl_->display=static_cast<Display*>(display);
    XSetWindowAttributes attributes{}; attributes.event_mask=ExposureMask|StructureNotifyMask;
    impl_->window=XCreateWindow(impl_->display,Window(reinterpret_cast<std::uintptr_t>(parent)),0,0,1,1,0,
        CopyFromParent,InputOutput,CopyFromParent,CWEventMask,&attributes);
    if (!impl_->window) throw std::runtime_error("Cannot create native Vulkan X11 child window");
}
VulkanWindow::~VulkanWindow()=default;
const char* VulkanWindow::extension() { return VK_KHR_XLIB_SURFACE_EXTENSION_NAME; }
VkSurfaceKHR VulkanWindow::surface(VkInstance instance) {
    VkXlibSurfaceCreateInfoKHR info{}; info.sType=VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    info.dpy=impl_->display; info.window=impl_->window; VkSurfaceKHR result=VK_NULL_HANDLE;
    if (const auto status=vkCreateXlibSurfaceKHR(instance,&info,nullptr,&result);status!=VK_SUCCESS)
        throw std::runtime_error("Cannot create Vulkan X11 surface: "+std::to_string(status));
    return result;
}
void VulkanWindow::resize(Rect r,double scale,double) {
    XMoveResizeWindow(impl_->display,impl_->window,int(std::lround(r.x*scale)),int(std::lround(r.y*scale)),
        unsigned(std::ceil(r.width*scale)),unsigned(std::ceil(r.height*scale)));
    XMapWindow(impl_->display,impl_->window); XSync(impl_->display,False);
}
VulkanWindowEvent VulkanWindow::event(const void* event) {
    const auto& e=*static_cast<const XEvent*>(event);
    if (e.xany.window!=impl_->window) return VulkanWindowEvent::ignored;
    if (e.type==DestroyNotify) impl_->window=0;
    if (e.type==Expose && e.xexpose.count==0) return VulkanWindowEvent::exposed;
    return VulkanWindowEvent::handled;
}
} // namespace ii::gui::detail
