#pragma once
#include <iiGUI/Host.h>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

namespace ii::gui::detail {
inline std::optional<DisplayPixels> checked_display_pixels(std::uint64_t width,std::uint64_t height) noexcept {
    if (!width || !height || width>std::uint64_t(std::numeric_limits<int>::max()) ||
        height>std::uint64_t(std::numeric_limits<int>::max())) return {};
    return DisplayPixels{int(width),int(height)};
}
// Some OSes report physical modes in a fixed orientation. Match the current
// screen coordinate space without deriving pixel size from logical scaling.
inline std::optional<DisplayPixels> oriented_display_pixels(std::uint64_t width,std::uint64_t height,
                                                            double oriented_width,double oriented_height) noexcept {
    auto result=checked_display_pixels(width,height);
    if (!result || !std::isfinite(oriented_width) || !std::isfinite(oriented_height) ||
        oriented_width<=0 || oriented_height<=0) return {};
    if (result->width!=result->height && oriented_width!=oriented_height &&
        (result->width>result->height)!=(oriented_width>oriented_height))
        std::swap(result->width,result->height);
    return result;
}
}
