#pragma once
#include <cstdint>
struct AInputEvent;
enum {
    AINPUT_EVENT_TYPE_KEY=1, AINPUT_EVENT_TYPE_MOTION=2,
    AINPUT_SOURCE_KEYBOARD=0x101, AINPUT_SOURCE_DPAD=0x201, AINPUT_SOURCE_GAMEPAD=0x401,
    AKEY_EVENT_ACTION_DOWN=0, AKEY_EVENT_ACTION_UP=1, AKEY_EVENT_ACTION_MULTIPLE=2,
    AKEY_EVENT_FLAG_CANCELED=0x20,
    AMETA_SHIFT_ON=1, AMETA_SHIFT_LEFT_ON=0x40, AMETA_SHIFT_RIGHT_ON=0x80,
    AMETA_ALT_ON=2, AMETA_ALT_LEFT_ON=0x10, AMETA_ALT_RIGHT_ON=0x20,
    AMETA_CTRL_ON=0x1000, AMETA_CTRL_LEFT_ON=0x2000, AMETA_CTRL_RIGHT_ON=0x4000,
    AMETA_META_ON=0x10000, AMETA_META_LEFT_ON=0x20000, AMETA_META_RIGHT_ON=0x40000,
    AMETA_CAPS_LOCK_ON=0x100000,
};
extern "C" {
std::int32_t AInputEvent_getType(const AInputEvent*);
std::int32_t AInputEvent_getSource(const AInputEvent*);
std::int32_t AInputEvent_getDeviceId(const AInputEvent*);
std::int32_t AKeyEvent_getAction(const AInputEvent*);
std::int32_t AKeyEvent_getFlags(const AInputEvent*);
std::int32_t AKeyEvent_getKeyCode(const AInputEvent*);
std::int32_t AKeyEvent_getMetaState(const AInputEvent*);
std::int64_t AKeyEvent_getEventTime(const AInputEvent*);
}
