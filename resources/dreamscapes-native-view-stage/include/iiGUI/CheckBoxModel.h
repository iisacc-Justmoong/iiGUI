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
enum class CheckBoxShape { round_rect, cylinder };
struct CheckBoxPalette final {
    WidgetColor checked,unchecked,disabled_checked,disabled_unchecked,mark,disabled_mark;
    WidgetColor checked_border,unchecked_border,disabled_checked_border,disabled_unchecked_border;
    WidgetColor text,disabled_text,focus,inner_shadow_soft,inner_shadow_strong;
    friend bool operator==(const CheckBoxPalette&,const CheckBoxPalette&)=default;
};
struct CheckBoxConfiguration final {
    std::string text,accessibility_name;
    bool enabled=true,checked=false;
    double box_size=17,frame_padding=.5,box_radius=3.5,mark_stroke_width=2,label_gap=6;
    double checked_border_width=0,unchecked_border_width=.5,disabled_checked_border_width=.5,disabled_unchecked_border_width=0;
    double font_size=13,line_height=13;unsigned font_weight=500;
    CheckBoxPalette palette;
    std::optional<bool> use_authored_checked_assets;
    CheckBoxShape shape_style=CheckBoxShape::round_rect;bool motion_enabled=true;
    WidgetMotion motion{.duration=std::chrono::milliseconds{360},.curve=WidgetMotionCurve::out_back,.overshoot=1.45};
    friend bool operator==(const CheckBoxConfiguration&,const CheckBoxConfiguration&)=default;
};
struct CheckBoxInteraction final {bool enabled=true,hovered=false,pressed=false,focused=false;
    friend bool operator==(const CheckBoxInteraction&,const CheckBoxInteraction&)=default;};
struct CheckBoxVisual final {WidgetColor fill,mark,border,text,focus;double border_width=0;
    bool draw_mark=false,focused=false,authored_mark=false,draw_inner_shadow=false;};
struct CheckBoxRect final {double x=0,y=0,width=0,height=0;};
struct CheckBoxGeometry final {CheckBoxRect box,text;double radius=0;bool draw_box=false,draw_text=false;};
struct DefaultCheckBoxModelPolicy final {
    using Configuration=CheckBoxConfiguration;using Interaction=CheckBoxInteraction;using Visual=CheckBoxVisual;
    using Geometry=CheckBoxGeometry;using Color=WidgetColor;using Shape=CheckBoxShape;
    [[nodiscard]] static bool valid_text(std::string_view text)noexcept {
        return text.size()<=65536&&text.find('\0')==std::string_view::npos&&ii::core::valid_utf8(text);
    }
    [[nodiscard]] static bool authored_mark(const Configuration& c)noexcept{return c.use_authored_checked_assets.value_or(false);}
    [[nodiscard]] static Color authored_mark_color(const Configuration& c,bool enabled)noexcept{return enabled?c.palette.mark:c.palette.disabled_mark;}
};
template<class Policy>
class BasicCheckBox final {
    using CheckBoxConfiguration=typename Policy::Configuration;using CheckBoxInteraction=typename Policy::Interaction;
    using CheckBoxVisual=typename Policy::Visual;using CheckBoxGeometry=typename Policy::Geometry;
    using StyleColor=typename Policy::Color;using Shape=typename Policy::Shape;
public:
    using TimePoint=std::chrono::steady_clock::time_point;
    [[nodiscard]] const CheckBoxConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] bool checked()const noexcept{return configuration_.checked;}
    [[nodiscard]] bool animating()const noexcept{return mark_transition_.active()||fill_transition_.active();}
    [[nodiscard]] double mark_scale()const noexcept{
        return initialized_?std::clamp(mark_transition_.value()[0],0.,4.):(configuration_.checked?1.:.55);
    }
    [[nodiscard]] static bool validate(const CheckBoxConfiguration& c)noexcept{
        const auto bound=[](double v,double low,double high){return std::isfinite(v)&&v>=low&&v<=high;};
        const auto color=[&](StyleColor v){return bound(v.red,0,1)&&bound(v.green,0,1)&&bound(v.blue,0,1)&&bound(v.alpha,0,1);};
        if(!Policy::valid_text(c.text)||!Policy::valid_text(c.accessibility_name)||
           (c.shape_style!=Shape::round_rect&&c.shape_style!=Shape::cylinder)||
           !bound(c.box_size,1,8192)||!bound(c.frame_padding,0,8192)||!bound(c.box_radius,0,8192)||
           !bound(c.mark_stroke_width,0,c.box_size)||!bound(c.label_gap,0,8192)||
           !bound(c.checked_border_width,0,c.box_size/2)||!bound(c.unchecked_border_width,0,c.box_size/2)||
           !bound(c.disabled_checked_border_width,0,c.box_size/2)||!bound(c.disabled_unchecked_border_width,0,c.box_size/2)||
           c.box_size+2*c.frame_padding>8192||c.box_size+2*c.frame_padding+(c.text.empty()?0:c.label_gap)>8192||
           !bound(c.font_size,1,256)||!bound(c.line_height,0,8192)||!valid_widget_motion(widget_motion_value(c.motion))||
           (c.font_weight!=400&&c.font_weight!=500&&c.font_weight!=600&&c.font_weight!=700))return false;
        const auto& p=c.palette;
        return color(p.checked)&&color(p.unchecked)&&color(p.disabled_checked)&&color(p.disabled_unchecked)&&
               color(p.mark)&&color(p.disabled_mark)&&color(p.checked_border)&&color(p.unchecked_border)&&
               color(p.disabled_checked_border)&&color(p.disabled_unchecked_border)&&color(p.text)&&color(p.disabled_text)&&color(p.focus)&&
               color(p.inner_shadow_soft)&&color(p.inner_shadow_strong);
    }
    bool set_configuration(CheckBoxConfiguration value,TimePoint now=std::chrono::steady_clock::now()){
        if(!validate(value))return false;
        auto mark=mark_transition_;auto fill=fill_transition_;
        const auto target=value.checked?1.:.55;const auto ink=channels(fill_target(value,interaction_,value.checked));
        if(!initialized_){if(!mark.reset({target})||!fill.reset(ink))return false;}
        else if(!mark.retarget({target},widget_motion_value(value.motion),motion_allowed(value,interaction_),now)||
                !fill.retarget(ink,color_motion(value),motion_allowed(value,interaction_),now))return false;
        configuration_=std::move(value);mark_transition_=std::move(mark);fill_transition_=std::move(fill);initialized_=true;return true;
    }
    [[nodiscard]] bool set_interaction(CheckBoxInteraction value,TimePoint now=std::chrono::steady_clock::now())noexcept{
        if(!initialized_){interaction_=value;return true;}
        auto mark=mark_transition_;auto fill=fill_transition_;const auto checked=configuration_.checked;
        if(!mark.retarget({checked?1.:.55},widget_motion_value(configuration_.motion),motion_allowed(configuration_,value),now)||
           !fill.retarget(channels(fill_target(configuration_,value,checked)),color_motion(configuration_),
                          motion_allowed(configuration_,value),now))return false;
        interaction_=value;mark_transition_=std::move(mark);fill_transition_=std::move(fill);return true;
    }
    [[nodiscard]] std::optional<bool> activate(TimePoint now=std::chrono::steady_clock::now())noexcept{
        if(!configuration_.enabled)return std::nullopt;
        auto mark=mark_transition_;auto fill=fill_transition_;
        if(!initialized_&&(!mark.reset({configuration_.checked?1.:.55})||
                            !fill.reset(channels(fill_target(configuration_,interaction_,configuration_.checked)))))return std::nullopt;
        const bool checked=!configuration_.checked;
        if(!mark.retarget({checked?1.:.55},widget_motion_value(configuration_.motion),motion_allowed(configuration_,interaction_),now)||
           !fill.retarget(channels(fill_target(configuration_,interaction_,checked)),color_motion(configuration_),
                          motion_allowed(configuration_,interaction_),now))return std::nullopt;
        configuration_.checked=checked;mark_transition_=std::move(mark);fill_transition_=std::move(fill);initialized_=true;return checked;
    }
    [[nodiscard]] bool advance(TimePoint now)noexcept{
        const bool mark=mark_transition_.advance(now),fill=fill_transition_.advance(now);return mark||fill;
    }
    void finish_animation()noexcept{mark_transition_.settle();fill_transition_.settle();}
    [[nodiscard]] CheckBoxVisual visual(CheckBoxInteraction i)const noexcept{
        const auto& c=configuration_;const auto& p=c.palette;const bool enabled=c.enabled&&i.enabled;
        const bool authored=c.checked&&c.shape_style==Shape::round_rect&&
            Policy::authored_mark(c);
        const bool observed=initialized_&&i.enabled==interaction_.enabled&&i.hovered==interaction_.hovered&&i.pressed==interaction_.pressed;
        auto fill=observed?displayed_fill():fill_target(c,i,c.checked);
        return {fill,authored?Policy::authored_mark_color(c,enabled):(enabled?p.mark:p.disabled_mark),
                enabled?(c.checked?p.checked_border:p.unchecked_border):(c.checked?p.disabled_checked_border:p.disabled_unchecked_border),
                enabled?p.text:p.disabled_text,p.focus,
                enabled?(c.checked?c.checked_border_width:c.unchecked_border_width):(c.checked?c.disabled_checked_border_width:c.disabled_unchecked_border_width),
                c.checked,enabled&&i.focused,authored,!(enabled&&c.checked)};
    }
    [[nodiscard]] std::optional<CheckBoxGeometry> geometry(double width,double height,double text_width,double text_height)const noexcept{
        const auto extent=[](double n){return std::isfinite(n)&&n>=0&&n<=8192;};
        const auto metric=[](double n){return std::isfinite(n)&&n>=0;};
        if(!extent(width)||!extent(height)||!metric(text_width)||!metric(text_height))return std::nullopt;
        const auto padding=(std::min)(configuration_.frame_padding,width/2);
        const auto size=(std::min)({configuration_.box_size,(std::max)(0.0,width-2*padding),(std::max)(0.0,height-2*configuration_.frame_padding)});
        CheckBoxGeometry result;result.box={padding,(height-size)/2,size,size};
        result.radius=configuration_.shape_style==Shape::cylinder?size/2:(std::min)(configuration_.box_radius,size/2);result.draw_box=size>0;
        if(!configuration_.text.empty()&&text_width>0&&text_height>0){
            const auto x=(std::min)(width,padding+size+configuration_.label_gap);
            const auto w=(std::min)(text_width,(std::max)(0.0,width-x-padding)),h=(std::min)(height,text_height);
            result.text={x,(height-h)/2,w,h};result.draw_text=w>0&&h>0;
        }
        return result;
    }
    // A rounded two-segment mark in normalized box coordinates. Sampling this
    // function produces a mask independent of any font or asset.
    [[nodiscard]] double mark_coverage(double x,double y)const noexcept{
        if(!std::isfinite(x)||!std::isfinite(y)||configuration_.mark_stroke_width<=0)return 0;
        const auto distance=[&](double ax,double ay,double bx,double by){
            const auto dx=bx-ax,dy=by-ay;
            const auto t=std::clamp(((x-ax)*dx+(y-ay)*dy)/(dx*dx+dy*dy),0.0,1.0);
            return std::hypot(x-(ax+t*dx),y-(ay+t*dy));
        };
        const auto radius=configuration_.mark_stroke_width/(2*configuration_.box_size);
        const auto signed_distance=radius-(std::min)(distance(.25,.52,.43,.70),distance(.43,.70,.69,.34));
        return std::clamp(signed_distance*272+.5,0.0,1.0);
    }
private:
    [[nodiscard]] static bool motion_allowed(const CheckBoxConfiguration& c,CheckBoxInteraction i)noexcept{
        return c.enabled&&i.enabled&&c.motion_enabled;
    }
    [[nodiscard]] static WidgetMotion color_motion(const CheckBoxConfiguration& c)noexcept{
        return {.duration=std::chrono::milliseconds{130},.curve=WidgetMotionCurve::out_cubic,.reduced_motion=widget_motion_value(c.motion).reduced_motion};
    }
    [[nodiscard]] static StyleColor fill_target(const CheckBoxConfiguration& c,CheckBoxInteraction i,bool checked)noexcept{
        const bool enabled=c.enabled&&i.enabled;const auto& p=c.palette;
        auto fill=enabled?(checked?p.checked:p.unchecked):(checked?p.disabled_checked:p.disabled_unchecked);
        if(enabled&&(i.pressed||i.hovered)){
            const auto divisor=checked?(i.pressed?1.2:1.08):(i.pressed?1.18:1.06);
            fill.red/=divisor;fill.green/=divisor;fill.blue/=divisor;
        }
        return fill;
    }
    [[nodiscard]] static std::array<double,4> channels(StyleColor value)noexcept{
        return {value.red,value.green,value.blue,value.alpha};
    }
    [[nodiscard]] StyleColor displayed_fill()const noexcept{
        const auto& c=fill_transition_.value();return {std::clamp(c[0],0.,1.),std::clamp(c[1],0.,1.),
            std::clamp(c[2],0.,1.),std::clamp(c[3],0.,1.)};
    }
    CheckBoxConfiguration configuration_;
    CheckBoxInteraction interaction_;
    WidgetMotionTransition<1> mark_transition_{{.55}};
    WidgetMotionTransition<4> fill_transition_{channels(configuration_.palette.unchecked)};
    bool initialized_=false;
};
using CheckBoxModel=BasicCheckBox<DefaultCheckBoxModelPolicy>;
} // namespace ii::gui
