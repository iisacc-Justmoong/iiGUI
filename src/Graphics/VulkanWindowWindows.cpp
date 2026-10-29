#include <windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
#include <iiGUI/VulkanWindow.h>
#include <cmath>
#include <stdexcept>

namespace ii::gui::detail {
struct VulkanWindow::Impl final {
    HINSTANCE instance=GetModuleHandleW(nullptr);
    HWND window=nullptr;
    std::wstring name;
    bool registered=false;
    static LRESULT CALLBACK procedure(HWND h,UINT message,WPARAM w,LPARAM l) noexcept {
        if (message==WM_ERASEBKGND) return 1;
        if (message==WM_PAINT) { PAINTSTRUCT paint{}; BeginPaint(h,&paint); EndPaint(h,&paint); return 0; }
        if(message==WM_NCHITTEST)return HTTRANSPARENT;
        return DefWindowProcW(h,message,w,l);
    }
    ~Impl() {
        if (window) DestroyWindow(window);
        if (registered) UnregisterClassW(name.c_str(),instance);
    }
};
VulkanWindow::VulkanWindow(void* parent,void*) : impl_(std::make_unique<Impl>()) {
    auto& p=*impl_; p.name=L"iiGUI.Vulkan."+std::to_wstring(reinterpret_cast<std::uintptr_t>(this));
    WNDCLASSEXW type{}; type.cbSize=sizeof(type); type.lpfnWndProc=Impl::procedure;
    type.hInstance=p.instance; type.lpszClassName=p.name.c_str();
    if (!RegisterClassExW(&type)) throw std::runtime_error("Cannot register native Vulkan child window");
    p.registered=true;
    p.window=CreateWindowExW(0,p.name.c_str(),L"",WS_CHILD|WS_CLIPSIBLINGS,0,0,1,1,
        static_cast<HWND>(parent),nullptr,p.instance,nullptr);
    if (!p.window) throw std::runtime_error("Cannot create native Vulkan child window");
}
VulkanWindow::~VulkanWindow()=default;
const char* VulkanWindow::extension() { return VK_KHR_WIN32_SURFACE_EXTENSION_NAME; }
VkSurfaceKHR VulkanWindow::surface(VkInstance instance) {
    VkWin32SurfaceCreateInfoKHR info{}; info.sType=VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    info.hinstance=impl_->instance; info.hwnd=impl_->window;
    VkSurfaceKHR result=VK_NULL_HANDLE;
    if (const auto status=vkCreateWin32SurfaceKHR(instance,&info,nullptr,&result);status!=VK_SUCCESS)
        throw std::runtime_error("Cannot create Vulkan Win32 surface: "+std::to_string(status));
    return result;
}
void VulkanWindow::resize(Rect r,double scale,double) {
    if (!SetWindowPos(impl_->window,nullptr,int(std::lround(r.x*scale)),int(std::lround(r.y*scale)),
        int(std::ceil(r.width*scale)),int(std::ceil(r.height*scale)),SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW))
        throw std::runtime_error("Cannot position Vulkan child window");
}
VulkanWindowEvent VulkanWindow::event(const void*) { return VulkanWindowEvent::ignored; }
} // namespace ii::gui::detail
