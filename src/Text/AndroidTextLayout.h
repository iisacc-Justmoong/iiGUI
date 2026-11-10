#pragma once

#include <iiGUI/Graphics.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <stdexcept>

namespace ii::gui::detail {

// The OS owns shaping and bidi. These values describe its physical line
// extents; iiGUI owns the final alignment and optional fixed line height.
struct AndroidTextLine final { float left, width, top, bottom; };
struct AndroidTextMetrics final { float width, height; };
struct AndroidTextPlacement final { float x, y, clip_top, clip_bottom; };

inline void validate_android_text_line(const AndroidTextLine& line) {
    if (!std::isfinite(line.left) || !std::isfinite(line.width) || !std::isfinite(line.top) ||
        !std::isfinite(line.bottom) || line.width < 0 || line.top < 0 || line.bottom < line.top)
        throw std::invalid_argument("Invalid Android shaped line geometry");
}

inline AndroidTextMetrics android_text_metrics(std::span<const AndroidTextLine> lines,
                                               float natural_height, float fixed_height) {
    if (lines.empty() || lines.size() > 65537 || !std::isfinite(natural_height) || natural_height < 0 ||
        !std::isfinite(fixed_height) || fixed_height < 0)
        throw std::invalid_argument("Invalid Android text layout metrics");
    float width = 0, previous_bottom = 0;
    for (const auto& line : lines) {
        validate_android_text_line(line);
        if (line.top < previous_bottom || line.bottom > natural_height)
            throw std::invalid_argument("Invalid Android text line ordering");
        width = std::max(width, line.width);
        previous_bottom = line.bottom;
    }
    const float height = fixed_height > 0 ? fixed_height * float(lines.size()) : natural_height;
    if (!std::isfinite(height)) throw std::invalid_argument("Android text height exceeds its budget");
    return {width, height};
}

inline AndroidTextPlacement android_text_placement(Rect bounds, TextAlignment alignment,
        const AndroidTextLine& line, std::size_t index, float fixed_height) {
    validate_android_text_line(line);
    if (!std::isfinite(bounds.x) || !std::isfinite(bounds.y) || !std::isfinite(bounds.width) ||
        bounds.width < 0 || !std::isfinite(fixed_height) || fixed_height < 0 || index > 65536)
        throw std::invalid_argument("Invalid Android text placement");
    const float offset = alignment == TextAlignment::center ? (bounds.width - line.width) / 2 :
                         alignment == TextAlignment::right ? bounds.width - line.width : 0;
    const float clip_top = bounds.y + (fixed_height > 0 ? float(index) * fixed_height : line.top);
    const float clip_bottom = fixed_height > 0 ? clip_top + fixed_height : bounds.y + line.bottom;
    const float top = fixed_height > 0 ? clip_top + (fixed_height - (line.bottom - line.top)) / 2 : clip_top;
    if (!std::isfinite(clip_top) || !std::isfinite(clip_bottom) || !std::isfinite(top))
        throw std::invalid_argument("Android text placement exceeds its budget");
    return {bounds.x + offset - line.left, top - line.top, clip_top, clip_bottom};
}

} // namespace ii::gui::detail
