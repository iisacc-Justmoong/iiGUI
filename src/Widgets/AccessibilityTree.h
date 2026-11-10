#pragma once

#include "AccessibilityGrouping.h"
#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ii::gui::detail {

enum class AccessibilityProjectionRole {
    group, text, button, link, check_box, radio_button, tab, tab_group,
    input, slider, stepper, progress, menu, menu_item, table, cell,
    column_header, image, scroll_area
};

struct AccessibilityProjectionNode final {
    WidgetSnapshot snapshot;
    std::optional<std::size_t> parent;
    std::vector<std::size_t> children;
    Rect bounds;
    AccessibilityProjectionRole role=AccessibilityProjectionRole::group;
    std::string name;
    std::optional<std::string> value;
    bool selected=false,actionable=false;
};

// An immutable owner-thread projection. Native peers keep only the generation-
// qualified snapshot ID and resolve it in a fresh projection on each request.
class AccessibilityTree final {
    using Role=AccessibilityProjectionRole;
    std::vector<AccessibilityProjectionNode> nodes_;
    std::vector<std::size_t> roots_;
    std::unordered_map<std::string,std::size_t> ids_;
    std::optional<std::size_t> focus_;

    static Rect intersect(Rect a,Rect b){
        const auto x=std::max(a.x,b.x),y=std::max(a.y,b.y);
        const auto r=std::min(a.x+a.width,b.x+b.width),bottom=std::min(a.y+a.height,b.y+b.height);
        return {x,y,std::max(0.f,r-x),std::max(0.f,bottom-y)};
    }
    static Role role(const WidgetSnapshot& n){
        if(n.accessibility_semantics)switch(n.accessibility_semantics->role){
            case AccessibilityRole::image:return Role::image;
            case AccessibilityRole::progress:return Role::progress;
            case AccessibilityRole::slider:return Role::slider;
            case AccessibilityRole::stepper:return Role::stepper;
            case AccessibilityRole::tab_group:return Role::tab_group;
            case AccessibilityRole::menu:return Role::menu;
            case AccessibilityRole::table:return Role::table;
            case AccessibilityRole::cell:return Role::cell;
            case AccessibilityRole::column_header:return Role::column_header;
            case AccessibilityRole::column_header_group:return Role::group;
            case AccessibilityRole::automatic:break;
        }
        if(n.text_input)return Role::input;
        switch(n.kind){
            case WidgetKind::label:return Role::text;
            case WidgetKind::button:case WidgetKind::combo_box:return Role::button;
            case WidgetKind::check_box:case WidgetKind::toggle_switch:return Role::check_box;
            case WidgetKind::radio_button:return Role::radio_button;
            case WidgetKind::tab:return Role::tab;
            case WidgetKind::input_field:return Role::input;
            case WidgetKind::slider:return Role::slider;
            case WidgetKind::stepper:return Role::stepper;
            case WidgetKind::progress:return Role::progress;
            case WidgetKind::menu:return Role::menu;
            case WidgetKind::menu_item:return Role::menu_item;
            case WidgetKind::image:return Role::image;
            case WidgetKind::scroll_view:return Role::scroll_area;
            case WidgetKind::action_surface:
                switch(n.action_role){
                    case ActionRole::button:return Role::button;
                    case ActionRole::link:return Role::link;
                    case ActionRole::check_box:case ActionRole::toggle_switch:return Role::check_box;
                    case ActionRole::radio_button:return Role::radio_button;
                    case ActionRole::tab:return Role::tab;
                    case ActionRole::menu_item:return Role::menu_item;
                }
                break;
            default:break;
        }
        return Role::group;
    }
    static bool can_activate(const WidgetSnapshot& n,Role r){
        if(!n.enabled||!n.accessibility_actionable)return false;
        if(r==Role::column_header||r==Role::cell)return n.kind==WidgetKind::action_surface;
        return r==Role::button||r==Role::link||r==Role::check_box||r==Role::radio_button||r==Role::tab||r==Role::menu_item;
    }
    static std::string name(const WidgetSnapshot& n){
        if(!n.accessibility_name.empty())return n.accessibility_name;
        if(n.text_input||n.kind==WidgetKind::input_field)return n.placeholder.empty()?n.id:n.placeholder;
        if(!n.text.empty()&&!n.password)return n.text;
        return n.icon_name;
    }
public:
    explicit AccessibilityTree(const std::vector<WidgetSnapshot>& source){
        const auto count=source.size(),absent=count;
        std::unordered_map<std::string_view,std::size_t> all;all.reserve(count);
        for(std::size_t i=0;i<count;++i)if(!source[i].id.empty()){
            auto [entry,inserted]=all.emplace(source[i].id,i);if(!inserted)entry->second=absent;
        }
        auto lookup=[&](std::string_view id){auto found=all.find(id);return found==all.end()?absent:found->second;};
        // A full ancestor check protects against malformed snapshots even if a
        // hidden/blocked flag was not propagated to a descendant.
        std::vector<unsigned char> valid(count,0);std::vector<std::size_t> path;path.reserve(count);
        for(std::size_t i=0;i<count;++i){
            if(valid[i])continue;path.clear();auto cursor=i;bool good=true;
            while(cursor!=absent){
                if(valid[cursor]==1)break;
                if(valid[cursor]==2||valid[cursor]==3){good=false;break;}
                const auto& n=source[cursor];
                if(n.id.empty()||lookup(n.id)!=cursor||!n.attached||!n.visible||n.input_blocked){good=false;break;}
                valid[cursor]=3;path.push_back(cursor);
                if(n.parent_id.empty())break;
                cursor=lookup(n.parent_id);if(cursor==absent){good=false;break;}
            }
            for(auto index:path)valid[index]=good?1:2;
            if(path.empty()&&valid[i]==0)valid[i]=2;
        }
        const auto groups=accessibility_group_parents(source);
        std::vector<std::size_t> projected(count,absent);
        for(std::size_t i=0;i<count;++i){
            const auto& n=source[i];if(valid[i]!=1||accessibility_structural_only(n))continue;
            AccessibilityProjectionNode item;item.snapshot=n;item.role=role(n);item.name=name(n);
            item.selected=n.selected;item.actionable=can_activate(n,item.role);
            if(n.password){item.snapshot.text.clear();item.snapshot.composition_text.clear();}
            else if((n.text_input&&(!n.text.empty()||n.text_length==0))||(item.role==Role::cell&&!n.text.empty()))item.value=n.text;
            item.bounds=n.visual_bounds;if(n.visual_clip_bounds)item.bounds=intersect(item.bounds,*n.visual_clip_bounds);
            projected[i]=nodes_.size();ids_.emplace(n.id,nodes_.size());nodes_.push_back(std::move(item));
        }
        for(std::size_t i=0;i<count;++i){
            if(projected[i]==absent)continue;
            auto parent=groups[i].value_or(absent);
            if(parent==absent||projected[parent]==absent){
                parent=source[i].parent_id.empty()?absent:lookup(source[i].parent_id);
                while(parent!=absent&&projected[parent]==absent)
                    parent=source[parent].parent_id.empty()?absent:lookup(source[parent].parent_id);
            }
            auto& item=nodes_[projected[i]];
            if(parent!=absent){item.parent=projected[parent];nodes_[*item.parent].children.push_back(projected[i]);}
            else roots_.push_back(projected[i]);
            if(source[i].focused&&!focus_)focus_=projected[i];
            // Apply any explicitly inherited clip even when a synthetic
            // snapshot did not precompute it on the descendant.
            auto ancestor=source[i].parent_id.empty()?absent:lookup(source[i].parent_id);
            while(ancestor!=absent){if(source[ancestor].visual_clip_bounds)item.bounds=intersect(item.bounds,*source[ancestor].visual_clip_bounds);
                ancestor=source[ancestor].parent_id.empty()?absent:lookup(source[ancestor].parent_id);}
        }
    }
    [[nodiscard]] const AccessibilityProjectionNode& node(std::size_t index)const{return nodes_.at(index);}
    [[nodiscard]] std::size_t size()const noexcept{return nodes_.size();}
    [[nodiscard]] const std::vector<std::size_t>& roots()const noexcept{return roots_;}
    [[nodiscard]] std::optional<std::size_t> find(std::string_view id)const{
        auto found=ids_.find(std::string(id));return found==ids_.end()?std::nullopt:std::optional<std::size_t>{found->second};
    }
    [[nodiscard]] std::optional<std::size_t> focus()const noexcept{return focus_;}
    [[nodiscard]] std::optional<std::size_t> hit(double x,double y)const{
        std::optional<std::size_t> result;std::size_t best=0;
        for(std::size_t i=0;i<nodes_.size();++i){const auto r=nodes_[i].bounds;
            if(x<r.x||y<r.y||x>=r.x+r.width||y>=r.y+r.height)continue;
            std::size_t depth=0;for(auto parent=nodes_[i].parent;parent;parent=nodes_[*parent].parent)++depth;
            if(!result||depth>=best){result=i;best=depth;}}
        return result;
    }
};
} // namespace ii::gui::detail
