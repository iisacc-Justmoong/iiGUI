#include <iiGUI/Graphics.h>
#include <algorithm>
#include <cmath>
#include <iiSDKCore/Core/Text/Utf8.h>

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
        (frame.renderer!=Renderer::automatic && frame.renderer!=Renderer::native && frame.renderer!=Renderer::vulkan) ||
        (frame.acceleration != Acceleration::prefer_hardware && frame.acceleration != Acceleration::require_hardware))
        return false;
    for (const auto& panel : frame.panels)
        if (!rect(panel.bounds) || !color(panel.top) || !color(panel.bottom) ||
            !std::isfinite(panel.radius) || panel.radius < 0 ||
            panel.radius > std::min(panel.bounds.width, panel.bounds.height)*0.5f) return false;
    if (frame.texts.size()>4096) return false;
    std::size_t bytes=0;
    for (const auto& text:frame.texts) {
        bytes+=text.text.size();
        if (!rect(text.bounds) || !color(text.color) || !std::isfinite(text.font_size) ||
            text.font_size<8 || text.font_size>256 || (text.font_weight!=400 && text.font_weight!=700) ||
            (text.alignment!=TextAlignment::left && text.alignment!=TextAlignment::center && text.alignment!=TextAlignment::right) ||
            text.text.size()>65536 || bytes>1024*1024 || text.text.find('\0')!=text.text.npos ||
            !core::valid_utf8(text.text)) return false;
    }
    return true;
}
} // namespace ii::gui
