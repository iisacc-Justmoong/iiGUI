#pragma once
#include "NativeCanvas.h"
#include <iiSDKCore/Core/Result/Result.h>
#include <optional>

namespace ii::gui::detail {
// The three hosts share the same validation, admission and coalescing contract.
// Native resources remain owned and destroyed on the host thread.
struct CanvasState final {
    std::unique_ptr<NativeCanvas> native;
    std::optional<CanvasFrame> frame;
    std::uint64_t requests = 0;
    bool dirty = false;
    double scale = 0, parent_height = 0;

    core::Result<void> set(CanvasFrame value, void* parent, void* display, double backing_scale) {
        if (!valid_canvas(value)) return core::fail(core::ErrorCode::invalid_argument, "Invalid GPU canvas geometry, color or acceleration policy");
        try {
            (void)canvas_pixels(value, backing_scale);
            if (!native || frame->acceleration != value.acceleration) {
                auto replacement = std::make_unique<NativeCanvas>(parent, display, value.acceleration);
                native = std::move(replacement);
                dirty = true;
            }
            ++requests;
            if (!frame || *frame != value) { frame = std::move(value); dirty = true; }
            return {};
        } catch (const std::invalid_argument& error) {
            return core::fail(core::ErrorCode::out_of_range, error.what());
        } catch (const std::exception& error) {
            return core::fail(core::ErrorCode::unsupported, error.what());
        }
    }
    void flush(double backing_scale, double height) {
        if (!native || !frame) return;
        if (dirty || scale != backing_scale || parent_height != height) {
            native->draw(*frame, backing_scale, height);
            dirty = false;
            scale = backing_scale; parent_height = height;
        }
    }
    void clear() { native.reset(); frame.reset(); dirty = false; scale = parent_height = 0; }
    GraphicsInfo info() const {
        auto result = native ? native->info() : GraphicsInfo{};
        result.frame_requests = requests;
        return result;
    }
    core::Result<PixelBuffer> capture(double backing_scale, double height) {
        if (!native) return core::fail(core::ErrorCode::not_found, "No GPU canvas has been attached");
        try { flush(backing_scale, height); return native->capture(); }
        catch (const std::exception& error) { return core::fail(core::ErrorCode::failed, error.what()); }
    }
};
} // namespace ii::gui::detail
