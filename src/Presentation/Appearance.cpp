#include <iiGUI/Appearance.h>

namespace ii::gui {
void require_text_weight_rules_abi_v1() noexcept {}
std::span<const TextStyleToken> default_text_tokens()noexcept{
    static constexpr std::array tokens{TextStyleToken{26,700,"Bold"},TextStyleToken{22,700,"Bold"},TextStyleToken{17,600,"SemiBold"},TextStyleToken{15,600,"SemiBold"},TextStyleToken{13,500,"Medium"},TextStyleToken{12,600,"SemiBold"},TextStyleToken{11,400,"Regular"}};
    return tokens;
}
WidgetAppearance default_widget_appearance() {
    WidgetAppearance value;
    value.styled=true;
    value.primary={0.08120492845773697,0.5099760293960571,1,1};
    value.window={20.0/255,20.0/255,20.0/255,1};
    // Scale RGB uniformly to retain the hue and saturation of the default accent.
    value.hover={value.primary.red/1.12,value.primary.green/1.12,value.primary.blue/1.12,1};
    value.pressed={value.primary.red/1.2,value.primary.green/1.2,value.primary.blue/1.2,1};
    value.disabled_background={0.09516431391239166,0.09655113518238068,0.09793794900178909,1};
    return value;
}
} // namespace ii::gui
