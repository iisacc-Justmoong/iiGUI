#pragma once

#include <iiGUI/Host.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <climits>
#include <optional>
#include <stdexcept>

namespace ii::gui::detail {
// Bound conversion to desktop window-system integer coordinates, including
// scaled Win32 client rectangles. These are content units, not file formats.
inline bool valid_size(Size size) {
    return std::isfinite(size.width) && std::isfinite(size.height) &&
           size.width > 0 && size.height > 0 &&
           size.width <= 32767 && size.height <= 32767;
}
inline bool valid_title(std::string_view title) {
    return core::valid_utf8(title) && title.find('\0') == std::string_view::npos;
}
inline HostOptions validate_options(HostOptions options) {
    if(unsigned(options.renderer)>unsigned(Renderer::vulkan))throw std::invalid_argument("Invalid widget renderer selection");
    if(options.frame_loop && options.event_loop!=&options.frame_loop->events())
        throw std::invalid_argument("Display frame loop must borrow the Host's event loop");
#if !defined(__APPLE__)

#endif
    if (!valid_size(options.size) || !valid_size(options.minimum_size) ||
        options.size.width < options.minimum_size.width ||
        options.size.height < options.minimum_size.height)
        throw std::invalid_argument("iiGUI sizes must be finite, in (0,32767], and initial size >= minimum");
    if (!valid_title(options.title))
        throw std::invalid_argument("iiGUI title must be UTF-8 without embedded NUL");
    if (!options.task_capacity || !options.budget.max_callbacks ||
        options.budget.max_time <= runtime::Clock::duration::zero())
        throw std::invalid_argument("iiGUI requires nonzero queue capacity and dispatch budget");
    return options;
}
inline bool valid_content_size(Size size, const HostOptions& options) {
    return valid_size(size) && size.width >= options.minimum_size.width &&
           size.height >= options.minimum_size.height;
}
// Round up, so a native timeout cannot spin just before a Core deadline.
inline int deadline_timeout(const std::optional<runtime::Clock::time_point>& next) {
    if (!next) return -1;
    const auto remaining = *next-runtime::Clock::now();
    if (remaining <= runtime::Clock::duration::zero()) return 0;
    const auto ms = std::chrono::ceil<std::chrono::milliseconds>(remaining).count();
    return static_cast<int>(std::min<decltype(ms)>(ms, INT_MAX));
}
} // namespace ii::gui::detail
