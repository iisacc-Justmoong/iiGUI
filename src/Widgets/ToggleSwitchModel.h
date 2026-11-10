#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/WidgetMotion.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
namespace ii::gui {
struct ToggleSwitchPalette final {
    WidgetColor on,on_hover,on_pressed,off,off_hover,off_pressed,disabled,knob,disabled_knob,text,disabled_text,focus;
    friend bool operator==(const ToggleSwitchPalette&,const ToggleSwitchPalette&)=default;
};
struct ToggleSwitchConfiguration final {
    std::string text,accessibility_name;
    bool enabled=true,checked=false,right_to_left=false,motion_enabled=true;
    double track_width=38,track_height=22,track_padding=2,knob_size=18,track_radius=20,knob_radius=9;
    double label_gap=8,font_size=13,line_height=13;
    unsigned font_weight=500;
    WidgetMotion motion{.duration=std::chrono::milliseconds{320},.curve=WidgetMotionCurve::out_back,.overshoot=1.45};
    ToggleSwitchPalette palette;
    friend bool operator==(const ToggleSwitchConfiguration&,const ToggleSwitchConfiguration&)=default;
};
struct ToggleSwitchInteraction final {bool enabled=true,hovered=false,pressed=false,focused=false;};
struct ToggleSwitchVisual final {WidgetColor track,knob,text,focus;bool focused=false;};
struct ToggleSwitchRect final {double x=0,y=0,width=0,height=0;};
struct ToggleSwitchGeometry final {
    ToggleSwitchRect track,knob,text;double track_radius=0,knob_radius=0;
    bool draw_track=false,draw_knob=false,draw_text=false;
};
struct DefaultToggleSwitchModelPolicy final {
    using Configuration=ToggleSwitchConfiguration;using Interaction=ToggleSwitchInteraction;using Visual=ToggleSwitchVisual;
    using Geometry=ToggleSwitchGeometry;using Color=WidgetColor;
    [[nodiscard]] static bool valid_text(std::string_view text)noexcept {
        return text.size()<=65536&&text.find('\0')==std::string_view::npos&&ii::core::valid_utf8(text);
    }
};
template<class Policy>
class BasicToggleSwitch final {
    using ToggleSwitchConfiguration=typename Policy::Configuration;using ToggleSwitchInteraction=typename Policy::Interaction;
    using ToggleSwitchVisual=typename Policy::Visual;using ToggleSwitchGeometry=typename Policy::Geometry;using StyleColor=typename Policy::Color;
public:
    using TimePoint=std::chrono::steady_clock::time_point;
    [[nodiscard]] const ToggleSwitchConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] bool checked()const noexcept{return configuration_.checked;}
    [[nodiscard]] bool dragging()const noexcept{return dragging_;}
    [[nodiscard]] bool drag_moved()const noexcept{return dragging_&&drag_moved_;}
    [[nodiscard]] bool pressed()const noexcept{return dragging_;}
    [[nodiscard]] bool animating()const noexcept{return transition_.active();}
    [[nodiscard]] double position()const noexcept{return std::clamp(transition_.value()[0],0.0,1.0);}
    [[nodiscard]] static bool validate(const ToggleSwitchConfiguration& c)noexcept{
        const auto bound=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
        const auto color=[&](StyleColor v){return bound(v.red,0,1)&&bound(v.green,0,1)&&bound(v.blue,0,1)&&bound(v.alpha,0,1);};
        if(!Policy::valid_text(c.text)||!Policy::valid_text(c.accessibility_name)||
           !bound(c.track_width,1,8192)||!bound(c.track_height,1,8192)||!bound(c.track_padding,0,8192)||
           !bound(c.knob_size,1,8192)||!bound(c.track_radius,0,8192)||!bound(c.knob_radius,0,8192)||
           !bound(c.label_gap,0,8192)||!bound(c.font_size,1,256)||!bound(c.line_height,0,8192)||
           c.knob_size+2*c.track_padding>c.track_width||c.knob_size+2*c.track_padding>c.track_height||
           c.track_width+(c.text.empty()?0:c.label_gap)>8192||
           (c.font_weight!=400&&c.font_weight!=500&&c.font_weight!=600&&c.font_weight!=700)||!valid_widget_motion(widget_motion_value(c.motion)))return false;
        const auto& p=c.palette;
        return color(p.on)&&color(p.on_hover)&&color(p.on_pressed)&&color(p.off)&&color(p.off_hover)&&
               color(p.off_pressed)&&color(p.disabled)&&color(p.knob)&&color(p.disabled_knob)&&
               color(p.text)&&color(p.disabled_text)&&color(p.focus);
    }
    bool set_configuration(ToggleSwitchConfiguration value,TimePoint now){
        if(!validate(value))return false;
        const auto target=value.checked?1.0:0.0;
        auto next=transition_;
        bool drag=dragging_;
        if(!initialized_){(void)next.reset({target});drag=false;}
        else {
            if(!value.enabled||value.checked!=configuration_.checked)drag=false;
            if(!drag){const auto changed=next.retarget({target},widget_motion_value(value.motion),value.enabled&&value.motion_enabled,now);if(!changed)return false;}
        }
        configuration_=std::move(value);transition_=std::move(next);dragging_=drag;initialized_=true;return true;
    }
    [[nodiscard]] std::optional<bool> activate(TimePoint now){
        if(!configuration_.enabled)return std::nullopt;
        auto value=configuration_;value.checked=!value.checked;
        if(!set_configuration(std::move(value),now))return std::nullopt;
        return configuration_.checked;
    }
    [[nodiscard]] bool begin_drag(double x,TimePoint now)noexcept{
        if(!initialized_||!configuration_.enabled||dragging_||!std::isfinite(x))return false;
        (void)transition_.advance(now);dragging_=true;drag_moved_=false;drag_origin_=x;drag_position_=position();return true;
    }
    [[nodiscard]] bool drag_to(double x)noexcept{
        if(!dragging_||!std::isfinite(x))return false;
        const auto delta=x-drag_origin_;
        if(std::abs(delta)<3&&!drag_moved_)return true;
        drag_moved_=true;
        const auto travel=configuration_.track_width-2*configuration_.track_padding-configuration_.knob_size;
        const auto direction=configuration_.right_to_left?-1.0:1.0;
        const auto target=travel>0?std::clamp(drag_position_+direction*delta/travel,0.0,1.0):drag_position_;
        (void)transition_.reset({target});return true;
    }
    [[nodiscard]] std::optional<bool> end_drag(bool commit,TimePoint now){
        if(!dragging_)return std::nullopt;
        dragging_=false;
        const auto previous=configuration_.checked;
        auto value=configuration_;
        if(commit)value.checked=drag_moved_?position()>=.5:!previous;
        if(!set_configuration(std::move(value),now))return std::nullopt;
        if(commit&&previous!=configuration_.checked)return configuration_.checked;
        return std::nullopt;
    }
    [[nodiscard]] bool advance(TimePoint now)noexcept{return dragging_?false:transition_.advance(now);}
    void finish_animation()noexcept{transition_.settle();}
    [[nodiscard]] ToggleSwitchVisual visual(ToggleSwitchInteraction i)const noexcept{
        const auto& p=configuration_.palette;const bool enabled=configuration_.enabled&&i.enabled;
        if(!enabled)return {p.disabled,p.disabled_knob,p.disabled_text,p.focus,false};
        const bool down=i.pressed||pressed();
        const auto track=configuration_.checked?(down?p.on_pressed:i.hovered?p.on_hover:p.on):
                                                  (down?p.off_pressed:i.hovered?p.off_hover:p.off);
        return {track,p.knob,p.text,p.focus,i.focused};
    }
    [[nodiscard]] std::optional<ToggleSwitchGeometry> geometry(double width,double height,double text_width,double text_height)const noexcept{
        const auto extent=[](double n){return std::isfinite(n)&&n>=0&&n<=8192;};
        const auto metric=[](double n){return std::isfinite(n)&&n>=0;};
        if(!extent(width)||!extent(height)||!metric(text_width)||!metric(text_height))return std::nullopt;
        ToggleSwitchGeometry result;
        const auto track_width=(std::min)(configuration_.track_width,width),track_height=(std::min)(configuration_.track_height,height);
        const auto knob=(std::min)({configuration_.knob_size,track_width,track_height});
        const auto padding=(std::min)(configuration_.track_padding,(std::max)(0.0,(track_width-knob)/2));
        const auto travel=(std::max)(0.0,track_width-2*padding-knob);
        const auto physical=configuration_.right_to_left?1-position():position();
        result.track={0,(height-track_height)/2,track_width,track_height};
        result.knob={padding+travel*physical,(height-knob)/2,knob,knob};
        result.track_radius=(std::min)(configuration_.track_radius,(std::min)(track_width,track_height)/2);
        result.knob_radius=(std::min)(configuration_.knob_radius,knob/2);
        result.draw_track=track_width>0&&track_height>0;result.draw_knob=knob>0;
        if(!configuration_.text.empty()&&text_width>0&&text_height>0){
            const auto x=(std::min)(width,track_width+configuration_.label_gap);
            const auto w=(std::min)(text_width,(std::max)(0.0,width-x)),h=(std::min)(height,text_height);
            result.text={x,(height-h)/2,w,h};result.draw_text=w>0&&h>0;
        }
        return result;
    }
private:
    ToggleSwitchConfiguration configuration_;
    WidgetMotionTransition<1> transition_{{0}};
    bool initialized_=false,dragging_=false,drag_moved_=false;
    double drag_origin_=0,drag_position_=0;
};
using ToggleSwitchModel=BasicToggleSwitch<DefaultToggleSwitchModelPolicy>;
} // namespace ii::gui
