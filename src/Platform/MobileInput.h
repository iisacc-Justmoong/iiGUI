#pragma once
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <cmath>
#include <numbers>
namespace ii::gui::detail {
// UIKit boundary expressed without ObjC types, also exercised by host contracts.
struct MobileTouch final {double x=0,y=0,force=0,maximum_force=0,altitude=std::numbers::pi/2,azimuth=0,timestamp=0;std::uint64_t identity=0;bool pencil=false;};
// Printable space belongs to UIKit's text service for a focused editor; a
// button or modified shortcut continues to receive ordinary key input.
inline bool mobile_text_service_key(const WidgetInput& input,bool editing)noexcept{
    return editing&&input.kind==WidgetInputKind::key_down&&input.key==WidgetKey::space&&!input.control&&!input.meta&&!input.alt;
}
inline WidgetInput touch_input(WidgetInputKind kind,MobileTouch touch){
    WidgetInput input;input.kind=kind;input.device=touch.pencil?PointerDevice::pen:PointerDevice::touch;
    input.x=std::isfinite(touch.x)?touch.x:0;input.y=std::isfinite(touch.y)?touch.y:0;
    input.timestamp=std::isfinite(touch.timestamp)?touch.timestamp:0;input.device_id=touch.identity;
    input.buttons=kind==WidgetInputKind::pointer_up||kind==WidgetInputKind::focus_lost?0:1;
    input.pressure=touch.pencil&&std::isfinite(touch.force)&&std::isfinite(touch.maximum_force)&&touch.maximum_force>0?std::clamp(touch.force/touch.maximum_force,0.,1.):1;
    if(touch.pencil&&std::isfinite(touch.altitude)&&std::isfinite(touch.azimuth)){
        const auto tilt=std::numbers::pi/2-std::clamp(touch.altitude,0.00001,std::numbers::pi/2);
        input.tilt_x=std::atan(std::tan(tilt)*std::cos(touch.azimuth))*180/std::numbers::pi;
        input.tilt_y=std::atan(std::tan(tilt)*std::sin(touch.azimuth))*180/std::numbers::pi;
    }return input;
}
}
