#pragma once
#include <iiGUI/Graphics.h>
#include <cmath>
namespace ii::gui {
// Convert application-supplied encoded sRGB to the renderer's linear-light color.
// Callers validate channel ranges before conversion; alpha remains linear.
template<class Encoded>
[[nodiscard]] inline Color srgb_to_linear(const Encoded& color) noexcept {
    const auto channel=[](double value){return value<=.04045?value/12.92:std::pow((value+.055)/1.055,2.4);};
    return {float(channel(color.red)),float(channel(color.green)),float(channel(color.blue)),float(color.alpha)};
}
struct SrgbColorConverter final {
    template<class Encoded>[[nodiscard]] static Color convert(const Encoded& color)noexcept {
        return srgb_to_linear(color);
    }
};
} // namespace ii::gui
