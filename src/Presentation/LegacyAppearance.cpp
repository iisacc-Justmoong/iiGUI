#include <iiGUI/Appearance.h>
#include <cmath>

namespace ii::gui {
std::span<const TextStyleToken> lvrs_text_tokens() noexcept { return default_text_tokens(); }
WidgetAppearance lvrs_appearance() {
    auto value=default_widget_appearance();
    const auto mix=[](double accent) { return std::round(((20.0/255)*.98+accent*.02)*255)/255; };
    value.window={mix(value.primary.red),mix(value.primary.green),mix(value.primary.blue),1};
    return value;
}
} // namespace ii::gui
