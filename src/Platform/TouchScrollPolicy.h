#pragma once
#include <iiGUI/NativeWidgets.h>
#include <cmath>

namespace ii::gui::detail {
// A scroll viewport may accept either finger or pencil; drawing surfaces do
// not delay or reroute their pointer input.
inline bool touch_scroll_candidate(PointerDevice device, bool scrollable) noexcept {
    return (device == PointerDevice::touch || device == PointerDevice::pen) && scrollable;
}
inline bool touch_scroll_vertical(double x, double y) noexcept {
    return std::isfinite(x) && std::isfinite(y) && std::abs(y) > std::abs(x);
}
}
