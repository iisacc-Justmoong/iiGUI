#include <iiGUI/ButtonModel.h>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
}

int main() {
    using namespace ii::gui;
    ButtonModel button;
    ButtonConfiguration config;
    config.text = "Save";
    config.accessibility_name = "Save image";
    config.palette.primary = {.8, .2, .1, 1};
    config.palette.primary_hover = {.7, .15, .1, 1};
    config.palette.primary_pressed = {.6, .1, .05, 1};
    config.palette.normal = {.2, .2, .2, 1};
    config.palette.normal_hover = {.3, .3, .3, 1};
    config.palette.normal_pressed = {.4, .4, .4, 1};
    config.palette.text = {1, 1, 1, .9};
    config.palette.disabled_background = {.1, .1, .1, 1};
    config.palette.disabled_text = {.5, .5, .5, 1};
    config.palette.selected_reference = config.palette.primary;
    config.palette.selected_default = {.12, .23, .34, 1};
    config.palette.selected_surface = {.4, .5, .6, 1};
    require(button.set_configuration(config), "Valid application palette was rejected");
    require(button.visual({}).background == config.palette.primary &&
            button.visual({.hovered = true}).background == config.palette.primary_hover &&
            button.visual({.hovered = true, .pressed = true}).background == config.palette.primary_pressed,
            "Primary state precedence changed");
    require(button.visual({.checked = true}).background == config.palette.selected_default,
            "Application selected reference was ignored");
    config.palette.primary = {1, .2, .1, 1};
    require(button.set_configuration(config) &&
            near(button.visual({.checked = true}).background.red, .4 * .75 + .25),
            "Selected color did not follow the supplied surface and primary");
    config.palette.selected = WidgetColor{.31, .41, .51, 1};
    require(button.set_configuration(config) &&
            button.visual({.checked = true}).background == *config.palette.selected,
            "Explicit selected color did not take precedence");
    config.tone = ButtonTone::borderless;
    config.icon_mode = true; config.has_icon = true; config.icon_size = 16;
    require(button.set_configuration(config), "Valid icon button was rejected");
    const auto geometry = button.geometry(100, 22, 32, 13);
    require(geometry && geometry->draw_icon && geometry->draw_text && near(geometry->icon.width, 16) &&
            near(geometry->text.width, 32), "Icon and label geometry changed");
    require(button.visual({}).background.alpha == 0 &&
            button.visual({.pressed = true}).background == config.palette.normal_pressed,
            "Borderless interaction state changed");
    config.enabled = false;
    require(button.set_configuration(config) && !button.visual({.pressed = true}).enabled &&
            button.visual({.pressed = true}).background == config.palette.disabled_background,
            "Disabled state admitted input");
    const auto old = button.configuration();
    auto invalid = old; invalid.palette.selected_surface.red = std::numeric_limits<double>::quiet_NaN();
    require(!button.set_configuration(invalid) && button.configuration() == old,
            "Invalid palette replaced the last valid state");
    invalid = old; invalid.text = std::string("a\0b", 3);
    require(!button.set_configuration(invalid) && !button.geometry(-1, 22, 32, 13),
            "Invalid text or geometry was accepted");
}
