#pragma once

#include <iiGUI/ButtonModel.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>

namespace ii::gui {

struct ListBarPalette final {
    WidgetColor background{};
    WidgetColor hover{};
    WidgetColor pressed{};
    WidgetColor focus{};
    friend bool operator==(const ListBarPalette&, const ListBarPalette&) = default;
};
struct ListBarOptions final {
    std::array<std::string, 3> icons;
    double icon_size = 18, button_size = 22, horizontal_padding = 8, vertical_padding = 4, spacing = 4;
    double width = 0, height = 0, corner_radius = 8;
    bool enabled = true, interactive = true;
    ListBarPalette palette;
    friend bool operator==(const ListBarOptions&, const ListBarOptions&) = default;
};
struct ListBarRect final { double x = 0, y = 0, width = 0, height = 0; };
struct ListBarGeometry final {
    double width = 0, height = 0;
    std::array<ListBarRect, 3> slots, icons;
};

// A list toolbar owns exactly three independent action slots. Its visual
// geometry is pure and does not depend on any windowing or legacy toolkit.
struct DefaultListToolbarModelPolicy final {
    using Configuration=ListBarOptions;using Geometry=ListBarGeometry;using Color=WidgetColor;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return DefaultButtonPolicy::valid_text(value);}
};
template<class Policy>
class BasicListToolbar final {
    using ListBarOptions=typename Policy::Configuration;
    using ListBarGeometry=typename Policy::Geometry;
    using WidgetColor=typename Policy::Color;
public:
    [[nodiscard]] static bool validate(const ListBarOptions& value) noexcept {
        const auto bound = [](double n, double low, double high) { return std::isfinite(n) && n >= low && n <= high; };
        const auto color = [&](WidgetColor c) { return bound(c.red, 0, 1) && bound(c.green, 0, 1) && bound(c.blue, 0, 1) && bound(c.alpha, 0, 1); };
        for (const auto& icon : value.icons) if (icon.size() > 256 || !Policy::valid_text(icon)) return false;
        const auto& p = value.palette;
        return bound(value.icon_size, 1, 256) && bound(value.button_size, 1, 8192) &&
            bound(value.horizontal_padding, 0, 8192) && bound(value.vertical_padding, 0, 8192) &&
            bound(value.spacing, 0, 8192) && bound(value.width, 0, 8192) && bound(value.height, 0, 8192) &&
            bound(value.corner_radius, 0, 8192) &&
            3 * value.button_size + 2 * (value.horizontal_padding + value.spacing) <= 8192 &&
            value.button_size + 2 * value.vertical_padding <= 8192 &&
            color(p.background) && color(p.hover) && color(p.pressed) && color(p.focus);
    }
    [[nodiscard]] static std::optional<ListBarGeometry> geometry(const ListBarOptions& value) noexcept {
        if (!validate(value)) return std::nullopt;
        ListBarGeometry result;
        result.width = value.width ? value.width : 3 * value.button_size + 2 * (value.horizontal_padding + value.spacing);
        result.height = value.height ? value.height : value.button_size + 2 * value.vertical_padding;
        const auto horizontal = (std::min)(value.horizontal_padding, result.width / 2);
        const auto available = (std::max)(0., result.width - 2 * horizontal);
        const auto gap = (std::min)(value.spacing, available / 2);
        const auto side = (std::min)({value.button_size, (available - 2 * gap) / 3,
                                    (std::max)(0., result.height - 2 * (std::min)(value.vertical_padding, result.height / 2))});
        for (std::size_t i = 0; i < 3; ++i) {
            const auto x = horizontal + i * (side + gap), y = (result.height - side) / 2;
            result.slots[i] = {x, y, side, side};
            const auto icon = (std::min)(side, value.icon_size);
            result.icons[i] = {(side - icon) / 2, (side - icon) / 2, icon, icon};
        }
        return result;
    }
};

using ListToolbarModel=BasicListToolbar<DefaultListToolbarModelPolicy>;
} // namespace ii::gui
