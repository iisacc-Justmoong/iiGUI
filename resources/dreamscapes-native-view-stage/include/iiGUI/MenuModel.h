#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace ii::gui {
enum class MenuEntryKind {action,submenu,divider};
enum class MenuNavigation {first,last,previous,next};
enum class MenuErrorCode {invalid,duplicate,limit,closed,unavailable,stale};
struct MenuError final {MenuErrorCode code;std::string message;};
template<class T>using MenuResult=std::expected<T,MenuError>;
struct MenuEntry final {
    std::string key,label,shortcut,icon_name,accessibility_name;MenuEntryKind kind=MenuEntryKind::action;
    MenuItemState state=MenuItemState::normal;bool enabled=true,visible=true,compact=true;
    std::optional<double> height;std::optional<bool> close_on_trigger;
    friend bool operator==(const MenuEntry&,const MenuEntry&)=default;
};
struct MenuConfiguration final {
    double item_width=141,item_spacing=2,left_padding=8,right_padding=8,top_padding=8,bottom_padding=8;
    double corner_radius=8,opacity=.12,edge_margin=4;
    bool auto_close_on_trigger=true,compact_items=true,show_icon_slot=true,dismiss_outside=true,dismiss_context=true;
    std::string accessibility_name;WidgetColor base{},background{},border{};
    friend bool operator==(const MenuConfiguration&,const MenuConfiguration&)=default;
};
struct MenuRect final {double x=0,y=0,width=0,height=0;};
struct MenuLayout final {double width=0,height=0;std::vector<MenuRect> rows;};
struct MenuActivation final {std::uint64_t epoch=0,revision=0;std::size_t index=0;std::string key;bool close_on_complete=false;};
struct DefaultMenuModelPolicy final {
    using Configuration=MenuConfiguration;using Entry=MenuEntry;using EntryKind=MenuEntryKind;using EntryState=MenuItemState;
    using Navigation=MenuNavigation;using Error=MenuError;using ErrorCode=MenuErrorCode;
    using Activation=MenuActivation;using Layout=MenuLayout;using Color=WidgetColor;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept {
        return value.size()<=65536&&value.find('\0')==std::string_view::npos&&ii::core::valid_utf8(value);
    }
};
// The framework owns identity, selection, activation epochs and menu layout.
template<class Policy>
class BasicMenu final {
    using MenuConfiguration=typename Policy::Configuration;
    using MenuEntry=typename Policy::Entry;
    using MenuEntryKind=typename Policy::EntryKind;
    using MenuEntryState=typename Policy::EntryState;
    using MenuNavigation=typename Policy::Navigation;
    using MenuError=typename Policy::Error;
    using MenuErrorCode=typename Policy::ErrorCode;
    using MenuActivation=typename Policy::Activation;
    using MenuLayout=typename Policy::Layout;
    using StyleColor=typename Policy::Color;
    template<class T>using MenuResult=std::expected<T,MenuError>;
    MenuConfiguration configuration_;
    std::vector<MenuEntry> entries_;
    int selected_index_=-1;
    std::uint64_t epoch_=0,revision_=0;
    bool open_=false;
    [[nodiscard]] static bool available(const MenuEntry& entry)noexcept{
        return entry.visible&&entry.enabled&&entry.kind!=MenuEntryKind::divider&&entry.state!=MenuEntryState::inactive;
    }
    [[nodiscard]] static bool valid_text(const std::string& value,std::size_t max)noexcept{
        return value.size()<=max&&Policy::valid_text(value);
    }
    [[nodiscard]] static bool valid_entry(const MenuEntry& entry)noexcept{
        if(entry.key.empty()||entry.key.find_first_not_of(" \t\r\n")==std::string::npos||!valid_text(entry.key,256)||
           !valid_text(entry.label,65536)||!valid_text(entry.shortcut,65536)||!valid_text(entry.icon_name,256)||
           !valid_text(entry.accessibility_name,65536)||(entry.height&&(!std::isfinite(*entry.height)||*entry.height<0||*entry.height>8192)))return false;
        if(entry.kind!=MenuEntryKind::action&&entry.kind!=MenuEntryKind::submenu&&entry.kind!=MenuEntryKind::divider)return false;
        return entry.state==MenuEntryState::normal||entry.state==MenuEntryState::selected||entry.state==MenuEntryState::inactive;
    }
    [[nodiscard]] MenuResult<void> mutation_allowed()const{
        if(revision_==(std::numeric_limits<std::uint64_t>::max)())return std::unexpected(MenuError{MenuErrorCode::limit,"Menu revision exhausted"});
        return {};
    }
public:
    [[nodiscard]] static bool validate(const MenuConfiguration& value)noexcept{
        const auto bound=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
        const auto color=[&](StyleColor c){return bound(c.red,0,1)&&bound(c.green,0,1)&&bound(c.blue,0,1)&&bound(c.alpha,0,1);};
        return bound(value.item_width,1,8192)&&bound(value.item_spacing,0,8192)&&bound(value.left_padding,0,8192)&&
               bound(value.right_padding,0,8192)&&bound(value.top_padding,0,8192)&&bound(value.bottom_padding,0,8192)&&
               bound(value.corner_radius,0,8192)&&bound(value.opacity,0,1)&&bound(value.edge_margin,0,8192)&&
               value.item_width+value.left_padding+value.right_padding<=8192&&Policy::valid_text(value.accessibility_name)&&
               color(value.base)&&color(value.background)&&color(value.border);
    }
    [[nodiscard]] const MenuConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] const std::vector<MenuEntry>& entries()const noexcept{return entries_;}
    [[nodiscard]] bool is_open()const noexcept{return open_;}
    [[nodiscard]] int selected_index()const noexcept{return selected_index_;}
    [[nodiscard]] std::uint64_t epoch()const noexcept{return epoch_;}
    [[nodiscard]] MenuResult<void> set_configuration(MenuConfiguration value){
        if(!validate(value))return std::unexpected(MenuError{MenuErrorCode::invalid,"Invalid menu configuration"});
        if(value==configuration_)return {};
        if(auto allowed=mutation_allowed();!allowed)return allowed;
        configuration_=std::move(value);++revision_;return {};
    }
    [[nodiscard]] MenuResult<void> set_entries(std::vector<MenuEntry> entries){
        if(entries.size()>4096)return std::unexpected(MenuError{MenuErrorCode::limit,"Too many menu entries"});
        std::set<std::string,std::less<>> keys;std::size_t bytes=0;
        for(const auto& entry:entries){
            if(!valid_entry(entry))return std::unexpected(MenuError{MenuErrorCode::invalid,"Invalid menu entry"});
            const auto size=entry.key.size()+entry.label.size()+entry.shortcut.size()+entry.icon_name.size()+entry.accessibility_name.size();
            if(size>4*1024*1024-bytes)return std::unexpected(MenuError{MenuErrorCode::limit,"Menu entry byte budget exceeded"});
            bytes+=size;if(!keys.insert(entry.key).second)return std::unexpected(MenuError{MenuErrorCode::duplicate,"Duplicate menu key"});
        }
        if(entries==entries_)return {};
        if(auto allowed=mutation_allowed();!allowed)return allowed;
        std::string selected;
        if(selected_index_>=0&&std::size_t(selected_index_)<entries_.size())selected=entries_[std::size_t(selected_index_)].key;
        int next=-1;
        if(!selected.empty())for(std::size_t i=0;i<entries.size();++i)if(entries[i].key==selected&&available(entries[i])){next=int(i);break;}
        entries_=std::move(entries);selected_index_=next;++revision_;return {};
    }
    [[nodiscard]] MenuResult<std::uint64_t> open(){
        if(open_)return epoch_;
        if(epoch_==(std::numeric_limits<std::uint64_t>::max)()||!mutation_allowed())
            return std::unexpected(MenuError{MenuErrorCode::limit,"Menu activation counter exhausted"});
        open_=true;++epoch_;++revision_;return epoch_;
    }
    [[nodiscard]] MenuResult<void> close(){
        if(!open_)return {};
        if(auto allowed=mutation_allowed();!allowed)return allowed;
        open_=false;++revision_;return {};
    }
    [[nodiscard]] MenuResult<void> select(int index){
        if(index < -1 || (index>=0&&(std::size_t(index)>=entries_.size()||!available(entries_[std::size_t(index)]))))
            return std::unexpected(MenuError{MenuErrorCode::unavailable,"Menu selection is unavailable"});
        if(index==selected_index_)return {};
        if(auto allowed=mutation_allowed();!allowed)return allowed;
        selected_index_=index;++revision_;return {};
    }
    [[nodiscard]] MenuResult<int> navigate(MenuNavigation direction){
        if(!open_)return std::unexpected(MenuError{MenuErrorCode::closed,"Menu is closed"});
        if(direction!=MenuNavigation::first&&direction!=MenuNavigation::last&&direction!=MenuNavigation::previous&&direction!=MenuNavigation::next)
            return std::unexpected(MenuError{MenuErrorCode::invalid,"Unknown menu navigation"});
        const auto count=entries_.size();if(!count){if(auto result=select(-1);!result)return std::unexpected(result.error());return -1;}
        const bool backward=direction==MenuNavigation::previous||direction==MenuNavigation::last;
        std::size_t index=direction==MenuNavigation::last?count-1:0;
        if(direction==MenuNavigation::previous&&selected_index_>=0)index=(std::size_t(selected_index_)+count-1)%count;
        if(direction==MenuNavigation::next&&selected_index_>=0)index=(std::size_t(selected_index_)+1)%count;
        for(std::size_t i=0;i<count;++i){
            if(available(entries_[index])){if(auto result=select(int(index));!result)return std::unexpected(result.error());return int(index);}
            index=backward?(index+count-1)%count:(index+1)%count;
        }
        if(auto result=select(-1);!result)return std::unexpected(result.error());return -1;
    }
    [[nodiscard]] MenuResult<MenuActivation> prepare_activation(std::size_t index)const{
        if(!open_)return std::unexpected(MenuError{MenuErrorCode::closed,"Menu is closed"});
        if(index>=entries_.size()||!available(entries_[index]))return std::unexpected(MenuError{MenuErrorCode::unavailable,"Menu entry is unavailable"});
        const auto& entry=entries_[index];const bool close=entry.close_on_trigger.value_or(entry.kind!=MenuEntryKind::submenu&&configuration_.auto_close_on_trigger);
        return MenuActivation{epoch_,revision_,index,entry.key,close};
    }
    [[nodiscard]] MenuResult<bool> complete_activation(const MenuActivation& plan){
        if(!open_||plan.epoch!=epoch_||plan.revision!=revision_||plan.index>=entries_.size()||entries_[plan.index].key!=plan.key)
            return std::unexpected(MenuError{MenuErrorCode::stale,"Menu changed during activation"});
        const auto& entry=entries_[plan.index];const bool should_close=entry.close_on_trigger.value_or(entry.kind!=MenuEntryKind::submenu&&configuration_.auto_close_on_trigger);
        if(plan.close_on_complete!=should_close)return std::unexpected(MenuError{MenuErrorCode::invalid,"Menu activation policy was changed"});
        if(!plan.close_on_complete){if(auto allowed=mutation_allowed();!allowed)return std::unexpected(allowed.error());++revision_;return false;}
        if(auto result=close();!result)return std::unexpected(result.error());return true;
    }
    [[nodiscard]] std::optional<MenuLayout> layout()const{
        MenuLayout result;result.width=configuration_.item_width+configuration_.left_padding+configuration_.right_padding;
        result.rows.resize(entries_.size());double y=configuration_.top_padding;bool first=true;
        for(std::size_t index=0;index<entries_.size();++index){const auto& entry=entries_[index];if(!entry.visible)continue;
            if(!first)y+=configuration_.item_spacing;first=false;
            const auto height=entry.height.value_or(entry.kind==MenuEntryKind::divider?3.:(entry.compact?18.:24.));
            if(y+height+configuration_.bottom_padding>32767)return std::nullopt;
            result.rows[index]={configuration_.left_padding,y,configuration_.item_width,height};y+=height;
        }
        result.height=y+configuration_.bottom_padding;return result;
    }
};
using MenuModel=BasicMenu<DefaultMenuModelPolicy>;
} // namespace ii::gui
