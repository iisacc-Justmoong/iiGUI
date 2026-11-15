#pragma once

#include <iiGUI/ButtonModel.h>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace ii::gui {
// A stepper emits a direction; the application owns the numeric value.
struct StepperConfiguration final {
    StepperArrow arrow = StepperArrow::up_down;
    ButtonTone tone = ButtonTone::primary;
    bool enabled = true;
    double width = 18, height = 18, corner_radius = 4;
    std::string accessibility_name = "Stepper";
    ButtonPalette palette = [] { ButtonPalette value; value.text = {1, 1, 1, 1}; value.disabled_text = {1, 1, 1, .38}; return value; }();
    std::optional<WidgetColor> hover_color, pressed_color, disabled_color, text_color, disabled_text_color;
    friend bool operator==(const StepperConfiguration&, const StepperConfiguration&) = default;
};
struct DefaultStepperModelPolicy final {
    using Configuration = StepperConfiguration; using Arrow = StepperArrow; using Tone = ButtonTone;
    using Color = WidgetColor; using Button = ButtonModel; using ButtonConfig = ButtonConfiguration;
    using Interaction = ButtonInteraction; using Visual = ButtonVisual;
    [[nodiscard]] static bool valid_text(std::string_view value) noexcept { return DefaultButtonPolicy::valid_text(value); }
};
template<class Policy>
class BasicStepper final {
    using Configuration = typename Policy::Configuration;
    using Arrow = typename Policy::Arrow; using Tone = typename Policy::Tone;
    using Color = typename Policy::Color; using Button = typename Policy::Button;
    using ButtonConfig = typename Policy::ButtonConfig;
    using Interaction = typename Policy::Interaction; using Visual = typename Policy::Visual;
    Configuration configuration_;
    [[nodiscard]] static ButtonConfig button(const Configuration& value) {
        ButtonConfig result; result.enabled = value.enabled; result.tone = value.tone;
        result.height = value.height; result.corner_radius = value.corner_radius; result.palette = value.palette;
        return result;
    }
public:
    [[nodiscard]] static bool validate(const Configuration& value) noexcept {
        if (!std::isfinite(value.width) || value.width < 1 || value.width > 8192 ||
            !std::isfinite(value.height) || value.height < 1 || value.height > 8192 ||
            !Policy::valid_text(value.accessibility_name)) return false;
        switch (value.arrow) { case Arrow::up_down: case Arrow::up: case Arrow::down: break; default: return false; }
        const auto color = [](const std::optional<Color>& c) {
            if (!c) return true;
            const auto channel = [](double n) { return std::isfinite(n) && n >= 0 && n <= 1; };
            return channel(c->red) && channel(c->green) && channel(c->blue) && channel(c->alpha);
        };
        if (!color(value.hover_color) || !color(value.pressed_color) || !color(value.disabled_color) ||
            !color(value.text_color) || !color(value.disabled_text_color)) return false;
        return Button::validate(button(value));
    }
    [[nodiscard]] bool set_configuration(Configuration value) {
        if (!validate(value)) return false; configuration_ = std::move(value); return true;
    }
    [[nodiscard]] const Configuration& configuration() const noexcept { return configuration_; }
    [[nodiscard]] bool accepts(int direction) const noexcept {
        return configuration_.enabled && configuration_.tone != Tone::disabled && (direction == 1 || direction == -1) &&
            (configuration_.arrow == Arrow::up_down || (configuration_.arrow == Arrow::up && direction == 1) ||
             (configuration_.arrow == Arrow::down && direction == -1));
    }
    [[nodiscard]] std::optional<int> direction_at(double y, double height) const noexcept {
        if (!std::isfinite(y) || !std::isfinite(height) || height <= 0 || y < 0 || y > height) return std::nullopt;
        const auto direction = configuration_.arrow == Arrow::up ? 1 : configuration_.arrow == Arrow::down ? -1 : y < height / 2 ? 1 : -1;
        return accepts(direction) ? std::optional<int>{direction} : std::nullopt;
    }
    [[nodiscard]] Visual visual(Interaction interaction) const {
        Button control; (void)control.set_configuration(button(configuration_)); auto result = control.visual(interaction);
        if (result.enabled && configuration_.tone == Tone::borderless) result.text = configuration_.palette.text;
        if (!result.enabled) {
            if (configuration_.disabled_color) result.background = *configuration_.disabled_color;
            if (configuration_.disabled_text_color) result.text = *configuration_.disabled_text_color;
        } else {
            if (interaction.pressed && configuration_.pressed_color) result.background = *configuration_.pressed_color;
            else if (interaction.hovered && configuration_.hover_color) result.background = *configuration_.hover_color;
            if (configuration_.text_color) result.text = *configuration_.text_color;
        }
        return result;
    }
    [[nodiscard]] std::string_view glyph() const noexcept {
        return configuration_.arrow == Arrow::up ? "⌃" : configuration_.arrow == Arrow::down ? "⌄" : "↕";
    }
};
using StepperModel = BasicStepper<DefaultStepperModelPolicy>;
} // namespace ii::gui
