#include "iiGUI/MobileInput.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

int main() {
    using namespace ii::gui;
    using namespace ii::gui::detail;
    try {
        auto finger = touch_input(WidgetInputKind::pointer_down,
                                 {.x=21, .y=34, .timestamp=12, .identity=42});
        check(finger.device == PointerDevice::touch && finger.pressure == 1 &&
              finger.buttons == 1 && finger.x == 21 && finger.y == 34 &&
              finger.device_id == 42 && finger.timestamp == 12,
              "Finger coordinates, identity or contact were lost");
        auto pencil = touch_input(WidgetInputKind::pointer_move,
                                  {.force=2, .maximum_force=4,
                                   .altitude=std::numbers::pi/4, .azimuth=0,
                                   .pencil=true});
        check(pencil.device == PointerDevice::pen && pencil.pressure == .5 &&
              std::abs(pencil.tilt_x - 45) < 1e-6 && std::abs(pencil.tilt_y) < 1e-6,
              "Pencil pressure or altitude conversion is incorrect");
        auto rotated = touch_input(WidgetInputKind::pointer_move,
                                   {.force=8, .maximum_force=4,
                                    .altitude=std::numbers::pi/4,
                                    .azimuth=std::numbers::pi/2, .pencil=true});
        check(rotated.pressure == 1 && std::abs(rotated.tilt_y - 45) < 1e-6,
              "Pressure clamping or azimuth conversion is incorrect");
        auto up = touch_input(WidgetInputKind::pointer_up, {.pencil=true});
        check(up.buttons == 0, "Pencil release retained contact");
        WidgetInput space{.kind=WidgetInputKind::key_down,.key=WidgetKey::space};
        check(mobile_text_service_key(space,true)&&!mobile_text_service_key(space,false),
              "UIKit space did not distinguish an editor from a button");
        space.meta=true;check(!mobile_text_service_key(space,true),"UIKit diverted a modified shortcut to text input");
        const auto nan = std::numeric_limits<double>::quiet_NaN();
        auto invalid = touch_input(WidgetInputKind::focus_lost,
                                   {.x=nan, .y=nan, .force=nan,
                                    .maximum_force=nan, .altitude=nan,
                                    .azimuth=nan, .timestamp=nan, .pencil=true});
        check(invalid.x == 0 && invalid.y == 0 && invalid.timestamp == 0 &&
              invalid.pressure == 1 && invalid.tilt_x == 0 && invalid.tilt_y == 0 &&
              invalid.buttons == 0, "Invalid UIKit geometry escaped normalization");
        std::cout << "MOBILE_INPUT_OK finger=true pressure=true tilt=true release=true finite=true\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
