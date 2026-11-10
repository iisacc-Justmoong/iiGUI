#include "../src/Platform/AndroidPointerNative.h"
#include <cmath>
#include <stdexcept>
#include <vector>

using namespace ii::gui;
using namespace ii::gui::detail;
struct NativePoint {int id=0,tool=AMOTION_EVENT_TOOL_TYPE_FINGER;float x=0,y=0,pressure=0,orientation=0,tilt=0;};
struct NativeHistory {std::int64_t time=0;std::vector<NativePoint> points;};
struct AInputEvent {
    int device=0,source=0,action=0,flags=0,meta=0,buttons=0;
    std::int64_t time=0;float sx=0,sy=0;
    std::vector<NativePoint> points;
    std::vector<NativeHistory> history;
};
extern "C" {
std::int32_t AInputEvent_getDeviceId(const AInputEvent* e){return e->device;}
std::int32_t AInputEvent_getSource(const AInputEvent* e){return e->source;}
std::int32_t AMotionEvent_getAction(const AInputEvent* e){return e->action;}
std::int32_t AMotionEvent_getFlags(const AInputEvent* e){return e->flags;}
std::int32_t AMotionEvent_getMetaState(const AInputEvent* e){return e->meta;}
std::int32_t AMotionEvent_getButtonState(const AInputEvent* e){return e->buttons;}
std::int64_t AMotionEvent_getEventTime(const AInputEvent* e){return e->time;}
std::size_t AMotionEvent_getPointerCount(const AInputEvent* e){return e->points.size();}
std::int32_t AMotionEvent_getPointerId(const AInputEvent* e,std::size_t i){return e->points.at(i).id;}
std::int32_t AMotionEvent_getToolType(const AInputEvent* e,std::size_t i){return e->points.at(i).tool;}
float AMotionEvent_getX(const AInputEvent* e,std::size_t i){return e->points.at(i).x;}
float AMotionEvent_getY(const AInputEvent* e,std::size_t i){return e->points.at(i).y;}
float AMotionEvent_getPressure(const AInputEvent* e,std::size_t i){return e->points.at(i).pressure;}
float AMotionEvent_getOrientation(const AInputEvent* e,std::size_t i){return e->points.at(i).orientation;}
float AMotionEvent_getAxisValue(const AInputEvent* e,std::int32_t axis,std::size_t i){return axis==AMOTION_EVENT_AXIS_TILT?e->points.at(i).tilt:axis==AMOTION_EVENT_AXIS_HSCROLL?e->sx:e->sy;}
std::size_t AMotionEvent_getHistorySize(const AInputEvent* e){return e->history.size();}
std::int64_t AMotionEvent_getHistoricalEventTime(const AInputEvent* e,std::size_t h){return e->history.at(h).time;}
float AMotionEvent_getHistoricalX(const AInputEvent* e,std::size_t i,std::size_t h){return e->history.at(h).points.at(i).x;}
float AMotionEvent_getHistoricalY(const AInputEvent* e,std::size_t i,std::size_t h){return e->history.at(h).points.at(i).y;}
float AMotionEvent_getHistoricalPressure(const AInputEvent* e,std::size_t i,std::size_t h){return e->history.at(h).points.at(i).pressure;}
float AMotionEvent_getHistoricalOrientation(const AInputEvent* e,std::size_t i,std::size_t h){return e->history.at(h).points.at(i).orientation;}
float AMotionEvent_getHistoricalAxisValue(const AInputEvent* e,std::int32_t,std::size_t i,std::size_t h){return e->history.at(h).points.at(i).tilt;}
}
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
    AInputEvent raw;raw.device=17;raw.source=AINPUT_SOURCE_TOUCHSCREEN;raw.action=AMOTION_EVENT_ACTION_DOWN;raw.time=1000000000;
    raw.points={{4,AMOTION_EVENT_TOOL_TYPE_STYLUS,20,40,.4f,0,.5f}};
    AndroidPointerState state;auto start=android_pointer_frames(&raw,2);
    check(start&&start->size()==1,"NDK pointer down was not decoded");
    auto down=state.translate(start->front());check(down.events.size()==1&&down.events[0].device==PointerDevice::pen,"NDK stylus was not passed to the model");
    raw.action=AMOTION_EVENT_ACTION_MOVE;raw.time=3000000000;raw.meta=AMETA_SHIFT_LEFT_ON;
    raw.points[0].x=60;raw.points[0].pressure=.8f;
    raw.history={{1500000000,{{4,AMOTION_EVENT_TOOL_TYPE_STYLUS,30,50,.5f,0,.6f}}},
                 {2000000000,{{4,AMOTION_EVENT_TOOL_TYPE_STYLUS,40,60,.6f,0,.7f}}}};
    auto frames=android_pointer_frames(&raw,2);check(frames&&frames->size()==3,"Batched pointer history was dropped");
    std::vector<WidgetInput> events;for(const auto& frame:*frames){auto value=state.translate(frame);events.insert(events.end(),value.events.begin(),value.events.end());}
    check(events.size()==3&&events[0].x==15&&events[1].x==20&&events[2].x==30&&events[0].timestamp==1.5&&
          events[1].timestamp==2&&events[2].timestamp==3&&events[0].shift,
          "Historical pointer samples lost order, scale, time, or modifiers");
    raw.history.clear();raw.action=AMOTION_EVENT_ACTION_BUTTON_PRESS;raw.buttons=32;
    auto button=android_pointer_frames(&raw,2);auto barrel=state.translate(button->front());
    check(barrel.events.size()==1&&barrel.events[0].kind==WidgetInputKind::pointer_move&&barrel.events[0].buttons==33,
          "NDK stylus button transition was lost or activated twice");
    raw.history.clear();raw.points.push_back({9,AMOTION_EVENT_TOOL_TYPE_FINGER,1,2,1,0,0});
    raw.action=AMOTION_EVENT_ACTION_POINTER_UP|(0<<AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);raw.flags=32;
    auto canceled=android_pointer_frames(&raw,2);auto result=state.translate(canceled->front());
    check(result.events.size()==1&&result.events[0].kind==WidgetInputKind::pointer_cancel,
          "NDK canceled pointer-up incorrectly became a click");
    raw.action=AMOTION_EVENT_ACTION_HOVER_MOVE;raw.flags=0;raw.source=AINPUT_SOURCE_MOUSE;
    raw.points={{1,0,2,4,0,0,0}};auto unknown=android_pointer_frames(&raw,2);
    check(unknown&&unknown->front().samples.front().tool==AndroidPointerTool::mouse,"Unknown mouse tool lost its source");
    raw.action=99;check(!android_pointer_frames(&raw,1),"Unsupported NDK action was consumed");
    raw.action=AMOTION_EVENT_ACTION_MOVE;raw.source=AINPUT_SOURCE_JOYSTICK;
    check(!android_pointer_frames(&raw,1),"A joystick motion was consumed as a pointer");
    raw.source=AINPUT_SOURCE_TOUCHSCREEN;
    raw.action=AMOTION_EVENT_ACTION_MOVE;raw.history.resize(4097);
    bool rejected=false;try{(void)android_pointer_frames(&raw,1);}catch(const std::length_error&){rejected=true;}
    check(rejected,"Unbounded Android motion history was admitted");
    AndroidPointerState touch;raw.history.clear();raw.flags=0;raw.meta=0;raw.buttons=0;
    raw.action=AMOTION_EVENT_ACTION_DOWN;raw.points={{1,AMOTION_EVENT_TOOL_TYPE_FINGER,20,40,1,0,0}};
    (void)touch.translate(android_pointer_frames(&raw,2)->front());
    raw.points.push_back({2,AMOTION_EVENT_TOOL_TYPE_FINGER,60,40,1,0,0});
    raw.action=AMOTION_EVENT_ACTION_POINTER_DOWN|(1<<AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
    check(touch.translate(android_pointer_frames(&raw,2)->front()).events.empty(),"Second NDK finger prematurely canceled a press");
    raw.action=AMOTION_EVENT_ACTION_MOVE;raw.points[0].x=0;raw.points[1].x=80;
    auto gesture=touch.translate(android_pointer_frames(&raw,2)->front());
    check(gesture.events.size()==2&&gesture.events[0].kind==WidgetInputKind::pointer_cancel&&
          gesture.events[1].control&&std::abs(pinch_scale_factor(gesture.events[1]).value_or(0)-2)<.001,
          "NDK two-pointer frame did not reach the pinch contract");
}
