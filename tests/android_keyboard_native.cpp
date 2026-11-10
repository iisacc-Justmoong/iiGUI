#include "../src/Platform/AndroidKeyboardNative.h"
#include "../src/Platform/AndroidBackInput.h"
#include <stdexcept>
#include <vector>
using namespace ii::gui;
using namespace ii::gui::detail;
struct AInputEvent {
    int type=AINPUT_EVENT_TYPE_KEY,source=AINPUT_SOURCE_KEYBOARD,device=5,action=AKEY_EVENT_ACTION_DOWN;
    int flags=0,code=AKEYCODE_A,meta=0;std::int64_t time=1200000000;
};
extern "C" {
std::int32_t AInputEvent_getType(const AInputEvent* e){return e->type;}
std::int32_t AInputEvent_getSource(const AInputEvent* e){return e->source;}
std::int32_t AInputEvent_getDeviceId(const AInputEvent* e){return e->device;}
std::int32_t AKeyEvent_getAction(const AInputEvent* e){return e->action;}
std::int32_t AKeyEvent_getFlags(const AInputEvent* e){return e->flags;}
std::int32_t AKeyEvent_getKeyCode(const AInputEvent* e){return e->code;}
std::int32_t AKeyEvent_getMetaState(const AInputEvent* e){return e->meta;}
std::int64_t AKeyEvent_getEventTime(const AInputEvent* e){return e->time;}
}
struct FakeMap {
    std::vector<int> metas;
    std::uint32_t character(int device,int code,int meta){
        if(device!=5||code!=AKEYCODE_A)throw std::runtime_error("Incorrect key map lookup");
        metas.push_back(meta);return (meta&AMETA_ALT_RIGHT_ON)?'@':'a';
    }
};
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
    FakeMap map;AInputEvent raw;
    raw.meta=AMETA_ALT_RIGHT_ON|AMETA_CTRL_LEFT_ON|AMETA_SHIFT_RIGHT_ON;
    auto value=android_keyboard_frame(&raw,map);
    check(value&&value->key==WidgetKey::character&&value->character=='@'&&value->shortcut=='a'&&
          value->control&&value->alt&&value->alt_graph&&value->shift&&value->timestamp==1.2&&
          map.metas.size()==2&&(map.metas[1]&AMETA_CTRL_LEFT_ON)==0&&(map.metas[1]&AMETA_ALT_RIGHT_ON)==0,
          "NDK key did not preserve the keyboard map, modifiers or timestamp");
    raw.action=AKEY_EVENT_ACTION_UP;raw.flags=AKEY_EVENT_FLAG_CANCELED;raw.code=AKEYCODE_SPACE;raw.meta=0;
    auto cancel=android_keyboard_frame(&raw,map);
    check(cancel&&!cancel->down&&cancel->canceled&&cancel->key==WidgetKey::space,"Canceled key release was lost");
    raw.action=AKEY_EVENT_ACTION_DOWN;raw.flags=0;raw.code=AKEYCODE_DPAD_LEFT;raw.source=AINPUT_SOURCE_DPAD;
    check(android_keyboard_frame(&raw,map)->key==WidgetKey::left,"DPAD navigation was not translated");
    raw.source=AINPUT_SOURCE_KEYBOARD;raw.code=AKEYCODE_PASTE;
    check(android_keyboard_frame(&raw,map)->paste,"Dedicated paste key was lost");
    raw.source=AINPUT_SOURCE_GAMEPAD;check(!android_keyboard_frame(&raw,map),"Gamepad keys were consumed as text");
    raw.source=AINPUT_SOURCE_KEYBOARD;raw.action=AKEY_EVENT_ACTION_MULTIPLE;
    check(!android_keyboard_frame(&raw,map),"Unsupported multiple-key event was consumed");
    raw.code=AKEYCODE_BACK;raw.action=AKEY_EVENT_ACTION_UP;raw.flags=AKEY_EVENT_FLAG_CANCELED;
    auto canceled_back=android_back_close(raw.type,raw.code,raw.action,raw.flags);
    check(canceled_back&&!*canceled_back,"Canceled Back event closed the Android activity");
    raw.flags=0;check(android_back_close(raw.type,raw.code,raw.action,raw.flags)==true,"Back release did not request close");
    raw.action=AKEY_EVENT_ACTION_DOWN;check(android_back_close(raw.type,raw.code,raw.action,raw.flags)==false,"Back press closed before release");
    check(!android_back_close(AINPUT_EVENT_TYPE_MOTION,AKEYCODE_BACK,AKEY_EVENT_ACTION_UP,0)&&
          !android_back_close(AINPUT_EVENT_TYPE_KEY,AKEYCODE_A,AKEY_EVENT_ACTION_UP,0),"Non-Back input was consumed by activity navigation");
    raw.type=AINPUT_EVENT_TYPE_MOTION;check(!android_keyboard_frame(&raw,map),"Motion was interpreted as a keyboard event");
}
