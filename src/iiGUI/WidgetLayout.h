#pragma once
#include "Host.h"
#include <cmath>
#include <initializer_list>
#include <optional>

namespace ii::gui {
enum class StackAlignment { center, leading, trailing, stretch };
struct StackAlignmentAxes final {
    StackAlignment horizontal = StackAlignment::center, vertical = StackAlignment::center;
    friend bool operator==(const StackAlignmentAxes&, const StackAlignmentAxes&) = default;
};
struct Insets final {
    double left=0, top=0, right=0, bottom=0;
    friend bool operator==(const Insets&, const Insets&) = default;
};
struct StackOptions final {
    double spacing = 12, padding = 0;
    StackAlignment alignment = StackAlignment::center;
    std::optional<Insets> insets;
    double minimum_width=0, minimum_height=0;
    // Implicit metrics may differ from actual content insets (native toolbars).
    std::optional<Insets> implicit_insets;
    bool intrinsic_width=false;
    bool measure_hidden_children=false;
    // Linear stacks use their cross axis. Overlay stacks use both axes.
    std::optional<StackAlignment> horizontal_alignment, vertical_alignment;
    friend bool operator==(const StackOptions&, const StackOptions&) = default;
};
struct StackLayoutOptions final {
    StackOptions stack;
    Size extent;
    double flex = 0;
    friend bool operator==(const StackLayoutOptions&, const StackLayoutOptions&) = default;
};
inline StackAlignment horizontal_alignment(const StackOptions& value) noexcept {
    return value.horizontal_alignment.value_or(value.alignment);
}
inline StackAlignment vertical_alignment(const StackOptions& value) noexcept {
    return value.vertical_alignment.value_or(value.alignment);
}
inline bool valid_stack_options(const StackOptions& value) noexcept {
    const auto bounded = [](double n, double high) { return std::isfinite(n) && n >= 0 && n <= high; };
    if (!bounded(value.spacing,32767) || !bounded(value.padding,32767) || unsigned(value.alignment)>3 ||
        !bounded(value.minimum_width,8192) || !bounded(value.minimum_height,8192)) return false;
    for (auto alignment : {value.horizontal_alignment, value.vertical_alignment})
        if (alignment && unsigned(*alignment)>3) return false;
    for (auto insets : {value.insets, value.implicit_insets}) if (insets)
        for (auto edge : {insets->left,insets->top,insets->right,insets->bottom})
            if (!bounded(edge,8192)) return false;
    return true;
}
inline bool valid_stack_layout(const StackLayoutOptions& value) noexcept {
    const auto bounded = [](double n, double high) { return std::isfinite(n) && n >= 0 && n <= high; };
    return valid_stack_options(value.stack) && bounded(value.extent.width,32767) &&
           bounded(value.extent.height,32767) && bounded(value.flex,4096);
}
}
