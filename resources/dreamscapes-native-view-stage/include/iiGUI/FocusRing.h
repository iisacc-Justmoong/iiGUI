#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/WidgetMotion.h>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace ii::gui {
struct FocusRingOptions final {
    double width=0,height=0,radius=0,stroke=0;
    WidgetColor color{0,0,0,0};
    bool active=false,motion_enabled=true;
    WidgetMotion motion;
    friend bool operator==(const FocusRingOptions&,const FocusRingOptions&)=default;
};

// The caller drives advance only while animating; no timer or polling is owned.
class FocusRing final {
public:
    using TimePoint=std::chrono::steady_clock::time_point;
    [[nodiscard]] bool update(const FocusRingOptions& options,TimePoint now)noexcept {
        const auto metric=[](double value){return std::isfinite(value)&&value>=0&&value<=8192;};
        const auto channel=[](double value){return std::isfinite(value)&&value>=0&&value<=1;};
        if(!metric(options.width)||!metric(options.height)||!metric(options.radius)||!metric(options.stroke)||
           !channel(options.color.red)||!channel(options.color.green)||!channel(options.color.blue)||!channel(options.color.alpha)||
           !valid_widget_motion(options.motion))return false;
        if(!initialized_){
            if(!opacity_.reset({options.active?1.:0.}))return false;
            initialized_=true;
        }else if(!opacity_.retarget({options.active?1.:0.},options.motion,options.motion_enabled,now))return false;
        options_=options;return true;
    }
    [[nodiscard]] bool advance(TimePoint now)noexcept{return opacity_.advance(now);}
    [[nodiscard]] bool animating()const noexcept{return opacity_.active();}
    void settle()noexcept{opacity_.settle();}
    [[nodiscard]] const FocusRingOptions& options()const noexcept{return options_;}
    [[nodiscard]] WidgetDrawing drawing(Size size)const {
        WidgetDrawing drawing;
        if(!std::isfinite(size.width)||!std::isfinite(size.height)||size.width<=0||size.height<=0)return drawing;
        const auto opacity=std::clamp(opacity_.value()[0],0.,1.);
        if(opacity<=0||options_.color.alpha<=0||options_.stroke<=0)return drawing;
        const auto scale=.96+.04*opacity,width=size.width*scale,height=size.height*scale;
        const auto linear=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
        const Color color{float(linear(options_.color.red)),float(linear(options_.color.green)),
                          float(linear(options_.color.blue)),float(options_.color.alpha*opacity)};
        const Rect bounds{float((size.width-width)/2),float((size.height-height)/2),float(width),float(height)};
        const auto radius=float((std::min)({options_.radius,width/2,height/2}));
        const auto stroke=float((std::min)({options_.stroke,width/2,height/2}));
        drawing.panels.push_back({bounds,color,color,radius,stroke});return drawing;
    }
private:
    FocusRingOptions options_;
    WidgetMotionTransition<1> opacity_;
    bool initialized_=false;
};
} // namespace ii::gui
