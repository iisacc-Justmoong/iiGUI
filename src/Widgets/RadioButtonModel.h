#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/WidgetMotion.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
namespace ii::gui {
struct RadioButtonPalette final {
    WidgetColor on,off,disabled_on,disabled_off,dot,disabled_dot,text,disabled_text,focus;
    friend bool operator==(const RadioButtonPalette&,const RadioButtonPalette&)=default;
};
struct RadioButtonConfiguration final {
    std::string text,accessibility_name,group;
    bool enabled=true,checked=false,motion_enabled=true;
    double indicator_size=18,dot_size=8,label_gap=8,font_size=13,line_height=13;
    unsigned font_weight=500;
    WidgetMotion motion{.duration=std::chrono::milliseconds{360},.curve=WidgetMotionCurve::out_back,.overshoot=1.45};
    RadioButtonPalette palette;
    friend bool operator==(const RadioButtonConfiguration&,const RadioButtonConfiguration&)=default;
};
struct RadioButtonInteraction final {bool enabled=true,focused=false;};
struct RadioButtonVisual final {WidgetColor indicator,dot,text,focus;bool draw_dot=false,focused=false;};
struct RadioButtonRect final {double x=0,y=0,width=0,height=0;};
struct RadioButtonGeometry final {RadioButtonRect indicator,dot,text;double indicator_radius=0,dot_radius=0;bool draw_indicator=false,draw_dot=false,draw_text=false;};
struct DefaultRadioButtonModelPolicy final {
    using Configuration=RadioButtonConfiguration;using Interaction=RadioButtonInteraction;using Visual=RadioButtonVisual;
    using Geometry=RadioButtonGeometry;using Color=WidgetColor;
    [[nodiscard]] static bool valid_text(std::string_view text)noexcept {
        return text.size()<=65536&&text.find('\0')==std::string_view::npos&&ii::core::valid_utf8(text);
    }
};
template<class Policy>
class BasicRadioButton final {
    using RadioButtonConfiguration=typename Policy::Configuration;using RadioButtonInteraction=typename Policy::Interaction;
    using RadioButtonVisual=typename Policy::Visual;using RadioButtonGeometry=typename Policy::Geometry;using StyleColor=typename Policy::Color;
public:
    using TimePoint=std::chrono::steady_clock::time_point;
    [[nodiscard]] const RadioButtonConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] bool checked()const noexcept{return configuration_.checked;}
    [[nodiscard]] bool animating()const noexcept{return transition_.active()||indicator_transition_.active();}
    [[nodiscard]] double dot_scale()const noexcept{
        const auto max_scale=configuration_.dot_size>0?(std::max)(1.0,configuration_.indicator_size/configuration_.dot_size):1;
        return std::clamp(transition_.value()[0],0.0,max_scale);
    }
    [[nodiscard]] static bool validate(const RadioButtonConfiguration& c)noexcept{
        const auto bound=[](double v,double lo,double hi){return std::isfinite(v)&&v>=lo&&v<=hi;};
        const auto color=[&](StyleColor v){return bound(v.red,0,1)&&bound(v.green,0,1)&&bound(v.blue,0,1)&&bound(v.alpha,0,1);};
        if(!Policy::valid_text(c.text)||!Policy::valid_text(c.accessibility_name)||
           !Policy::valid_text(c.group)||c.group.size()>1024||!bound(c.indicator_size,1,8192)||
           !bound(c.dot_size,0,c.indicator_size)||!bound(c.label_gap,0,8192)||
           c.indicator_size+(c.text.empty()?0:c.label_gap)>8192||!bound(c.font_size,1,256)||
           !bound(c.line_height,0,8192)||(c.font_weight!=400&&c.font_weight!=500&&c.font_weight!=600&&c.font_weight!=700)||
           !valid_widget_motion(widget_motion_value(c.motion)))return false;
        const auto& p=c.palette;
        return color(p.on)&&color(p.off)&&color(p.disabled_on)&&color(p.disabled_off)&&color(p.dot)&&
               color(p.disabled_dot)&&color(p.text)&&color(p.disabled_text)&&color(p.focus);
    }
    bool set_configuration(RadioButtonConfiguration value,TimePoint now){
        if(!validate(value))return false;
        auto next=transition_;auto indicator=indicator_transition_;const auto target=value.checked?1.0:0.0;
        const auto color=channels(indicator_target(value,value.checked));
        if(!initialized_){if(!next.reset({target})||!indicator.reset(color))return false;}
        else if(!next.retarget({target},widget_motion_value(value.motion),value.enabled&&value.motion_enabled,now)||
                !indicator.retarget(color,color_motion(value),value.enabled&&value.motion_enabled,now))return false;
        configuration_=std::move(value);transition_=std::move(next);indicator_transition_=std::move(indicator);
        initialized_=true;return true;
    }
    bool set_checked(bool value,TimePoint now)noexcept{
        auto next=transition_;auto indicator=indicator_transition_;
        const bool enabled=configuration_.enabled&&configuration_.motion_enabled;
        if(!next.retarget({value?1.0:0.0},widget_motion_value(configuration_.motion),enabled,now)||
           !indicator.retarget(channels(indicator_target(configuration_,value)),color_motion(configuration_),enabled,now))return false;
        configuration_.checked=value;transition_=std::move(next);indicator_transition_=std::move(indicator);return true;
    }
    [[nodiscard]] std::optional<bool> activate(TimePoint now)noexcept{
        if(!configuration_.enabled||configuration_.checked||!set_checked(true,now))return std::nullopt;return true;
    }
    [[nodiscard]] bool advance(TimePoint now)noexcept{
        const bool dot=transition_.advance(now),indicator=indicator_transition_.advance(now);return dot||indicator;
    }
    void finish_animation()noexcept{transition_.settle();indicator_transition_.settle();}
    [[nodiscard]] RadioButtonVisual visual(RadioButtonInteraction i)const noexcept{
        const auto& c=configuration_;const auto& p=c.palette;const bool enabled=c.enabled&&i.enabled;
        return {enabled?displayed_indicator():(c.checked?p.disabled_on:p.disabled_off),enabled?p.dot:p.disabled_dot,
                enabled?p.text:p.disabled_text,p.focus,c.checked&&dot_scale()>0&&c.dot_size>0,enabled&&i.focused};
    }
    [[nodiscard]] std::optional<RadioButtonGeometry> geometry(double width,double height,double text_width,double text_height)const noexcept{
        const auto extent=[](double n){return std::isfinite(n)&&n>=0&&n<=8192;};
        const auto metric=[](double n){return std::isfinite(n)&&n>=0;};
        if(!extent(width)||!extent(height)||!metric(text_width)||!metric(text_height))return std::nullopt;
        RadioButtonGeometry result;const auto size=(std::min)({configuration_.indicator_size,width,height});
        result.indicator={0,(height-size)/2,size,size};result.indicator_radius=size/2;result.draw_indicator=size>0;
        const auto dot=(std::min)(size,configuration_.dot_size*dot_scale());
        result.dot={(size-dot)/2,(height-dot)/2,dot,dot};result.dot_radius=dot/2;result.draw_dot=dot>0;
        if(!configuration_.text.empty()&&text_width>0&&text_height>0){
            const auto x=(std::min)(width,size+configuration_.label_gap);
            const auto w=(std::min)(text_width,(std::max)(0.0,width-x)),h=(std::min)(height,text_height);
            result.text={x,(height-h)/2,w,h};result.draw_text=w>0&&h>0;
        }
        return result;
    }
private:
    [[nodiscard]] static StyleColor indicator_target(const RadioButtonConfiguration& c,bool checked)noexcept{
        return c.enabled?(checked?c.palette.on:c.palette.off):(checked?c.palette.disabled_on:c.palette.disabled_off);
    }
    [[nodiscard]] static WidgetMotion color_motion(const RadioButtonConfiguration& c)noexcept{
        return {.duration=std::chrono::milliseconds{130},.curve=WidgetMotionCurve::out_cubic,.reduced_motion=widget_motion_value(c.motion).reduced_motion};
    }
    [[nodiscard]] static std::array<double,4> channels(StyleColor c)noexcept{return {c.red,c.green,c.blue,c.alpha};}
    [[nodiscard]] StyleColor displayed_indicator()const noexcept{
        const auto& c=indicator_transition_.value();
        return {std::clamp(c[0],0.,1.),std::clamp(c[1],0.,1.),std::clamp(c[2],0.,1.),std::clamp(c[3],0.,1.)};
    }
    RadioButtonConfiguration configuration_;
    WidgetMotionTransition<1> transition_{{0}};
    WidgetMotionTransition<4> indicator_transition_{channels(configuration_.palette.off)};
    bool initialized_=false;
};
using RadioButtonModel=BasicRadioButton<DefaultRadioButtonModelPolicy>;
} // namespace ii::gui
