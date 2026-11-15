#pragma once
#include <iiGUI/ButtonModel.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
namespace ii::gui {
enum class AlertShape { rounded, cylinder };
struct AlertPalette final {
    WidgetColor frame{},backdrop{},border{},title{},description{};
    friend bool operator==(const AlertPalette&,const AlertPalette&)=default;
};
struct AlertLayoutMetrics final {
    double action_horizontal_inset=24,action_gap=14,action_height=56,tertiary_action_height=44;
    double three_top_spacing=18,three_bottom_spacing=18,action_vertical_gap=12,two_top_spacing=28,bottom_spacing=32;
    double rounded_radius_fraction=.4;
    friend bool operator==(const AlertLayoutMetrics&,const AlertLayoutMetrics&)=default;
};

struct AlertConfiguration final {
    std::string title,description,message,accessibility_name="Alert";
    std::array<std::string,3> action_text;
    std::array<bool,3> action_enabled{true,true,true};
    unsigned action_count=0;
    std::optional<bool> secondary_destructive;
    std::optional<WidgetColor> primary_color;
    bool show_icon=true,dismiss_on_background=false,dismiss_on_escape=false;
    bool motion_enabled=true,glass_enabled=false,shadow_enabled=false;
    double minimum_width=280,maximum_width=500,preferred_width=500,side_padding=24;
    double content_top=46,content_bottom=36,text_padding=24,icon_spacing=28,text_spacing=14;
    double icon_extent=86,corner_radius=36,border_width=1,blur_radius=28;
    double enter_duration_ms=420,exit_duration_ms=150,overshoot=1.45,initial_scale=.92;
    AlertShape shape=AlertShape::rounded;AlertPalette palette;AlertLayoutMetrics metrics;
    std::string fallback_action;
    friend bool operator==(const AlertConfiguration&,const AlertConfiguration&)=default;
};
struct AlertRect final {
    double x=0,y=0,width=0,height=0;
    friend bool operator==(const AlertRect&,const AlertRect&)=default;
};
struct AlertAction final {
    std::string_view text;bool visible=false,enabled=false,destructive=false;
    ButtonTone tone=ButtonTone::normal;
};
struct AlertLayout final {AlertRect frame,icon,title,description;std::array<AlertRect,3> actions;double radius=0;};
struct DefaultAlertModelPolicy final {
    using Configuration=AlertConfiguration;using Shape=AlertShape;using Rect=AlertRect;
    using Action=AlertAction;using Layout=AlertLayout;using Tone=ButtonTone;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{
        return value.size()<=65536&&value.find('\0')==std::string_view::npos&&ii::core::valid_utf8(value);
    }
    [[nodiscard]] static bool valid_color(WidgetColor value)noexcept{
        const auto valid=[](double n){return std::isfinite(n)&&n>=0&&n<=1;};
        return valid(value.red)&&valid(value.green)&&valid(value.blue)&&valid(value.alpha);
    }
};
// Values, theme and metrics are supplied by the application. This pure model
// computes alert validation, action policy and local geometry only on request.
template<class Policy>
class BasicAlert final {
    using AlertConfiguration=typename Policy::Configuration;using AlertShape=typename Policy::Shape;
    using AlertRect=typename Policy::Rect;using AlertAction=typename Policy::Action;using AlertLayout=typename Policy::Layout;
    using ButtonTone=typename Policy::Tone;
public:
    [[nodiscard]] static bool validate(const AlertConfiguration& value) noexcept {
        const auto range = [](double n, double low, double high) {
            return std::isfinite(n) && n >= low && n <= high;
        };
        if (!Policy::valid_text(value.title) || !Policy::valid_text(value.description) ||
            !Policy::valid_text(value.message) || !Policy::valid_text(value.accessibility_name)) return false;
        for (const auto& text : value.action_text) if (!Policy::valid_text(text)) return false;
        const auto& m=value.metrics;
        if(!Policy::valid_text(value.fallback_action)||!range(m.action_horizontal_inset,0,8192)||!range(m.action_gap,0,8192)||
           !range(m.action_height,1,8192)||!range(m.tertiary_action_height,1,8192)||!range(m.three_top_spacing,0,8192)||
           !range(m.three_bottom_spacing,0,8192)||!range(m.action_vertical_gap,0,8192)||!range(m.two_top_spacing,0,8192)||
           !range(m.bottom_spacing,0,8192)||!range(m.rounded_radius_fraction,0,.5))return false;
        if (value.action_count > 3 || !range(value.minimum_width, 0, 8192) ||
            !range(value.maximum_width, 1, 8192) || value.minimum_width > value.maximum_width ||
            !range(value.preferred_width, 1, 8192) || !range(value.side_padding, 0, 8192) ||
            !range(value.content_top, 0, 8192) || !range(value.content_bottom, 0, 8192) ||
            !range(value.text_padding, 0, 8192) || !range(value.icon_spacing, 0, 8192) ||
            !range(value.text_spacing, 0, 8192) || !range(value.icon_extent, 1, 256) ||
            !range(value.corner_radius, 0, 8192) || !range(value.border_width, 0, 8192) ||
            !range(value.blur_radius, 0, 64) || !range(value.enter_duration_ms, 0, 86400000) ||
            !range(value.exit_duration_ms, 0, 86400000) || !range(value.overshoot, 0, 4) ||
            !range(value.initial_scale, .5, 1)) return false;
        if (value.shape != AlertShape::rounded && value.shape != AlertShape::cylinder) return false;
        return (!value.primary_color || Policy::valid_color(*value.primary_color)) &&
               Policy::valid_color(value.palette.frame) && Policy::valid_color(value.palette.backdrop) &&
               Policy::valid_color(value.palette.border) && Policy::valid_color(value.palette.title) &&
               Policy::valid_color(value.palette.description);
    }

    bool set_configuration(AlertConfiguration value) {
        if (!validate(value)) return false;
        configuration_ = std::move(value);
        return true;
    }

    [[nodiscard]] const AlertConfiguration& configuration() const noexcept { return configuration_; }
    [[nodiscard]] std::string_view description() const noexcept {
        return configuration_.description.empty() ? configuration_.message : configuration_.description;
    }

    [[nodiscard]] unsigned action_count() const noexcept {
        if (configuration_.action_count) return configuration_.action_count;
        for (unsigned index = 3; index > 1; --index) {
            if (!configuration_.action_text[index - 1].empty()) return index;
        }
        return 1;
    }

    [[nodiscard]] AlertAction action(unsigned index) const noexcept {
        if (index >= action_count()) return {};
        const auto& text = configuration_.action_text[index];
        const bool destructive = index == 1 && configuration_.secondary_destructive.value_or(action_count() == 3);
        const auto tone = index == 0 ? ButtonTone::primary : ButtonTone::normal;
        return {text.empty() ? std::string_view{configuration_.fallback_action} : std::string_view{text},
                true, configuration_.action_enabled[index], destructive, tone};
    }

    [[nodiscard]] std::optional<AlertLayout> layout(double viewport_width, double viewport_height,
                                                     double title_height, double description_height) const noexcept {
        const auto extent = [](double n) { return std::isfinite(n) && n >= 0 && n <= 8192; };
        if (!extent(viewport_width) || !extent(viewport_height) || !extent(title_height) || !extent(description_height)) {
            return std::nullopt;
        }
        const auto& c = configuration_;
        const auto width = (std::max)(0., (std::min)({viewport_width - 2 * c.side_padding, c.maximum_width,
                                                  (std::max)(c.minimum_width, c.preferred_width)}));
        auto result = layout_for_width(width, title_height, description_height);
        if (!result) return std::nullopt;
        result->frame.x = (viewport_width - width) / 2;
        result->frame.y = (viewport_height - result->frame.height) / 2;
        return result;
    }

    // Use an already allocated popup width without assuming its host viewport.
    [[nodiscard]] std::optional<AlertLayout> layout_for_width(double width, double title_height,
                                                               double description_height) const noexcept {
        const auto extent = [](double n) { return std::isfinite(n) && n >= 0 && n <= 8192; };
        if (!extent(width) || !extent(title_height) || !extent(description_height)) return std::nullopt;
        const auto& c = configuration_;
        const auto text_width = (std::max)(0., width - 2 * c.text_padding);
        AlertLayout result;
        auto y = c.content_top;
        if (c.show_icon) {
            const auto icon_width = (std::min)(width, c.icon_extent);
            result.icon = {(width - icon_width) / 2, y, icon_width, c.icon_extent};
            y += c.icon_extent;
            if (!c.title.empty() || !description().empty()) y += c.icon_spacing;
        }
        if (!c.title.empty()) {
            result.title = {(std::min)(c.text_padding, width / 2), y, text_width, title_height};
            y += title_height;
        }
        if (!description().empty()) {
            if (!c.title.empty()) y += c.text_spacing;
            result.description = {(std::min)(c.text_padding, width / 2), y, text_width, description_height};
            y += description_height;
        }
        y += c.content_bottom;
        const auto& m=c.metrics;
        const auto action_width = (std::max)(0., width - 2*m.action_horizontal_inset);
        const auto action_x = (std::min)(m.action_horizontal_inset, width / 2);
        if (action_count() == 3) {
            y += m.three_top_spacing;
            result.actions[0] = {action_x, y, action_width, m.action_height}; y += m.action_height+m.action_vertical_gap;
            result.actions[1] = {action_x, y, action_width, m.action_height}; y += m.action_height+m.action_vertical_gap;
            result.actions[2] = {action_x, y, action_width, m.tertiary_action_height}; y += m.tertiary_action_height+m.three_bottom_spacing;
        } else {
            y += m.two_top_spacing;
            if (action_count() == 2) {
                const auto gap = (std::min)(m.action_gap, action_width);
                const auto half = (action_width - gap) / 2;
                result.actions[1] = {action_x, y, half, m.action_height};
                result.actions[0] = {action_x + half + gap, y, half, m.action_height};
            } else {
                result.actions[0] = {action_x, y, action_width, m.action_height};
            }
            y += m.action_height+m.bottom_spacing;
        }
        y += c.border_width;
        if (!extent(width) || !extent(y)) return std::nullopt;
        result.frame = {0, 0, width, y};
        const auto short_edge = (std::min)(width, y);
        result.radius = c.shape == AlertShape::cylinder ? short_edge / 2 : (std::min)(c.corner_radius, short_edge * m.rounded_radius_fraction);
        return result;
    }

private:
    AlertConfiguration configuration_;
};

using AlertModel=BasicAlert<DefaultAlertModelPolicy>;
} // namespace ii::gui
