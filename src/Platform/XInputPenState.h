#pragma once
#include <iiGUI/NativeWidgets.h>
#include <cmath>
#include <optional>

namespace ii::gui::detail {
struct XInputPenUpdate final {
    std::optional<double> pressure, tilt_x, tilt_y, rotation;
};

// XInput device events can omit unchanged axes, including on button events.
// One state instance belongs to one XInput source device on the host thread.
class XInputPenState final {
public:
    [[nodiscard]] bool merge(const XInputPenUpdate& update) noexcept {
        const auto valid=[](const std::optional<double>& value,double minimum,double maximum) {
            return !value || (std::isfinite(*value) && *value>=minimum && *value<=maximum);
        };
        if (!valid(update.pressure,0,1) || !valid(update.tilt_x,-90,90) ||
            !valid(update.tilt_y,-90,90) || (update.rotation && !std::isfinite(*update.rotation)))
            return false;
        if (update.pressure) pressure_=*update.pressure;
        if (update.tilt_x) tilt_x_=*update.tilt_x;
        if (update.tilt_y) tilt_y_=*update.tilt_y;
        if (update.rotation) rotation_=*update.rotation;
        return true;
    }
    void fill(WidgetInput& input) const noexcept {
        input.pressure=pressure_; input.tilt_x=tilt_x_; input.tilt_y=tilt_y_; input.rotation=rotation_;
    }
private:
    double pressure_=1,tilt_x_=0,tilt_y_=0,rotation_=0;
};
} // namespace ii::gui::detail
