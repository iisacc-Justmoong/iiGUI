#pragma once

#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Runtime/TouchInput.h>
#include <cmath>
#include <optional>

namespace ii::gui::detail {
inline ii::runtime::touch::TouchSample runtime_touch_sample(const WidgetInput& input) noexcept {
    return {.identity=input.device_id,.x=input.x,.y=input.y,.timestamp=input.timestamp,
            .pressure=input.pressure,.tilt_x=input.tilt_x,.tilt_y=input.tilt_y,.rotation=input.rotation,
            .tap_count=input.native_click_count,.pen=input.device==PointerDevice::pen};
}
inline std::optional<WidgetInput> widget_touch_event(const ii::runtime::touch::TouchEvent& input) noexcept {
    using K=ii::runtime::touch::TouchEventKind;
    WidgetInput value;
    value.x=input.sample.x;value.y=input.sample.y;value.timestamp=input.sample.timestamp;
    value.device_id=input.sample.identity;value.pressure=input.sample.pressure;
    value.tilt_x=input.sample.tilt_x;value.tilt_y=input.sample.tilt_y;value.rotation=input.sample.rotation;
    value.device=input.sample.pen?PointerDevice::pen:PointerDevice::touch;
    switch(input.kind) {
    case K::pointer_down:value.kind=WidgetInputKind::pointer_down;value.buttons=1;value.native_click_count=input.sample.tap_count;break;
    case K::pointer_move:value.kind=WidgetInputKind::pointer_move;value.buttons=1;break;
    case K::pointer_up:value.kind=WidgetInputKind::pointer_up;break;
    case K::pointer_cancel:value.kind=WidgetInputKind::pointer_cancel;break;
    case K::pan:value.kind=WidgetInputKind::scroll;value.device=PointerDevice::touch;value.precise_scroll=true;
        value.scroll_x=input.dx;value.scroll_y=input.dy;break;
    case K::pinch:
        if(!std::isfinite(input.scale)||input.scale<=0)return {};
        value.kind=WidgetInputKind::scroll;value.device=PointerDevice::touch;value.precise_scroll=true;value.control=true;
        value.scroll_y=std::log(input.scale)*pinch_scroll_units;
        if(!std::isfinite(value.scroll_y))return {};
        break;
    }
    return value;
}
}
