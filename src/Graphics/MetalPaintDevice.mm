#import <QuartzCore/CAMetalLayer.h>
#include <iiGUI/MetalPaintDevice.h>
#include <iiGUI/NativeCanvas.h>

#include <cmath>
#include <exception>
#include <thread>
#include <utility>

namespace ii::gui {
struct MetalPaintDevice::Impl final {
    explicit Impl(void* layer)
        : canvas(detail::PlatformCanvas::ExternalMetalLayer{layer}, Acceleration::require_hardware),
          owner(std::this_thread::get_id()) {}
    detail::PlatformCanvas canvas;
    std::thread::id owner;
};

MetalPaintDevice::MetalPaintDevice(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
MetalPaintDevice::~MetalPaintDevice() = default;

core::Result<std::shared_ptr<MetalPaintDevice>> MetalPaintDevice::create(void* metal_layer) {
    if (!metal_layer) return core::fail(core::ErrorCode::invalid_argument, "External Metal layer is missing");
    @autoreleasepool {
        id layer = (__bridge id)metal_layer;
        if (![layer isKindOfClass:[CAMetalLayer class]])
            return core::fail(core::ErrorCode::invalid_argument, "External surface is not a CAMetalLayer");
        try {
            return std::shared_ptr<MetalPaintDevice>(new MetalPaintDevice(std::make_unique<Impl>(metal_layer)));
        } catch (...) {
            return core::fail(core::ErrorCode::failed, "Cannot initialize external Metal paint device", std::current_exception());
        }
    }
}

PaintCapabilities MetalPaintDevice::capabilities() const noexcept {
    return {.api=GraphicsApi::metal, .hardware=true, .native_api=true,
            .backdrop=true, .shadow=true, .content_transform=true, .scene_sources=true};
}

core::Result<void> MetalPaintDevice::paint_frame(const CanvasFrame& frame, const PaintFrameInfo& info,
                                                std::stop_token stop) {
    if (impl_->owner != std::this_thread::get_id())
        return core::fail(core::ErrorCode::wrong_executor, "Metal device requires its owner thread");
    if (stop.stop_requested()) return core::fail(core::ErrorCode::cancelled, "Metal frame was cancelled");
    if (!valid_canvas(frame) || frame.renderer == Renderer::vulkan || frame.viewport != info.viewport ||
        frame.background != info.background || !std::isfinite(info.scale) || info.scale <= 0)
        return core::fail(core::ErrorCode::invalid_argument, "Invalid external Metal frame");
    try {
        const auto [width,height] = detail::canvas_pixels(frame,info.scale);
        if (width != info.pixel_width || height != info.pixel_height)
            return core::fail(core::ErrorCode::invalid_argument, "Metal frame dimensions do not match its target");
        if (stop.stop_requested()) return core::fail(core::ErrorCode::cancelled, "Metal frame was cancelled");
        impl_->canvas.draw(frame,info.scale,frame.viewport.height,stop);
        return {};
    } catch (...) {
        if (stop.stop_requested()) return core::fail(core::ErrorCode::cancelled, "Metal frame was cancelled");
        return core::fail(core::ErrorCode::failed, "Metal frame submission failed", std::current_exception());
    }
}
} // namespace ii::gui
