#pragma once
#include <iiGUI/Export.h>
#include <array>
#include <string>
#include <span>
#include <string_view>

namespace ii::gui {
// Encoded sRGB, matching authored LVRS visual tokens (not GPU linear colors).
struct WidgetColor final {
    static constexpr bool encoded_srgb = true;
    double red=0, green=0, blue=0, alpha=1;
    friend bool operator==(const WidgetColor&,const WidgetColor&)=default;
};
enum class TextRole { title, title2, header, header2, body, description, caption, disabled };
struct TextStyleToken final {int pixels,weight;std::string_view style;};
[[nodiscard]] IIGUI_EXPORT std::span<const TextStyleToken> lvrs_text_tokens()noexcept;
struct WidgetAppearance final {
    bool styled=false;
    WidgetColor window, primary, hover, pressed, disabled_background;
    std::array<double,8> text_alpha{.9,.9,.9,.9,.8,.6,.5,.3};
    std::string font_family="Pretendard";
    double button_height=22, button_padding=8, button_radius=8;
    friend bool operator==(const WidgetAppearance&,const WidgetAppearance&)=default;
};
// Checked-in design snapshot; no LVRS/Qt/QML runtime dependency.
[[nodiscard]] IIGUI_EXPORT WidgetAppearance lvrs_appearance();
} // namespace ii::gui
