#include <iiGUI/SheetModel.h>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}

int main() {
    ii::gui::Sheet sheet;
    auto config = sheet.configuration();
    auto desktop = sheet.layout(1200, 800, 120, 60, false);
    require(desktop && desktop->frame == ii::gui::SheetRect{320, 190, 560, 420} &&
            !desktop->mobile && desktop->radius == 16 && !desktop->show_grabber,
            "Desktop Sheet geometry or defaults changed");
    auto narrow = sheet.layout(300, 300, 600, 60, false);
    require(narrow && narrow->frame == ii::gui::SheetRect{24, 24, 252, 252},
            "Desktop Sheet did not respect its viewport margins");
    config.presentation = ii::gui::SheetPresentation::mobile;
    config.top_safe_inset = 40; config.bottom_safe_inset = 34;
    require(sheet.set_configuration(config), "Mobile Sheet configuration was rejected");
    auto fit = sheet.layout(400, 800, 300, 70, false);
    require(fit && fit->mobile && fit->show_grabber && fit->frame == ii::gui::SheetRect{0, 396, 400, 404} &&
            fit->radius == 24, "Mobile fit Sheet geometry changed");
    require(sheet.layout(400, 800, 1000000, 70, false)->frame.height == 760,
            "A long scrolling body was rejected instead of fitting the available viewport");
    config.detent = ii::gui::SheetDetent::medium;
    require(sheet.set_configuration(config) && sheet.layout(400, 800, 300, 70, false)->frame.height == 456,
            "Medium Sheet detent is incorrect");
    config.detent = ii::gui::SheetDetent::large; config.top_safe_inset = 200;
    require(sheet.set_configuration(config) && sheet.layout(400, 800, 300, 70, false)->frame.height == 600,
            "Large Sheet detent ignored the top safe inset");
    config.presentation = ii::gui::SheetPresentation::automatic; config.show_grabber = false;
    require(sheet.set_configuration(config) && !sheet.layout(400, 800, 300, 70, true)->show_grabber &&
            !sheet.layout(400, 800, 300, 70, false)->mobile,
            "Automatic presentation or explicit grabber visibility failed");
    const auto old = sheet.configuration(); config.preferred_height = std::numeric_limits<double>::quiet_NaN();
    require(!sheet.set_configuration(config) && sheet.configuration() == old &&
            !sheet.layout(-1, 800, 20, 20, false), "Invalid Sheet dimensions replaced its state");
    config = old; config.detent = static_cast<ii::gui::SheetDetent>(-1);
    require(!sheet.set_configuration(config), "Unknown Sheet detent was accepted");

    ii::gui::SheetDrag drag;
    require(!drag.begin(10, 400, false, true) && !drag.active(), "Desktop Sheet admitted a mobile drag");
    require(drag.begin(10, 400, true, true) && !drag.begin(0, 400, true, true) && drag.move(89) == 79 &&
            drag.end(89) == ii::gui::SheetDragResult::restore && !drag.active(),
            "Short Sheet drag did not restore");
    require(drag.begin(10, 400, true, true) && drag.move(90) == 80 &&
            drag.end(90) == ii::gui::SheetDragResult::dismiss,
            "Sheet drag did not dismiss at its 20 percent threshold");
    require(drag.begin(100, 1000, true, true) && drag.move(20) == 0 &&
            drag.end(220) == ii::gui::SheetDragResult::dismiss,
            "Sheet drag did not clamp upward movement or its maximum threshold");
    require(!drag.begin(0, 400, true, false) && !drag.begin(0, 0, true, true),
            "Disabled or empty Sheet drag was accepted");
}
