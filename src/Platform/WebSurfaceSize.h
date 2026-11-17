#pragma once
#include <iiGUI/Host.h>
#include <cmath>

namespace ii::gui::detail {
enum class WebSurfaceSize { drawable, suspended, invalid };

// A CSS-hidden canvas has zero area; retain the last drawable viewport until
// it becomes visible again. Invalid or over-budget dimensions remain errors.
inline WebSurfaceSize classify_web_surface_size(Size size, double scale) noexcept {
    if (!std::isfinite(size.width) || !std::isfinite(size.height) ||
        size.width < 0 || size.height < 0 || size.width > 32767 || size.height > 32767 ||
        !std::isfinite(scale) || scale <= 0 || scale > 8)
        return WebSurfaceSize::invalid;
    if (size.width == 0 || size.height == 0) return WebSurfaceSize::suspended;
    return WebSurfaceSize::drawable;
}

struct WebSurfaceChange final {
    WebSurfaceSize size;
    bool changed = false;
    bool resumed = false;
};

class WebSurfaceVisibility final {
    bool suspended_ = false;
public:
    [[nodiscard]] WebSurfaceChange update(Size size, double scale, Size previous, double previous_scale) noexcept {
        const auto status = classify_web_surface_size(size, scale);
        if (status == WebSurfaceSize::invalid) return {status};
        const bool resumed = suspended_ && status == WebSurfaceSize::drawable;
        suspended_ = status == WebSurfaceSize::suspended;
        return {status, !suspended_ && (size != previous || scale != previous_scale), resumed};
    }
    [[nodiscard]] bool visible(bool document_hidden) const noexcept { return !document_hidden && !suspended_; }
};
}
