#pragma once
#include "AndroidPointer.h"
#include <android/input.h>
#include <optional>

namespace ii::gui::detail {
// MotionEvent.FLAG_CANCELED (API 33) is absent from the public NDK header;
// the NDK's stable palm tool value is absent from some older headers.
inline constexpr std::int32_t android_motion_flag_canceled=0x20;
inline constexpr std::int32_t android_motion_tool_palm=5;

inline std::optional<std::vector<AndroidPointerFrame>> android_pointer_frames(const AInputEvent* event,double scale){
    if(!event)throw std::invalid_argument("Android motion event is null");
    const auto source=std::uint32_t(AInputEvent_getSource(event));
    if((source&AINPUT_SOURCE_CLASS_MASK)!=AINPUT_SOURCE_CLASS_POINTER)return std::nullopt;
    const auto raw=AMotionEvent_getAction(event),action=raw&AMOTION_EVENT_ACTION_MASK;
    AndroidPointerAction translated;
    switch(action){
    case AMOTION_EVENT_ACTION_DOWN:translated=AndroidPointerAction::down;break;
    case AMOTION_EVENT_ACTION_UP:translated=AndroidPointerAction::up;break;
    case AMOTION_EVENT_ACTION_POINTER_DOWN:translated=AndroidPointerAction::secondary_down;break;
    case AMOTION_EVENT_ACTION_POINTER_UP:translated=AndroidPointerAction::secondary_up;break;
    case AMOTION_EVENT_ACTION_MOVE:translated=AndroidPointerAction::move;break;
    case AMOTION_EVENT_ACTION_CANCEL:translated=AndroidPointerAction::cancel;break;
    case AMOTION_EVENT_ACTION_HOVER_ENTER:translated=AndroidPointerAction::hover_enter;break;
    case AMOTION_EVENT_ACTION_HOVER_MOVE:translated=AndroidPointerAction::hover_move;break;
    case AMOTION_EVENT_ACTION_HOVER_EXIT:translated=AndroidPointerAction::hover_exit;break;
    case AMOTION_EVENT_ACTION_SCROLL:translated=AndroidPointerAction::scroll;break;
    case AMOTION_EVENT_ACTION_BUTTON_PRESS:case AMOTION_EVENT_ACTION_BUTTON_RELEASE:
        translated=AndroidPointerAction::buttons;break;
    default:return std::nullopt;
    }
    const auto count=AMotionEvent_getPointerCount(event);
    if(count>32)throw std::length_error("Android motion pointer budget exceeded");
    if(!count&&translated!=AndroidPointerAction::cancel)throw std::invalid_argument("Android motion has no pointer");
    const auto history=AMotionEvent_getHistorySize(event);
    if(history>4096)throw std::length_error("Android motion history budget exceeded");
    const auto meta=AMotionEvent_getMetaState(event);
    const auto make=[&](AndroidPointerAction current,std::optional<std::size_t> historical){
        AndroidPointerFrame frame;frame.action=current;frame.device=AInputEvent_getDeviceId(event);frame.scale=scale;
        frame.index=(current==AndroidPointerAction::secondary_down||current==AndroidPointerAction::secondary_up)
            ?std::size_t((raw&AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)>>AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT):0;
        frame.time=double(historical?AMotionEvent_getHistoricalEventTime(event,*historical):AMotionEvent_getEventTime(event))/1e9;
        frame.buttons=std::uint32_t(AMotionEvent_getButtonState(event));
        frame.shift=(meta&(AMETA_SHIFT_ON|AMETA_SHIFT_LEFT_ON|AMETA_SHIFT_RIGHT_ON))!=0;
        frame.control=(meta&(AMETA_CTRL_ON|AMETA_CTRL_LEFT_ON|AMETA_CTRL_RIGHT_ON))!=0;
        frame.alt=(meta&(AMETA_ALT_ON|AMETA_ALT_LEFT_ON|AMETA_ALT_RIGHT_ON))!=0;
        frame.meta=(meta&(AMETA_META_ON|AMETA_META_LEFT_ON|AMETA_META_RIGHT_ON))!=0;
        frame.canceled=(AMotionEvent_getFlags(event)&android_motion_flag_canceled)!=0;
        if(current==AndroidPointerAction::scroll){
            frame.scroll_x=AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_HSCROLL,0);
            frame.scroll_y=AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_VSCROLL,0);
        }
        frame.samples.reserve(count);
        for(std::size_t i=0;i<count;++i){
            AndroidPointerSample point;point.id=AMotionEvent_getPointerId(event,i);
            switch(AMotionEvent_getToolType(event,i)){
            case AMOTION_EVENT_TOOL_TYPE_FINGER:point.tool=AndroidPointerTool::touch;break;
            case AMOTION_EVENT_TOOL_TYPE_MOUSE:point.tool=AndroidPointerTool::mouse;break;
            case AMOTION_EVENT_TOOL_TYPE_STYLUS:point.tool=AndroidPointerTool::pen;break;
            case AMOTION_EVENT_TOOL_TYPE_ERASER:point.tool=AndroidPointerTool::eraser;break;
            case android_motion_tool_palm:point.tool=AndroidPointerTool::palm;break;
            default:point.tool=(source&AINPUT_SOURCE_MOUSE)==AINPUT_SOURCE_MOUSE?AndroidPointerTool::mouse:AndroidPointerTool::touch;break;
            }
            if(historical){
                point.x=AMotionEvent_getHistoricalX(event,i,*historical);point.y=AMotionEvent_getHistoricalY(event,i,*historical);
                point.pressure=AMotionEvent_getHistoricalPressure(event,i,*historical);
                point.orientation=AMotionEvent_getHistoricalOrientation(event,i,*historical);
                point.tilt=AMotionEvent_getHistoricalAxisValue(event,AMOTION_EVENT_AXIS_TILT,i,*historical);
            }else{
                point.x=AMotionEvent_getX(event,i);point.y=AMotionEvent_getY(event,i);
                point.pressure=AMotionEvent_getPressure(event,i);point.orientation=AMotionEvent_getOrientation(event,i);
                point.tilt=AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_TILT,i);
            }
            frame.samples.push_back(point);
        }
        return frame;
    };
    const bool moving=translated==AndroidPointerAction::move||translated==AndroidPointerAction::up||translated==AndroidPointerAction::secondary_up;
    std::vector<AndroidPointerFrame> frames;frames.reserve((moving?history:0)+1);
    if(moving)for(std::size_t h=0;h<history;++h)frames.push_back(make(AndroidPointerAction::move,h));
    frames.push_back(make(translated,std::nullopt));return frames;
}
} // namespace ii::gui::detail
