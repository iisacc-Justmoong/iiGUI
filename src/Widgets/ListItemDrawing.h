#pragma once
#include <iiGUI/ListItemModel.h>
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/SrgbColor.h>
#include <algorithm>
#include <cmath>
#include <exception>
#include <memory>
#include <string>
#include <string_view>

namespace ii::gui {
struct DefaultListItemDrawingPolicy final {
    using Model=ListItemModel;using Variant=ListRowVariant;using Rect=ListRowRect;
    [[nodiscard]] static Color encoded_color(WidgetColor value) noexcept {
        return srgb_to_linear(value);
    }
    [[nodiscard]] static double value_font_size(const ListRowConfiguration& c) noexcept{return c.value_font_size;}
    [[nodiscard]] static double value_line_height(const ListRowConfiguration& c) noexcept{return c.value_line_height;}
};
template<class Policy>
struct BasicListItemDrawing final {
    typename Policy::Model model;
    std::shared_ptr<const ii::gui::RasterImage> icon, chevron;
};
template<class Policy>
[[nodiscard]] inline ii::gui::ActionOptions list_item_action(const BasicListItemDrawing<Policy>& drawing) {
    using ListRowVariant=typename Policy::Variant;
    const auto& c = drawing.model.configuration(); const auto size = drawing.model.intrinsic_size();
    auto description=c.description;
    if(c.variant==ListRowVariant::detail){const auto append=[&](const std::string& v){if(v.empty())return;if(!description.empty())description+=" · ";description+=v;};
        if(c.show_detail_date)append(c.detail_date);if(c.show_detail_folders){append(c.detail_folder1);append(c.detail_folder2);}
        if(c.show_detail_tags){append(c.detail_tag1);append(c.detail_tag2);}}
    if((c.variant==ListRowVariant::navigation||c.variant==ListRowVariant::checkable)&&c.show_value&&c.value&&!c.value->empty()){
        if(!description.empty())description+=" · ";description+=*c.value;
    }
    return {.accessibility_name = c.accessibility_name.empty() ? c.label : c.accessibility_name,
            .enabled = c.enabled, .focus_ring = false, .width = size.width, .height = size.height,
            .interaction_enabled = c.interactive && c.selectable, .accessibility_description = std::move(description)};
}
template<class Rect>
[[nodiscard]] inline ii::gui::Rect list_item_rect(Rect value) noexcept {
    return {float(value.x), float(value.y), float(value.width), float(value.height)};
}
template<class Policy>
[[nodiscard]] inline ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> list_item_paint(
        const BasicListItemDrawing<Policy>& drawing, const ii::gui::WidgetPaintState& state,bool include_content=true) {
    using ListRowRect=typename Policy::Rect;
    try {
        const auto geometry = drawing.model.geometry(state.size.width, state.size.height);
        if (!geometry) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid list item extent");
        const auto& c = drawing.model.configuration(); auto visual = drawing.model.visual({state.hovered, state.pressed, state.focused});
        if (!state.enabled) { visual.text = c.palette.disabled_text; visual.description = c.palette.disabled_text; visual.focused = false; }
        auto result = std::make_shared<ii::gui::WidgetDrawing>();
        const auto background = Policy::encoded_color(visual.background);
        const auto radius = float((std::min)({c.corner_radius, state.size.width / 2, state.size.height / 2}));
        if (background.alpha > 0 && state.size.width > 0 && state.size.height > 0)
            result->panels.push_back({list_item_rect(geometry->surface), background, background, radius});
        if (visual.focused) { const auto focus = Policy::encoded_color(visual.focus);
            result->panels.push_back({list_item_rect(geometry->surface), focus, focus, radius, 1.5f}); }
        if (include_content && drawing.icon && geometry->icon.width > 0) result->images.push_back({.bounds=list_item_rect(geometry->icon), .image=drawing.icon, .opacity=state.enabled ? 1.f : .35f});
        if (include_content && drawing.chevron && geometry->chevron.width > 0) result->images.push_back({.bounds=list_item_rect(geometry->chevron), .image=drawing.chevron, .opacity=state.enabled ? 1.f : .35f});
        const auto clip = list_item_rect(geometry->surface);
        if (include_content && !c.label.empty() && geometry->label.width > 0 && geometry->label.height > 0)
            result->texts.push_back({.bounds=list_item_rect(geometry->label), .text=c.label, .color=Policy::encoded_color(visual.text),
                .font_size=13, .font_weight=500, .alignment=ii::gui::TextAlignment::left, .font_family=state.font_family,
                .line_height=16, .elide_right=true, .clip=clip});
        if (include_content && geometry->description.width > 0 && geometry->description.height > 0)
            result->texts.push_back({.bounds=list_item_rect(geometry->description), .text=c.description, .color=Policy::encoded_color(visual.description),
                .font_size=11, .font_weight=400, .alignment=ii::gui::TextAlignment::left, .font_family=state.font_family,
                .line_height=14, .elide_right=true, .clip=clip});
        if(include_content&&c.value&&geometry->value.width>0&&geometry->value.height>0)
            result->texts.push_back({.bounds=list_item_rect(geometry->value),.text=*c.value,.color=Policy::encoded_color(visual.description),
                .font_size=float(Policy::value_font_size(c)),.font_weight=400,.alignment=ii::gui::TextAlignment::right,
                .font_family=state.font_family,.line_height=float(Policy::value_line_height(c)),.elide_right=true,.clip=clip});
        const auto metadata=[&](ListRowRect area,const std::string& first,const std::string& second={}){
            if(!include_content||area.width<=0||area.height<=0)return;auto text=first;
            if(!second.empty()){if(!text.empty())text+=" · ";text+=second;}if(text.empty())return;
            result->texts.push_back({.bounds=list_item_rect(area),.text=std::move(text),.color=Policy::encoded_color(visual.description),
                .font_size=11,.font_weight=400,.alignment=ii::gui::TextAlignment::left,.font_family=state.font_family,.line_height=14,.elide_right=true,.clip=clip});};
        metadata(geometry->detail_date,c.detail_date);metadata(geometry->detail_folders,c.detail_folder1,c.detail_folder2);
        metadata(geometry->detail_tags,c.detail_tag1,c.detail_tag2);
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(result));
    } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "List item paint failed", std::current_exception()); }
}

using ListItemDrawing=BasicListItemDrawing<DefaultListItemDrawingPolicy>;
// Application composition can inject this port without depending on a concrete
// design-system policy; the drawing and input types remain owned by iiGUI.
struct ListItemDrawingPort final {
    template<class Drawing>[[nodiscard]] static auto action(const Drawing& drawing){return list_item_action(drawing);}
    template<class Drawing>[[nodiscard]] static auto paint(const Drawing& drawing,const WidgetPaintState& state,bool content){
        return list_item_paint(drawing,state,content);
    }
};
} // namespace ii::gui
