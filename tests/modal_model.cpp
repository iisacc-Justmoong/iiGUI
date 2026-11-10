#include <iiGUI/ModalModel.h>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
}

int main() {
    ii::gui::ModalModel modal;
    ii::gui::ModalConfiguration initial;
    initial.action_text = {"OK", "", ""};
    initial.action_fallback = {"OK", "Cancel", "More"};
    require(modal.set_configuration(initial), "Valid modal configuration was rejected");
    auto config = modal.configuration();
    require(modal.action_count() == 1 && modal.action(0).text == "OK" && modal.action(0).enabled,
            "Modal default action changed");
    auto frame = modal.frame(800, 600, 100);
    require(frame && frame->width == 752 && frame->height == 210 && frame->x == 24 && frame->y == 179,
            "Modal default centered geometry changed");
    frame = modal.frame(200, 300, 400);
    require(frame && frame->width == 520 && frame->x == -160 && frame->height == 440,
            "Modal did not retain its minimum width or measured content");
    auto narrow = config; narrow.minimum_width = 0;
    require(modal.set_configuration(narrow) && modal.frame(0, 300, 0)->width == 1,
            "Modal minimum width diverged from the host for an empty viewport");

    config.action_text = {"", "", ""}; config.description = ""; config.message = "Fallback";
    require(modal.set_configuration(config) && modal.action_count() == 0 && modal.description() == "Fallback",
            "Empty actions or description fallback were not resolved");
    config.action_text[2] = "More"; config.action_enabled[1] = false;
    require(modal.set_configuration(config) && modal.action_count() == 3 && modal.action(0).text == "OK" &&
            modal.action(1).text == "Cancel" && !modal.action(1).enabled && !modal.action(3).visible,
            "Automatic modal actions or fallback text are incorrect");
    config.action_count = 2; config.shape = ii::gui::ModalShape::cylinder;
    require(modal.set_configuration(config) && modal.action_count() == 2 && modal.radius({0, 0, 100, 40}) == 20,
            "Explicit action count or cylindrical shape was ignored");
    const auto previous = modal.configuration();
    config.minimum_width = 1000; config.maximum_width = 500;
    require(!modal.set_configuration(config) && modal.configuration() == previous, "Invalid width replaced modal state");
    config = previous; config.palette.frame.alpha = std::numeric_limits<double>::quiet_NaN();
    require(!modal.set_configuration(config) && !modal.frame(-1, 100, 0), "Invalid modal color or extent was accepted");
}
