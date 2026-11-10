#include <iiGUI/FocusRing.h>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
    using namespace std::chrono_literals;
    using namespace ii::gui;
    const auto start=std::chrono::steady_clock::time_point{};
    FocusRing ring;
    FocusRingOptions options{.width=100,.height=40,.radius=6,.stroke=2,.color={.5,.25,.75,.8},.active=false,
                             .motion_enabled=true,.motion={.duration=100ms,.curve=WidgetMotionCurve::linear}};
    require(ring.update(options,start),"Valid focus-ring options were rejected");
    require(!ring.animating()&&ring.drawing({100,40}).panels.empty(),"Inactive ring requested frames or emitted ink");
    options.active=true;
    require(ring.update(options,start)&&ring.animating(),"A focus change did not start the transition");
    require(ring.advance(start+50ms),"Focus transition stopped before its final frame");
    const auto halfway=ring.drawing({100,40});
    require(halfway.panels.size()==1&&std::abs(halfway.panels[0].top.alpha-.4)<1e-5,
            "Focus ring did not draw its interpolated opacity");
    require(!ring.advance(start+100ms)&&!ring.animating(),"Settled focus ring kept requesting frames");
    const auto settled=ring.drawing({100,40});
    require(settled.panels.size()==1&&std::abs(settled.panels[0].bounds.width-100)<1e-5&&
            std::abs(settled.panels[0].stroke_width-2)<1e-5,"Focus ring geometry or border is incorrect");
    auto invalid=options;invalid.stroke=std::numeric_limits<double>::quiet_NaN();
    require(!ring.update(invalid,start+100ms)&&ring.drawing({100,40})==settled,"Invalid options modified a committed ring");
    options.active=false;options.motion_enabled=false;
    require(ring.update(options,start+100ms)&&!ring.animating()&&ring.drawing({100,40}).panels.empty(),
            "Disabled motion did not apply the new state immediately");
}
