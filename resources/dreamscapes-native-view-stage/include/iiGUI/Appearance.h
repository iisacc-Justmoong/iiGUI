#pragma once
#include <iiGUI/Export.h>
#include <array>
#include <optional>
#include <string>
#include <span>
#include <string_view>
#include <vector>

#define IIGUI_TEXT_WEIGHT_RULES_API 1

namespace ii::gui {
// Encoded sRGB, not GPU linear colors.
struct WidgetColor final {
    static constexpr bool encoded_srgb = true;
    double red=0, green=0, blue=0, alpha=1;
    friend bool operator==(const WidgetColor&,const WidgetColor&)=default;
};
enum class TextRole { title, title2, header, header2, body, description, caption, disabled };
struct TextStyleToken final {int pixels,weight;std::string_view style;};
// Historical policy API retained for source compatibility.
[[nodiscard]] IIGUI_EXPORT std::span<const TextStyleToken> lvrs_text_tokens()noexcept;
[[nodiscard]] IIGUI_EXPORT std::span<const TextStyleToken> default_text_tokens()noexcept;
// Referencing this symbol makes a new-header/old-library mismatch fail at link time.
IIGUI_EXPORT void require_text_weight_rules_abi_v1() noexcept;
struct TextWeightRule final {
    double font_size=13;
    unsigned weight=500;
    friend bool operator==(const TextWeightRule&,const TextWeightRule&)=default;
};
inline constexpr double default_widget_corner_radius=8;
struct WidgetAppearance final {
    bool styled=false;
    WidgetColor window, primary, hover, pressed, disabled_background;
    std::array<double,8> text_alpha{.9,.9,.9,.9,.8,.6,.5,.3};
    std::string font_family="Pretendard";
    double button_height=22, button_padding=8, button_radius=default_widget_corner_radius;
    // nullopt retains the historical defaults; an empty vector uses role weights.
    // Matching sizes are applied in order, so the final matching rule wins.
    std::optional<std::vector<TextWeightRule>> text_weight_rules;
    friend bool operator==(const WidgetAppearance&,const WidgetAppearance&)=default;
};
// The default preserves the existing generic host appearance.
[[nodiscard]] IIGUI_EXPORT WidgetAppearance default_widget_appearance();
// Historical API retained for source compatibility.
[[nodiscard]] IIGUI_EXPORT WidgetAppearance lvrs_appearance();
} // namespace ii::gui
