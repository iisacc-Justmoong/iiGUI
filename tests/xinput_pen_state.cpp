#include "iiGUI/XInputPenState.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
bool close(double a,double b) { return std::abs(a-b)<1e-12; }
void sparseEvents() {
    XInputPenState pen;
    check(pen.merge({.pressure=.37,.tilt_x=-20,.tilt_y=31,.rotation=12}),"initial update rejected");
    WidgetInput motion{.kind=WidgetInputKind::pointer_move,.device=PointerDevice::pen}; pen.fill(motion);
    check(close(motion.pressure,.37) && close(motion.tilt_x,-20) && close(motion.tilt_y,31) &&
          close(motion.rotation,12),"initial pen state missing");
    check(pen.merge({}),"empty update rejected");
    WidgetInput down{.kind=WidgetInputKind::pointer_down,.device=PointerDevice::pen}; pen.fill(down);
    check(close(down.pressure,.37) && close(down.tilt_x,-20) && close(down.tilt_y,31) &&
          close(down.rotation,12),"sparse button event lost pen state");
    check(pen.merge({.tilt_y=5}),"one-axis update rejected");
    WidgetInput moved{.kind=WidgetInputKind::pointer_move,.device=PointerDevice::pen}; pen.fill(moved);
    check(close(moved.pressure,.37) && close(moved.tilt_x,-20) && close(moved.tilt_y,5) &&
          close(moved.rotation,12),"partial update reset other axes");
    check(pen.merge({.pressure=0,.tilt_x=0,.tilt_y=0,.rotation=0}),"explicit zeros rejected");
    WidgetInput up{.kind=WidgetInputKind::pointer_up,.device=PointerDevice::pen}; pen.fill(up);
    check(close(up.pressure,0) && close(up.tilt_x,0) && close(up.tilt_y,0) && close(up.rotation,0),
          "zero samples were mistaken for missing axes");
}
void isolationAndInvalidValues() {
    XInputPenState first,second;
    check(first.merge({.pressure=.5,.tilt_x=12}),"first device setup failed");
    WidgetInput other;second.fill(other);
    check(close(other.pressure,1) && close(other.tilt_x,0),"device state leaked");
    const auto nan=std::numeric_limits<double>::quiet_NaN();
    check(!first.merge({.pressure=.8,.tilt_y=nan}),"nonfinite update accepted");
    check(!first.merge({.pressure=-.01}) && !first.merge({.pressure=1.01}) &&
          !first.merge({.tilt_x=91}) && !first.merge({.tilt_y=-91}),"out-of-range update accepted");
    other={};first.fill(other);
    check(close(other.pressure,.5) && close(other.tilt_x,12) && close(other.tilt_y,0),
          "rejected update changed state");
    check(second.merge({.pressure=0,.tilt_x=-90,.tilt_y=90,.rotation=720}),"valid boundaries rejected");
    other={};second.fill(other);
    check(close(other.pressure,0) && close(other.tilt_x,-90) && close(other.tilt_y,90) &&
          close(other.rotation,720),"valid boundaries lost");
}
}
int main() {
    try { sparseEvents(); isolationAndInvalidValues(); std::cout<<"XINPUT_PEN_STATE_OK\n"; }
    catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
