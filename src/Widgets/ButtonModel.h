#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace ii::gui {
// Colors are encoded sRGB and supplied by the application's design system.
enum class ButtonSelectionStyle { accent, segmented };
struct ButtonPalette final {
    WidgetColor primary{},primary_hover{},primary_pressed{};
    WidgetColor normal{},normal_hover{},normal_pressed{};
    std::optional<WidgetColor> selected;
    WidgetColor danger{},disabled_background{},text{},disabled_text{};
    std::optional<WidgetColor> danger_hover,danger_pressed;
    WidgetColor danger_reference{},danger_hover_default{},danger_pressed_default{};
    WidgetColor selected_surface{},selected_reference{},selected_default{};
    friend bool operator==(const ButtonPalette&,const ButtonPalette&)=default;
};
struct ButtonConfiguration final {
    std::string text,accessibility_name;
    bool enabled=true;
    ButtonTone tone=ButtonTone::primary;
    double height=22;
    std::optional<double> width;
    double horizontal_padding=8,corner_radius=8;
    double icon_size=16,icon_spacing=7,font_size=13,line_height=13;
    double text_vertical_offset=0;
    unsigned font_weight=500;
    bool icon_mode=false,has_icon=false;
    bool checkable=false,checked=false;
    ButtonPalette palette;
    std::optional<WidgetColor> border_color;
    double border_width=0;
    bool motion_enabled=true,enter_key_activation=true;
    ButtonSelectionStyle selection_style=ButtonSelectionStyle::accent;
    friend bool operator==(const ButtonConfiguration&,const ButtonConfiguration&)=default;
};
struct ButtonInteraction final {
    bool enabled=true,hovered=false,pressed=false,focused=false,checked=false;
};
struct ButtonVisual final {
    bool enabled=true,focused=false;
    WidgetColor background,text,focus;
};
struct ButtonRect final { double x=0,y=0,width=0,height=0; };
struct ButtonGeometry final {
    ButtonRect surface,icon,text;
    double radius=0;
    bool draw_icon=false,draw_text=false;
};
struct DefaultButtonPolicy final {
    using Configuration=ButtonConfiguration;
    using Interaction=ButtonInteraction;
    using Visual=ButtonVisual;
    using Geometry=ButtonGeometry;
    using Tone=ButtonTone;
    using SelectionStyle=ButtonSelectionStyle;
    using Color=WidgetColor;
    [[nodiscard]] static bool valid_text(std::string_view text) noexcept {
        return text.size()<=65536 && text.find('\0')==std::string_view::npos && ii::core::valid_utf8(text);
    }
};
// A policy supplies value contracts, not behavior. The framework owns the
// validation, state precedence and layout calculations for all button styles.
template<class Policy>
class BasicButton final {
    using ButtonConfiguration = typename Policy::Configuration;
    using ButtonInteraction = typename Policy::Interaction;
    using ButtonVisual = typename Policy::Visual;
    using ButtonGeometry = typename Policy::Geometry;
    using ButtonTone = typename Policy::Tone;
    using ButtonSelectionStyle = typename Policy::SelectionStyle;
    using StyleColor = typename Policy::Color;
public:
    [[nodiscard]] const ButtonConfiguration& configuration() const noexcept { return configuration_; }
    [[nodiscard]] static bool validate(const ButtonConfiguration& value) noexcept {
        const auto bound=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
        const auto color=[&](StyleColor c){return bound(c.red,0,1)&&bound(c.green,0,1)&&bound(c.blue,0,1)&&bound(c.alpha,0,1);};
        if(!Policy::valid_text(value.text)||!Policy::valid_text(value.accessibility_name)||
           !bound(value.height,1,8192)||(value.width&&!bound(*value.width,1,8192))||!bound(value.horizontal_padding,0,8192)||
           !bound(value.corner_radius,0,8192)||!bound(value.icon_size,1,256)||
           !bound(value.icon_spacing,0,8192)||!bound(value.font_size,1,256)||!bound(value.line_height,0,8192)||
           !bound(value.text_vertical_offset,-8192,8192)||!bound(value.border_width,0,8192)||
           (value.border_color&&!color(*value.border_color))||
           (value.font_weight!=400&&value.font_weight!=500&&value.font_weight!=600&&value.font_weight!=700)||
           (value.icon_mode&&(!value.has_icon||value.horizontal_padding+value.icon_size+
             (value.text.empty()?0:value.icon_spacing)>8192))) return false;
        switch(value.tone){case ButtonTone::primary:case ButtonTone::normal:case ButtonTone::borderless:
            case ButtonTone::destructive:case ButtonTone::disabled:break;default:return false;}
        switch(value.selection_style){case ButtonSelectionStyle::accent:case ButtonSelectionStyle::segmented:break;default:return false;}
        const auto& p=value.palette;
        return color(p.primary)&&color(p.primary_hover)&&color(p.primary_pressed)&&color(p.normal)&&
               color(p.normal_hover)&&color(p.normal_pressed)&&(!p.selected||color(*p.selected))&&color(p.danger)&&
               (!p.danger_hover||color(*p.danger_hover))&&(!p.danger_pressed||color(*p.danger_pressed))&&color(p.disabled_background)&&
               color(p.text)&&color(p.disabled_text)&&color(p.danger_reference)&&color(p.danger_hover_default)&&color(p.danger_pressed_default)&&
               color(p.selected_surface)&&color(p.selected_reference)&&color(p.selected_default);
    }
    bool set_configuration(ButtonConfiguration value) {
        if(!validate(value))return false;
        configuration_=std::move(value);return true;
    }
    [[nodiscard]] double icon_gap() const noexcept {
        return configuration_.icon_mode&&!configuration_.text.empty()&&configuration_.tone!=ButtonTone::primary
            ?configuration_.icon_spacing:0;
    }
    [[nodiscard]] ButtonVisual visual(ButtonInteraction interaction) const noexcept {
        const auto& p=configuration_.palette;
        const bool enabled=configuration_.enabled&&interaction.enabled&&configuration_.tone!=ButtonTone::disabled;
        ButtonVisual result{enabled,enabled&&interaction.focused,p.primary,p.text,p.primary};
        if(!enabled){result.background=p.disabled_background;result.text=p.disabled_text;return result;}
        const auto darken=[](StyleColor c,double factor){return StyleColor{c.red/factor,c.green/factor,c.blue/factor,c.alpha};};
        const auto tone=interaction.checked&&configuration_.selection_style==ButtonSelectionStyle::segmented
            ?ButtonTone::normal:configuration_.tone;
        switch(tone){
        case ButtonTone::primary:
            result.background=interaction.pressed?p.primary_pressed:interaction.hovered?p.primary_hover:p.primary;break;
        case ButtonTone::normal:
            result.background=interaction.pressed?p.normal_pressed:interaction.hovered?p.normal_hover:p.normal;break;
        case ButtonTone::borderless:
            result.text=p.primary;result.background=interaction.pressed?p.normal_pressed:interaction.hovered?p.normal_hover:StyleColor{0,0,0,0};break;
        case ButtonTone::destructive:
            result.background=interaction.pressed?p.danger_pressed.value_or(p.danger==p.danger_reference
                ?p.danger_pressed_default:darken(p.danger,1.2)):
                interaction.hovered?p.danger_hover.value_or(p.danger==p.danger_reference
                ?p.danger_hover_default:darken(p.danger,1.12)):p.danger;break;
        case ButtonTone::disabled:break;
        }
        if(interaction.checked && !interaction.pressed && configuration_.selection_style==ButtonSelectionStyle::accent){
            const double amount=.25*p.primary.alpha;
            const auto surface=p.selected_surface;
            const auto fallback=p.primary==p.selected_reference
                ?p.selected_default
                :StyleColor{surface.red*(1-amount)+p.primary.red*amount,
                            surface.green*(1-amount)+p.primary.green*amount,
                            surface.blue*(1-amount)+p.primary.blue*amount,1};
            result.background=p.selected.value_or(fallback);
        }
        return result;
    }
    [[nodiscard]] std::optional<ButtonGeometry> geometry(double width,double height,double text_width,double text_height) const noexcept {
        const auto extent=[](double n){return std::isfinite(n)&&n>=0&&n<=8192;};
        const auto metric=[](double n){return std::isfinite(n)&&n>=0;};
        if(!extent(width)||!extent(height)||!metric(text_width)||!metric(text_height))return std::nullopt;
        ButtonGeometry result;
        result.surface={0,0,width,height};
        result.radius=(std::min)(configuration_.corner_radius,(std::min)(width,height)/2);
        if(width==0||height==0)return result;
        const auto padding=(std::min)(configuration_.horizontal_padding,width/2);
        const auto available=(std::max)(0.0,width-2*padding);
        const bool has_text=!configuration_.text.empty()&&text_width>0&&text_height>0;
        const auto icon=configuration_.icon_mode?(std::min)({configuration_.icon_size,available,height}):0;
        const auto gap=icon>0&&has_text?(std::min)(icon_gap(),(std::max)(0.0,available-icon)):0;
        const auto fitted_text=has_text?(std::min)(text_width,(std::max)(0.0,available-icon-gap)):0;
        const auto total=icon+gap+fitted_text;
        const auto left=(width-total)/2;
        result.icon={left,(height-icon)/2,icon,icon};
        const auto fitted_height=(std::min)(height,text_height);
        result.text={left+icon+gap,(height-fitted_height)/2+configuration_.text_vertical_offset,fitted_text,fitted_height};
        result.draw_icon=icon>0;result.draw_text=fitted_text>0&&fitted_height>0;
        return result;
    }
private:
    ButtonConfiguration configuration_;
};
using ButtonModel=BasicButton<DefaultButtonPolicy>;
} // namespace ii::gui
