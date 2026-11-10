#pragma once
#include <iiGUI/ButtonModel.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace ii::gui {
struct AlertButtonOptions final {
    std::string text="Button",accessibility_name;
    ButtonTone tone=ButtonTone::normal;
    bool enabled=true,dialog_style=false;
    double height=20;
    std::optional<double> width,horizontal_padding,corner_radius;
    std::optional<WidgetColor> text_color,primary_color;
    ButtonPalette palette;
    WidgetColor dialog_text{1,1,1,1},dialog_border{0,0,0,1};
    WidgetColor disabled_background{},normal_pressed{},primary_reference{};
    WidgetColor dialog_background{0,0,0,0},dialog_hover{1,1,1,.05},dialog_pressed{1,1,1,.09};
    double compact_padding=8,dialog_padding=16,compact_radius=4,dialog_radius=16,border_width=1;
    double hover_factor=1.12,pressed_factor=1.2,normal_hover_factor=1.08;
    bool motion_enabled=true,enter_key_activation=false;
    friend bool operator==(const AlertButtonOptions&,const AlertButtonOptions&)=default;
};
struct AlertButtonAppearance final {
    ButtonConfiguration button;
    std::optional<WidgetColor> border;
    double border_width=0;
};
struct DefaultAlertButtonModelPolicy final {
    using Options=AlertButtonOptions;using Appearance=AlertButtonAppearance;using Tone=ButtonTone;
    using Color=WidgetColor;using Button=ButtonModel;
    [[nodiscard]] static bool valid_color(Color c)noexcept {
        const auto v=[](double n){return std::isfinite(n)&&n>=0&&n<=1;};
        return v(c.red)&&v(c.green)&&v(c.blue)&&v(c.alpha);
    }
    [[nodiscard]] static bool valid_extra(const Options& o)noexcept {
        const auto bounded=[](double n){return std::isfinite(n)&&n>=0&&n<=8192;};
        const auto factor=[](double n){return std::isfinite(n)&&n>0&&n<=256;};
        return bounded(o.compact_padding)&&bounded(o.dialog_padding)&&bounded(o.compact_radius)&&bounded(o.dialog_radius)&&
            bounded(o.border_width)&&factor(o.hover_factor)&&factor(o.pressed_factor)&&factor(o.normal_hover_factor);
    }
    [[nodiscard]] static Color primary_reference(const Options& o)noexcept{return o.primary_reference;}
    [[nodiscard]] static Color dialog_background(const Options& o)noexcept{return o.dialog_background;}
    [[nodiscard]] static Color dialog_hover(const Options& o)noexcept{return o.dialog_hover;}
    [[nodiscard]] static Color dialog_pressed(const Options& o)noexcept{return o.dialog_pressed;}
    [[nodiscard]] static double padding(const Options& o)noexcept{return o.dialog_style?o.dialog_padding:o.compact_padding;}
    [[nodiscard]] static double radius(const Options& o)noexcept{return o.dialog_style?o.dialog_radius:o.compact_radius;}
    [[nodiscard]] static double border_width(const Options& o)noexcept{return o.border_width;}
    [[nodiscard]] static double hover_factor(const Options& o)noexcept{return o.hover_factor;}
    [[nodiscard]] static double pressed_factor(const Options& o)noexcept{return o.pressed_factor;}
    [[nodiscard]] static double normal_hover_factor(const Options& o)noexcept{return o.normal_hover_factor;}
};
template<class Policy>
class BasicAlertButton final {
    using AlertButtonConfiguration=typename Policy::Options;
    using AlertButtonAppearance=typename Policy::Appearance;
    using ButtonTone=typename Policy::Tone;
    using StyleColor=typename Policy::Color;
    using Button=typename Policy::Button;
public:
    [[nodiscard]] static std::optional<AlertButtonAppearance> resolve(const AlertButtonConfiguration& value) {
        if ((value.text_color && !Policy::valid_color(*value.text_color)) ||
            (value.primary_color && !Policy::valid_color(*value.primary_color)) ||
            !Policy::valid_color(value.dialog_text) || !Policy::valid_color(value.dialog_border) ||
            !Policy::valid_color(value.disabled_background) || !Policy::valid_color(value.normal_pressed) ||
            !Policy::valid_color(Policy::dialog_background(value)) || !Policy::valid_color(Policy::dialog_hover(value)) ||
            !Policy::valid_color(Policy::dialog_pressed(value)) || !Policy::valid_color(Policy::primary_reference(value)) ||
            !Policy::valid_extra(value)) return std::nullopt;
        AlertButtonAppearance result;
        auto& button = result.button;
        button.text = value.text;
        button.accessibility_name = value.accessibility_name.empty() ? value.text : value.accessibility_name;
        button.enabled = value.enabled;
        button.motion_enabled = value.motion_enabled;
        button.enter_key_activation = value.enter_key_activation;
        button.tone = value.tone;
        button.width = value.width;
        button.height = value.height;
        button.horizontal_padding = value.horizontal_padding.value_or(Policy::padding(value));
        button.corner_radius = value.corner_radius.value_or(Policy::radius(value));
        button.palette = value.palette;
        button.palette.disabled_background = value.disabled_background;
        if (value.primary_color) button.palette.primary = *value.primary_color;
        const auto darken = [](StyleColor color, double factor) {
            return StyleColor{color.red / factor, color.green / factor, color.blue / factor, color.alpha};
        };
        if (value.primary_color || button.palette.primary != Policy::primary_reference(value)) {
            button.palette.primary_hover = darken(button.palette.primary, Policy::hover_factor(value));
            button.palette.primary_pressed = darken(button.palette.primary, Policy::pressed_factor(value));
        }
        if (value.dialog_style) {
            button.palette.text = value.dialog_text;
            button.palette.normal = Policy::dialog_background(value);
            button.palette.normal_hover = Policy::dialog_hover(value);
            button.palette.normal_pressed = Policy::dialog_pressed(value);
            if (value.tone == ButtonTone::normal) {
                result.border = value.dialog_border;
                result.border_width = Policy::border_width(value);
                button.border_color = result.border;
                button.border_width = result.border_width;
            }
        } else {
            const auto lighter = [&](double channel) { return (std::min)(1., channel * Policy::normal_hover_factor(value)); };
            const auto normal = button.palette.normal;
            button.palette.normal_hover = {lighter(normal.red), lighter(normal.green), lighter(normal.blue), normal.alpha};
            button.palette.normal_pressed = value.normal_pressed;
        }
        if (value.text_color) button.palette.text = *value.text_color;
        if (value.tone == ButtonTone::borderless && (value.dialog_style || value.text_color)) {
            button.palette.primary = value.text_color.value_or(value.dialog_text);
        }
        if (!Button::validate(button)) return std::nullopt;
        return result;
    }

    [[nodiscard]] const AlertButtonConfiguration& configuration() const noexcept { return configuration_; }
    [[nodiscard]] const AlertButtonAppearance& appearance() const noexcept { return appearance_; }

    bool set_configuration(AlertButtonConfiguration value) {
        auto appearance = resolve(value);
        if (!appearance) return false;
        configuration_ = std::move(value);
        appearance_ = std::move(*appearance);
        return true;
    }

private:
    AlertButtonConfiguration configuration_;
    AlertButtonAppearance appearance_ = *resolve(configuration_);
};

using AlertButtonModel=BasicAlertButton<DefaultAlertButtonModelPolicy>;
} // namespace ii::gui
