#include <iiGUI/Graphics.h>
#include <algorithm>
#include <cmath>

namespace ii::gui {
bool valid_canvas(const CanvasFrame& frame) noexcept {
    const auto color = [](Color c) {
        for (float v : {c.red, c.green, c.blue, c.alpha})
            if (!std::isfinite(v) || v < 0 || v > 1) return false;
        return true;
    };
    const auto rect = [](Rect r) {
        return std::isfinite(r.x) && std::isfinite(r.y) && std::abs(r.x) <= 32767 &&
            std::abs(r.y) <= 32767 && std::isfinite(r.width) && std::isfinite(r.height) &&
            r.width > 0 && r.height > 0 && r.width <= 8192 && r.height <= 8192;
    };
    if (!rect(frame.viewport) || !color(frame.background) || frame.background.alpha != 1 ||
        frame.panels.size() > 4096 ||
        (frame.acceleration != Acceleration::prefer_hardware && frame.acceleration != Acceleration::require_hardware))
        return false;
    for (const auto& panel : frame.panels)
        if (!rect(panel.bounds) || !color(panel.top) || !color(panel.bottom) ||
            !std::isfinite(panel.radius) || panel.radius < 0 ||
            panel.radius > std::min(panel.bounds.width, panel.bounds.height)*0.5f) return false;
    return true;
}
} // namespace ii::gui
