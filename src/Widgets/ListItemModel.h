#pragma once
#include <iiGUI/Appearance.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace ii::gui {
enum class ListRowVariant { mini, detail, navigation, toggle, checkable, action, action_group, stepper, select, inline_edit, detail_actions, detail_quantity, detail_settings, task, resource, media, form };
struct ListRowPalette final {
    WidgetColor text{},description{},disabled_text{},idle{},hover{},pressed{},selected{},focus{};
    friend bool operator==(const ListRowPalette&,const ListRowPalette&)=default;
};
struct ListRowConfiguration final {
    ListRowVariant variant=ListRowVariant::mini;
    std::string label="Label",description,accessibility_name,icon_name;
    std::optional<std::string> value;
    std::string detail_date,detail_folder1,detail_folder2,detail_tag1,detail_tag2;
    double width=0,height=0,left_padding=4,right_padding=4;
    double icon_size=14,spacing=6,corner_radius=4,chevron_size=14;
    double reserved_leading=0;
    std::optional<double> reserved_leading_height;
    bool has_icon=false,enabled=true,interactive=true,selectable=true,selected=false;
    bool show_description=true,show_chevron=false,visible=true,show_value=true;
    bool show_detail_date=true,show_detail_folders=true,show_detail_tags=true;
    double value_width=72,value_gap=4,value_line_height=11,value_font_size=11;
    ListRowPalette palette;
    friend bool operator==(const ListRowConfiguration&,const ListRowConfiguration&)=default;
};
struct ListRowSize final {double width=0,height=0;};
struct ListRowRect final {double x=0,y=0,width=0,height=0;};
struct ListRowGeometry final {ListRowRect surface,icon,label,description,chevron,value,detail_date,detail_folders,detail_tags;};
struct ListRowInteraction final {bool hovered=false,pressed=false,focused=false;};
struct ListRowVisual final {WidgetColor background,text,description,focus;bool focused=false;};
struct DefaultListItemModelPolicy final {
    using Configuration=ListRowConfiguration;using Variant=ListRowVariant;using Size=ListRowSize;
    using Rect=ListRowRect;using Geometry=ListRowGeometry;using Interaction=ListRowInteraction;
    using Visual=ListRowVisual;using Color=WidgetColor;
    [[nodiscard]] static bool valid_text(std::string_view text) noexcept {
        return text.size()<=65536&&text.find('\0')==std::string_view::npos&&ii::core::valid_utf8(text);
    }
    [[nodiscard]] static double value_line_height(const Configuration& c) noexcept{return c.value_line_height;}
    [[nodiscard]] static double value_gap(const Configuration& c) noexcept{return c.value_gap;}
    [[nodiscard]] static bool validate_extra(const Configuration& c) noexcept {
        return std::isfinite(c.value_gap)&&c.value_gap>=0&&c.value_gap<=8192&&
               std::isfinite(c.value_line_height)&&c.value_line_height>=0&&c.value_line_height<=8192&&
               std::isfinite(c.value_font_size)&&c.value_font_size>=1&&c.value_font_size<=256;
    }
};
// The framework owns row validation, geometry and interaction-state precedence.
template<class Policy>
class BasicListItem final {
    using ListRowConfiguration=typename Policy::Configuration;
    using ListRowVariant=typename Policy::Variant;
    using ListRowSize=typename Policy::Size;
    using ListRowRect=typename Policy::Rect;
    using ListRowGeometry=typename Policy::Geometry;
    using ListRowInteraction=typename Policy::Interaction;
    using ListRowVisual=typename Policy::Visual;
    using StyleColor=typename Policy::Color;
public:
    [[nodiscard]] static bool validate(const ListRowConfiguration& value) noexcept {
        const auto number = [](double n, double max) { return std::isfinite(n) && n >= 0 && n <= max; };
        const auto color = [&](StyleColor c) { return number(c.red, 1) && number(c.green, 1) && number(c.blue, 1) && number(c.alpha, 1); };
        const auto& p = value.palette;
        return Policy::validate_extra(value) && (value.variant == ListRowVariant::mini || value.variant == ListRowVariant::detail || value.variant == ListRowVariant::navigation ||
             value.variant == ListRowVariant::toggle || value.variant == ListRowVariant::checkable ||
             value.variant == ListRowVariant::action || value.variant == ListRowVariant::action_group || value.variant == ListRowVariant::stepper || value.variant == ListRowVariant::select || value.variant == ListRowVariant::inline_edit || value.variant == ListRowVariant::detail_actions ||
             value.variant == ListRowVariant::detail_quantity || value.variant == ListRowVariant::detail_settings || value.variant == ListRowVariant::task ||
             value.variant == ListRowVariant::resource || value.variant == ListRowVariant::media || value.variant == ListRowVariant::form) &&
            Policy::valid_text(value.label) && Policy::valid_text(value.description) &&
            Policy::valid_text(value.detail_date) && Policy::valid_text(value.detail_folder1) && Policy::valid_text(value.detail_folder2) &&
            Policy::valid_text(value.detail_tag1) && Policy::valid_text(value.detail_tag2) &&
            (!value.value||Policy::valid_text(*value.value)) && number(value.value_width,8192) &&
            Policy::valid_text(value.accessibility_name) && value.icon_name.size() <= 256 && Policy::valid_text(value.icon_name) &&
            number(value.width, 8192) && number(value.height, 8192) && number(value.left_padding, 8192) && number(value.right_padding, 8192) &&
            number(value.icon_size, 256) && number(value.spacing, 8192) && number(value.corner_radius, 8192) && number(value.chevron_size, 256) && number(value.reserved_leading, 8192) &&
            (!value.reserved_leading_height||number(*value.reserved_leading_height,8192)) &&
            color(p.text) && color(p.description) && color(p.disabled_text) && color(p.idle) && color(p.hover) &&
            color(p.pressed) && color(p.selected) && color(p.focus);
    }
    bool set_configuration(ListRowConfiguration value) {
        if (!validate(value)) return false;
        configuration_ = std::move(value); return true;
    }
    [[nodiscard]] const ListRowConfiguration& configuration() const noexcept { return configuration_; }
    [[nodiscard]] ListRowSize intrinsic_size() const noexcept {
        const auto& c = configuration_;
        const double width = c.variant == ListRowVariant::mini ? 170 : c.variant == ListRowVariant::detail ? 194 :
            c.variant == ListRowVariant::action || c.variant == ListRowVariant::action_group || c.variant == ListRowVariant::stepper || c.variant == ListRowVariant::select || c.variant == ListRowVariant::inline_edit || c.variant == ListRowVariant::detail_actions ||
            c.variant == ListRowVariant::detail_quantity || c.variant == ListRowVariant::detail_settings || c.variant == ListRowVariant::task ||
            c.variant == ListRowVariant::resource || c.variant == ListRowVariant::media || c.variant == ListRowVariant::form ? 400 : 280;
        const double detail_rows=double((c.show_detail_date&&!c.detail_date.empty())+
            (c.show_detail_folders&&(!c.detail_folder1.empty()||!c.detail_folder2.empty()))+
            (c.show_detail_tags&&(!c.detail_tag1.empty()||!c.detail_tag2.empty())));
        const double height = c.variant == ListRowVariant::detail ? 44+18*detail_rows : c.variant == ListRowVariant::mini ? 20 : c.variant == ListRowVariant::detail_actions ? 120 :
            c.variant == ListRowVariant::detail_quantity ? 118 : c.variant == ListRowVariant::detail_settings ? 159 : c.variant == ListRowVariant::task ? 135 :
            c.variant == ListRowVariant::resource ? 129 : c.variant == ListRowVariant::media ? 113 : c.variant == ListRowVariant::form ? 170 : 44;
        return {c.width > 0 ? c.width : width, c.height > 0 ? c.height : height};
    }
    [[nodiscard]] std::optional<ListRowGeometry> geometry(double width, double height) const noexcept {
        if (!std::isfinite(width) || !std::isfinite(height) || width < 0 || height < 0 || width > 8192 || height > 8192)
            return std::nullopt;
        const auto& c = configuration_; ListRowGeometry result; result.surface = {0,0,width,height};
        const bool date=c.variant==ListRowVariant::detail&&c.show_detail_date&&!c.detail_date.empty();
        const bool folders=c.variant==ListRowVariant::detail&&c.show_detail_folders&&(!c.detail_folder1.empty()||!c.detail_folder2.empty());
        const bool tags=c.variant==ListRowVariant::detail&&c.show_detail_tags&&(!c.detail_tag1.empty()||!c.detail_tag2.empty());
        const auto main_height=(std::max)(0.,height-18*double(date+folders+tags));
        auto left = (std::min)(c.left_padding, width / 2);
        auto right = width - (std::min)(c.right_padding, width - left);
        if (c.show_chevron && c.chevron_size > 0) {
            const auto size = (std::min)({c.chevron_size, main_height, (std::max)(0., right - left)});
            result.chevron = {right - size, (main_height - size) / 2, size, size};
            right = (std::max)(left, right - size - (std::min)(c.spacing, (std::max)(0., right - size - left)));
        }
        if ((c.variant==ListRowVariant::navigation||c.variant==ListRowVariant::checkable)&&c.show_value&&c.value){
            const auto size=(std::min)(c.value_width,(std::max)(0.,right-left));
            const auto h=(std::min)(Policy::value_line_height(configuration_),main_height);
            result.value={right-size,(main_height-h)/2,size,h};
            right=(std::max)(left,right-size-(std::min)(Policy::value_gap(configuration_),(std::max)(0.,right-size-left)));
        }
        const auto leading_size=c.reserved_leading>0?c.reserved_leading:(c.has_icon?c.icon_size:0.);
        if (leading_size > 0) {
            const auto size = (std::min)(leading_size, (std::max)(0., right - left));
            const auto w = c.reserved_leading_height?size:(std::min)(size,main_height);
            const auto h = (std::min)(c.reserved_leading_height.value_or(w),main_height);
            result.icon = {left, (main_height - h) / 2, w, h};
            left += w + (w > 0 ? (std::min)(c.spacing, (std::max)(0., right - left - w)) : 0);
        }
        const auto available = (std::max)(0., right - left);
        const bool details = c.show_description && !c.description.empty() && main_height >= 32;
        if (details) {
            const auto y = (main_height - 30) / 2;
            result.label = {left, y, available, 16}; result.description = {left, y + 16, available, 14};
        } else {
            const auto text_height = (std::min)(16., main_height);
            result.label = {left, (main_height - text_height) / 2, available, text_height};
        }
        auto y=main_height;const auto meta=[&](bool show,ListRowRect& target){if(!show)return;y=(std::min)(height,y+4);
            const auto h=(std::min)(14.,height-y);target={left,y,available,h};y+=h;};
        meta(date,result.detail_date);meta(folders,result.detail_folders);meta(tags,result.detail_tags);
        return result;
    }
    [[nodiscard]] ListRowVisual visual(ListRowInteraction interaction) const noexcept {
        const auto& c = configuration_; const auto& p = c.palette;
        const auto background = c.selected ? p.selected : interaction.pressed ? p.pressed : interaction.hovered ? p.hover : p.idle;
        return {background, c.enabled ? p.text : p.disabled_text, c.enabled ? p.description : p.disabled_text,
                p.focus, c.enabled && c.interactive && c.selectable && interaction.focused};
    }
private:
    ListRowConfiguration configuration_;
};

using ListItemModel=BasicListItem<DefaultListItemModelPolicy>;
} // namespace ii::gui
