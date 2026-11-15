#pragma once
#include <iiGUI/MenuModel.h>
#include <iiGUI/NativeDivider.h>
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
namespace ii::gui {
// Applications supply menu values and ports; the framework owns rendering and input.
template<class Policy>
class BasicNativeMenu final {
    using MenuModel=typename Policy::Model;using MenuInput=typename Policy::Input;using NativeMenuCallbacks=typename Policy::Callbacks;
    using MenuItemPort=typename Policy::ItemPort;using MenuItemPresentation=typename Policy::Presentation;
    using MenuCloseAction=typename Policy::CloseAction;using MenuClosePlan=typename Policy::ClosePlan;
    using MenuEntry=typename Policy::Entry;using MenuEntryKind=typename Policy::EntryKind;using MenuEntryState=typename Policy::EntryState;
    using MenuConfiguration=typename Policy::Configuration;using MenuLayout=typename Policy::Layout;using StyleColor=typename Policy::Color;
    using MenuError=typename Policy::Error;using MenuErrorCode=typename Policy::ErrorCode;using MenuNavigation=typename Policy::Navigation;
    using MenuDirection=typename Policy::Direction;using MenuPopupPort=typename Policy::PopupPort;
    using MenuSectionPort=typename Policy::SectionPort;using MenuSectionAccess=typename Policy::SectionAccess;
    using MenuRegistrationPort=typename Policy::RegistrationPort;using MenuItemActivation=typename Policy::Activation;
    struct State final {
        struct Entry final {std::uint64_t id,section;std::string widget_id,key;MenuItemPort port;MenuItemPresentation applied;std::vector<std::string> group_ids;};
        struct Section final {std::uint64_t id;ii::gui::Widget host;std::string widget_id;};
        MenuModel model;
        MenuInput authored;
        ii::gui::Widget root,content;
        std::string input_scope_prefix;
        std::vector<Entry> entries;
        std::vector<Section> sections;
        NativeMenuCallbacks callbacks;
        MenuCloseAction parent_close;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0,next_id=0,next_section=0,interaction_revision=0;
        bool live=false,finalized=false,subordinate=false,retired=false,released=false;
        std::size_t activities=0;
    };
    static void release_retired(State& value)noexcept {
        if(!value.retired||value.activities||value.released)return;
        value.released=true;value.callbacks={};value.parent_close={};value.entries.clear();value.sections.clear();
    }
    struct Activity final {
        std::shared_ptr<State> state;bool counted=false;
        explicit Activity(std::shared_ptr<State> value):state(std::move(value)),counted(state->owner==std::this_thread::get_id()){
            if(counted)++state->activities;
        }
        ~Activity(){if(counted){--state->activities;release_retired(*state);}}
    };
    std::shared_ptr<State> state_;
    explicit BasicNativeMenu(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::core::Error convert(const MenuError& error){
        const auto code=error.code==MenuErrorCode::closed?ii::core::ErrorCode::closed:
            error.code==MenuErrorCode::duplicate?ii::core::ErrorCode::already_exists:
            error.code==MenuErrorCode::limit?ii::core::ErrorCode::out_of_range:
            error.code==MenuErrorCode::stale?ii::core::ErrorCode::cancelled:ii::core::ErrorCode::invalid_argument;
        return {code,error.message};
    }
    [[nodiscard]] static ii::core::Result<void> check(const std::shared_ptr<State>& state){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Menu requires its owner thread");
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Menu has retired");
        if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());return {};
    }
    [[nodiscard]] static ii::gui::StackOptions layout(const MenuConfiguration& c){
        ii::gui::StackOptions result;result.spacing=c.item_spacing;result.alignment=ii::gui::StackAlignment::stretch;
        result.insets=ii::gui::Insets{c.left_padding,c.top_padding,c.right_padding,c.bottom_padding};return result;
    }
    [[nodiscard]] static ii::gui::AccessibilitySemantics semantics(const MenuConfiguration& c){
        return {.role=ii::gui::AccessibilityRole::menu,.name=c.accessibility_name};
    }
    [[nodiscard]] static ii::gui::PopoverOptions popup_options(const MenuConfiguration& c,const MenuLayout& extent){
        ii::gui::PopoverOptions value;value.width=extent.width;value.height=(std::max)(1.,extent.height);value.padding=0;value.margins=c.edge_margin;
        value.corner_radius=c.corner_radius;value.surface_opacity=0;value.opaque_base=false;value.fit_to_viewport=true;value.clip_content=true;
        value.dismiss_on_outside_press=c.dismiss_outside;value.dismiss_on_context_request=c.dismiss_context;value.accessibility_name=c.accessibility_name;return value;
    }
    [[nodiscard]] static ii::gui::Color linear(StyleColor c)noexcept{
        const auto channel=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
        return {float(channel(c.red)),float(channel(c.green)),float(channel(c.blue)),float(c.alpha)};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint(
            const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& input){
        try{
            if(!std::isfinite(input.size.width)||!std::isfinite(input.size.height)||input.size.width<0||input.size.height<0||
               input.size.width>8192||input.size.height>8192)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid menu paint extent");
            auto result=std::make_shared<ii::gui::WidgetDrawing>();const auto& c=state->model.configuration();
            const ii::gui::Rect bounds{0,0,float(input.size.width),float(input.size.height)};
            if(bounds.width>0&&bounds.height>0){const auto radius=float((std::min)({c.corner_radius,input.size.width/2,input.size.height/2}));
                auto ink=linear(c.base);if(ink.alpha>0)result->panels.push_back({bounds,ink,ink,radius});
                auto background=c.background;background.alpha*=c.opacity;ink=linear(background);
                if(ink.alpha>0)result->panels.push_back({bounds,ink,ink,float((std::min)({c.corner_radius,input.size.width/2,input.size.height/2}))});
                ink=linear(c.border);if(ink.alpha>0)
                    result->panels.push_back({bounds,ink,ink,float((std::min)({c.corner_radius,input.size.width/2,input.size.height/2})),1});}
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(result));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Menu paint failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> present(const MenuItemPort& port,MenuItemPresentation value){
        try{return port.present(value);}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Menu item presentation failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<MenuCloseAction> close_plan(const std::shared_ptr<State>& state){
        Activity activity(state);
        if(auto valid=check(state);!valid)return std::unexpected(valid.error());
        auto snapshot=state->root.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
        if(!state->live||!snapshot->opened||!state->model.is_open())return ii::core::fail(ii::core::ErrorCode::closed,"Parent menu is closed");
        const auto epoch=state->model.epoch();const auto ancestor=state->parent_close;std::weak_ptr<State> weak=state;
        return MenuCloseAction{[weak,epoch,ancestor]()->ii::core::Result<void>{
            auto value=weak.lock();if(!value)return {};Activity activity(value);
            if(auto valid=check(value);!valid)return valid;
            auto current=value->root.snapshot();if(!current)return std::unexpected(current.error());
            if(!value->live||!current->opened||value->model.epoch()!=epoch)return {};
            if(auto closed=value->root.close_popup();!closed)return closed;
            current=value->root.snapshot();if(!current)return std::unexpected(current.error());
            if(!current->opened&&value->model.epoch()==epoch&&ancestor)return ancestor();return {};
        }};
    }
    [[nodiscard]] static ii::core::Result<void> sync(const std::shared_ptr<State>& state){
        Activity activity(state);
        try{
            if(auto valid=check(state);!valid)return valid;
            std::erase_if(state->entries,[](const auto& entry){return entry.port.retired&&entry.port.retired();});
            std::vector<MenuEntry> items;items.reserve(state->entries.size());
            for(const auto& item:state->entries){auto value=item.port.entry();if(!value)return std::unexpected(value.error());
                value->key=item.key;value->compact=state->authored.configuration.compact_items;items.push_back(std::move(*value));}
            if(auto updated=state->model.set_entries(std::move(items));!updated)return std::unexpected(convert(updated.error()));
            const auto extent=state->model.layout();if(!extent||extent->height>8192)
                return ii::core::fail(ii::core::ErrorCode::out_of_range,"Menu extent exceeds the host limit");
            const auto revision=state->revision;const auto c=state->authored.configuration;
            if(auto result=state->content.set_stack_options(layout(c));!result)return result;
            if(state->revision!=revision)return {};
            std::unordered_set<std::string> groups;
            // Group identities were validated at publication. Reading a widget
            // snapshot here would flush layout between individual updates and
            // expose an inconsistent partially updated menu to observers.
            for(const auto& item:state->entries)for(std::size_t i=0;i<item.group_ids.size();++i){
                if(groups.insert(item.group_ids[i]).second){
                    const auto& group=item.port.layout_groups[i];
                    auto sizing=group.stack_layout();if(!sizing)return std::unexpected(sizing.error());
                    if(sizing->extent.width!=0||sizing->extent.height!=0||sizing->flex!=0)
                        return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Menu layout group cannot have fixed extent or flex");
                    if(auto result=group.set_stack_options({.spacing=c.item_spacing,.alignment=ii::gui::StackAlignment::stretch});!result)return result;
                    if(state->revision!=revision)return {};
                }
            }
            if(auto result=state->content.set_accessibility_semantics(semantics(c));!result)return result;
            if(state->revision!=revision)return {};
            if(auto result=state->root.set_popover_options(popup_options(c,*extent));!result)return result;
            if(state->revision!=revision)return {};
            const auto entries=state->entries;std::vector<std::size_t> changed;changed.reserve(entries.size());
            const auto mark=[&](std::uint64_t id,MenuItemPresentation value){auto item=std::ranges::find_if(state->entries,[&](const auto& entry){return entry.id==id;});
                if(item!=state->entries.end())item->applied=value;};
            for(std::size_t index=0;index<entries.size();++index){const auto& item=entries[index];
                MenuItemPresentation value{.selected=state->model.selected_index()<0?std::nullopt:std::optional<bool>{state->model.selected_index()==int(index)},
                    .compact=c.compact_items,.show_icon_slot=c.show_icon_slot,.width=c.item_width};
                if(item.applied==value)continue;auto result=present(item.port,value);if(state->revision!=revision)return {};
                if(!result){for(auto prior=changed.rbegin();prior!=changed.rend()&&state->revision==revision;++prior)
                        if(auto restored=present(entries[*prior].port,entries[*prior].applied);restored&&state->revision==revision)mark(entries[*prior].id,entries[*prior].applied);
                    return result;}
                changed.push_back(index);mark(item.id,value);
            }
            if(state->revision!=revision)return {};return state->content.request_repaint();
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Menu synchronization failed",std::current_exception());}
    }
    static void activated(const std::weak_ptr<State>& weak,std::uint64_t id,std::function<void()> child){
        auto state=weak.lock();if(!state||!state->live)return;
        Activity activity(state);
        try{
            if(state->interaction_revision==(std::numeric_limits<std::uint64_t>::max)()){
                state->failure=ii::core::Error{ii::core::ErrorCode::out_of_range,"Menu interaction revision exhausted"};return;}
            ++state->interaction_revision;
            auto popup=state->root.snapshot();if(!popup||!popup->opened){state->failure=ii::core::Error{ii::core::ErrorCode::closed,"Menu is closed"};return;}
            if(!state->model.is_open()){auto opened=state->model.open();if(!opened){state->failure=convert(opened.error());return;}}
            const auto entry=std::ranges::find_if(state->entries,[&](const auto& value){return value.id==id;});
            if(entry==state->entries.end()){state->failure=ii::core::Error{ii::core::ErrorCode::closed,"Menu item retired"};return;}
            const auto index=std::size_t(entry-state->entries.begin());auto plan=state->model.prepare_activation(index);
            if(!plan){state->failure=convert(plan.error());return;}
            if(child)child();if(!state->live)return;
            auto still=state->root.snapshot();if(!still||!still->opened)return;
            auto triggered=state->callbacks.triggered;if(triggered)triggered(int(index));if(!state->live)return;
            still=state->root.snapshot();if(!still||!still->opened)return;
            auto completed=state->model.complete_activation(*plan);
            if(!completed){if(completed.error().code!=MenuErrorCode::stale)state->failure=convert(completed.error());return;}
            if(*completed){const auto epoch=state->model.epoch();auto parent=state->parent_close;
                auto closed=state->root.close_popup();if(!closed){state->failure=closed.error();return;}
                auto current=state->root.snapshot();if(!current){state->failure=current.error();return;}
                if(!current->opened&&state->model.epoch()==epoch&&parent)if(auto result=parent();!result)state->failure=result.error();}
        }catch(...){state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Menu activation failed",std::current_exception()};}
    }
    [[nodiscard]] static ii::core::Result<bool> pointer_move(const std::shared_ptr<State>& state,const ii::gui::WidgetInput& event){
        Activity activity(state);
        try {
            if(event.device==ii::gui::PointerDevice::touch||event.buttons||!state->live||!state->finalized||!state->model.is_open())return false;
            const auto target=state->input_scope_prefix+event.target;
            auto item=std::ranges::find_if(state->entries,[&](const auto& entry){return entry.widget_id==target;});
            // Disabled action surfaces and dividers are not hit targets. If
            // the menu content itself was hit, locate the retained row without
            // making it interactive or activating it.
            if(item==state->entries.end()&&event.local_position){
                const auto revision=state->revision,epoch=state->model.epoch();
                auto content=state->content.snapshot();if(!content)return std::unexpected(content.error());
                const bool section_hit=std::ranges::any_of(state->sections,[&](const auto& section){return section.widget_id==target;});
                if(content->id!=target&&!section_hit)return false;
                const auto x=content->bounds.x+event.local_position->x,y=content->bounds.y+event.local_position->y;
                const auto entries=state->entries;
                for(const auto& entry:entries){
                    auto row=entry.port.snapshot();if(!row)return std::unexpected(row.error());
                    if(!state->live||state->revision!=revision||state->model.epoch()!=epoch)return false;
                    if(row->visible&&x>=row->bounds.x&&x<row->bounds.x+row->bounds.width&&
                       y>=row->bounds.y&&y<row->bounds.y+row->bounds.height){
                        item=std::ranges::find_if(state->entries,[&](const auto& current){return current.id==entry.id;});break;}
                }
            }
            if(item==state->entries.end())return false;
            const auto index=std::size_t(item-state->entries.begin());
            if(index>=state->model.entries().size()||state->model.entries()[index].key!=item->key)return false;
            const auto& candidate=state->model.entries()[index];
            const bool available=candidate.visible&&candidate.enabled&&candidate.kind!=MenuEntryKind::divider&&candidate.state!=MenuEntryState::inactive;
            if(state->interaction_revision==(std::numeric_limits<std::uint64_t>::max)())
                return ii::core::fail(ii::core::ErrorCode::out_of_range,"Menu interaction revision exhausted");
            const auto interaction=++state->interaction_revision,epoch=state->model.epoch(),revision=state->revision,id=item->id;
            const auto current=[&]{return state->live&&state->finalized&&state->model.is_open()&&state->model.epoch()==epoch&&
                state->revision==revision&&state->interaction_revision==interaction&&index<state->entries.size()&&
                state->entries[index].id==id&&index<state->model.entries().size()&&
                state->entries[index].key==state->model.entries()[index].key;};
            const auto entries=state->entries;
            for(const auto& sibling:entries){
                if((available&&sibling.id==id)||!sibling.port.close_submenu)continue;
                if(auto closed=sibling.port.close_submenu();!closed)return std::unexpected(closed.error());
                if(!current())return false;
                if(sibling.port.submenu_open){auto reopened=sibling.port.submenu_open();if(!reopened)return std::unexpected(reopened.error());if(*reopened)return false;}
            }
            if(!available)return false;
            if(state->model.selected_index()!=int(index)){
                if(auto selected=state->model.select(int(index));!selected)return std::unexpected(convert(selected.error()));
                if(auto refreshed=sync(state);!refreshed)return std::unexpected(refreshed.error());
            }
            if(!current())return false;
            auto chosen=std::ranges::find_if(state->entries,[&](const auto& entry){return entry.id==id;});
            if(chosen==state->entries.end())return false;
            auto port=chosen->port;bool already_open=false;
            if(port.submenu_open){auto opened=port.submenu_open();if(!opened)return std::unexpected(opened.error());already_open=*opened;}
            if(!already_open){auto focused=port.snapshot();if(!focused)return std::unexpected(focused.error());
                if(!focused->focused){if(auto result=port.focus();!result)return std::unexpected(result.error());}
                if(!current())return false;
                auto opened=port.open_submenu();if(!opened)return std::unexpected(opened.error());}
            return false;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Menu pointer handling failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<bool> input(const std::shared_ptr<State>& state,const ii::gui::WidgetInput& event){
        Activity activity(state);
        using ii::gui::WidgetInputKind;using ii::gui::WidgetKey;
        if(event.kind==WidgetInputKind::pointer_move)return pointer_move(state,event);
        if(event.kind==WidgetInputKind::pointer_down||event.kind==WidgetInputKind::pointer_up||event.kind==WidgetInputKind::pointer_leave||event.kind==WidgetInputKind::key_down){
            if(state->interaction_revision==(std::numeric_limits<std::uint64_t>::max)())
                return ii::core::fail(ii::core::ErrorCode::out_of_range,"Menu interaction revision exhausted");
            ++state->interaction_revision;
        }
        if(event.kind!=WidgetInputKind::key_down)return false;
        if(event.key==WidgetKey::left&&state->subordinate){auto result=state->root.close_popup();if(!result)return std::unexpected(result.error());return true;}
        if(event.key==WidgetKey::right){
            for(const auto& entry:state->entries){auto focused=entry.port.snapshot();if(!focused)return std::unexpected(focused.error());
                if(focused->focused){auto opened=entry.port.open_submenu();if(!opened)return std::unexpected(opened.error());return *opened;}}
            return false;
        }
        MenuNavigation direction;
        switch(event.key){case WidgetKey::up:direction=MenuNavigation::previous;break;case WidgetKey::down:direction=MenuNavigation::next;break;
            case WidgetKey::home:direction=MenuNavigation::first;break;case WidgetKey::end:direction=MenuNavigation::last;break;default:return false;}
        try{
            if(auto valid=check(state);!valid)return std::unexpected(valid.error());
            auto popup=state->root.snapshot();if(!popup)return std::unexpected(popup.error());if(!popup->opened)return false;
            if(!state->model.is_open()){auto opened=state->model.open();if(!opened)return std::unexpected(convert(opened.error()));}
            if(direction==MenuNavigation::previous||direction==MenuNavigation::next){
                for(std::size_t i=0;i<state->entries.size();++i){auto focused=state->entries[i].port.snapshot();
                    if(!focused)return std::unexpected(focused.error());
                    if(focused->focused&&state->model.entries()[i].kind!=MenuEntryKind::divider){auto selected=state->model.select(int(i));if(!selected)return std::unexpected(convert(selected.error()));break;}}
            }
            auto index=state->model.navigate(direction);if(!index)return std::unexpected(convert(index.error()));
            if(auto refreshed=sync(state);!refreshed)return std::unexpected(refreshed.error());
            if(*index>=0&&std::size_t(*index)<state->entries.size()){auto result=state->entries[std::size_t(*index)].port.focus();if(!result)return std::unexpected(result.error());}
            return true;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Menu keyboard handling failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> replace_section(const std::shared_ptr<State>& state,std::uint64_t section_id,
            std::vector<MenuItemPort> ports){
        Activity activity(state);
        if(auto valid=check(state);!valid)return valid;
        struct Attachment final {MenuItemPort port;std::uint64_t id;bool owner=false,action=false,observer=false;};
        std::vector<Attachment> attached;
        const auto detach=[&]()noexcept{for(auto it=attached.rbegin();it!=attached.rend();++it){
            if(it->observer)try{(void)it->port.changed({});}catch(...){}
            if(it->action)try{(void)it->port.activated({});}catch(...){}
            if(it->owner)try{(void)it->port.parent_close({});}catch(...){}
        }};
        try{
            const auto slot=std::ranges::find_if(state->sections,[&](const auto& value){return value.id==section_id;});
            if(slot==state->sections.end())return ii::core::fail(ii::core::ErrorCode::closed,"Menu section has retired");
            if(ports.size()>4096)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Menu item limit exceeded");
            const auto host=slot->host;auto host_snapshot=host.snapshot();if(!host_snapshot)return std::unexpected(host_snapshot.error());
            const auto revision=state->revision,old_next=state->next_id;
            if(revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Menu revision exhausted");
            const auto unchanged=[&]{return !state->retired&&state->revision==revision&&state->next_id==old_next;};
            // Snapshots and adapter callbacks can re-enter the menu. Keep the
            // inspected entries stable until the revision check rejects them.
            auto old_entries=state->entries;
            std::unordered_map<std::string,const typename State::Entry*> existing;
            for(const auto& item:old_entries)existing.emplace(item.widget_id,&item);
            std::unordered_set<std::string> seen;std::vector<typename State::Entry> replacement;
            replacement.reserve(ports.size());attached.reserve(ports.size());auto next_id=old_next;
            for(auto& port:ports){
                if(!port.present||!port.activated||!port.changed||!port.entry||!port.snapshot||!port.focus||!port.open_submenu||!port.parent_close||!port.retired||!port.widget)
                    return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Incomplete dynamic menu item port");
                auto widget=port.widget();if(!widget)return std::unexpected(widget.error());
                auto inside=widget->is_within(host);if(!inside)return std::unexpected(inside.error());
                auto item=port.snapshot();if(!item)return std::unexpected(item.error());
                auto actual=widget->snapshot();if(!actual)return std::unexpected(actual.error());
                if(!*inside||item->id!=actual->id||item->id==host_snapshot->id)
                    return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Dynamic menu item is outside its section");
                if(port.layout_groups.size()>64)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Menu group nesting limit exceeded");
                std::unordered_set<std::string> seen_groups;std::vector<std::string> group_ids;group_ids.reserve(port.layout_groups.size());
                for(const auto& group:port.layout_groups){
                    auto geometry=group.snapshot();if(!geometry)return std::unexpected(geometry.error());
                    auto group_inside=group.is_within(host);if(!group_inside)return std::unexpected(group_inside.error());
                    auto item_inside=widget->is_within(group);if(!item_inside)return std::unexpected(item_inside.error());
                    if(geometry->kind!=ii::gui::WidgetKind::vertical_stack||!*group_inside||!*item_inside||!seen_groups.insert(geometry->id).second)
                        return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Menu layout group must be a distinct containing vertical stack in its section");
                    auto sizing=group.stack_layout();if(!sizing)return std::unexpected(sizing.error());
                    if(sizing->extent.width!=0||sizing->extent.height!=0||sizing->flex!=0)
                        return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Menu layout group cannot have fixed extent or flex");
                    group_ids.push_back(std::move(geometry->id));
                }
                if(!seen.insert(item->id).second)return ii::core::fail(ii::core::ErrorCode::already_exists,"Duplicate dynamic menu item");
                if(auto found=existing.find(item->id);found!=existing.end()){
                    if(found->second->section!=section_id)return ii::core::fail(ii::core::ErrorCode::already_exists,"Menu item belongs to another section");
                    if(found->second->group_ids!=group_ids)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Retained menu item layout groups changed");
                    replacement.push_back(*found->second);
                }else{
                    if(next_id==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Menu item identity exhausted");
                    const auto id=++next_id;replacement.push_back({id,section_id,item->id,"item-"+std::to_string(id),port,{},std::move(group_ids)});
                    attached.push_back({port,id});
                }
                if(!unchanged())return ii::core::fail(ii::core::ErrorCode::cancelled,"Menu changed while inspecting a section");
            }
            std::vector<typename State::Entry> next;next.reserve(old_entries.size()+replacement.size());bool inserted=false;
            for(const auto& entry:old_entries){
                if(entry.section==section_id)continue;
                if(!inserted&&entry.section>section_id){next.insert(next.end(),replacement.begin(),replacement.end());inserted=true;}
                next.push_back(entry);
            }
            if(!inserted)next.insert(next.end(),replacement.begin(),replacement.end());
            if(next.size()>4096)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Menu item limit exceeded");
            auto old_model=state->model;auto planned=old_model;
            std::vector<MenuEntry> values;values.reserve(next.size());
            for(const auto& entry:next){auto value=entry.port.entry();if(!value)return std::unexpected(value.error());
                value->key=entry.key;value->compact=state->authored.configuration.compact_items;values.push_back(std::move(*value));
                if(!unchanged())return ii::core::fail(ii::core::ErrorCode::cancelled,"Menu changed while preparing a section");}
            const bool same=next.size()==old_entries.size()&&std::equal(next.begin(),next.end(),old_entries.begin(),
                [](const auto& a,const auto& b){return a.id==b.id;});
            if(same&&values==state->model.entries())return {};
            if(auto valid=planned.set_entries(std::move(values));!valid)return std::unexpected(convert(valid.error()));
            const auto extent=planned.layout();if(!extent||extent->height>8192)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Menu extent exceeds the host limit");
            std::weak_ptr<State> weak=state;
            for(auto& item:attached){
                const auto id=item.id;
                if(auto r=item.port.parent_close([weak]()->ii::core::Result<MenuCloseAction>{auto value=weak.lock();if(!value)return ii::core::fail(ii::core::ErrorCode::closed,"Menu has retired");return close_plan(value);});!r){detach();return r;}item.owner=true;
                if(auto r=item.port.activated([weak,id](std::function<void()> callback){activated(weak,id,std::move(callback));});!r){detach();return r;}item.action=true;
                if(auto r=item.port.changed([weak,id]()->ii::core::Result<void>{auto value=weak.lock();if(!value)return ii::core::fail(ii::core::ErrorCode::closed,"Menu has retired");
                        if(std::ranges::none_of(value->entries,[&](const auto& entry){return entry.id==id;}))return {};
                        if(value->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Menu revision exhausted");
                        ++value->revision;auto result=sync(value);if(result)value->failure.reset();else value->failure=result.error();return result;});!r){detach();return r;}item.observer=true;
                if(!unchanged()){detach();return ii::core::fail(ii::core::ErrorCode::cancelled,"Menu changed while attaching a section");}
            }
            if(auto valid=check(state);!valid){detach();return valid;}
            if(!unchanged()){detach();return ii::core::fail(ii::core::ErrorCode::cancelled,"Menu changed before section publication");}
            state->entries=std::move(next);state->model=std::move(planned);state->next_id=next_id;
            const auto applied=++state->revision;
            if(auto result=sync(state);!result){
                if(state->revision==applied){state->entries=std::move(old_entries);state->model=std::move(old_model);state->next_id=old_next;
                    detach();if(auto restored=sync(state);!restored)state->failure=restored.error();}
                return result;
            }
            return {};
        }catch(...){auto cause=std::current_exception();detach();return ii::core::fail(ii::core::ErrorCode::failed,"Menu section replacement failed",cause);}
    }
public:
    [[nodiscard]] static ii::core::Result<MenuItemPort> divider_port(ii::gui::DividerGroupPort divider){
        if(!divider.options||!divider.visible||!divider.changed||!divider.snapshot||!divider.retired)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Incomplete menu divider port");
        try{return MenuItemPort{
            .present=[](MenuItemPresentation)->ii::core::Result<void>{return {};},
            .activated=[](MenuItemActivation)->ii::core::Result<void>{return {};},
            .changed=divider.changed,
            .entry=[divider]()->ii::core::Result<MenuEntry>{
                auto configuration=divider.options();if(!configuration)return std::unexpected(configuration.error());
                auto visible=divider.visible();if(!visible)return std::unexpected(visible.error());
                auto snapshot=divider.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
                ii::gui::Divider model;if(!model.set_configuration(*configuration))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid menu divider");
                return MenuEntry{.key=snapshot->id,.kind=MenuEntryKind::divider,.enabled=false,.visible=*visible,.height=model.extent().height};},
            .snapshot=divider.snapshot,
            .focus=[]()->ii::core::Result<void>{return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Menu divider cannot receive focus");},
            .open_submenu=[]()->ii::core::Result<bool>{return false;},
            .parent_close=[](MenuClosePlan)->ii::core::Result<void>{return {};},
            .retired=divider.retired,.widget=divider.widget
        };}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Menu divider adapter failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<BasicNativeMenu> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,MenuInput input={},NativeMenuCallbacks callbacks={}){
        if(!MenuModel::validate(input.configuration)||input.selected_index < -1||input.selected_index>=4096)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid menu configuration");
        try{
            auto state=std::make_shared<State>();state->authored=input;state->callbacks=std::move(callbacks);
            if(auto configured=state->model.set_configuration(input.configuration);!configured)return std::unexpected(convert(configured.error()));
            auto initial=state->model.layout();if(!initial)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Invalid menu extent");
            std::weak_ptr<State> weak=state;
            auto opened=[weak]{if(auto value=weak.lock();value&&value->live){Activity activity(value);auto current=value->root.snapshot();if(!current||!current->opened)return;
                auto result=value->model.open();if(!result){value->failure=convert(result.error());return;}
                auto selected=value->model.selected_index();
                if(selected<0)for(std::size_t i=0;i<value->model.entries().size();++i){const auto& entry=value->model.entries()[i];
                    if(entry.visible&&entry.enabled&&entry.kind!=MenuEntryKind::divider&&entry.state!=MenuEntryState::inactive){selected=int(i);break;}}
                if(selected>=0&&std::size_t(selected)<value->entries.size())if(auto focused=value->entries[std::size_t(selected)].port.focus();!focused){value->failure=focused.error();return;}
                if(!value->live)return;current=value->root.snapshot();if(!current||!current->opened)return;auto callback=value->callbacks.opened;if(callback)callback();}};
            auto closed=[weak]{if(auto value=weak.lock();value&&value->live){Activity activity(value);auto current=value->root.snapshot();if(!current||current->opened)return;
                auto result=value->model.close();if(!result){value->failure=convert(result.error());return;}value->parent_close={};value->subordinate=false;auto callback=value->callbacks.closed;if(callback)callback();}};
            auto canceled=[weak]{if(auto value=weak.lock();value&&value->live){Activity activity(value);auto callback=value->callbacks.canceled;if(callback)callback();}};
            auto popup=builder.popover(parent,id,popup_options(input.configuration,*initial),{.opened=std::move(opened),.closed=std::move(closed),.canceled=std::move(canceled)});
            if(!popup)return std::unexpected(popup.error());state->root=*popup;
            const auto content_id=std::string(id)+"/content";
            auto content=builder.vstack(*popup,content_id,layout(input.configuration));if(!content)return std::unexpected(content.error());state->content=*content;
            auto content_state=content->snapshot();if(!content_state)return std::unexpected(content_state.error());
            if(!content_state->id.ends_with(content_id))return ii::core::fail(ii::core::ErrorCode::failed,"Menu content scope is invalid");
            state->input_scope_prefix=content_state->id.substr(0,content_state->id.size()-content_id.size());
            if(auto r=content->set_accessibility_semantics(semantics(input.configuration));!r)return std::unexpected(r.error());
            if(auto r=content->set_paint_handler([state](const auto& value){return paint(state,value);});!r)return std::unexpected(r.error());
            if(auto r=content->set_input_handler([state](const auto& value){return BasicNativeMenu::input(state,value);});!r)return std::unexpected(r.error());
            if(auto r=builder.on_publish([weak]{if(auto value=weak.lock())value->live=true;});!r)return std::unexpected(r.error());
            if(auto r=builder.on_retire([weak]{if(auto value=weak.lock()){
                value->live=false;value->retired=true;
                if(value->revision!=(std::numeric_limits<std::uint64_t>::max)())++value->revision;
                release_retired(*value);
            }});!r)return std::unexpected(r.error());
            return BasicNativeMenu(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Menu creation failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::gui::Widget content_root()const{return state_->content;}
    [[nodiscard]] ii::core::Result<MenuInput> options()const{
        auto state=state_;Activity activity(state);
        if(auto valid=check(state);!valid)return std::unexpected(valid.error());try{return state->authored;}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Menu options failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<int> selected_index()const{auto state=state_;Activity activity(state);if(auto valid=check(state);!valid)return std::unexpected(valid.error());return state->model.selected_index();}
    [[nodiscard]] ii::core::Result<void> status()const{auto state=state_;Activity activity(state);if(auto valid=check(state);!valid)return valid;if(state->failure)return std::unexpected(*state->failure);return {};}
    [[nodiscard]] ii::core::Result<void> set_options(MenuInput value)const{
        auto state=state_;Activity activity(state);
        if(auto valid=check(state);!valid)return valid;
        if(!MenuModel::validate(value.configuration)||value.selected_index < -1||value.selected_index>=4096)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid menu configuration");
        try{
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Menu revision exhausted");
            auto previous=state->authored;auto old=state->model;
            if(auto configured=state->model.set_configuration(value.configuration);!configured)return std::unexpected(convert(configured.error()));
            if(state->finalized&&value.selected_index!=previous.selected_index){auto selected=state->model.select(value.selected_index);
                if(!selected){state->model=std::move(old);return std::unexpected(convert(selected.error()));}}
            const auto revision=++state->revision;state->authored=std::move(value);
            if(auto result=sync(state);!result){if(state->revision==revision){state->authored=std::move(previous);state->model=std::move(old);(void)sync(state);}return result;}
            if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Menu retired during update");
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Menu update failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> register_item(MenuItemPort port)const{
        auto state=state_;Activity activity(state);
        if(auto valid=check(state);!valid)return valid;
        if(!port.layout_groups.empty())return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Menu layout groups require a dynamic section");
        if(!port.present||!port.activated||!port.changed||!port.entry||!port.snapshot||!port.focus||!port.open_submenu||!port.parent_close||!port.retired)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Incomplete menu item port");
        bool action_attached=false,observer_attached=false,owner_attached=false;
        try{
            if(state->entries.size()>=4096||state->next_id==(std::numeric_limits<std::uint64_t>::max)()||state->next_section==(std::numeric_limits<std::uint64_t>::max)()||state->revision==(std::numeric_limits<std::uint64_t>::max)())
                return ii::core::fail(ii::core::ErrorCode::out_of_range,"Menu item or revision limit exceeded");
            auto item=port.snapshot(),parent=state->content.snapshot();if(!item)return std::unexpected(item.error());if(!parent)return std::unexpected(parent.error());
            if(item->parent_id!=parent->id)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Menu currently accepts direct item children only");
            if(std::ranges::any_of(state->entries,[&](const auto& entry){return entry.widget_id==item->id;}))return ii::core::fail(ii::core::ErrorCode::already_exists,"Menu item already registered");
            const auto id=++state->next_id;std::weak_ptr<State> weak=state;state->entries.reserve(state->entries.size()+1);
            typename State::Entry entry{id,++state->next_section,item->id,"item-"+std::to_string(id),port,{},{}};
            if(auto attached=port.parent_close([weak]()->ii::core::Result<MenuCloseAction>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Menu has retired");return close_plan(state);});!attached)return attached;owner_attached=true;
            if(auto attached=port.activated([weak,id](std::function<void()> callback){activated(weak,id,std::move(callback));});!attached){(void)port.parent_close({});return attached;}action_attached=true;
            if(auto attached=port.changed([weak,id]()->ii::core::Result<void>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Menu has retired");
                    if(std::ranges::none_of(state->entries,[&](const auto& entry){return entry.id==id;}))return {};
                    if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Menu revision exhausted");
                    ++state->revision;auto result=sync(state);if(result)state->failure.reset();else state->failure=result.error();return result;});!attached){(void)port.activated({});(void)port.parent_close({});return attached;}
            observer_attached=true;++state->revision;state->entries.push_back(std::move(entry));
            if(auto result=sync(state);!result){std::erase_if(state->entries,[&](const auto& value){return value.id==id;});(void)port.activated({});(void)port.changed({});(void)port.parent_close({});(void)sync(state);return result;}
            return {};
        }catch(...){auto cause=std::current_exception();try{if(observer_attached)(void)port.changed({});if(action_attached)(void)port.activated({});if(owner_attached)(void)port.parent_close({});}catch(...){}
            return ii::core::fail(ii::core::ErrorCode::failed,"Menu registration failed",cause);}
    }
    [[nodiscard]] ii::core::Result<MenuSectionPort> register_section(ii::gui::Widget host)const{
        auto state=state_;Activity activity(state);
        if(auto valid=check(state);!valid)return std::unexpected(valid.error());
        try{
            auto item=host.snapshot(),parent=state->content.snapshot();if(!item)return std::unexpected(item.error());if(!parent)return std::unexpected(parent.error());
            if(item->kind!=ii::gui::WidgetKind::content_host||item->parent_id!=parent->id)
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Menu section must be a direct content host");
            if(std::ranges::any_of(state->sections,[&](const auto& value){return value.widget_id==item->id;}))
                return ii::core::fail(ii::core::ErrorCode::already_exists,"Menu section is already registered");
            if(state->next_section==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Menu section identity exhausted");
            const auto id=state->next_section+1;std::weak_ptr<State> weak=state;
            MenuSectionPort port=[weak,id](std::vector<MenuItemPort> items)->ii::core::Result<void>{auto state=weak.lock();
                if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Menu has retired");return replace_section(state,id,std::move(items));};
            typename State::Section section{id,host,item->id};state->sections.reserve(state->sections.size()+1);
            if(auto collapsed=host.set_collapse_when_empty(true);!collapsed)return std::unexpected(collapsed.error());
            if(auto valid=check(state);!valid)return std::unexpected(valid.error());
            if(state->next_section+1!=id)return ii::core::fail(ii::core::ErrorCode::cancelled,"Menu changed during section registration");
            state->next_section=id;state->sections.push_back(std::move(section));return port;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Menu section registration failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> register_divider(DividerGroupPort divider)const{
        auto port=divider_port(std::move(divider));if(!port)return std::unexpected(port.error());return register_item(std::move(*port));
    }
    [[nodiscard]] MenuPopupPort weak_popup_port()const{
        std::weak_ptr<State> weak=state_;
        return {
            .snapshot=[weak]()->ii::core::Result<ii::gui::WidgetSnapshot>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Menu has retired");return state->root.snapshot();},
            .open=[weak](ii::gui::Widget anchor,MenuDirection direction,MenuCloseAction parent)->ii::core::Result<void>{
                auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Menu has retired");Activity activity(state);
                if(auto valid=check(state);!valid)return valid;if(!state->live||!state->finalized)return ii::core::fail(ii::core::ErrorCode::closed,"Menu is not ready");
                auto owner=anchor.snapshot(),popup=state->root.snapshot();if(!owner)return std::unexpected(owner.error());if(!popup)return std::unexpected(popup.error());
                if(popup->parent_id!=owner->id)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Submenu anchor does not own the popup");
                auto extent=state->model.layout();if(!extent)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Invalid submenu extent");
                double x=0,y=0;switch(direction){case MenuDirection::right:x=owner->bounds.width;break;case MenuDirection::left:x=-extent->width;break;
                    case MenuDirection::up:y=-extent->height;break;case MenuDirection::down:y=owner->bounds.height;break;
                    default:return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid submenu direction");}
                auto old=state->parent_close;const bool previous=state->subordinate;state->parent_close=std::move(parent);state->subordinate=true;
                auto opened=state->root.open_for(anchor,x,y);if(!opened){state->parent_close=std::move(old);state->subordinate=previous;}return opened;
            },
            .close=[weak]()->ii::core::Result<void>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Menu has retired");
                Activity activity(state);if(auto valid=check(state);!valid)return valid;auto popup=state->root.snapshot();if(!popup)return std::unexpected(popup.error());
                return popup->opened?state->root.close_popup():ii::core::Result<void>{};}
        };
    }
    [[nodiscard]] ii::core::Result<void> finalize()const{
        auto state=state_;Activity activity(state);
        if(auto valid=check(state);!valid)return valid;
        if(auto result=sync(state);!result)return result;
        auto previous=state->model;
        if(state->authored.selected_index>=0){auto selected=state->model.select(state->authored.selected_index);if(!selected)return std::unexpected(convert(selected.error()));}
        if(auto result=sync(state);!result){state->model=std::move(previous);(void)sync(state);return result;}
        state->finalized=true;return {};
    }
    [[nodiscard]] MenuRegistrationPort weak_registration_port()const{
        std::weak_ptr<State> weak=state_;
        return [weak](MenuItemPort item)->ii::core::Result<void>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Menu has retired");
            return BasicNativeMenu(std::move(state)).register_item(std::move(item));};
    }
    [[nodiscard]] MenuSectionAccess weak_section_access()const{
        std::weak_ptr<State> weak=state_;
        return {
            .register_section=[weak](ii::gui::Widget host)->ii::core::Result<MenuSectionPort>{auto state=weak.lock();
                if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Menu has retired");
                return BasicNativeMenu(std::move(state)).register_section(std::move(host));},
            .options=[weak]()->ii::core::Result<MenuInput>{auto state=weak.lock();
                if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Menu has retired");
                return BasicNativeMenu(std::move(state)).options();}
        };
    }
};
} // namespace ii::gui
