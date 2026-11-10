#pragma once
#include "AndroidKeyboard.h"
#include <android/input.h>
#include <android/keycodes.h>
#include <optional>

namespace ii::gui::detail {
inline WidgetKey android_keyboard_key(std::int32_t code) noexcept {
    switch(code){
    case AKEYCODE_TAB:return WidgetKey::tab;
    case AKEYCODE_ENTER:case AKEYCODE_NUMPAD_ENTER:case AKEYCODE_DPAD_CENTER:return WidgetKey::enter;
    case AKEYCODE_SPACE:return WidgetKey::space;
    case AKEYCODE_ESCAPE:return WidgetKey::escape;
    case AKEYCODE_DPAD_LEFT:return WidgetKey::left;
    case AKEYCODE_DPAD_RIGHT:return WidgetKey::right;
    case AKEYCODE_DPAD_UP:return WidgetKey::up;
    case AKEYCODE_DPAD_DOWN:return WidgetKey::down;
    case AKEYCODE_MOVE_HOME:return WidgetKey::home;
    case AKEYCODE_MOVE_END:return WidgetKey::end;
    case AKEYCODE_DEL:return WidgetKey::backspace;
    case AKEYCODE_FORWARD_DEL:return WidgetKey::delete_forward;
    case AKEYCODE_PAGE_UP:return WidgetKey::page_up;
    case AKEYCODE_PAGE_DOWN:return WidgetKey::page_down;
    case AKEYCODE_INSERT:return WidgetKey::insert;
    case AKEYCODE_COPY:return WidgetKey::copy;
    case AKEYCODE_CUT:return WidgetKey::cut;
    default:return WidgetKey::none;
    }
}
template<class KeyMap>
std::optional<AndroidKeyboardFrame> android_keyboard_frame(const AInputEvent* event,KeyMap& map){
    if(!event||AInputEvent_getType(event)!=AINPUT_EVENT_TYPE_KEY)return std::nullopt;
    const auto source=AInputEvent_getSource(event);
    if((source&AINPUT_SOURCE_KEYBOARD)!=AINPUT_SOURCE_KEYBOARD&&
       (source&AINPUT_SOURCE_DPAD)!=AINPUT_SOURCE_DPAD)return std::nullopt;
    const auto action=AKeyEvent_getAction(event);
    if(action!=AKEY_EVENT_ACTION_DOWN&&action!=AKEY_EVENT_ACTION_UP)return std::nullopt;
    const auto meta=AKeyEvent_getMetaState(event),code=AKeyEvent_getKeyCode(event);
    constexpr auto shift=AMETA_SHIFT_ON|AMETA_SHIFT_LEFT_ON|AMETA_SHIFT_RIGHT_ON;
    constexpr auto control=AMETA_CTRL_ON|AMETA_CTRL_LEFT_ON|AMETA_CTRL_RIGHT_ON;
    constexpr auto alt=AMETA_ALT_ON|AMETA_ALT_LEFT_ON|AMETA_ALT_RIGHT_ON;
    constexpr auto command=AMETA_META_ON|AMETA_META_LEFT_ON|AMETA_META_RIGHT_ON;
    AndroidKeyboardFrame frame;
    frame.down=action==AKEY_EVENT_ACTION_DOWN;frame.canceled=(AKeyEvent_getFlags(event)&AKEY_EVENT_FLAG_CANCELED)!=0;
    frame.key=android_keyboard_key(code);frame.paste=code==AKEYCODE_PASTE;
    frame.device=AInputEvent_getDeviceId(event);frame.timestamp=double(AKeyEvent_getEventTime(event))/1e9;
    frame.shift=(meta&shift)!=0;frame.control=(meta&control)!=0;frame.alt=(meta&alt)!=0;frame.meta=(meta&command)!=0;
    frame.alt_graph=frame.alt&&(meta&AMETA_ALT_RIGHT_ON)!=0;
    if(!frame.paste&&(frame.key==WidgetKey::none||(frame.key==WidgetKey::space&&frame.down&&!frame.canceled))){
        frame.character=map.character(frame.device,code,meta);
        const auto shortcut_meta=meta&~(control|alt|command);
        frame.shortcut=shortcut_meta==meta?frame.character:map.character(frame.device,code,shortcut_meta);
        if(frame.key==WidgetKey::none&&(frame.character||frame.shortcut))frame.key=WidgetKey::character;
    }
    if(frame.key==WidgetKey::none&&!frame.paste)return std::nullopt;
    return frame;
}
} // namespace ii::gui::detail
