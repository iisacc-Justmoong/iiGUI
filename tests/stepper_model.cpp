#include <iiGUI/StepperModel.h>
#include <limits>
#include <stdexcept>

namespace { void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); } }
int main() {
    using namespace ii::gui;
    StepperModel stepper; StepperConfiguration options;
    options.palette.primary = {.2, .4, .6, 1}; options.palette.normal_hover = {.3, .3, .3, 1};
    options.palette.text = {1, 1, 1, 1}; options.palette.disabled_text = {1, 1, 1, .38};
    require(stepper.set_configuration(options) && stepper.accepts(1) && stepper.accepts(-1) &&
            stepper.direction_at(2, 18) == 1 && stepper.direction_at(16, 18) == -1 && !stepper.direction_at(-1, 18),
            "Stepper directions or bounds changed");
    options.arrow = StepperArrow::up;
    require(stepper.set_configuration(options) && stepper.accepts(1) && !stepper.accepts(-1) && stepper.direction_at(17, 18) == 1,
            "Up-only stepper accepted the wrong direction");
    options.arrow = StepperArrow::down;
    require(stepper.set_configuration(options) && !stepper.accepts(1) && stepper.accepts(-1), "Down-only stepper accepted the wrong direction");
    options.enabled = false; require(stepper.set_configuration(options) && !stepper.direction_at(5, 18), "Disabled stepper accepted input");
    const auto previous = stepper.configuration(); options.height = 0;
    require(!stepper.set_configuration(options) && stepper.configuration() == previous, "Invalid geometry replaced valid state");
    options = previous; options.corner_radius = std::numeric_limits<double>::quiet_NaN();
    require(!stepper.set_configuration(options), "Non-finite radius was accepted");
    options = previous; options.arrow = static_cast<StepperArrow>(99); require(!stepper.set_configuration(options), "Unknown arrow was accepted");
    options = previous; options.enabled = true; options.tone = ButtonTone::borderless;
    require(stepper.set_configuration(options) && stepper.visual({}).background.alpha == 0, "Borderless stepper has an idle fill");
    options.hover_color = WidgetColor{.4, .2, .1, 1}; options.tone = ButtonTone::destructive;
    require(stepper.set_configuration(options) && stepper.visual({.hovered = true}).background == *options.hover_color,
            "Supplied hover color was ignored");
}
