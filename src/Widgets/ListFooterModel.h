#pragma once

#include <iiGUI/ToolbarButtonModel.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <string>

namespace ii::gui {

class FooterCommand {
public:
    virtual ~FooterCommand() = default;
    virtual void invoke() const = 0;
};
struct FooterSlot final {
    std::string icon_name, text, accessibility_name;
    bool enabled = true, visible = true, menu_indicator = false;
    std::shared_ptr<const FooterCommand> command;
    friend bool operator==(const FooterSlot&, const FooterSlot&) = default;
};
struct FooterOptions final {
    std::array<FooterSlot, 3> slots{};
    double slot_padding=4,icon_text_spacing=4,menu_indicator_width=18;
    std::string menu_indicator_text="⌄";
    double width = 0, height = 0, horizontal_padding = 2, vertical_padding = 2, spacing = 0;
    double icon_size = 18, button_height = 22, font_size = 12, corner_radius = 8, slot_width = 0;
    bool enabled = true, interactive = true;
    ToolbarButtonPalette palette;
    friend bool operator==(const FooterOptions&, const FooterOptions&) = default;
};
struct FooterRect final { double x = 0, y = 0, width = 0, height = 0; };
struct FooterGeometry final {
    double width = 0, height = 0;
    std::array<FooterRect, 3> slots;
};

struct DefaultListFooterModelPolicy final {
    using Configuration=FooterOptions;using Geometry=FooterGeometry;
    using ButtonConfiguration=ToolbarButtonConfiguration;using ButtonModel=ToolbarButtonModel;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return DefaultButtonPolicy::valid_text(value);}
    [[nodiscard]] static bool valid_extra(const Configuration& value)noexcept {
        const auto bound=[](double n){return std::isfinite(n)&&n>=0&&n<=8192;};
        return bound(value.slot_padding)&&bound(value.icon_text_spacing)&&bound(value.menu_indicator_width)&&valid_text(value.menu_indicator_text);
    }
    [[nodiscard]] static double gap(const Configuration& value)noexcept{return value.icon_text_spacing;}
    [[nodiscard]] static double slot_padding(const Configuration& value)noexcept{return value.slot_padding;}
    [[nodiscard]] static double menu_width(const Configuration& value)noexcept{return value.menu_indicator_width;}
};
template<class Policy>
class BasicListFooter final {
    using FooterOptions=typename Policy::Configuration;
    using FooterGeometry=typename Policy::Geometry;
    using ToolbarButtonConfiguration=typename Policy::ButtonConfiguration;
    using ToolbarButton=typename Policy::ButtonModel;
public:
    [[nodiscard]] static bool validate(const FooterOptions& value) noexcept {
        const auto bound = [](double n, double low, double high) { return std::isfinite(n) && n >= low && n <= high; };
        for (const auto& slot : value.slots)
            if (slot.icon_name.size() > 256 || !Policy::valid_text(slot.icon_name) || !Policy::valid_text(slot.text) ||
                !Policy::valid_text(slot.accessibility_name)) return false;
        ToolbarButtonConfiguration button; button.palette = value.palette; button.icon_size = value.icon_size;
        button.height = value.button_height; button.font_size = value.font_size; button.corner_radius = value.corner_radius;
        return ToolbarButton::validate(button) && Policy::valid_extra(value) && bound(value.width, 0, 8192) && bound(value.height, 0, 8192) &&
            bound(value.horizontal_padding, 0, 8192) && bound(value.vertical_padding, 0, 8192) && bound(value.spacing, 0, 8192) && bound(value.slot_width, 0, 8192) &&
            2 * value.vertical_padding + value.button_height <= 8192;
    }
    [[nodiscard]] static std::optional<FooterGeometry> geometry(
            const FooterOptions& value, const std::array<double, 3>& measured_text = {}) noexcept {
        if (!validate(value)) return std::nullopt;
        std::array<double, 3> widths{}; double total = 0; std::size_t visible = 0;
        for (std::size_t i = 0; i < value.slots.size(); ++i) {
            if (!std::isfinite(measured_text[i]) || measured_text[i] < 0 || measured_text[i] > 8192) return std::nullopt;
            const auto& slot = value.slots[i]; if (!slot.visible) continue; ++visible;
            const auto icon = slot.icon_name.empty() ? 0. : value.icon_size;
            const auto text = slot.text.empty() ? 0. : measured_text[i];
            const auto gap = icon > 0 && text > 0 ? Policy::gap(value) : 0.;
            const auto menu = slot.menu_indicator ? Policy::menu_width(value) : 0.;
            widths[i] = value.slot_width > 0 ? value.slot_width : (std::max)(value.button_height, Policy::slot_padding(value) + icon + gap + text + menu); total += widths[i];
        }
        const auto gaps = visible > 1 ? (visible - 1) * value.spacing : 0.;
        const auto natural_width = 2 * value.horizontal_padding + total + gaps;
        if (natural_width > 8192) return std::nullopt;
        FooterGeometry result; result.width = value.width ? value.width : natural_width;
        result.height = value.height ? value.height : value.button_height + 2 * value.vertical_padding;
        const auto inset = (std::min)(value.horizontal_padding, result.width / 2);
        const auto available = (std::max)(0., result.width - 2 * inset);
        const auto gap = visible > 1 ? (std::min)(value.spacing, available / double(visible - 1)) : 0.;
        const auto remaining = (std::max)(0., available - gap * (visible ? visible - 1 : 0));
        const auto scale = total > 0 ? (std::min)(1., remaining / total) : 0.;
        const auto top = (std::min)(value.vertical_padding, result.height / 2);
        const auto height = (std::min)(value.button_height, (std::max)(0., result.height - 2 * top));
        double x = inset; bool first = true;
        for (std::size_t i = 0; i < value.slots.size(); ++i) if (value.slots[i].visible) {
            if (!first) x += gap; first = false;
            result.slots[i] = {x, (result.height - height) / 2, widths[i] * scale, height}; x += widths[i] * scale;
        }
        return result;
    }
};

using ListFooterModel=BasicListFooter<DefaultListFooterModelPolicy>;
} // namespace ii::gui
