#pragma once

#include "AccessibilityTree.h"
#include "AtSpiText.h"
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ii::gui::detail {

// The numeric values follow the AT-SPI Accessible.GetRole contract.
enum class AtSpiRole : std::uint32_t {
    check_box=7, column_header=10, frame=23, image=27, label=29,
    menu=33, menu_item=35, tab=37, tab_group=38, panel=39,
    password=40, progress=42, button=43, radio_button=44,
    scroll_area=49, slider=51, stepper=52, table=55, cell=56,
    text=61, application=75, entry=79, link=88
};

struct AtSpiObject final {
    std::string path, parent, name, description, id;
    AtSpiRole role=AtSpiRole::panel;
    std::vector<std::string> children;
    Rect bounds;
    // An absent origin means that screen-relative geometry is unavailable.
    std::optional<WidgetPoint> screen_origin;
    std::optional<std::string> value;
    std::optional<AccessibilityRange> numeric;
    std::optional<AtSpiTextTarget> text_state;
    bool enabled=true,focused=false,selected=false,checked=false;
    bool read_only=false,actionable=false;
};

struct AtSpiSurfaceToken final {
    std::uint64_t window=0, generation=0;
    friend bool operator==(const AtSpiSurfaceToken&,const AtSpiSurfaceToken&)=default;
};

// Owner-thread-only model; no D-Bus connection, native windows or callbacks are
// owned here. A future transport must revalidate actions at actual dispatch.
class AtSpiApplication final {
    struct Surface final {
        std::uint64_t generation=0, next_node=1;
        std::shared_ptr<const AccessibilityTree> tree;
        std::unordered_map<std::string,std::uint64_t> ids;
        std::unordered_map<std::uint64_t,std::size_t> indices;
        std::vector<std::uint64_t> serials;
    };
    struct Window final {
        std::string title;
        Size size;
        std::optional<WidgetPoint> origin;
        std::optional<Surface> active,staged;
    };
    struct Parsed final { std::uint64_t window=0,generation=0,node=0; };

    const std::thread::id owner_=std::this_thread::get_id();
    std::string name_;
    std::map<std::uint64_t,Window> windows_;
    std::uint64_t next_window_=1,next_generation_=1;

    [[nodiscard]] bool on_owner()const noexcept{return owner_==std::this_thread::get_id();}
    static void advance(std::uint64_t& value)noexcept{
        value=value==std::numeric_limits<std::uint64_t>::max()?0:value+1;
    }
    [[nodiscard]] static bool valid_string(const std::string& value){
        return value.find('\0')==std::string::npos&&bool(TextIndex::create(value));
    }
    [[nodiscard]] static bool positive_size(Size size)noexcept{
        constexpr auto limit=std::numeric_limits<float>::max();
        return std::isfinite(size.width)&&std::isfinite(size.height)&&
            size.width>0&&size.height>0&&size.width<=limit&&size.height<=limit;
    }
    [[nodiscard]] static bool number(std::string_view text,std::uint64_t& value)noexcept{
        if(text.empty()||(text.size()>1&&text.front()=='0'))return false;
        const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);
        return error==std::errc{}&&end==text.data()+text.size()&&value!=0;
    }
    [[nodiscard]] static std::optional<Parsed> parse_node(std::string_view path)noexcept{
        constexpr std::string_view prefix="/org/a11y/atspi/accessible/n";
        if(!path.starts_with(prefix))return std::nullopt;
        path.remove_prefix(prefix.size());
        const auto first=path.find('_');if(first==path.npos)return std::nullopt;
        const auto second=path.find('_',first+1);if(second==path.npos)return std::nullopt;
        Parsed result;
        if(!number(path.substr(0,first),result.window)||
           !number(path.substr(first+1,second-first-1),result.generation)||
           !number(path.substr(second+1),result.node))return std::nullopt;
        return result;
    }
    [[nodiscard]] static std::optional<std::uint64_t> parse_window(std::string_view path)noexcept{
        constexpr std::string_view prefix="/org/a11y/atspi/accessible/w";
        if(!path.starts_with(prefix))return std::nullopt;
        std::uint64_t id=0;return number(path.substr(prefix.size()),id)?std::optional{id}:std::nullopt;
    }
    [[nodiscard]] static std::string node_path(std::uint64_t window,std::uint64_t generation,std::uint64_t node){
        return "/org/a11y/atspi/accessible/n"+std::to_string(window)+"_"+
            std::to_string(generation)+"_"+std::to_string(node);
    }
    [[nodiscard]] static AtSpiRole role(const AccessibilityProjectionNode& node)noexcept{
        using R=AccessibilityProjectionRole;
        switch(node.role){
            case R::group:return AtSpiRole::panel;
            case R::text:return AtSpiRole::label;
            case R::button:return AtSpiRole::button;
            case R::link:return AtSpiRole::link;
            case R::check_box:return AtSpiRole::check_box;
            case R::radio_button:return AtSpiRole::radio_button;
            case R::tab:return AtSpiRole::tab;
            case R::tab_group:return AtSpiRole::tab_group;
            case R::input:return node.snapshot.password?AtSpiRole::password:
                (node.snapshot.multiline?AtSpiRole::text:AtSpiRole::entry);
            case R::slider:return AtSpiRole::slider;
            case R::stepper:return AtSpiRole::stepper;
            case R::progress:return AtSpiRole::progress;
            case R::menu:return AtSpiRole::menu;
            case R::menu_item:return AtSpiRole::menu_item;
            case R::table:return AtSpiRole::table;
            case R::cell:return AtSpiRole::cell;
            case R::column_header:return AtSpiRole::column_header;
            case R::image:return AtSpiRole::image;
            case R::scroll_area:return AtSpiRole::scroll_area;
        }
        return AtSpiRole::panel;
    }
    [[nodiscard]] static std::optional<Surface> make_surface(std::uint64_t generation,
            const std::vector<WidgetSnapshot>& snapshots,const Surface* previous){
        Surface candidate;candidate.generation=generation;
        candidate.next_node=previous?previous->next_node:1;
        candidate.tree=std::make_shared<AccessibilityTree>(snapshots);
        candidate.ids.reserve(candidate.tree->size());candidate.indices.reserve(candidate.tree->size());
        candidate.serials.reserve(candidate.tree->size());
        for(std::size_t i=0;i<candidate.tree->size();++i){
            const auto& id=candidate.tree->node(i).snapshot.id;
            std::uint64_t serial=0;
            if(previous){const auto found=previous->ids.find(id);if(found!=previous->ids.end())serial=found->second;}
            if(!serial){if(!candidate.next_node)return std::nullopt;serial=candidate.next_node;advance(candidate.next_node);}
            candidate.ids.emplace(id,serial);candidate.indices.emplace(serial,i);candidate.serials.push_back(serial);
        }
        return candidate;
    }
    [[nodiscard]] Surface* surface(AtSpiSurfaceToken token)noexcept{
        const auto found=windows_.find(token.window);if(found==windows_.end())return nullptr;
        auto& window=found->second;
        if(window.active&&window.active->generation==token.generation)return &*window.active;
        if(window.staged&&window.staged->generation==token.generation)return &*window.staged;
        return nullptr;
    }
public:
    explicit AtSpiApplication(std::string name):name_(std::move(name)){}
    AtSpiApplication(const AtSpiApplication&)=delete;
    AtSpiApplication& operator=(const AtSpiApplication&)=delete;
    [[nodiscard]] static constexpr std::string_view root_path()noexcept{return "/org/a11y/atspi/accessible/root";}
    [[nodiscard]] static std::string window_path(std::uint64_t window){return "/org/a11y/atspi/accessible/w"+std::to_string(window);}
    [[nodiscard]] std::optional<std::uint64_t> open_window(std::string title,Size size,
            std::optional<WidgetPoint> origin=std::nullopt){
        if(!on_owner()||!next_window_||!valid_string(name_)||!valid_string(title)||!positive_size(size)||
           (origin&&(!std::isfinite(origin->x)||!std::isfinite(origin->y))))return std::nullopt;
        const auto id=next_window_;
        windows_.emplace(id,Window{std::move(title),size,origin,std::nullopt,std::nullopt});advance(next_window_);return id;
    }
    [[nodiscard]] bool close_window(std::uint64_t window){return on_owner()&&windows_.erase(window)!=0;}
    [[nodiscard]] bool update_window(std::uint64_t id,std::string title,Size size,
            std::optional<WidgetPoint> origin=std::nullopt){
        if(!on_owner()||!valid_string(title)||!positive_size(size)||
           (origin&&(!std::isfinite(origin->x)||!std::isfinite(origin->y))))return false;
        const auto found=windows_.find(id);if(found==windows_.end())return false;
        found->second.title=std::move(title);found->second.size=size;found->second.origin=origin;return true;
    }
    [[nodiscard]] std::optional<std::uint64_t> window_of(std::string_view path)const{
        if(!on_owner())return std::nullopt;
        const auto parsed=parse_node(path);if(!parsed)return std::nullopt;
        const auto found=windows_.find(parsed->window);if(found==windows_.end()||!found->second.active)return std::nullopt;
        const auto& active=*found->second.active;
        return active.generation==parsed->generation&&active.indices.contains(parsed->node)?std::optional{parsed->window}:std::nullopt;
    }
    [[nodiscard]] std::optional<AtSpiTextTarget> text_target(std::string_view path)const{
        if(!on_owner())return std::nullopt;const auto parsed=parse_node(path);if(!parsed)return std::nullopt;
        const auto window=windows_.find(parsed->window);if(window==windows_.end()||!window->second.active)return std::nullopt;
        const auto& active=*window->second.active;if(active.generation!=parsed->generation)return std::nullopt;
        const auto found=active.indices.find(parsed->node);if(found==active.indices.end())return std::nullopt;
        return atspi_text_target(active.tree->node(found->second).snapshot);
    }
    [[nodiscard]] std::optional<AtSpiSurfaceToken> stage(std::uint64_t window,const std::vector<WidgetSnapshot>& snapshots){
        if(!on_owner()||!next_generation_)return std::nullopt;
        const auto found=windows_.find(window);if(found==windows_.end()||found->second.staged)return std::nullopt;
        auto candidate=make_surface(next_generation_,snapshots,nullptr);if(!candidate)return std::nullopt;
        const AtSpiSurfaceToken token{window,next_generation_};
        found->second.staged=std::move(*candidate);advance(next_generation_);return token;
    }
    [[nodiscard]] bool commit(AtSpiSurfaceToken token){
        if(!on_owner())return false;
        const auto found=windows_.find(token.window);if(found==windows_.end())return false;
        auto& window=found->second;
        if(!window.staged||window.staged->generation!=token.generation)return false;
        window.active=std::move(window.staged);window.staged.reset();return true;
    }
    [[nodiscard]] bool update(AtSpiSurfaceToken token,const std::vector<WidgetSnapshot>& snapshots){
        if(!on_owner())return false;
        auto* current=surface(token);if(!current)return false;
        auto candidate=make_surface(token.generation,snapshots,current);if(!candidate)return false;
        *current=std::move(*candidate);return true;
    }
    [[nodiscard]] bool retire(AtSpiSurfaceToken token){
        if(!on_owner())return false;
        const auto found=windows_.find(token.window);if(found==windows_.end())return false;
        auto& window=found->second;
        if(window.active&&window.active->generation==token.generation){window.active.reset();return true;}
        if(window.staged&&window.staged->generation==token.generation){window.staged.reset();return true;}
        return false;
    }
    [[nodiscard]] std::optional<AtSpiObject> object(std::string_view path)const{
        if(!on_owner())return std::nullopt;
        if(path==root_path()){
            AtSpiObject result;result.path=path;result.name=name_;result.role=AtSpiRole::application;
            for(const auto& [id,window]:windows_)result.children.push_back(window_path(id));
            return result;
        }
        if(auto id=parse_window(path)){
            const auto found=windows_.find(*id);if(found==windows_.end())return std::nullopt;
            const auto& window=found->second;AtSpiObject result;
            result.path=path;result.parent=root_path();result.name=window.title;result.role=AtSpiRole::frame;
            result.bounds={0,0,static_cast<float>(window.size.width),static_cast<float>(window.size.height)};
            result.screen_origin=window.origin;
            if(window.active)for(auto index:window.active->tree->roots())
                result.children.push_back(node_path(*id,window.active->generation,window.active->serials[index]));
            return result;
        }
        const auto parsed=parse_node(path);if(!parsed)return std::nullopt;
        const auto found=windows_.find(parsed->window);if(found==windows_.end())return std::nullopt;
        const auto& window=found->second;if(!window.active||window.active->generation!=parsed->generation)return std::nullopt;
        const auto& active=*window.active;const auto index=active.indices.find(parsed->node);
        if(index==active.indices.end())return std::nullopt;
        const auto& node=active.tree->node(index->second);AtSpiObject result;
        result.path=path;result.parent=node.parent?node_path(parsed->window,parsed->generation,active.serials[*node.parent]):window_path(parsed->window);
        result.name=node.name;result.description=node.snapshot.accessibility_description;result.role=role(node);result.id=node.snapshot.id;
        result.bounds=node.bounds;result.screen_origin=window.origin;result.value=node.value;
        result.enabled=node.snapshot.enabled;result.focused=active.tree->focus()==index->second;
        result.selected=node.selected;result.checked=node.snapshot.checked;result.read_only=node.snapshot.read_only;
        result.actionable=node.actionable;
        if(result.role==AtSpiRole::slider||result.role==AtSpiRole::progress){
            std::optional<AccessibilityRange> range;
            if(node.snapshot.accessibility_semantics)range=node.snapshot.accessibility_semantics->range;
            else if(node.snapshot.kind==WidgetKind::slider||node.snapshot.kind==WidgetKind::progress)
                range=AccessibilityRange{node.snapshot.minimum,node.snapshot.maximum,node.snapshot.value};
            if(range&&std::isfinite(range->minimum)&&std::isfinite(range->maximum)&&std::isfinite(range->value)&&
               std::isfinite(range->maximum-range->minimum)&&range->minimum<range->maximum&&
               range->value>=range->minimum&&range->value<=range->maximum)result.numeric=range;
        }
        result.text_state=atspi_text_target(node.snapshot);
        for(auto child:node.children)result.children.push_back(node_path(parsed->window,parsed->generation,active.serials[child]));
        return result;
    }
    // Resolves only the current projection. The caller must resolve again on
    // the owner thread immediately before delivering the returned input.
    [[nodiscard]] std::optional<WidgetInput> action(std::string_view path,WidgetInputKind kind)const{
        if(!on_owner())return std::nullopt;
        const auto parsed=parse_node(path);if(!parsed)return std::nullopt;
        const auto found=windows_.find(parsed->window);if(found==windows_.end()||!found->second.active)return std::nullopt;
        const auto& active=*found->second.active;if(active.generation!=parsed->generation)return std::nullopt;
        const auto index=active.indices.find(parsed->node);if(index==active.indices.end())return std::nullopt;
        const auto& node=active.tree->node(index->second);
        if(!node.snapshot.enabled||(kind==WidgetInputKind::activate&&!node.actionable)||
           (kind!=WidgetInputKind::activate&&kind!=WidgetInputKind::focus))return std::nullopt;
        WidgetInput input;input.kind=kind;input.target=node.snapshot.id;return input;
    }
};
} // namespace ii::gui::detail
