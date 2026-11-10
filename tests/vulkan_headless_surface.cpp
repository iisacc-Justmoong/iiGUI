#include <iiGUI/VulkanPaintDevice.h>
#if defined(IIGUI_STATIC_DEFINE)
#include <iiGUI/VulkanCanvas.h>
#endif

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace ii::gui;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
    try {
        std::uint32_t count=0;
        check(vkEnumerateInstanceExtensionProperties(nullptr,&count,nullptr)==VK_SUCCESS,"Cannot enumerate instance extensions");
        std::vector<VkExtensionProperties> extensions(count);
        check(vkEnumerateInstanceExtensionProperties(nullptr,&count,extensions.data())==VK_SUCCESS,"Cannot read instance extensions");
        if(std::ranges::none_of(extensions,[](const auto& item){return std::string_view(item.extensionName)==VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME;})){
            std::cout << "Vulkan headless surfaces are unavailable\n";
            return 77;
        }
        int calls=0;
        VulkanSurfaceProvider provider;
        provider.extensions={VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME};
        const auto make_surface=[&](VkInstance instance)->ii::core::Result<VkSurfaceKHR>{
            ++calls;
            const auto create=reinterpret_cast<PFN_vkCreateHeadlessSurfaceEXT>(vkGetInstanceProcAddr(instance,"vkCreateHeadlessSurfaceEXT"));
            if(!create)return ii::core::fail(ii::core::ErrorCode::failed,"Vulkan headless factory is unavailable");
            VkHeadlessSurfaceCreateInfoEXT info{};info.sType=VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT;
            VkSurfaceKHR surface=VK_NULL_HANDLE;
            if(create(instance,&info,nullptr,&surface)!=VK_SUCCESS)
                return ii::core::fail(ii::core::ErrorCode::failed,"Cannot create Vulkan headless surface");
            return surface;
        };
        provider.create=make_surface;
        auto device=VulkanPaintDevice::create(std::move(provider),Acceleration::prefer_hardware);
        check(bool(device)&&calls==1,"Cannot initialize Vulkan headless paint device");
        CanvasFrame frame{.viewport={0,0,64,64},.background={0,0,0,1}};
        frame.panels.push_back({.bounds={4,4,24,24},.top={1,0,0,1},.bottom={0,0,1,1}});
        Painter painter(*device);
        std::stop_source cancellation;cancellation.request_stop();
        auto cancelled=painter.paint(frame,1,cancellation.get_token());
        check(!cancelled&&cancelled.error().code==ii::core::ErrorCode::cancelled,"Pre-cancelled Vulkan frame was not rejected");
        PaintFrameInfo target{frame.viewport,frame.background,1,64,64};
        ii::core::ErrorCode other_thread=ii::core::ErrorCode::failed;
        std::thread worker([&]{auto result=(*device)->paint_frame(frame,target,{});if(!result)other_thread=result.error().code;});
        worker.join();
        check(other_thread==ii::core::ErrorCode::wrong_executor,"Vulkan device accepted a non-owner thread");
        auto mismatched=target;mismatched.pixel_width=63;
        auto invalid=(*device)->paint_frame(frame,mismatched,{});
        check(!invalid&&invalid.error().code==ii::core::ErrorCode::invalid_argument,"Vulkan accepted mismatched surface dimensions");
        auto first=painter.paint(frame);
        if(!first)throw std::runtime_error("Vulkan headless first frame failed: "+first.error().message);
        frame.panels[0].bounds.x=8;
        auto changed=painter.paint(frame);
        if(!changed)throw std::runtime_error("Vulkan headless changed frame failed: "+changed.error().message);
        auto repeat=painter.paint(frame);
        if(!repeat)throw std::runtime_error("Vulkan headless retained frame failed: "+repeat.error().message);
        frame.viewport.width=80;frame.viewport.height=72;
        auto resized=painter.paint(frame);
        if(!resized)throw std::runtime_error("Vulkan headless resized frame failed: "+resized.error().message);
#if defined(IIGUI_STATIC_DEFINE)
        // The internal canvas is linkable from this test only in a static
        // build; the public surface tests above also run with shared iiGUI.
        VulkanSurfaceProvider capture_provider{{VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME},make_surface};
        detail::VulkanCanvas capture(capture_provider,Acceleration::prefer_hardware);
        check(capture.draw_external(frame,1,{}),"Vulkan headless readback frame was not presented");
        const auto pixels=capture.capture();
        check(pixels.width==80&&pixels.height==72,"Vulkan readback dimensions changed");
        const auto pixel=[&](unsigned x,unsigned y,unsigned channel){return pixels.rgba[(std::size_t(y)*pixels.width+x)*4+channel];};
        check(pixel(10,6,0)>100&&pixel(10,6,0)>pixel(10,6,2),"Vulkan GPU panel was not drawn into the target");
        check(pixel(70,60,0)<5&&pixel(70,60,1)<5&&pixel(70,60,2)<5,"Vulkan GPU background was not retained");
        std::cout << "VULKAN_HEADLESS_READBACK_OK\n";
#endif
        std::cout << "VULKAN_HEADLESS_FRAMES_OK\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
