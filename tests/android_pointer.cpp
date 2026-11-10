#include "../src/Platform/AndroidPointer.h"
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool near(double a,double b){return std::abs(a-b)<.001;}
AndroidPointerSample sample(int id,AndroidPointerTool tool,double x,double y,double pressure=1,
                            double tilt=0,double orientation=0){
    return {.id=id,.tool=tool,.x=x,.y=y,.pressure=pressure,.tilt=tilt,.orientation=orientation};
}
const WidgetInput& only(const AndroidPointerResult& r,WidgetInputKind kind){
    check(r.handled&&r.events.size()==1&&r.events.front().kind==kind,"Unexpected translated pointer event");
    return r.events.front();
}
void pen_and_competing_contacts(){
    AndroidPointerState state;
    AndroidPointerFrame frame;frame.action=AndroidPointerAction::down;frame.device=9;frame.scale=2;frame.time=1.25;
    frame.samples={sample(7,AndroidPointerTool::pen,40,60,.65,std::numbers::pi/4,std::numbers::pi/2)};
    auto down=state.translate(frame);const auto& p=only(down,WidgetInputKind::pointer_down);
    check(p.device==PointerDevice::pen&&p.device_id==((std::uint64_t{9}<<32)|7)&&near(p.x,20)&&near(p.y,30)&&
          near(p.pressure,.65)&&near(p.tilt_x,45)&&near(p.tilt_y,0)&&p.buttons==1&&near(p.timestamp,1.25),
          "Pen identity, scale, pressure, or screen-axis tilt was lost");
    frame.action=AndroidPointerAction::secondary_down;frame.index=1;
    frame.samples.push_back(sample(2,AndroidPointerTool::touch,200,100));
    auto other=state.translate(frame);check(other.handled&&other.events.empty(),"Secondary touch stole the active pen");
    frame.action=AndroidPointerAction::secondary_up;frame.canceled=true;
    check(state.translate(frame).events.empty(),"Palm cancellation canceled the active pen");frame.canceled=false;
    frame.action=AndroidPointerAction::move;frame.index=0;
    frame.samples={sample(2,AndroidPointerTool::touch,300,200),sample(7,AndroidPointerTool::pen,50,80,.8)};
    auto moved=state.translate(frame);const auto& q=only(moved,WidgetInputKind::pointer_move);
    check(q.device_id==p.device_id&&near(q.x,25)&&near(q.y,40)&&near(q.pressure,.8),
          "Pointer index reordering changed the active pen");
    frame.action=AndroidPointerAction::buttons;frame.buttons=32;
    auto barrel=state.translate(frame);
    check(only(barrel,WidgetInputKind::pointer_move).buttons==33,"Stationary stylus barrel button was lost");
    frame.action=AndroidPointerAction::secondary_up;frame.index=1;frame.canceled=true;
    auto canceled=state.translate(frame);only(canceled,WidgetInputKind::pointer_cancel);
    frame.action=AndroidPointerAction::move;frame.canceled=false;frame.samples={sample(2,AndroidPointerTool::touch,4,6)};
    check(state.translate(frame).events.empty(),"A canceled pen transferred capture to a competing touch");
    check(!state.cancel(),"Already canceled pointer remained captured");
}
void release_and_lifecycle(){
    AndroidPointerState state;AndroidPointerFrame frame;frame.action=AndroidPointerAction::down;frame.device=-3;
    frame.samples={sample(15,AndroidPointerTool::eraser,3,4,.4)};
    auto started=state.translate(frame);check(only(started,WidgetInputKind::pointer_down).device==PointerDevice::eraser,"Eraser was not distinguished");
    frame.action=AndroidPointerAction::up;auto ended=state.translate(frame);
    const auto& p=only(ended,WidgetInputKind::pointer_up);
    check(p.device==PointerDevice::eraser&&p.buttons==0&&p.device_id==((std::uint64_t{0xfffffffd}<<32)|15),
          "Release changed the source identity or kept the contact button down");
    check(!state.cancel(),"Released pointer remained captured");
    frame.action=AndroidPointerAction::down;state.translate(frame);
    auto canceled=state.cancel();check(canceled&&canceled->kind==WidgetInputKind::focus_lost,"Lifecycle did not cancel capture");
    check(!state.cancel(),"Lifecycle cancellation was delivered twice");
    frame.action=AndroidPointerAction::down;state.translate(frame);
    frame.action=AndroidPointerAction::move;frame.samples={sample(18,AndroidPointerTool::touch,0,0)};
    only(state.translate(frame),WidgetInputKind::pointer_cancel);
}
void stylus_takes_over_a_touch(){
    AndroidPointerState state;AndroidPointerFrame frame;frame.device=2;frame.action=AndroidPointerAction::down;
    frame.samples={sample(1,AndroidPointerTool::touch,10,10)};state.translate(frame);
    frame.action=AndroidPointerAction::secondary_down;frame.index=1;
    frame.samples.push_back(sample(2,AndroidPointerTool::pen,30,40,.7));
    auto takeover=state.translate(frame);
    check(takeover.events.size()==2&&takeover.events[0].kind==WidgetInputKind::pointer_cancel&&
          takeover.events[1].kind==WidgetInputKind::pointer_down&&takeover.events[1].device==PointerDevice::pen,
          "Stylus did not cancel a competing touch before taking capture");
    frame.action=AndroidPointerAction::move;frame.index=0;frame.samples[1].x=32;
    check(near(only(state.translate(frame),WidgetInputKind::pointer_move).x,32),"Stylus takeover lost the selected pointer");
    state.cancel();
    frame.device=4;frame.action=AndroidPointerAction::down;frame.samples={sample(1,AndroidPointerTool::touch,0,0)};
    state.translate(frame);frame.device=8;frame.samples={sample(3,AndroidPointerTool::eraser,10,10)};
    auto other_device=state.translate(frame);
    check(other_device.events.size()==2&&other_device.events[0].kind==WidgetInputKind::pointer_cancel&&
          other_device.events[1].device==PointerDevice::eraser,"A separate stylus device could not supersede a touch");
}
void hover_wheel_and_buttons(){
    AndroidPointerState state;AndroidPointerFrame frame;frame.action=AndroidPointerAction::hover_enter;frame.device=4;
    frame.samples={sample(1,AndroidPointerTool::pen,12,24,0,0,0)};
    auto hover=state.translate(frame);check(only(hover,WidgetInputKind::pointer_move).device==PointerDevice::pen,"Pen hover was treated as touch");
    frame.action=AndroidPointerAction::hover_exit;only(state.translate(frame),WidgetInputKind::pointer_leave);
    frame.action=AndroidPointerAction::scroll;frame.samples={sample(1,AndroidPointerTool::mouse,12,24,0)};
    frame.scroll_x=-1;frame.scroll_y=2;auto wheel=state.translate(frame);const auto& scroll=only(wheel,WidgetInputKind::scroll);
    check(scroll.device==PointerDevice::mouse&&near(scroll.scroll_x,-48)&&near(scroll.scroll_y,96),"Mouse wheel direction or units changed");
    frame.action=AndroidPointerAction::down;frame.buttons=2;
    only(state.translate(frame),WidgetInputKind::context_request);
    frame.action=AndroidPointerAction::buttons;
    check(only(state.translate(frame),WidgetInputKind::pointer_move).buttons==2,"Stationary mouse button state was lost");
    frame.action=AndroidPointerAction::up;check(state.translate(frame).events.empty(),"Secondary click produced a primary release");
}
void malformed_and_different_devices(){
    AndroidPointerState state;AndroidPointerFrame frame;frame.action=AndroidPointerAction::down;frame.device=1;
    frame.samples={sample(0,AndroidPointerTool::touch,4,8)};state.translate(frame);
    frame.action=AndroidPointerAction::move;frame.device=2;
    check(state.translate(frame).events.empty(),"A different device disturbed the captured pointer");
    frame.device=1;frame.time=std::numeric_limits<double>::quiet_NaN();
    frame.samples[0].pressure=std::numeric_limits<double>::infinity();
    auto move=state.translate(frame);const auto& p=only(move,WidgetInputKind::pointer_move);
    check(near(p.timestamp,0)&&near(p.pressure,0),"Invalid sensor values escaped into widget input");
    frame.action=AndroidPointerAction::secondary_up;frame.index=99;
    bool rejected=false;try{(void)state.translate(frame);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"Invalid Android action index was admitted");
    frame.action=AndroidPointerAction::down;frame.index=0;frame.scale=0;
    rejected=false;try{(void)state.translate(frame);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected&&state.cancel().has_value(),"A malformed replacement press silently discarded the previous capture");
}
void reentrant_takeover(){
    AndroidPointerState state;AndroidPointerFrame frame;frame.device=1;frame.action=AndroidPointerAction::down;
    frame.samples={sample(1,AndroidPointerTool::touch,0,0)};state.translate(frame);
    frame.action=AndroidPointerAction::secondary_down;frame.index=1;frame.samples.push_back(sample(2,AndroidPointerTool::pen,2,2));
    auto result=state.translate(frame);int delivered=0;
    const auto completed=dispatch_android_pointer(state,std::move(result),[&](WidgetInput value){
        ++delivered;if(value.kind==WidgetInputKind::pointer_cancel)state.cancel();return true;
    });
    check(!completed&&delivered==1,"Reentrant cancellation delivered a stale stylus press");
}
void two_finger_gestures(){
    AndroidPointerState state;AndroidPointerFrame frame;frame.device=3;frame.scale=2;
    frame.action=AndroidPointerAction::down;frame.samples={sample(8,AndroidPointerTool::touch,40,40)};
    only(state.translate(frame),WidgetInputKind::pointer_down);
    frame.action=AndroidPointerAction::secondary_down;frame.index=1;
    frame.samples.push_back(sample(5,AndroidPointerTool::touch,80,40));
    check(state.translate(frame).events.empty(),"An unmoved second finger canceled the first press");
    frame.action=AndroidPointerAction::move;frame.index=0;
    frame.samples={sample(5,AndroidPointerTool::touch,100,40),sample(8,AndroidPointerTool::touch,20,40)};
    auto zoom=state.translate(frame);
    check(zoom.events.size()==2&&zoom.events[0].kind==WidgetInputKind::pointer_cancel&&
          zoom.events[1].kind==WidgetInputKind::scroll&&zoom.events[1].control&&zoom.events[1].precise_scroll&&
          zoom.events[1].device==PointerDevice::touch&&near(zoom.events[1].x,30)&&near(zoom.events[1].y,20)&&
          near(std::exp(zoom.events[1].scroll_y/pinch_scroll_units),2),
          "Two-finger pinch lost its capture cancellation, center, or scale");
    auto factor=pinch_scale_factor(zoom.events[1]);
    check(factor&&near(*factor,2),"The public pinch helper lost the zoom factor");
    auto invalid=zoom.events[1];invalid.scroll_y=std::numeric_limits<double>::quiet_NaN();
    check(!pinch_scale_factor(invalid),"The pinch helper accepted a non-finite delta");
    invalid.scroll_y=1e9;check(!pinch_scale_factor(invalid),"The pinch helper returned an infinite scale");
    invalid=zoom.events[1];invalid.device=PointerDevice::mouse;
    check(!pinch_scale_factor(invalid),"The pinch helper interpreted a mouse wheel as touch zoom");
    frame.samples[0].x+=8;frame.samples[1].x+=8;frame.samples[0].y+=4;frame.samples[1].y+=4;
    auto pan=state.translate(frame);const auto& movement=only(pan,WidgetInputKind::scroll);
    check(!movement.control&&movement.precise_scroll&&near(movement.scroll_x,4)&&near(movement.scroll_y,2),
          "Two-finger pan lost its logical centroid movement");
    check(!pinch_scale_factor(movement),"The pinch helper accepted a pan");
    frame.action=AndroidPointerAction::secondary_up;frame.index=0;
    check(state.translate(frame).events.empty(),"A pinch release became a click");
    frame.action=AndroidPointerAction::move;frame.samples.resize(1);
    check(state.translate(frame).events.empty(),"A remaining finger was promoted into a new press");
    frame.action=AndroidPointerAction::up;check(state.translate(frame).events.empty(),"Gesture end emitted an unmatched release");
    frame.action=AndroidPointerAction::down;frame.index=0;
    only(state.translate(frame),WidgetInputKind::pointer_down);
}
void gesture_reentrancy(){
    AndroidPointerState state;AndroidPointerFrame frame;frame.device=4;frame.action=AndroidPointerAction::down;
    frame.samples={sample(1,AndroidPointerTool::touch,10,20)};state.translate(frame);
    frame.action=AndroidPointerAction::secondary_down;frame.index=1;
    frame.samples.push_back(sample(2,AndroidPointerTool::touch,30,20));state.translate(frame);
    frame.action=AndroidPointerAction::move;frame.samples[1].x=60;
    auto events=state.translate(frame);int delivered=0;
    const bool complete=dispatch_android_pointer(state,std::move(events),[&](WidgetInput event){
        ++delivered;if(event.kind==WidgetInputKind::pointer_cancel)(void)state.cancel();return true;
    });
    check(!complete&&delivered==1,"Canceled surface received a stale gesture after reentry");
}
void gesture_boundaries(){
    AndroidPointerState state;AndroidPointerFrame frame;frame.device=5;frame.action=AndroidPointerAction::down;
    frame.samples={sample(1,AndroidPointerTool::touch,10,20)};state.translate(frame);
    frame.action=AndroidPointerAction::secondary_down;frame.index=1;frame.samples.push_back(sample(2,AndroidPointerTool::touch,10,20));state.translate(frame);
    frame.action=AndroidPointerAction::move;frame.samples[1].x=16;
    auto first=state.translate(frame);check(!first.events.empty()&&first.events.front().kind==WidgetInputKind::pointer_cancel,
        "Coincident touches did not safely enter a gesture");
    for(const auto& e:first.events)check(std::isfinite(e.scroll_y),"Coincident touches produced a non-finite scale");
    frame.device=6;frame.samples[1].x=100;check(state.translate(frame).events.empty(),"Another device drove an active pinch");
    frame.device=5;frame.action=AndroidPointerAction::cancel;check(state.translate(frame).events.empty(),"Already canceled capture was canceled twice");
    frame.action=AndroidPointerAction::down;frame.index=0;frame.samples={sample(9,AndroidPointerTool::touch,0,0)};
    only(state.translate(frame),WidgetInputKind::pointer_down);
    frame.action=AndroidPointerAction::secondary_down;frame.index=1;frame.samples.push_back(sample(10,AndroidPointerTool::touch,30,0));state.translate(frame);
    frame.action=AndroidPointerAction::secondary_up;check(state.translate(frame).events.empty(),"An unmoved second finger canceled the original press");
    frame.action=AndroidPointerAction::up;frame.index=0;only(state.translate(frame),WidgetInputKind::pointer_up);
    frame.action=AndroidPointerAction::down;frame.samples={sample(1,AndroidPointerTool::touch,0,0)};state.translate(frame);
    frame.action=AndroidPointerAction::secondary_down;frame.index=1;frame.samples.push_back(sample(2,AndroidPointerTool::touch,20,0));state.translate(frame);
    frame.action=AndroidPointerAction::secondary_up;frame.index=0;
    only(state.translate(frame),WidgetInputKind::pointer_cancel);
    frame.action=AndroidPointerAction::up;frame.samples={sample(2,AndroidPointerTool::touch,20,0)};
    check(state.translate(frame).events.empty(),"A remaining touch became a click after the primary finger lifted");
    frame.action=AndroidPointerAction::down;frame.samples={sample(1,AndroidPointerTool::touch,0,0)};state.translate(frame);
    frame.action=AndroidPointerAction::secondary_down;frame.index=1;frame.samples.push_back(sample(2,AndroidPointerTool::touch,20,0));state.translate(frame);
    frame.action=AndroidPointerAction::move;frame.samples[1].x=40;state.translate(frame);
    frame.action=AndroidPointerAction::secondary_down;frame.index=2;frame.samples.push_back(sample(3,AndroidPointerTool::touch,50,0));
    check(state.translate(frame).events.empty(),"A third finger produced a second cancellation or press");
    frame.action=AndroidPointerAction::move;frame.samples[1].x=80;
    check(state.translate(frame).events.empty(),"A three-finger sequence continued the old pinch");
    frame.action=AndroidPointerAction::up;frame.samples={sample(1,AndroidPointerTool::touch,0,0)};frame.index=0;state.translate(frame);
    frame.action=AndroidPointerAction::down;frame.device=5;frame.samples={sample(1,AndroidPointerTool::touch,0,0)};state.translate(frame);
    frame.action=AndroidPointerAction::secondary_down;frame.index=1;frame.samples.push_back(sample(2,AndroidPointerTool::touch,20,0));state.translate(frame);
    frame.action=AndroidPointerAction::down;frame.device=8;frame.index=0;frame.samples={sample(3,AndroidPointerTool::pen,10,10)};
    auto takeover=state.translate(frame);
    check(takeover.events.size()==2&&takeover.events[0].kind==WidgetInputKind::pointer_cancel&&
          takeover.events[1].device==PointerDevice::pen,"Pen takeover did not cancel a pending two-finger sequence");
    frame.action=AndroidPointerAction::move;frame.device=5;
    frame.samples={sample(1,AndroidPointerTool::touch,0,0),sample(2,AndroidPointerTool::touch,80,0)};
    check(state.translate(frame).events.empty(),"An old touch gesture reactivated during a pen capture");
    (void)state.cancel();frame.action=AndroidPointerAction::down;frame.device=9;frame.index=0;
    frame.samples={sample(1,AndroidPointerTool::touch,0,0)};state.translate(frame);
    frame.action=AndroidPointerAction::secondary_down;frame.index=1;
    frame.samples.push_back(sample(2,AndroidPointerTool::touch,1e-300,0));state.translate(frame);
    frame.action=AndroidPointerAction::move;frame.samples[1].x=1e300;
    for(const auto& event:state.translate(frame).events)
        check(std::isfinite(event.scroll_x)&&std::isfinite(event.scroll_y),"Finite contacts overflowed gesture scale conversion");
}
}
int main(){pen_and_competing_contacts();release_and_lifecycle();stylus_takes_over_a_touch();hover_wheel_and_buttons();malformed_and_different_devices();reentrant_takeover();two_finger_gestures();gesture_reentrancy();gesture_boundaries();}
