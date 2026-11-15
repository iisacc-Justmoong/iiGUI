#pragma once
#include <cstddef>
#include <string>
#include <vector>
namespace ii::gui {
template<class Policy>
class BasicListItemMenu final {
    using ListItemMenuEntry=typename Policy::ListItemMenuEntry;
    using MenuEntryState=typename Policy::MenuEntryState;
    using MenuDirection=typename Policy::MenuDirection;

    static bool validate(const std::vector<ListItemMenuEntry>& entries,std::size_t depth,
                         std::size_t& count,std::size_t& bytes)noexcept{
        if(depth>maximum_depth||entries.size()>maximum_entries-count)return false;
        count+=entries.size();
        for(const auto& entry:entries){
            if((entry.divider&&(!entry.children.empty()||entry.command||!entry.label.empty()||!entry.shortcut.empty()||
               !entry.icon_name.empty()||!entry.accessibility_name.empty()||entry.state!=MenuEntryState::normal||entry.expanded||
               entry.direction!=MenuDirection::automatic||entry.show_icon_slot||entry.show_shortcut||entry.show_chevron||entry.has_child_items||entry.close_on_trigger))||
               (!entry.children.empty()&&entry.has_child_items&&!*entry.has_child_items)||
               (entry.state!=MenuEntryState::normal&&entry.state!=MenuEntryState::selected&&entry.state!=MenuEntryState::inactive)||
               (entry.direction!=MenuDirection::automatic&&entry.direction!=MenuDirection::right&&entry.direction!=MenuDirection::left&&
                entry.direction!=MenuDirection::up&&entry.direction!=MenuDirection::down))return false;
            for(const auto* text:{&entry.label,&entry.shortcut,&entry.icon_name,&entry.accessibility_name}){
                if(text->size()>(text==&entry.icon_name?256u:8192u)||!Policy::valid_text(*text)||
                   text->size()>maximum_text_bytes-bytes)return false;
                bytes+=text->size();
            }
            if(!entry.children.empty()&&!validate(entry.children,depth+1,count,bytes))return false;
        }
        return true;
    }
public:
    static constexpr std::size_t maximum_entries=400,maximum_depth=16,maximum_text_bytes=1024*1024;
    [[nodiscard]] static bool validate_options(const std::vector<ListItemMenuEntry>& entries)noexcept{
        std::size_t count=0,bytes=0;return validate(entries,1,count,bytes);
    }
};

} // namespace ii::gui
