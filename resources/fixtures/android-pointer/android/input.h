#pragma once
#include <cstddef>
#include <cstdint>
struct AInputEvent;
enum {AMOTION_EVENT_ACTION_MASK=0xff, AMOTION_EVENT_ACTION_DOWN=0, AMOTION_EVENT_ACTION_UP=1,
      AMOTION_EVENT_ACTION_MOVE=2, AMOTION_EVENT_ACTION_CANCEL=3, AMOTION_EVENT_ACTION_POINTER_DOWN=5,
      AMOTION_EVENT_ACTION_POINTER_UP=6, AMOTION_EVENT_ACTION_HOVER_MOVE=7, AMOTION_EVENT_ACTION_SCROLL=8,
      AMOTION_EVENT_ACTION_HOVER_ENTER=9, AMOTION_EVENT_ACTION_HOVER_EXIT=10,
      AMOTION_EVENT_ACTION_BUTTON_PRESS=11, AMOTION_EVENT_ACTION_BUTTON_RELEASE=12,
      AMOTION_EVENT_ACTION_POINTER_INDEX_MASK=0xff00, AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT=8,
      AMOTION_EVENT_TOOL_TYPE_FINGER=1, AMOTION_EVENT_TOOL_TYPE_STYLUS=2, AMOTION_EVENT_TOOL_TYPE_MOUSE=3,
      AMOTION_EVENT_TOOL_TYPE_ERASER=4, AMOTION_EVENT_AXIS_VSCROLL=9, AMOTION_EVENT_AXIS_HSCROLL=10,
      AMOTION_EVENT_AXIS_TILT=25, AINPUT_SOURCE_CLASS_MASK=0xff, AINPUT_SOURCE_CLASS_POINTER=2,
      AINPUT_SOURCE_TOUCHSCREEN=0x1002, AINPUT_SOURCE_MOUSE=0x2002, AINPUT_SOURCE_JOYSTICK=0x1000010,
      AMETA_SHIFT_ON=1, AMETA_SHIFT_LEFT_ON=0x40, AMETA_SHIFT_RIGHT_ON=0x80,
      AMETA_ALT_ON=2, AMETA_ALT_LEFT_ON=0x10, AMETA_ALT_RIGHT_ON=0x20,
      AMETA_CTRL_ON=0x1000, AMETA_CTRL_LEFT_ON=0x2000, AMETA_CTRL_RIGHT_ON=0x4000,
      AMETA_META_ON=0x10000, AMETA_META_LEFT_ON=0x20000, AMETA_META_RIGHT_ON=0x40000};
extern "C" {
std::int32_t AInputEvent_getDeviceId(const AInputEvent*);
std::int32_t AInputEvent_getSource(const AInputEvent*);
std::int32_t AMotionEvent_getAction(const AInputEvent*);
std::int32_t AMotionEvent_getFlags(const AInputEvent*);
std::int32_t AMotionEvent_getMetaState(const AInputEvent*);
std::int32_t AMotionEvent_getButtonState(const AInputEvent*);
std::int64_t AMotionEvent_getEventTime(const AInputEvent*);
std::size_t AMotionEvent_getPointerCount(const AInputEvent*);
std::int32_t AMotionEvent_getPointerId(const AInputEvent*,std::size_t);
std::int32_t AMotionEvent_getToolType(const AInputEvent*,std::size_t);
float AMotionEvent_getX(const AInputEvent*,std::size_t);
float AMotionEvent_getY(const AInputEvent*,std::size_t);
float AMotionEvent_getPressure(const AInputEvent*,std::size_t);
float AMotionEvent_getOrientation(const AInputEvent*,std::size_t);
float AMotionEvent_getAxisValue(const AInputEvent*,std::int32_t,std::size_t);
std::size_t AMotionEvent_getHistorySize(const AInputEvent*);
std::int64_t AMotionEvent_getHistoricalEventTime(const AInputEvent*,std::size_t);
float AMotionEvent_getHistoricalX(const AInputEvent*,std::size_t,std::size_t);
float AMotionEvent_getHistoricalY(const AInputEvent*,std::size_t,std::size_t);
float AMotionEvent_getHistoricalPressure(const AInputEvent*,std::size_t,std::size_t);
float AMotionEvent_getHistoricalOrientation(const AInputEvent*,std::size_t,std::size_t);
float AMotionEvent_getHistoricalAxisValue(const AInputEvent*,std::int32_t,std::size_t,std::size_t);
}
