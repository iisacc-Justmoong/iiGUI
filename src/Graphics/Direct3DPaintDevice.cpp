#include <iiGUI/Direct3DPaintDevice.h>
#include <iiGUI/NativeCanvas.h>

#include <cmath>
#include <exception>
#include <thread>
#include <utility>

namespace ii::gui {
struct Direct3DPaintDevice::Impl final {
    explicit Impl(IDXGISwapChain* chain)
        : canvas(detail::PlatformCanvas::ExternalDirect3DSwapChain{chain},Acceleration::require_hardware),
          owner(std::this_thread::get_id()) {}
    detail::PlatformCanvas canvas;
    std::thread::id owner;
    bool painting=false;
};
Direct3DPaintDevice::Direct3DPaintDevice(std::unique_ptr<Impl> value):impl_(std::move(value)) {}
Direct3DPaintDevice::~Direct3DPaintDevice()=default;

core::Result<std::shared_ptr<Direct3DPaintDevice>> Direct3DPaintDevice::create(IDXGISwapChain* chain) {
    if(!chain)return core::fail(core::ErrorCode::invalid_argument,"External D3D11 swapchain is missing");
    try {
        return std::shared_ptr<Direct3DPaintDevice>(new Direct3DPaintDevice(std::make_unique<Impl>(chain)));
    } catch(...) {
        return core::fail(core::ErrorCode::failed,"Cannot initialize external D3D11 paint device",std::current_exception());
    }
}
PaintCapabilities Direct3DPaintDevice::capabilities()const noexcept {
    return {.api=GraphicsApi::direct3d11,.hardware=true,.native_api=true,
            .backdrop=true,.shadow=true,.content_transform=true,.scene_sources=true};
}
core::Result<void> Direct3DPaintDevice::paint_frame(const CanvasFrame& frame,const PaintFrameInfo& info,std::stop_token stop) {
    if(impl_->owner!=std::this_thread::get_id())return core::fail(core::ErrorCode::wrong_executor,"Direct3D device requires its owner thread");
    if(impl_->painting)return core::fail(core::ErrorCode::busy,"A Direct3D frame is already active");
    if(stop.stop_requested())return core::fail(core::ErrorCode::cancelled,"Direct3D frame was cancelled");
    if(!valid_canvas(frame) || frame.renderer==Renderer::vulkan || frame.viewport!=info.viewport ||
       frame.background!=info.background || !std::isfinite(info.scale) || info.scale<=0)
        return core::fail(core::ErrorCode::invalid_argument,"Invalid external Direct3D frame");
    try {
        const auto [width,height]=detail::canvas_pixels(frame,info.scale,detail::CanvasPixelRounding::outward);
        if(width!=info.pixel_width || height!=info.pixel_height)
            return core::fail(core::ErrorCode::invalid_argument,"Direct3D frame dimensions do not match its target");
        struct Active final { bool& flag; explicit Active(bool& value):flag(value){flag=true;} ~Active(){flag=false;} } active(impl_->painting);
        impl_->canvas.draw(frame,info.scale,frame.viewport.height,stop);
        return {};
    } catch(...) {
        if(stop.stop_requested())return core::fail(core::ErrorCode::cancelled,"Direct3D frame was cancelled");
        return core::fail(core::ErrorCode::failed,"Direct3D frame submission failed",std::current_exception());
    }
}
} // namespace ii::gui
