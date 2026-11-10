#pragma once

#include <iiGUI/NativeWidgets.h>

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ii::gui::detail {

// Preserve the existing native host filter. A custom text provider or explicit
// semantics always remains exposed, including on a structural widget.
[[nodiscard]] inline bool accessibility_structural_only(const WidgetSnapshot& node) noexcept {
    if(node.text_input||node.accessibility_semantics)return false;
    return node.kind==WidgetKind::vertical_stack||node.kind==WidgetKind::horizontal_stack||
        node.kind==WidgetKind::overlay_stack||node.kind==WidgetKind::absolute_stack||node.kind==WidgetKind::spacer||
        node.kind==WidgetKind::tab_bar||node.kind==WidgetKind::segmented_control||node.kind==WidgetKind::menu_divider;
}

// Group tab/menu actions and table cells/headers under their nearest semantic
// owner through layout and activation wrappers. Table elements may also cross
// a scroll viewport. Other semantic or interactive surfaces, hidden ancestors,
// and ambiguous identities are boundaries. Cache paths for large trees.
[[nodiscard]] inline std::vector<std::optional<std::size_t>> accessibility_group_parents(
        const std::vector<WidgetSnapshot>& snapshots) {
    const auto size=snapshots.size();
    std::vector<std::optional<std::size_t>> result(size);
    std::unordered_map<std::string_view,std::size_t> ids;ids.reserve(size);
    for(std::size_t i=0;i<size;++i)if(!snapshots[i].id.empty()){
        auto [entry,inserted]=ids.emplace(snapshots[i].id,i);
        if(!inserted)entry->second=size;
    }
    const auto lookup=[&](std::string_view id){
        const auto found=ids.find(id);return found==ids.end()?size:found->second;
    };
    const auto wrapper=[](WidgetKind kind){return kind==WidgetKind::vertical_stack||kind==WidgetKind::horizontal_stack||
        kind==WidgetKind::overlay_stack||kind==WidgetKind::absolute_stack||kind==WidgetKind::content_host;};
    constexpr std::ptrdiff_t unknown=-2,none=-1;
    std::array<std::vector<std::ptrdiff_t>,4> cached;
    for(auto& entries:cached)entries.assign(size,unknown);
    std::vector<std::size_t> visited(size,0),path;path.reserve(size);
    for(std::size_t item=0;item<size;++item){
        const auto& node=snapshots[item];
        if(!node.visible||node.input_blocked||node.kind!=WidgetKind::action_surface)continue;
        std::size_t role=0;
        if(node.accessibility_semantics&&node.accessibility_semantics->role==AccessibilityRole::cell)role=2;
        else if(node.accessibility_semantics&&node.accessibility_semantics->role==AccessibilityRole::column_header)role=3;
        else if(node.action_role==ActionRole::tab)role=0;
        else if(node.action_role==ActionRole::menu_item)role=1;
        else continue;
        const auto expected=role==0?AccessibilityRole::tab_group:role==1?AccessibilityRole::menu:AccessibilityRole::table;
        auto current=lookup(node.parent_id);std::ptrdiff_t found=none;path.clear();const auto visit=item+1;
        while(current<size){
            if(cached[role][current]!=unknown){found=cached[role][current];break;}
            if(visited[current]==visit)break;
            visited[current]=visit;
            const auto& parent=snapshots[current];
            if(!parent.visible||parent.input_blocked){cached[role][current]=none;break;}
            if(parent.accessibility_semantics){
                const auto parent_role=parent.accessibility_semantics->role;
                found=(parent_role==expected||(role==3&&parent_role==AccessibilityRole::column_header_group))?static_cast<std::ptrdiff_t>(current):none;
                cached[role][current]=found;break;
            }
            if(!wrapper(parent.kind)&&!(role>=2&&parent.kind==WidgetKind::scroll_view)){cached[role][current]=none;break;}
            path.push_back(current);current=lookup(parent.parent_id);
        }
        for(const auto step:path)cached[role][step]=found;
        if(found>=0)result[item]=static_cast<std::size_t>(found);
    }
    return result;
}
} // namespace ii::gui::detail
