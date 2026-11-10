#pragma once
#include <iiGUI/ButtonModel.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
namespace ii::gui {
struct DropdownConfiguration final {
    std::string text,accessibility_name;
    bool enabled=true,icon_mode=false;
    ButtonTone tone=ButtonTone::primary;
    std::optional<double> width;
    double height=22,left_padding=8,right_padding=2,spacing=0,corner_radius=8;
    double icon_size=18,indicator_size=18,font_size=13,line_height=13;
    unsigned font_weight=500;
    ButtonPalette palette;
    friend bool operator==(const DropdownConfiguration&,const DropdownConfiguration&)=default;
};
struct DropdownGeometry final {ButtonRect surface,label,icon,indicator;double radius=0;};
struct DefaultDropdownModelPolicy final {
    using Configuration=DropdownConfiguration;using Geometry=DropdownGeometry;using ButtonConfig=ButtonConfiguration;
    using Tone=ButtonTone;using Visual=ButtonVisual;using Interaction=ButtonInteraction;using Color=WidgetColor;using Button=ButtonModel;
    [[nodiscard]] static bool valid_text(std::string_view value) noexcept {return DefaultButtonPolicy::valid_text(value);}
};
template<class Policy>
class BasicDropdownButton final {
    using DropdownConfiguration=typename Policy::Configuration;using DropdownGeometry=typename Policy::Geometry;
    using ButtonConfiguration=typename Policy::ButtonConfig;using ButtonTone=typename Policy::Tone;
    using ButtonVisual=typename Policy::Visual;using ButtonInteraction=typename Policy::Interaction;
    using StyleColor=typename Policy::Color;using Button=typename Policy::Button;
    DropdownConfiguration configuration_;
    [[nodiscard]] static ButtonConfiguration button(const DropdownConfiguration& v){
        ButtonConfiguration result;result.text=v.text;result.accessibility_name=v.accessibility_name;result.enabled=v.enabled;
        result.tone=v.tone;result.width=v.width;result.height=v.height;result.corner_radius=v.corner_radius;
        result.font_size=v.font_size;result.line_height=v.line_height;result.font_weight=v.font_weight;result.palette=v.palette;return result;
    }
public:
    [[nodiscard]] static bool validate(const DropdownConfiguration& v)noexcept{
        const auto bound=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
        const auto color=[&](StyleColor c){return bound(c.red,0,1)&&bound(c.green,0,1)&&bound(c.blue,0,1)&&bound(c.alpha,0,1);};
        if(!Policy::valid_text(v.text)||!Policy::valid_text(v.accessibility_name)||
           !bound(v.height,1,8192)||(v.width&&!bound(*v.width,1,8192))||!bound(v.corner_radius,0,8192)||
           !bound(v.font_size,1,256)||!bound(v.line_height,0,8192)||
           (v.font_weight!=400&&v.font_weight!=500&&v.font_weight!=600&&v.font_weight!=700)||
           !bound(v.left_padding,0,8192)||!bound(v.right_padding,0,8192)||!bound(v.spacing,-256,8192)||
           !bound(v.icon_size,1,256)||!bound(v.indicator_size,1,256)||
           !bound(v.left_padding+(v.icon_mode?v.icon_size:0),0,8192)||
           !bound(v.right_padding+v.indicator_size+v.spacing,0,8192))return false;
        switch(v.tone){case ButtonTone::primary:case ButtonTone::normal:case ButtonTone::borderless:
            case ButtonTone::destructive:case ButtonTone::disabled:break;default:return false;}
        const auto& p=v.palette;return color(p.primary)&&color(p.primary_hover)&&color(p.primary_pressed)&&color(p.normal)&&
            color(p.normal_hover)&&color(p.normal_pressed)&&color(p.danger)&&color(p.disabled_background)&&color(p.text)&&color(p.disabled_text);
    }
    [[nodiscard]] bool set_configuration(DropdownConfiguration value){
        if(!validate(value))return false;configuration_=std::move(value);return true;
    }
    [[nodiscard]] const DropdownConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] ButtonVisual visual(ButtonInteraction interaction)const{
        Button control;(void)control.set_configuration(button(configuration_));return control.visual(interaction);
    }
    [[nodiscard]] std::optional<double> preferred_width(double text_width)const noexcept{
        if(!std::isfinite(text_width)||text_width<0||text_width>8192)return std::nullopt;
        const auto& v=configuration_;const auto content=v.icon_mode?v.icon_size:text_width;
        return std::clamp(v.left_padding+content+v.spacing+v.indicator_size+v.right_padding,0.,8192.);
    }
    [[nodiscard]] std::optional<DropdownGeometry> geometry(double width,double height,double text_width,double text_height)const noexcept{
        const auto extent=[](double n){return std::isfinite(n)&&n>=0&&n<=8192;};
        if(!extent(width)||!extent(height)||!extent(text_width)||!extent(text_height))return std::nullopt;
        const auto& v=configuration_;DropdownGeometry g;g.surface={0,0,width,height};g.radius=(std::min)({v.corner_radius,width/2,height/2});
        const auto right=(std::min)(v.right_padding,width),indicator=(std::min)({v.indicator_size,width-right,height});
        const auto indicator_x=width-right-indicator;g.indicator={indicator_x,(height-indicator)/2,indicator,indicator};
        const auto left=(std::min)(v.left_padding,width),available=(std::max)(0.,indicator_x-left-v.spacing);
        const auto content_height=v.icon_mode?(std::min)({v.icon_size,height,available}):(std::min)(text_height,height);
        const auto content_width=v.icon_mode?content_height:(std::min)(text_width,available);
        if(v.icon_mode)g.icon={left,(height-content_height)/2,content_width,content_height};
        else g.label={left,(height-content_height)/2,content_width,content_height};
        return g;
    }
};
using DropdownButtonModel=BasicDropdownButton<DefaultDropdownModelPolicy>;
} // namespace ii::gui
