#pragma once

#include <iiGUI/ButtonModel.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace ii::gui {

using ToolbarButtonId = std::variant<std::int64_t, std::string>;
struct ToolbarButtonPalette final {
    WidgetColor text{1, 1, 1, .8}, disabled_text{1, 1, 1, .3};
    WidgetColor hover{};
    WidgetColor pressed{};
    WidgetColor primary{};
    WidgetColor danger{};
    WidgetColor focus{};
    friend bool operator==(const ToolbarButtonPalette&, const ToolbarButtonPalette&) = default;
};
struct ToolbarButtonConfiguration final {
    ToolbarButtonId id{std::int64_t(-1)};
    std::string text, accessibility_name;
    ButtonTone tone = ButtonTone::borderless;
    bool enabled = true, visible = true, has_icon = false;
    double width = 0, height = 22, icon_size = 18, left_padding = 2, right_padding = 2;
    double spacing = 4, corner_radius = 8, font_size = 12;
    ToolbarButtonPalette palette;
    friend bool operator==(const ToolbarButtonConfiguration&, const ToolbarButtonConfiguration&) = default;
};
struct ToolbarButtonRect final { double x = 0, y = 0, width = 0, height = 0; };
struct ToolbarButtonGeometry final { ToolbarButtonRect icon, text; double radius = 0; };
struct ToolbarButtonVisual final { WidgetColor background, text, focus; bool enabled = false, focused = false; };

// Policies supply value contracts and validation; iiGUI owns the calculations.
struct DefaultToolbarButtonModelPolicy final {
    using Configuration=ToolbarButtonConfiguration;using Geometry=ToolbarButtonGeometry;using Visual=ToolbarButtonVisual;
    using Interaction=ButtonInteraction;using Tone=ButtonTone;using Color=WidgetColor;
    [[nodiscard]] static bool valid_text(std::string_view value) noexcept {return DefaultButtonPolicy::valid_text(value);}
};
template<class Policy>
class BasicToolbarButton final {
    using ToolbarButtonConfiguration=typename Policy::Configuration;
    using ToolbarButtonGeometry=typename Policy::Geometry;
    using ToolbarButtonVisual=typename Policy::Visual;
    using ButtonInteraction=typename Policy::Interaction;
    using ButtonTone=typename Policy::Tone;
    using WidgetColor=typename Policy::Color;
public:
    [[nodiscard]] static bool validate(const ToolbarButtonConfiguration& value) noexcept {
        const auto bounded = [](double n, double low, double high) { return std::isfinite(n) && n >= low && n <= high; };
        const auto color = [&](WidgetColor c) { return bounded(c.red, 0, 1) && bounded(c.green, 0, 1) && bounded(c.blue, 0, 1) && bounded(c.alpha, 0, 1); };
        if (const auto* id = std::get_if<std::string>(&value.id); id && (id->empty() || id->size() > 1024 || !Policy::valid_text(*id))) return false;
        if (!Policy::valid_text(value.text) || !Policy::valid_text(value.accessibility_name) ||
            !bounded(value.width, 0, 8192) || !bounded(value.height, 1, 8192) || !bounded(value.icon_size, 1, 256) ||
            !bounded(value.left_padding, 0, 8192) || !bounded(value.right_padding, 0, 8192) ||
            !bounded(value.spacing, 0, 8192) || !bounded(value.corner_radius, 0, 8192) || !bounded(value.font_size, 1, 256) ||
            value.left_padding + value.right_padding + (value.has_icon ? value.icon_size : 0) +
                (value.has_icon && !value.text.empty() ? value.spacing : 0) > 8192) return false;
        switch (value.tone) { case ButtonTone::primary: case ButtonTone::normal: case ButtonTone::borderless:
            case ButtonTone::destructive: case ButtonTone::disabled: break; default: return false; }
        const auto& p = value.palette;
        return color(p.text) && color(p.disabled_text) && color(p.hover) && color(p.pressed) && color(p.primary) && color(p.danger) && color(p.focus);
    }
    [[nodiscard]] static std::optional<double> natural_width(const ToolbarButtonConfiguration& value, double text_width) noexcept {
        if (!validate(value) || !std::isfinite(text_width) || text_width < 0 || text_width > 8192) return std::nullopt;
        if (value.width) return value.width;
        const auto content = (value.has_icon ? value.icon_size : 0) + (value.text.empty() ? 0 : text_width) +
            (value.has_icon && !value.text.empty() ? value.spacing : 0);
        return (std::min)(8192., (std::max)(value.height, value.left_padding + content + value.right_padding));
    }
    [[nodiscard]] static std::optional<ToolbarButtonGeometry> geometry(
            const ToolbarButtonConfiguration& value, double width, double height, double measured_text) noexcept {
        if (!validate(value) || !std::isfinite(width) || width < 0 || width > 8192 || !std::isfinite(height) || height < 0 || height > 8192 ||
            !std::isfinite(measured_text) || measured_text < 0 || measured_text > 8192) return std::nullopt;
        ToolbarButtonGeometry result; result.radius = (std::min)({value.corner_radius, width / 2, height / 2});
        const auto left = (std::min)(value.left_padding, width), right = (std::min)(value.right_padding, width - left);
        const auto available = (std::max)(0., width - left - right);
        const auto icon = value.has_icon ? (std::min)({value.icon_size, available, height}) : 0.;
        const auto gap = icon > 0 && !value.text.empty() && measured_text > 0 ? (std::min)(value.spacing, (std::max)(0., available - icon)) : 0.;
        const auto text_width = value.text.empty() ? 0. : (std::min)(measured_text, (std::max)(0., available - icon - gap));
        const auto offset = left + (std::max)(0., (available - icon - gap - text_width) / 2);
        result.icon = {offset, (height - icon) / 2, icon, icon};
        result.text = {offset + icon + gap, 0, text_width, height}; return result;
    }
    [[nodiscard]] static ToolbarButtonVisual visual(const ToolbarButtonConfiguration& value, ButtonInteraction input) noexcept {
        const auto& p = value.palette; const bool enabled = value.enabled && input.enabled && value.tone != ButtonTone::disabled;
        ToolbarButtonVisual result{{0, 0, 0, 0}, enabled ? p.text : p.disabled_text, p.focus, enabled, enabled && input.focused};
        if (!enabled) return result;
        if (value.tone == ButtonTone::primary) result.background = p.primary;
        else if (value.tone == ButtonTone::destructive) result.background = p.danger;
        else if (input.pressed) result.background = p.pressed;
        else if (input.hovered || value.tone == ButtonTone::normal) result.background = p.hover;
        return result;
    }
};

using ToolbarButtonModel=BasicToolbarButton<DefaultToolbarButtonModelPolicy>;
} // namespace ii::gui
