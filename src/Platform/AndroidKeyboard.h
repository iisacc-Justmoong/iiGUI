#pragma once
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ii::gui::detail {
struct AndroidKeyboardFrame final {
    bool down=false,canceled=false,paste=false;
    WidgetKey key=WidgetKey::none;
    std::uint32_t character=0,shortcut=0;
    std::int32_t device=0;
    bool shift=false,control=false,alt=false,meta=false,alt_graph=false;
    double timestamp=0;
};
struct AndroidKeyboardResult final {
    bool handled=false;
    std::vector<WidgetInput> events;
};
namespace android_keyboard_detail {
inline bool printable(std::uint32_t value) noexcept {
    return value>=32&&!(value>=0x7f&&value<=0x9f)&&value<=0x10ffff&&!(value>=0xd800&&value<=0xdfff);
}
inline void append_utf8(std::string& out,std::uint32_t value){
    if(!printable(value))return;
    if(value<0x80)out.push_back(char(value));
    else if(value<0x800){out.push_back(char(0xc0|(value>>6)));out.push_back(char(0x80|(value&0x3f)));}
    else if(value<0x10000){out.push_back(char(0xe0|(value>>12)));out.push_back(char(0x80|((value>>6)&0x3f)));out.push_back(char(0x80|(value&0x3f)));}
    else {out.push_back(char(0xf0|(value>>18)));out.push_back(char(0x80|((value>>12)&0x3f)));out.push_back(char(0x80|((value>>6)&0x3f)));out.push_back(char(0x80|(value&0x3f)));}
}
}
class AndroidKeyboardState final {
    std::uint32_t accent_=0;
    std::int32_t device_=0;
public:
    void reset() noexcept {accent_=0;device_=0;}
    template<class Combine>
    AndroidKeyboardResult translate(const AndroidKeyboardFrame& frame,Combine&& combine){
        if(frame.key==WidgetKey::none)return {};
        WidgetInput input;
        input.kind=frame.canceled?WidgetInputKind::key_cancel:frame.down?WidgetInputKind::key_down:WidgetInputKind::key_up;
        input.key=frame.key;input.shift=frame.shift;input.control=frame.control;input.alt=frame.alt;input.meta=frame.meta;
        input.device_id=std::uint32_t(frame.device);
        input.timestamp=std::isfinite(frame.timestamp)?std::max(0.,frame.timestamp):0;
        auto shortcut=frame.shortcut;
        if(shortcut>='A'&&shortcut<='Z')shortcut+=('a'-'A');
        android_keyboard_detail::append_utf8(input.text,shortcut);
        AndroidKeyboardResult result{true,{}};result.events.push_back(input);
        if(frame.canceled){reset();return result;}
        if(!frame.down)return result;
        if(accent_&&device_!=frame.device)reset();
        if(frame.meta||(frame.control&&!frame.alt_graph)) {reset();return result;}
        if(frame.key!=WidgetKey::character&&frame.key!=WidgetKey::space){reset();return result;}
        constexpr std::uint32_t accent_flag=0x80000000u,accent_mask=0x7fffffffu;
        const bool dead=(frame.character&accent_flag)!=0;
        const auto value=frame.character&accent_mask;
        if(!android_keyboard_detail::printable(value))return result;
        std::string text;
        if(accent_){
            const auto composed=std::uint32_t(combine(accent_,value));
            const bool combined=android_keyboard_detail::printable(composed);
            if(combined)android_keyboard_detail::append_utf8(text,composed);
            else {android_keyboard_detail::append_utf8(text,accent_);if(!dead)android_keyboard_detail::append_utf8(text,value);}
            accent_=0;
            if(dead&&!combined){accent_=value;device_=frame.device;}
        }else if(dead){accent_=value;device_=frame.device;}
        else android_keyboard_detail::append_utf8(text,value);
        if(!text.empty()){
            WidgetInput committed;committed.kind=WidgetInputKind::text;committed.text=std::move(text);
            committed.device_id=input.device_id;committed.timestamp=input.timestamp;
            result.events.push_back(std::move(committed));
        }
        return result;
    }
};
} // namespace ii::gui::detail
