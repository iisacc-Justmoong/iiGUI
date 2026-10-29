#import <UIKit/UIKit.h>
#import <QuartzCore/CAMetalLayer.h>
#define VK_USE_PLATFORM_METAL_EXT
#include "VulkanWindow.h"
#include <stdexcept>
#include <cmath>

namespace ii::gui::detail {
struct VulkanWindow::Impl final {
    UIView* view=nil;
    CAMetalLayer* layer=nil;
    ~Impl() { [view removeFromSuperview]; }
};
VulkanWindow::VulkanWindow(void* parent,void*) : impl_(std::make_unique<Impl>()) {
    impl_->view=[[UIView alloc] initWithFrame:CGRectZero];
    impl_->layer=[CAMetalLayer layer];
    impl_->layer.opaque=YES;
    impl_->layer.maximumDrawableCount=2;
    auto space=CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    impl_->layer.colorspace=space; CGColorSpaceRelease(space);
    impl_->view.userInteractionEnabled=NO;[impl_->view.layer addSublayer:impl_->layer];
    [(__bridge UIView*)parent addSubview:impl_->view];
}
VulkanWindow::~VulkanWindow()=default;
const char* VulkanWindow::extension() { return VK_EXT_METAL_SURFACE_EXTENSION_NAME; }
VkSurfaceKHR VulkanWindow::surface(VkInstance instance) {
    auto create=reinterpret_cast<PFN_vkCreateMetalSurfaceEXT>(vkGetInstanceProcAddr(instance,"vkCreateMetalSurfaceEXT"));
    if (!create) throw std::runtime_error("Vulkan driver does not expose VK_EXT_metal_surface");
    VkMetalSurfaceCreateInfoEXT info{}; info.sType=VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    info.pLayer=impl_->layer; VkSurfaceKHR result=VK_NULL_HANDLE;
    if (const auto status=create(instance,&info,nullptr,&result);status!=VK_SUCCESS)
        throw std::runtime_error("Cannot create Vulkan Metal surface: "+std::to_string(status));
    return result;
}
void VulkanWindow::resize(Rect r,double scale,double parent_height) {
    (void)parent_height;impl_->view.frame=CGRectMake(r.x,r.y,r.width,r.height);impl_->layer.frame=impl_->view.bounds;
    impl_->layer.contentsScale=scale;
    impl_->layer.drawableSize=CGSizeMake(std::ceil(r.width*scale),std::ceil(r.height*scale));
}
VulkanWindowEvent VulkanWindow::event(const void*) { return VulkanWindowEvent::ignored; }
} // namespace ii::gui::detail
