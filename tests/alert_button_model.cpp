#include <iiGUI/AlertButtonModel.h>

#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
}

int main() {
    ii::gui::AlertButtonModel alert;
    auto config = alert.configuration();
    config.palette.normal={.2,.3,.4,1};config.palette.primary={.5,.4,.3,1};
    config.palette.primary_hover={.45,.35,.25,1};config.palette.primary_pressed={.4,.3,.2,1};
    config.primary_reference=config.palette.primary;config.normal_pressed={.1,.2,.3,1};
    require(alert.set_configuration(config),"Supplied AlertButton colors were rejected");
    auto resolved = alert.appearance();
    require(!config.enter_key_activation && !resolved.button.enter_key_activation,"AlertButton enabled Enter by default");
    config.enter_key_activation = true;
    require(alert.set_configuration(config) && alert.appearance().button.enter_key_activation,
            "AlertButton did not forward the Enter policy");
    require(resolved.button.height == 20 && resolved.button.horizontal_padding == 8 &&
            resolved.button.corner_radius == 4 && resolved.button.tone == ii::gui::ButtonTone::normal && !resolved.border,
            "Standalone AlertButton defaults changed");
    require(config.motion_enabled && resolved.button.motion_enabled,"AlertButton did not enable motion by default");
    config.motion_enabled = false;
    require(alert.set_configuration(config) && !alert.appearance().button.motion_enabled,
            "AlertButton did not forward its local motion setting");
    require(resolved.button.palette.normal_hover.red > resolved.button.palette.normal.red &&
            resolved.button.palette.normal_pressed == config.normal_pressed &&
            resolved.button.palette.primary_hover == config.palette.primary_hover &&
            resolved.button.palette.primary_pressed == config.palette.primary_pressed,
            "Standalone AlertButton state colors changed");
    config.dialog_style = true;
    require(alert.set_configuration(config), "Dialog AlertButton configuration was rejected");
    resolved = alert.appearance();
    require(resolved.button.horizontal_padding == 16 && resolved.button.corner_radius == 16 &&
            resolved.border && resolved.border_width == 1 && resolved.button.palette.normal.alpha == 0 &&
            resolved.button.palette.normal_hover.alpha == .05 && resolved.button.palette.normal_pressed.alpha == .09,
            "Dialog AlertButton did not use its outlined design");
    config.tone = ii::gui::ButtonTone::primary;
    config.primary_color = ii::gui::WidgetColor{.8, .4, .2, 1};
    config.height = 56; config.horizontal_padding = 12;
    require(alert.set_configuration(config) && !alert.appearance().border &&
            alert.appearance().button.height == 56 && alert.appearance().button.horizontal_padding == 12 &&
            alert.appearance().button.palette.primary == *config.primary_color &&
            alert.appearance().button.palette.primary_hover.red == config.primary_color->red / 1.12 &&
            alert.appearance().button.palette.primary_pressed.red == config.primary_color->red / 1.2,
            "Primary AlertButton did not preserve its explicit metrics and fill");
    config.tone = ii::gui::ButtonTone::borderless;
    config.text_color = ii::gui::WidgetColor{1, 0, 0, 1};
    require(alert.set_configuration(config) && alert.appearance().button.palette.primary == *config.text_color,
            "Borderless AlertButton ignored its explicit text color");
    const auto previous = alert.configuration();
    config.height = std::numeric_limits<double>::quiet_NaN();
    require(!alert.set_configuration(config) && alert.configuration() == previous,
            "Invalid AlertButton geometry changed the current configuration");
    config = previous; config.dialog_border.alpha = 2;
    require(!alert.set_configuration(config), "Invalid AlertButton border color was accepted");
}
