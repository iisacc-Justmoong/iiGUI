#include <iiGUI/VulkanPaintDevice.h>
#include <iiGUI/VulkanCanvas.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <thread>
#include <utility>

namespace ii::gui {
namespace {
bool valid_provider(const VulkanSurfaceProvider& provider) {
    if (!provider.create || provider.extensions.empty() || provider.extensions.size()>32) return false;
    for (std::size_t index=0; index<provider.extensions.size(); ++index) {
        const auto& name=provider.extensions[index];
        if (!name.starts_with("VK_") || name.size()>255 ||
            !std::ranges::all_of(name,[](unsigned char c) {
                return (c>='A'&&c<='Z') || (c>='a'&&c<='z') || (c>='0'&&c<='9') || c=='_';
            })) return false;
        for(std::size_t before=0; before<index; ++before)
            if(provider.extensions[before]==name)return false;
    }
    return true;
}
}
struct VulkanPaintDevice::Impl final {
    Impl(VulkanSurfaceProvider value,Acceleration acceleration)
        : provider(std::move(value)),canvas(provider,acceleration),owner(std::this_thread::get_id()) {
        const auto info=canvas.info();
        capabilities={.api=GraphicsApi::vulkan,.hardware=info.hardware,.native_api=false,
            .backdrop=info.backdrop_supported,.shadow=info.shadow_supported,
            .content_transform=info.content_transform_supported,.scene_sources=info.scene_sources_supported};
    }
    // Destroy the canvas, including its surface, before the provider's captures.
    VulkanSurfaceProvider provider;
    detail::VulkanCanvas canvas;
    std::thread::id owner;
    PaintCapabilities capabilities;
    bool painting=false;
};

VulkanPaintDevice::VulkanPaintDevice(std::unique_ptr<Impl> value):impl_(std::move(value)) {}
VulkanPaintDevice::~VulkanPaintDevice()=default;

core::Result<std::shared_ptr<VulkanPaintDevice>> VulkanPaintDevice::create(VulkanSurfaceProvider provider,Acceleration acceleration) {
    if(!valid_provider(provider) || (acceleration!=Acceleration::require_hardware && acceleration!=Acceleration::prefer_hardware))
        return core::fail(core::ErrorCode::invalid_argument,"Invalid external Vulkan surface provider or acceleration policy");
    try {
        return std::shared_ptr<VulkanPaintDevice>(new VulkanPaintDevice(std::make_unique<Impl>(std::move(provider),acceleration)));
    } catch(const detail::VulkanSurfaceFailure& failure) {
        return std::unexpected(failure.error);
    } catch(...) {
        return core::fail(core::ErrorCode::failed,"Cannot initialize external Vulkan paint device",std::current_exception());
    }
}

PaintCapabilities VulkanPaintDevice::capabilities()const noexcept {
    return impl_->capabilities;
}

core::Result<void> VulkanPaintDevice::paint_frame(const CanvasFrame& frame,const PaintFrameInfo& info,std::stop_token stop) {
    if(impl_->owner!=std::this_thread::get_id())return core::fail(core::ErrorCode::wrong_executor,"Vulkan device requires its owner thread");
    if(impl_->painting)return core::fail(core::ErrorCode::busy,"A Vulkan frame is already active");
    if(stop.stop_requested())return core::fail(core::ErrorCode::cancelled,"Vulkan frame was cancelled");
    if(!valid_canvas(frame) || frame.renderer==Renderer::native || frame.viewport!=info.viewport ||
       frame.background!=info.background || !std::isfinite(info.scale) || info.scale<=0)
        return core::fail(core::ErrorCode::invalid_argument,"Invalid external Vulkan frame");
    try {
        const auto [width,height]=detail::canvas_pixels(frame,info.scale,detail::CanvasPixelRounding::outward);
        if(width!=info.pixel_width || height!=info.pixel_height)
            return core::fail(core::ErrorCode::invalid_argument,"Vulkan frame dimensions do not match its target");
        struct Active final { bool& flag; explicit Active(bool& value):flag(value){flag=true;} ~Active(){flag=false;} } active(impl_->painting);
        if(!impl_->canvas.draw_external(frame,info.scale,stop))
            return core::fail(core::ErrorCode::busy,"External Vulkan surface cannot present this frame");
        return {};
    } catch(const detail::VulkanSurfaceFailure& failure) {
        return std::unexpected(failure.error);
    } catch(...) {
        return core::fail(core::ErrorCode::failed,"Vulkan frame submission failed",std::current_exception());
    }
}
} // namespace ii::gui
