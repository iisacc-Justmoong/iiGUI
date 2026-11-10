#include <iiGUI/Appearance.h>

namespace ii::gui {
std::span<const TextStyleToken> lvrs_text_tokens() noexcept { return default_text_tokens(); }
WidgetAppearance lvrs_appearance() { return default_widget_appearance(); }
} // namespace ii::gui
