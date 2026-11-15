#pragma once
#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
namespace ii::gui {
template<class Policy>class BasicHierarchyItem final {
    using HierarchyItemConfiguration=typename Policy::Configuration;using HierarchyItemGeometry=typename Policy::Geometry;
    using HierarchyItemInteraction=typename Policy::Interaction;using HierarchyItemVisual=typename Policy::Visual;
    using HierarchyItemPhase=typename Policy::Phase;using StyleColor=typename Policy::Color;
public:
    [[nodiscard]] static bool validate(const HierarchyItemConfiguration& value) noexcept {
        const auto bound = [](double n, double low, double high) { return std::isfinite(n) && n >= low && n <= high; };
        const auto color = [&](StyleColor c) { return bound(c.red, 0, 1) && bound(c.green, 0, 1) && bound(c.blue, 0, 1) && bound(c.alpha, 0, 1); };
        const auto& p = value.palette;
        return Policy::valid_text(value.key) && value.key.size() <= 1024 && Policy::valid_text(value.label) &&
            Policy::valid_text(value.icon_name) && Policy::valid_text(value.accessibility_name) && value.count >= -1 &&
            value.depth <= 65536 && bound(value.width, 1, 8192) && bound(value.height, 1, 8192) &&
            bound(value.indent_step, 0, 8192) && bound(value.icon_size, 0, 256) && bound(value.chevron_size, 0, 256) &&
            bound(value.left_padding, 0, 8192) && bound(value.right_padding, 0, 8192) &&
            bound(value.leading_spacing, 0, 8192) && bound(value.count_width, 0, 8192) && bound(value.corner_radius, 0, 8192) &&
            color(p.idle) && color(p.hover) && color(p.pressed) && color(p.active) && color(p.inactive) && color(p.drag) &&
            color(p.text) && color(p.disabled_text) && color(p.focus);
    }
    template<class Row>[[nodiscard]] static std::optional<HierarchyItemConfiguration> project(const Row& row,HierarchyItemConfiguration appearance={}) {
        appearance.key=row.entry.key;appearance.id=row.entry.id;appearance.label=row.entry.label;appearance.icon_name=row.entry.icon_name;
        appearance.count=row.entry.count;appearance.depth=row.depth;appearance.enabled=row.entry.enabled;appearance.activatable=row.entry.selectable;
        appearance.selected=row.selected;appearance.expanded=row.expanded;appearance.has_children=row.child_count>0;
        if(row.row_extent)appearance.height=*row.row_extent;if(!validate(appearance))return std::nullopt;return appearance;
    }
    bool set_configuration(HierarchyItemConfiguration value) {
        if (!validate(value)) return false; configuration_ = std::move(value); return true;
    }
    [[nodiscard]] const HierarchyItemConfiguration& configuration() const noexcept { return configuration_; }
    [[nodiscard]] std::optional<HierarchyItemGeometry> geometry(double width, double height) const noexcept {
        if (!std::isfinite(width) || !std::isfinite(height) || width < 0 || height < 0 || width > 8192 || height > 8192) return std::nullopt;
        const auto& c = configuration_; HierarchyItemGeometry result;
        result.surface = {0, 0, width, height};
        result.show_icon = !c.icon_name.empty() && c.icon_size > 0;
        result.show_count = c.count >= 0 && c.count_width > 0;
        result.show_chevron = c.show_chevron && c.has_children && c.chevron_size > 0;
        auto left = std::clamp(c.left_padding + double(c.depth) * c.indent_step, 0., width);
        if (result.show_icon) {
            const auto size = (std::min)({c.icon_size, height, (std::max)(0., width - left)});
            result.icon = {left, (height - size) / 2, size, size}; left = (std::min)(width, left + size + c.leading_spacing);
        }
        auto right = std::clamp(width - c.right_padding, 0., width);
        if (result.show_chevron || result.show_count) {
            const auto size = (std::min)({c.chevron_size, height, right});
            const auto anchor = (std::max)(0., right - size);
            if (result.show_chevron) result.chevron = {anchor, (height - size) / 2, size, size};
            right = anchor;
        }
        if (result.show_count) {
            const auto end = (std::max)(0., right - Policy::count_gap);
            const auto size = (std::min)(c.count_width, end);
            result.count = {end - size, 0, size, height}; right = end - size;
        }
        result.label = {left, 0, (std::max)(0., right - left), height};
        return result;
    }
    [[nodiscard]] HierarchyItemVisual visual(HierarchyItemInteraction interaction) const noexcept {
        const auto& c = configuration_; const auto& p = c.palette;
        const auto phase = c.drag_preview ? HierarchyItemPhase::drag : !c.enabled ? HierarchyItemPhase::inactive :
            c.selected ? HierarchyItemPhase::active : interaction.pressed ? HierarchyItemPhase::pressed :
            interaction.hovered ? HierarchyItemPhase::hover : HierarchyItemPhase::idle;
        const auto background = phase == HierarchyItemPhase::drag ? p.drag : phase == HierarchyItemPhase::inactive ? p.inactive :
            phase == HierarchyItemPhase::active ? p.active : phase == HierarchyItemPhase::pressed ? p.pressed :
            phase == HierarchyItemPhase::hover ? p.hover : p.idle;
        return {phase, background, c.enabled ? p.text : p.disabled_text, p.focus, c.enabled && interaction.focused};
    }
    [[nodiscard]] std::optional<bool> toggle_expansion() noexcept {
        if (!configuration_.enabled || !configuration_.show_chevron || !configuration_.has_children || !configuration_.chevron_size) return std::nullopt;
        configuration_.expanded = !configuration_.expanded; return configuration_.expanded;
    }
private:
    HierarchyItemConfiguration configuration_;
};

} // namespace ii::gui
