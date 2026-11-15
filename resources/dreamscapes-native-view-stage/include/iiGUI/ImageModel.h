#pragma once
#include <iiGUI/Appearance.h>
#include <iiGUI/Image.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace ii::gui {
[[nodiscard]] inline bool valid_image_accessibility_text(std::string_view text) noexcept {
    return text.size()<=65536 && text.find('\0')==std::string_view::npos && ii::core::valid_utf8(text);
}
struct ImageSize final {
    double width = 0, height = 0;
    friend bool operator==(const ImageSize&, const ImageSize&) = default;
};
struct ImageRect final {
    double x = 0, y = 0, width = 0, height = 0;
    friend bool operator==(const ImageRect&, const ImageRect&) = default;
};
struct ImageConfiguration final {
    double width = 32, height = 32, padding = 0, corner_radius = 0, border_width = 0;
    std::optional<WidgetColor> background, border;
    std::optional<ImageFit> fit;
    bool preserve_aspect = false, circular = false, allow_empty = true;
    std::string accessibility_name;
    friend bool operator==(const ImageConfiguration&, const ImageConfiguration&) = default;
};
struct ImageGeometry final {
    ImageRect outer, image, source{0, 0, 1, 1};
    double radius = 0, image_radius = 0, border_width = 0;
    bool draw_image = false, border_on_image = false;
};

// Owns validation and layout; it neither decodes pixels nor assumes a host.
class Image final {
public:
    [[nodiscard]] const ImageConfiguration& configuration() const noexcept { return configuration_; }
    [[nodiscard]] std::optional<ImageSize> source_size() const noexcept { return source_size_; }

    [[nodiscard]] static bool validate(const ImageConfiguration& value,
                                       std::optional<ImageSize> source) noexcept {
        const auto dimension = [](double n) { return std::isfinite(n) && n >= 0 && n <= 32767; };
        const auto color = [](const std::optional<WidgetColor>& c) {
            const auto channel = [](double n) { return std::isfinite(n) && n >= 0 && n <= 1; };
            return !c || (channel(c->red) && channel(c->green) && channel(c->blue) && channel(c->alpha));
        };
        if (!dimension(value.width) || value.width == 0 || !dimension(value.height) || value.height == 0 ||
            !dimension(value.padding) || value.padding * 2 > (std::min)(value.width, value.height) ||
            !dimension(value.corner_radius) || !dimension(value.border_width) ||
            !color(value.background) || !color(value.border) || !valid_image_accessibility_text(value.accessibility_name))
            return false;
        if (value.fit && *value.fit != ImageFit::stretch && *value.fit != ImageFit::contain &&
            *value.fit != ImageFit::cover) return false;
        if (!source) return value.allow_empty;
        return std::isfinite(source->width) && std::isfinite(source->height) &&
               source->width > 0 && source->height > 0 && source->width <= 8192 && source->height <= 8192;
    }

    bool set_configuration(ImageConfiguration value, std::optional<ImageSize> source) {
        if (!validate(value, source)) return false;
        configuration_ = std::move(value);
        source_size_ = source;
        return true;
    }

    [[nodiscard]] std::optional<ImageGeometry> geometry(double width, double height,
            std::optional<double> relative_padding = std::nullopt) const noexcept {
        if (!std::isfinite(width) || !std::isfinite(height) || width < 0 || height < 0 ||
            width > 32767 || height > 32767 || (relative_padding &&
            (!std::isfinite(*relative_padding) || *relative_padding < 0 || *relative_padding > .5))) return std::nullopt;
        ImageGeometry result;
        result.outer = {0, 0, width, height};
        if (configuration_.circular) {
            const auto side = (std::min)(width, height);
            result.outer = {(width - side) / 2, (height - side) / 2, side, side};
        }
        const auto half = (std::min)(result.outer.width, result.outer.height) / 2;
        result.radius = configuration_.circular ? half : (std::min)(configuration_.corner_radius, half);
        result.border_width = (std::min)(configuration_.border_width, half);
        const auto pad = relative_padding ? (std::min)(result.outer.width,result.outer.height) * *relative_padding
                                          : configuration_.padding;
        result.image = {result.outer.x + pad, result.outer.y + pad,
                        (std::max)(0.0, result.outer.width - 2 * pad),
                        (std::max)(0.0, result.outer.height - 2 * pad)};
        if (!source_size_ || result.image.width == 0 || result.image.height == 0) return result;
        const auto fit = configuration_.fit.value_or(configuration_.preserve_aspect ? ImageFit::contain : ImageFit::stretch);
        if (fit == ImageFit::contain) {
            const auto scale = (std::min)(result.image.width / source_size_->width, result.image.height / source_size_->height);
            const auto fitted_width = source_size_->width * scale, fitted_height = source_size_->height * scale;
            result.image.x += (result.image.width - fitted_width) / 2;
            result.image.y += (result.image.height - fitted_height) / 2;
            result.image.width = fitted_width; result.image.height = fitted_height;
        } else if (fit == ImageFit::cover) {
            const auto source_ratio = source_size_->width / source_size_->height;
            const auto destination_ratio = result.image.width / result.image.height;
            if (source_ratio > destination_ratio) {
                result.source.width = destination_ratio / source_ratio;
                result.source.x = (1 - result.source.width) / 2;
            } else {
                result.source.height = source_ratio / destination_ratio;
                result.source.y = (1 - result.source.height) / 2;
            }
        }
        result.draw_image = result.image.width > 0 && result.image.height > 0;
        if (fit != ImageFit::contain) {
            result.image_radius = (std::min)((std::max)(0.0, result.radius - pad),
                                           (std::min)(result.image.width, result.image.height) / 2);
            result.border_on_image = pad == 0 && configuration_.border.has_value();
        }
        return result;
    }
private:
    ImageConfiguration configuration_;
    std::optional<ImageSize> source_size_;
};
} // namespace ii::gui
