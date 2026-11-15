#pragma once
#include <iiGUI/Appearance.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace ii::gui {
enum class ModalShape { rounded, cylinder };
struct ModalPalette final {
    WidgetColor frame{},backdrop{0,0,0,0},icon_background{},icon_border{0,0,0,0};
    friend bool operator==(const ModalPalette&,const ModalPalette&)=default;
};
struct ModalConfiguration final {
    std::string title,description,message,icon_name;
    std::string accessibility_name="Modal";
    std::array<std::string,3> action_text{};
    std::array<std::string,3> action_fallback{};
    std::array<bool,3> action_enabled{true,true,true};
    unsigned action_count=0;
    bool show_icon=true,dismiss_on_background=true,dismiss_on_escape=true,motion_enabled=true;
    double minimum_width=520,maximum_width=980,preferred_width=760;
    double side_padding=24,minimum_height=210,vertical_offset=-16;
    double content_padding=20,section_spacing=20,action_spacing=12;
    double corner_radius=12,icon_size=72,icon_corner_radius=16;
    double enter_duration_ms=420,exit_duration_ms=150,overshoot=1.45,initial_scale=.92;
    ModalShape shape=ModalShape::rounded;
    ModalPalette palette;
    friend bool operator==(const ModalConfiguration&,const ModalConfiguration&)=default;
};
struct ModalRect final {
    double x=0,y=0,width=0,height=0;
    friend bool operator==(const ModalRect&,const ModalRect&)=default;
};
struct ModalAction final { std::string_view text; bool visible=false,enabled=false; };
struct DefaultModalModelPolicy final {
    using Configuration=ModalConfiguration; using Shape=ModalShape;
    using Rect=ModalRect; using Action=ModalAction;
    [[nodiscard]] static bool valid_text(std::string_view text) noexcept {
        return text.size()<=65536 && text.find('\0')==std::string_view::npos && ii::core::valid_utf8(text);
    }
    [[nodiscard]] static bool valid_color(WidgetColor color) noexcept {
        const auto channel=[](double value){return std::isfinite(value)&&value>=0&&value<=1;};
        return channel(color.red)&&channel(color.green)&&channel(color.blue)&&channel(color.alpha);
    }
    [[nodiscard]] static bool validate_extra(const Configuration& value) noexcept {
        for(const auto& text:value.action_fallback)if(!valid_text(text))return false;
        return true;
    }
    [[nodiscard]] static std::string_view fallback_action(const Configuration& value,unsigned index) noexcept {
        return value.action_fallback[index];
    }
};
// Value contracts and defaults are supplied by the application; iiGUI owns
// validation, action selection and spatial calculations.
template<class Policy>
class BasicModal final {
    using ModalConfiguration=typename Policy::Configuration;
    using ModalShape=typename Policy::Shape;
    using ModalRect=typename Policy::Rect;
    using ModalAction=typename Policy::Action;
public:
    [[nodiscard]] static bool validate(const ModalConfiguration& value) noexcept {
        const auto range = [](double number, double low, double high) {
            return std::isfinite(number) && number >= low && number <= high;
        };
        if (!Policy::valid_text(value.title) || !Policy::valid_text(value.description) ||
            !Policy::valid_text(value.message) || !Policy::valid_text(value.icon_name) ||
            !Policy::valid_text(value.accessibility_name)) return false;
        for (const auto& text : value.action_text) if (!Policy::valid_text(text)) return false;
        if (!Policy::validate_extra(value) || value.action_count > 3 || !range(value.minimum_width, 0, 8192) ||
            !range(value.maximum_width, 1, 8192) || value.minimum_width > value.maximum_width ||
            !range(value.preferred_width, 1, 8192) || !range(value.side_padding, 0, 8192) ||
            !range(value.minimum_height, 0, 8192) || !range(value.vertical_offset, -8192, 8192) ||
            !range(value.content_padding, 0, 8192) || !range(value.section_spacing, 0, 8192) ||
            !range(value.action_spacing, 0, 8192) || !range(value.corner_radius, 0, 8192) ||
            !range(value.icon_size, 1, 256) || !range(value.icon_corner_radius, 0, 8192) ||
            !range(value.enter_duration_ms, 0, 86400000) || !range(value.exit_duration_ms, 0, 86400000) ||
            !range(value.overshoot, 0, 4) || !range(value.initial_scale, .5, 1)) return false;
        if (value.shape != ModalShape::rounded && value.shape != ModalShape::cylinder) return false;
        return Policy::valid_color(value.palette.frame) && Policy::valid_color(value.palette.backdrop) &&
               Policy::valid_color(value.palette.icon_background) && Policy::valid_color(value.palette.icon_border);
    }

    bool set_configuration(ModalConfiguration value) {
        if (!validate(value)) return false;
        configuration_ = std::move(value);
        return true;
    }

    [[nodiscard]] const ModalConfiguration& configuration() const noexcept { return configuration_; }
    [[nodiscard]] std::string_view description() const noexcept {
        return configuration_.description.empty() ? configuration_.message : configuration_.description;
    }

    [[nodiscard]] unsigned action_count() const noexcept {
        if (configuration_.action_count) return configuration_.action_count;
        for (unsigned index = 3; index > 0; --index) {
            if (!configuration_.action_text[index - 1].empty()) return index;
        }
        return 0;
    }

    [[nodiscard]] ModalAction action(unsigned index) const noexcept {
        if (index >= action_count()) return {};
        const auto& text = configuration_.action_text[index];
        return {text.empty() ? Policy::fallback_action(configuration_, index) : std::string_view{text}, true,
                configuration_.action_enabled[index]};
    }

    // Content height is the host's measured inner chrome and body together.
    [[nodiscard]] std::optional<ModalRect> frame(double viewport_width, double viewport_height,
                                                  double content_height) const noexcept {
        const auto valid_extent = [](double value) {
            return std::isfinite(value) && value >= 0 && value <= 8192;
        };
        if (!valid_extent(viewport_width) || !valid_extent(viewport_height) || !valid_extent(content_height)) {
            return std::nullopt;
        }
        const auto& c = configuration_;
        const auto available = (std::max)(1., viewport_width - 2 * c.side_padding);
        const auto width = (std::max)(c.minimum_width, (std::min)({c.maximum_width, c.preferred_width, available}));
        const auto height = (std::max)(c.minimum_height, content_height + 2 * c.content_padding);
        if (!valid_extent(width) || !valid_extent(height)) return std::nullopt;
        return ModalRect{(viewport_width - width) / 2,
                         (viewport_height - height) / 2 + c.vertical_offset, width, height};
    }

    [[nodiscard]] double radius(ModalRect frame) const noexcept {
        const auto limit = (std::max)(0., (std::min)(frame.width, frame.height) / 2);
        return configuration_.shape == ModalShape::cylinder ? limit : (std::min)(configuration_.corner_radius, limit);
    }

private:
    ModalConfiguration configuration_;
};

using ModalModel=BasicModal<DefaultModalModelPolicy>;
} // namespace ii::gui
