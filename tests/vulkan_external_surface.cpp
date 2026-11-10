#include <iiGUI/VulkanPaintDevice.h>

#include <iostream>
#include <algorithm>
#include <stdexcept>

using namespace ii::gui;
namespace {
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void invalid(VulkanSurfaceProvider provider) {
    auto result = VulkanPaintDevice::create(std::move(provider));
    check(!result && result.error().code == ii::core::ErrorCode::invalid_argument,
          "Invalid external Vulkan surface provider was accepted");
}
}
int main() {
    try {
        invalid({});
        VulkanSurfaceProvider missing_extension;
        missing_extension.create = [](VkInstance) -> ii::core::Result<VkSurfaceKHR> { return VK_NULL_HANDLE; };
        invalid(std::move(missing_extension));
        VulkanSurfaceProvider missing_factory;
        missing_factory.extensions = {"VK_KHR_wayland_surface"};
        invalid(std::move(missing_factory));
        VulkanSurfaceProvider invalid_name;
        invalid_name.extensions = {std::string("VK_BAD\0EXT", 10)};
        invalid_name.create = [](VkInstance) -> ii::core::Result<VkSurfaceKHR> { return VK_NULL_HANDLE; };
        invalid(std::move(invalid_name));
        VulkanSurfaceProvider duplicate;
        duplicate.extensions = {"VK_KHR_wayland_surface", "VK_KHR_wayland_surface"};
        duplicate.create = [](VkInstance) -> ii::core::Result<VkSurfaceKHR> { return VK_NULL_HANDLE; };
        invalid(std::move(duplicate));
#if defined(__APPLE__)
        std::uint32_t count=0;
        check(vkEnumerateInstanceExtensionProperties(nullptr,&count,nullptr)==VK_SUCCESS,"Cannot enumerate Vulkan extensions");
        std::vector<VkExtensionProperties> extensions(count);
        check(vkEnumerateInstanceExtensionProperties(nullptr,&count,extensions.data())==VK_SUCCESS,"Cannot read Vulkan extensions");
        if(std::ranges::any_of(extensions,[](const auto& item){return std::string_view(item.extensionName)=="VK_EXT_metal_surface";})) {
            int calls=0;
            auto captured=std::make_shared<int>(7);
            std::weak_ptr<int> lifetime=captured;
            VulkanSurfaceProvider rejected;
            rejected.extensions={"VK_EXT_metal_surface"};
            rejected.create=[captured,&calls](VkInstance instance)->ii::core::Result<VkSurfaceKHR>{
                ++calls;
                if(!instance || *captured!=7)throw std::runtime_error("Missing Vulkan instance or provider lifetime");
                return ii::core::fail(ii::core::ErrorCode::cancelled,"Surface creation rejected by host");
            };
            captured.reset();
            auto result=VulkanPaintDevice::create(std::move(rejected));
            check(!result && result.error().code==ii::core::ErrorCode::cancelled && calls==1,
                  "Host surface creation failure was not preserved");
            check(lifetime.expired(),"A rejected surface provider leaked its capture");
            std::cout << "VULKAN_EXTERNAL_FACTORY_ERROR_OK\n";
        } else {
            std::cout << "VULKAN_METAL_SURFACE_EXTENSION_UNAVAILABLE\n";
        }
#endif
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
