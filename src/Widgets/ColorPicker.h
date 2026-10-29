#pragma once
#include <iiGUI/Appearance.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <iiSDKCore/Runtime/Query.h>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace ii::gui {
enum class ColorChannel { hue,saturation,brightness,red,green,blue,cyan,magenta,yellow,black,alpha,gray };
enum class ColorField { wheel,hue_saturation,saturation_brightness,spectrum,hue_rail,brightness_rail };
struct ColorPickerState final {
    WidgetColor color{1,1,1,1},previous_color{1,1,1,1};
    std::string hex="FFFFFF";
    double hue=0,saturation=0,brightness=1;
    std::uint64_t revision=1;
};
struct ColorPickerChange final {
    std::uint64_t revision=0;
    bool color_changed=false,previous_changed=false,channels_changed=false,edited=false;
};
using ColorChangePort=runtime::QueryPort<ColorPickerChange>;
// Native, owner-thread color state. HSV uses degrees/unit/unit; public channel
// edits use degrees, RGB 0..255 and other channels 0..100. Colors are encoded sRGB.
// Explicit weak Core receivers provide notifications without a GUI polling loop.
class IIGUI_EXPORT ColorPickerModel final {
public:
    ColorPickerModel();
    ~ColorPickerModel();
    ColorPickerModel(const ColorPickerModel&)=delete;
    ColorPickerModel& operator=(const ColorPickerModel&)=delete;
    [[nodiscard]] core::Result<ColorPickerState> state() const;
    [[nodiscard]] core::Result<double> channel_value(ColorChannel) const;
    [[nodiscard]] core::Result<WidgetColor> channel_color(ColorChannel,double position) const;
    [[nodiscard]] core::Result<void> set_color(WidgetColor);
    [[nodiscard]] core::Result<void> set_previous_color(WidgetColor);
    [[nodiscard]] core::Result<void> edit_color(WidgetColor);
    [[nodiscard]] core::Result<void> edit_hsv(double hue,double saturation,double brightness);
    [[nodiscard]] core::Result<void> edit_channel(ColorChannel,double value);
    // Six digits preserve alpha; eight mean RRGGBBAA. Malformed text returns false.
    [[nodiscard]] core::Result<bool> edit_hex(std::string_view);
    [[nodiscard]] core::Result<void> accept();
    [[nodiscard]] core::Result<void> cancel();
    // Idempotent per port identity; expired receivers are pruned. At most 256 live
    // receivers. Close/drop the port to detach. Mutating from a receiver is busy.
    // Notification failure reports an already committed edit; do not retry it.
    [[nodiscard]] core::Result<void> observe(std::shared_ptr<const ColorChangePort>);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace ii::gui
