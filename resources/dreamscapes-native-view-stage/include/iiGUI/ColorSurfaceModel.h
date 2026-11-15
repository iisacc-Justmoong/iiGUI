#pragma once
#include <iiGUI/ButtonModel.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
namespace ii::gui {
enum class ColorSurfaceKind {wheel,hue_saturation,saturation_brightness,spectrum,hue_rail,brightness_rail};
struct ColorSurfacePoint final {double x=0,y=0;friend bool operator==(const ColorSurfacePoint&,const ColorSurfacePoint&)=default;};
struct ColorSurfaceFrame final {WidgetColor color;ColorSurfacePoint marker;std::uint64_t revision=0;std::uint32_t width=0,height=0;std::vector<std::uint8_t> rgba;};
enum class ColorSurfaceErrorCode {invalid,stale,unavailable};
struct ColorSurfaceError final {ColorSurfaceErrorCode code;std::string message;};
template<class T>using ColorSurfaceResult=std::expected<T,ColorSurfaceError>;
class ColorSurfaceSource {public:virtual ~ColorSurfaceSource()=default;
 [[nodiscard]] virtual std::uint64_t revision()const noexcept=0;
 [[nodiscard]] virtual ColorSurfaceResult<ColorSurfaceFrame> frame(ColorSurfaceKind,std::uint32_t,std::uint32_t)const=0;
 [[nodiscard]] virtual ColorSurfaceResult<std::uint64_t> edit(ColorSurfaceKind,ColorSurfacePoint,std::uint64_t)=0;};
struct ColorSurfaceConfiguration final {ColorSurfaceKind kind=ColorSurfaceKind::wheel;double width=192,height=192,marker_diameter=12;
 bool enabled=true;std::string accessibility_name="Choose color";friend bool operator==(const ColorSurfaceConfiguration&,const ColorSurfaceConfiguration&)=default;};
struct DefaultColorSurfaceModelPolicy final {
 using Configuration=ColorSurfaceConfiguration;using Kind=ColorSurfaceKind;using Point=ColorSurfacePoint;using Frame=ColorSurfaceFrame;
 [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return DefaultButtonPolicy::valid_text(value);}
 [[nodiscard]] static bool valid_color(WidgetColor c)noexcept{const auto n=[](double v){return std::isfinite(v)&&v>=0&&v<=1;};return n(c.red)&&n(c.green)&&n(c.blue)&&n(c.alpha);}
};
template<class Policy> class BasicColorSurface final {
    using ColorSurfaceConfiguration=typename Policy::Configuration;using ColorSurfaceKind=typename Policy::Kind;
    using ColorSurfacePoint=typename Policy::Point;using ColorSurfaceFrame=typename Policy::Frame;
public:
    [[nodiscard]] static bool valid_kind(ColorSurfaceKind kind) noexcept {
        switch (kind) {
        case ColorSurfaceKind::wheel:
        case ColorSurfaceKind::hue_saturation:
        case ColorSurfaceKind::saturation_brightness:
        case ColorSurfaceKind::spectrum:
        case ColorSurfaceKind::hue_rail:
        case ColorSurfaceKind::brightness_rail: return true;
        }
        return false;
    }

    [[nodiscard]] static bool validate(const ColorSurfaceConfiguration& value) noexcept {
        const auto within = [](double value, double low, double high) {
            return std::isfinite(value) && value >= low && value <= high;
        };
        return valid_kind(value.kind) && within(value.width, 12, 1024) &&
               within(value.height, 12, 1024) && within(value.marker_diameter, 2, 64) &&
               Policy::valid_text(value.accessibility_name);
    }

    bool set_configuration(ColorSurfaceConfiguration value) {
        if (!validate(value)) return false;
        configuration_ = std::move(value);
        return true;
    }

    [[nodiscard]] const ColorSurfaceConfiguration& configuration() const noexcept {
        return configuration_;
    }

    // Pointer input is clipped at the rectangular surface edge; the source
    // resolves circular wheel coordinates according to its color model.
    [[nodiscard]] std::optional<ColorSurfacePoint> position(
            double x, double y, double width, double height) const noexcept {
        if (!configuration_.enabled || !std::isfinite(x) || !std::isfinite(y) ||
            !std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0 ||
            width > 8192 || height > 8192) return std::nullopt;
        return ColorSurfacePoint{std::clamp(x / width, 0., 1.), std::clamp(y / height, 0., 1.)};
    }

    [[nodiscard]] static bool valid_frame(const ColorSurfaceFrame& frame) noexcept {
        const auto normalized = [](double value) {
            return std::isfinite(value) && value >= 0 && value <= 1;
        };
        return frame.width > 0 && frame.width <= 1024 && frame.height > 0 && frame.height <= 1024 &&
               frame.rgba.size() == std::size_t(frame.width) * frame.height * 4 &&
               Policy::valid_color(frame.color) && normalized(frame.marker.x) && normalized(frame.marker.y);
    }

private:
    ColorSurfaceConfiguration configuration_;
};

using ColorSurfaceModel=BasicColorSurface<DefaultColorSurfaceModelPolicy>;
} // namespace ii::gui
