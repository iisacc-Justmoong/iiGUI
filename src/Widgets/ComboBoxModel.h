#pragma once
#include <iiGUI/ButtonModel.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
namespace ii::gui {
struct ComboBoxPalette final {
    WidgetColor surface{},hover{},pressed{},disabled{},text{},disabled_text{},focus{};
    friend bool operator==(const ComboBoxPalette&,const ComboBoxPalette&)=default;
};
struct ComboBoxConfiguration final {
    std::string text="Label",accessibility_name;ButtonTone tone=ButtonTone::primary;StepperArrow arrow=StepperArrow::up_down;
    bool enabled=true;double width=97,height=20,left_padding=8,right_padding=1,indicator_size=18,corner_radius=5;
    double font_size=13,line_height=13;unsigned font_weight=500;ComboBoxPalette palette;
    friend bool operator==(const ComboBoxConfiguration&,const ComboBoxConfiguration&)=default;
};
struct ComboBoxGeometry final {ButtonRect surface,label,indicator;double radius=0;};
struct ComboBoxVisual final {WidgetColor background,text,focus;bool enabled=true,focused=false;};
struct DefaultComboBoxModelPolicy final {
    using Configuration=ComboBoxConfiguration;using Geometry=ComboBoxGeometry;using Visual=ComboBoxVisual;
    using Rect=ButtonRect;using Interaction=ButtonInteraction;using Tone=ButtonTone;using Arrow=StepperArrow;using Color=WidgetColor;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return DefaultButtonPolicy::valid_text(value);}
};
// A click field: the application owns its menu and selected value.
template<class Policy>
class BasicComboBox final {
    using ComboBoxConfiguration=typename Policy::Configuration;using ComboBoxGeometry=typename Policy::Geometry;
    using ComboBoxVisual=typename Policy::Visual;using ButtonRect=typename Policy::Rect;
    using ButtonInteraction=typename Policy::Interaction;using ButtonTone=typename Policy::Tone;
    using StepperArrow=typename Policy::Arrow;using StyleColor=typename Policy::Color;
    ComboBoxConfiguration configuration_;
public:
    [[nodiscard]] static bool validate(const ComboBoxConfiguration& v)noexcept{
        const auto range=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
        const auto color=[&](StyleColor c){return range(c.red,0,1)&&range(c.green,0,1)&&range(c.blue,0,1)&&range(c.alpha,0,1);};
        if(!Policy::valid_text(v.text)||!Policy::valid_text(v.accessibility_name)||
           !range(v.width,19,8192)||!range(v.height,1,8192)||!range(v.left_padding,0,8192)||
           !range(v.right_padding,0,8192)||!range(v.indicator_size,1,256)||!range(v.corner_radius,0,8192)||
           !range(v.font_size,1,256)||!range(v.line_height,1,8192)||
           (v.font_weight!=400&&v.font_weight!=500&&v.font_weight!=600&&v.font_weight!=700)||
           (v.tone!=ButtonTone::primary&&v.tone!=ButtonTone::borderless))return false;
        switch(v.arrow){case StepperArrow::up_down:case StepperArrow::up:case StepperArrow::down:break;default:return false;}
        const auto& p=v.palette;return color(p.surface)&&color(p.hover)&&color(p.pressed)&&color(p.disabled)&&
            color(p.text)&&color(p.disabled_text)&&color(p.focus);
    }
    [[nodiscard]] bool set_configuration(ComboBoxConfiguration value){
        if(!validate(value))return false;configuration_=std::move(value);return true;
    }
    [[nodiscard]] const ComboBoxConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] ComboBoxVisual visual(ButtonInteraction interaction)const noexcept{
        const auto& p=configuration_.palette;const auto enabled=configuration_.enabled&&interaction.enabled;
        const auto background=!enabled?p.disabled:interaction.pressed?p.pressed:interaction.hovered?p.hover:
            configuration_.tone==ButtonTone::borderless?StyleColor{0,0,0,0}:p.surface;
        return {background,enabled?p.text:p.disabled_text,p.focus,enabled,enabled&&interaction.focused};
    }
    [[nodiscard]] std::optional<ComboBoxGeometry> geometry(double width,double height)const noexcept{
        if(!std::isfinite(width)||!std::isfinite(height)||width<0||height<0||width>8192||height>8192)return std::nullopt;
        const auto& v=configuration_;ComboBoxGeometry result;result.surface={0,0,width,height};
        result.radius=(std::min)({v.corner_radius,width/2,height/2});
        const auto right=(std::min)(v.right_padding,width),size=(std::min)({v.indicator_size,width-right,height});
        const auto indicator_x=width-right-size;
        result.indicator={indicator_x,(height-size)/2,size,size};
        const auto left=(std::min)(v.left_padding,indicator_x),line=(std::min)(v.line_height,height);
        result.label={left,(height-line)/2,(std::max)(0.,indicator_x-left),line};return result;
    }
    [[nodiscard]] std::string_view glyph()const noexcept{
        return configuration_.arrow==StepperArrow::up?"⌃":configuration_.arrow==StepperArrow::down?"⌄":"↕";
    }
};
using ComboBoxModel=BasicComboBox<DefaultComboBoxModelPolicy>;
} // namespace ii::gui
