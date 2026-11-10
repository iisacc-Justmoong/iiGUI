#include <iiGUI/Painter.h>
#include "GraphicsValidation.h"

#include <cmath>
#include <exception>
#include <utility>

namespace ii::gui {
namespace {
core::Result<void> frame_only() { return core::fail(core::ErrorCode::unsupported, "This device requires a complete frame"); }
struct Transaction final {
    PaintDevice& device;
    bool committed = false;
    ~Transaction() { if (!committed) device.abort_frame(); }
};
struct Activity final {
    bool& active;
    explicit Activity(bool& value) : active(value) { active = true; }
    ~Activity() { active = false; }
};

core::Result<void> check_stop(std::stop_token stop) {
    if (stop.stop_requested()) return core::fail(core::ErrorCode::cancelled, "Paint was cancelled");
    return {};
}
core::Result<void> draw_layer(PaintDevice& device, const PaintLayerInfo& info,
                              std::span<const Panel> panels, std::span<const TextRun> texts,
                              std::span<const ImageRun> images, bool images_before_text, std::stop_token stop) {
    if (auto result = check_stop(stop); !result) return result;
    if (auto result = device.begin_layer(info); !result) return result;
    if (auto result = check_stop(stop); !result) return result;
    if (!panels.empty()) if (auto result = device.draw_panels(panels); !result) return result;
    if (auto result = check_stop(stop); !result) return result;
    if (images_before_text) {
        if (!images.empty()) if (auto result = device.draw_images(images); !result) return result;
        if (auto result = check_stop(stop); !result) return result;
        if (!texts.empty()) if (auto result = device.draw_text(texts); !result) return result;
    } else {
        if (!texts.empty()) if (auto result = device.draw_text(texts); !result) return result;
        if (auto result = check_stop(stop); !result) return result;
        if (!images.empty()) if (auto result = device.draw_images(images); !result) return result;
    }
    if (auto result = check_stop(stop); !result) return result;
    if (auto result = device.end_layer(); !result) return result;
    return check_stop(stop);
}
core::Result<void> draw_layer(PaintDevice& device, const CanvasLayer& layer, bool first_is_base, std::stop_token stop) {
    return draw_layer(device, {layer.clip, layer.backdrop, layer.shadow, layer.content_transform, first_is_base},
                      layer.panels, layer.texts, layer.images, layer.images_before_text, stop);
}
}

core::Result<void> FramePaintDevice::begin_frame(const PaintFrameInfo&) { return frame_only(); }
core::Result<void> FramePaintDevice::begin_scene(const PaintSceneInfo&) { return frame_only(); }
core::Result<void> FramePaintDevice::end_scene() { return frame_only(); }
core::Result<void> FramePaintDevice::begin_layer(const PaintLayerInfo&) { return frame_only(); }
core::Result<void> FramePaintDevice::draw_panels(std::span<const Panel>) { return frame_only(); }
core::Result<void> FramePaintDevice::draw_text(std::span<const TextRun>) { return frame_only(); }
core::Result<void> FramePaintDevice::draw_images(std::span<const ImageRun>) { return frame_only(); }
core::Result<void> FramePaintDevice::end_layer() { return frame_only(); }
core::Result<void> FramePaintDevice::present() { return frame_only(); }

Painter::Painter(std::shared_ptr<PaintDevice> device)
    : device_(std::move(device)), owner_(std::this_thread::get_id()) {}

core::Result<void> Painter::paint(const CanvasFrame& frame, double scale, std::stop_token stop) {
    if (std::this_thread::get_id() != owner_)
        return core::fail(core::ErrorCode::wrong_executor, "Painter requires its owning thread");
    if (active_) return core::fail(core::ErrorCode::busy, "Painter cannot paint reentrantly");
    if (!device_) return core::fail(core::ErrorCode::invalid_argument, "Painter requires a paint device");
    if (auto result = check_stop(stop); !result) return result;
    const auto plan = detail::valid_surface_scene_plan(frame);
    if (!plan) return core::fail(core::ErrorCode::invalid_argument, "Invalid paint scene");
    const auto width = std::ceil(double(frame.viewport.width) * scale);
    const auto height = std::ceil(double(frame.viewport.height) * scale);
    if (!std::isfinite(scale) || scale <= 0 || !std::isfinite(width) || !std::isfinite(height) ||
        width < 1 || height < 1 || width > 8192 || height > 8192 ||
        std::abs(double(frame.viewport.x) * scale) > 32767 ||
        std::abs(double(frame.viewport.y) * scale) > 32767)
        return core::fail(core::ErrorCode::invalid_argument, "Invalid physical paint extent or scale");
    Activity activity(active_);
    const auto capabilities = device_->capabilities();
    if ((frame.acceleration == Acceleration::require_hardware && !capabilities.hardware) ||
        (frame.renderer == Renderer::vulkan && capabilities.api != GraphicsApi::vulkan) ||
        (frame.renderer == Renderer::native && !capabilities.native_api))
        return core::fail(core::ErrorCode::unsupported, "Paint device does not meet the requested renderer or acceleration");
    if (plan->count && !capabilities.scene_sources)
        return core::fail(core::ErrorCode::unsupported, "Paint device does not support scene sources");
    const auto supported = [&](const CanvasLayer& layer) {
        return (!layer.backdrop || capabilities.backdrop) && (!layer.shadow || capabilities.shadow) &&
               (layer.content_transform.identity() || capabilities.content_transform);
    };
    for (const auto& layer : frame.overlays)
        if (!supported(layer)) return core::fail(core::ErrorCode::unsupported, "Paint device does not support a required layer effect");
    for (std::size_t pass = 0; pass < plan->count; ++pass)
        for (const auto& layer : frame.scenes[plan->order[pass]].layers)
            if (!supported(layer)) return core::fail(core::ErrorCode::unsupported, "Paint device does not support a required scene effect");
    if (auto result = check_stop(stop); !result) return result;
    Transaction transaction{*device_};
    try {
        const PaintFrameInfo info{frame.viewport, frame.background, scale,
                                  static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
        if (auto* direct = dynamic_cast<FramePaintDevice*>(device_.get())) {
            if (auto result = direct->paint_frame(frame, info, stop); !result) return result;
            transaction.committed = true;
            return {};
        }
        if (auto result = device_->begin_frame(info); !result) return result;
        if (auto result = check_stop(stop); !result) return result;
        for (std::size_t pass = 0; pass < plan->count; ++pass) {
            const auto index = plan->order[pass];
            const auto& scene = frame.scenes[index];
            if (auto result = device_->begin_scene({index, scene.viewport, scene.bounds, scene.first_is_base}); !result) return result;
            if (auto result = check_stop(stop); !result) return result;
            for (std::size_t i = 0; i < scene.layers.size(); ++i)
                if (auto result = draw_layer(*device_, scene.layers[i], scene.first_is_base && i == 0, stop); !result) return result;
            if (auto result = device_->end_scene(); !result) return result;
            if (auto result = check_stop(stop); !result) return result;
        }
        if (auto result = draw_layer(*device_, {}, frame.panels, frame.texts, frame.images, frame.images_before_text, stop); !result) return result;
        for (const auto& overlay : frame.overlays)
            if (auto result = draw_layer(*device_, overlay, false, stop); !result) return result;
        if (auto result = check_stop(stop); !result) return result;
        if (auto result = device_->present(); !result) return result;
        transaction.committed = true;
        return {};
    } catch (...) {
        return core::fail(core::ErrorCode::failed, "Paint device callback failed", std::current_exception());
    }
}
} // namespace ii::gui
