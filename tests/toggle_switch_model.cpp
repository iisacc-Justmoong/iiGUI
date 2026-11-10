#include <iiGUI/ToggleSwitchModel.h>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool near(double a,double b){return std::abs(a-b)<1e-6;}
}
int main(){
    using namespace std::chrono_literals;
    using Clock=std::chrono::steady_clock;
    const auto start=Clock::time_point{};
    ii::gui::ToggleSwitchModel toggle;
    ii::gui::ToggleSwitchConfiguration config;
    config.text="Snap";config.motion.duration=320ms;config.motion.curve=ii::gui::WidgetMotionCurve::linear;
    require(toggle.set_configuration(config,start),"Valid switch rejected");
    require(toggle.visual({}).track==config.palette.off&&toggle.visual({.hovered=true}).track==config.palette.off_hover&&
            toggle.visual({.hovered=true,.pressed=true}).track==config.palette.off_pressed,
            "Off, hover or pressed palette is incorrect");
    auto layout=toggle.geometry(100,22,28,13);
    require(layout&&near(layout->track.width,38)&&near(layout->track.height,22)&&
            near(layout->knob.x,2)&&near(layout->knob.width,18)&&near(layout->text.x,46),
            "Default switch geometry is incorrect");
    require(toggle.activate(start)==true&&toggle.checked()&&toggle.animating(),"Switch did not activate");
    require(toggle.visual({}).track==config.palette.on&&toggle.visual({.hovered=true}).track==config.palette.on_hover,
            "On palette is incorrect");
    require(toggle.advance(start+160ms)&&near(toggle.position(),.5),"Switch did not interpolate");
    require(toggle.activate(start+160ms)==false&&!toggle.checked(),"Switch did not retarget during motion");
    require(toggle.advance(start+320ms)&&near(toggle.position(),.25),"Switch retarget lost its displayed position");
    require(!toggle.advance(start+480ms)&&near(toggle.position(),0),"Switch did not reach its target");
    require(toggle.begin_drag(5,start+480ms)&&toggle.drag_to(21)&&near(toggle.position(),1),"Switch drag did not move the knob");
    require(toggle.end_drag(true,start+481ms)==true&&toggle.checked(),"Dragging right did not commit on");
    config=toggle.configuration();config.right_to_left=true;config.motion_enabled=false;
    require(toggle.set_configuration(config,start+482ms),"Right-to-left switch rejected");
    layout=toggle.geometry(100,22,28,13);
    require(layout&&near(layout->knob.x,2),"RTL switch did not mirror the on position");
    require(toggle.begin_drag(21,start+483ms)&&toggle.drag_to(37)&&near(toggle.position(),0),"RTL drag was not mirrored");
    require(toggle.end_drag(false,start+484ms)==std::nullopt&&toggle.checked()&&near(toggle.position(),1),
            "Canceled drag did not preserve checked state");
    config=toggle.configuration();config.enabled=false;
    require(toggle.set_configuration(config,start+485ms)&&!toggle.activate(start+486ms)&&!toggle.begin_drag(0,start+486ms),
            "Disabled switch admitted interaction");
    require(toggle.visual({.hovered=true,.pressed=true}).track==config.palette.disabled,
            "Disabled palette did not override interaction");
    const auto previous=toggle.configuration();auto invalid=previous;invalid.knob_size=100;
    require(!toggle.set_configuration(invalid,start+487ms)&&toggle.configuration()==previous,
            "Invalid switch geometry changed retained state");
    invalid=previous;invalid.palette.on.alpha=std::numeric_limits<double>::quiet_NaN();
    require(!toggle.set_configuration(invalid,start+488ms),"Invalid color accepted");
    require(!toggle.geometry(-1,22,28,13),"Invalid allocation accepted");
}
