#pragma once
#include <iiGUI/NativeTabView.h>
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
namespace ii::gui {
using TabRegistrationPort=std::function<core::Result<void>(TabGroupPort)>;
using TabSectionPort=std::function<core::Result<void>(std::vector<TabGroupPort>)>;
struct TabSectionAccess final {std::function<core::Result<TabSectionPort>(Widget)> register_section;std::function<core::Result<TabBarConfiguration>()> options;};
struct DefaultTabBarHostPolicy final {using Model=TabBarModel;using Configuration=TabBarConfiguration;using GroupPort=TabGroupPort;
 using Presentation=TabPresentation;using FocusMove=TabFocusMove;using Activation=TabActivation;using RegistrationPort=TabRegistrationPort;
 using SectionPort=TabSectionPort;using SectionAccess=TabSectionAccess;};
template<class Policy>class BasicNativeTabBar final {
    using TabBar=typename Policy::Model;using TabBarConfiguration=typename Policy::Configuration;
    using TabGroupPort=typename Policy::GroupPort;using TabPresentation=typename Policy::Presentation;
    using TabFocusMove=typename Policy::FocusMove;using TabActivation=typename Policy::Activation;
    using TabRegistrationPort=typename Policy::RegistrationPort;using TabSectionPort=typename Policy::SectionPort;
    using TabSectionAccess=typename Policy::SectionAccess;
    struct State final {
        struct Entry final {std::uint64_t id,section;std::string key;TabGroupPort port;TabPresentation applied;std::vector<std::string> group_ids;};
        struct Section final {std::uint64_t id;ii::gui::Widget host;std::string widget_id;};
        TabBar model;
        ii::gui::Widget root;
        std::vector<Entry> entries;
        std::vector<Section> sections;
        std::optional<double> available_width;
        std::function<void(int)> changed;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0,next_id=0,next_section=0;
        bool finalized=false,retired=false;
        unsigned activity=0;
    };
    static void release(State& state)noexcept {state.entries.clear();state.sections.clear();state.changed={};state.failure.reset();}
    struct Activity final {std::shared_ptr<State> state;explicit Activity(std::shared_ptr<State> value):state(std::move(value)){++state->activity;}
        ~Activity(){if(--state->activity==0&&state->retired)release(*state);}};
    std::shared_ptr<State> state_;
    explicit BasicNativeTabBar(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::gui::StackOptions layout(const TabBarConfiguration& value){
        return {.spacing=value.spacing,.alignment=ii::gui::StackAlignment::center};
    }
    [[nodiscard]] static ii::gui::AccessibilitySemantics semantics(const TabBarConfiguration& value){
        return {.role=ii::gui::AccessibilityRole::tab_group,.name=value.accessibility_name};
    }
    [[nodiscard]] static ii::core::Result<void> check(const std::shared_ptr<State>& state){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"TabBar requires its owner thread");
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"TabBar has retired");
        if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());return {};
    }
    [[nodiscard]] static ii::core::Result<void> present(const TabGroupPort& port,TabPresentation value){
        try{return port.present(value);}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"TabBar child presentation failed",std::current_exception());}
    }
    // Retain selection by registration identity when earlier children retire.
    [[nodiscard]] static ii::core::Result<void> collect(const std::shared_ptr<State>& state){
        Activity activity(state);
        try{
        auto current=state->model.configuration().current_index;std::optional<std::uint64_t> selected;
        if(current>=0&&std::size_t(current)<state->entries.size())selected=state->entries[std::size_t(current)].id;
        std::erase_if(state->entries,[](const auto& entry){return entry.port.retired&&entry.port.retired();});
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"TabBar has retired");
        if(selected){auto item=std::ranges::find_if(state->entries,[&](const auto& entry){return entry.id==*selected;});
            auto config=state->model.configuration();config.current_index=item==state->entries.end()?-1:int(item-state->entries.begin());
            (void)state->model.set_configuration(config);}
        std::vector<bool> enabled;enabled.reserve(state->entries.size());
        for(const auto& entry:state->entries){auto value=entry.port.availability();if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"TabBar has retired");if(!value)return std::unexpected(value.error());enabled.push_back(value->enabled&&value->visible);}
        if(!state->model.set_items(std::move(enabled)))return ii::core::fail(ii::core::ErrorCode::out_of_range,"Too many tabs");return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"TabBar child inspection failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> sync(const std::shared_ptr<State>& state){
        Activity activity(state);
        try{
        if(auto valid=check(state);!valid)return valid;
        const auto previous_index=state->model.configuration().current_index;
        if(auto collected=collect(state);!collected)return collected;
        auto config=state->model.configuration();
        if(state->finalized&&config.auto_select&&previous_index>=0&&
           (config.current_index<0||!state->model.enabled(std::size_t(config.current_index)))){
            auto first=state->model.focus_target(std::nullopt,TabFocusMove::first);config.current_index=first?int(*first):-1;
            (void)state->model.set_configuration(config);
        }
        const auto observed=state->revision;
        std::unordered_set<std::string> groups;
        for(const auto& entry:state->entries)for(std::size_t i=0;i<entry.group_ids.size();++i){
            if(groups.insert(entry.group_ids[i]).second){
                const auto& group=entry.port.layout_groups[i];auto sizing=group.stack_layout();if(!sizing)return std::unexpected(sizing.error());
                if(sizing->extent.width!=0||sizing->extent.height!=0||sizing->flex!=0)
                    return ii::core::fail(ii::core::ErrorCode::invalid_argument,"TabBar layout group cannot have fixed extent or flex");
                if(auto result=group.set_stack_options(layout(config));!result)return result;
                if(state->revision!=observed)return {};
            }
        }
        std::optional<double> width;
        if(state->available_width&&*state->available_width>0&&state->model.count()){
            std::size_t visible=0;double minimum=config.minimum_tab_width;
            for(const auto& entry:state->entries){auto value=entry.port.availability();if(!value)return std::unexpected(value.error());
                if(value->visible){++visible;auto required=entry.port.minimum_width();if(!required)return std::unexpected(required.error());minimum=(std::max)(minimum,*required);}}
            if(visible){const auto gaps=double(visible-1)*config.spacing;
                width=(std::min)(8192.,(std::max)(minimum,(std::max)(0.,*state->available_width-gaps)/double(visible)));}
        }
        const auto entries=state->entries;std::vector<std::size_t> changed;changed.reserve(entries.size());
        const auto mark=[&](std::uint64_t id,TabPresentation value){
            auto found=std::ranges::find_if(state->entries,[&](const auto& entry){return entry.id==id;});
            if(found!=state->entries.end())found->applied=value;
        };
        for(std::size_t index=0;index<entries.size();++index){
            const auto& entry=entries[index];TabPresentation desired{.selected=config.auto_select?std::optional<bool>{config.current_index==int(index)}:std::nullopt,.width=width};
            if(entry.applied==desired)continue;
            auto result=present(entry.port,desired);if(state->revision!=observed)return {};
            if(!result){
                for(auto previous=changed.rbegin();previous!=changed.rend()&&state->revision==observed;++previous)
                    if(auto restored=present(entries[*previous].port,entries[*previous].applied);restored&&state->revision==observed)mark(entries[*previous].id,entries[*previous].applied);
                return result;
            }
            changed.push_back(index);mark(entry.id,desired);
        }
        if(state->finalized&&config.auto_select&&config.current_index!=previous_index&&state->revision==observed){
            auto callback=state->changed;if(callback)callback(config.current_index);
        }
        return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"TabBar child synchronization failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> apply(const std::shared_ptr<State>& state,TabBarConfiguration next){
        Activity activity(state);
        if(auto valid=check(state);!valid)return valid;
        if(!TabBar::validate(next))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI TabBar configuration");
        try{
            if(auto collected=collect(state);!collected)return collected;
            if(state->finalized&&state->entries.empty()&&next.current_index==0)next.current_index=-1;
            if(state->finalized&&next.current_index>=0){
                if(std::size_t(next.current_index)>=state->entries.size())return ii::core::fail(ii::core::ErrorCode::out_of_range,"TabBar selection is outside its children");
                auto selected=state->entries[std::size_t(next.current_index)].port.availability();
                if(!selected)return std::unexpected(selected.error());
                if(!selected->enabled||!selected->visible)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"TabBar selection must be enabled and visible");
            }
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"TabBar revision exhausted");
            const auto previous=state->model.configuration();const auto revision=++state->revision;(void)state->model.set_configuration(next);
            const auto restore=[&]{if(state->revision!=revision)return;(void)state->model.set_configuration(previous);
                (void)state->root.set_stack_options(layout(previous));if(state->revision!=revision)return;
                (void)state->root.set_accessibility_semantics(semantics(previous));if(state->revision==revision)(void)sync(state);};
            if(auto changed=state->root.set_stack_options(layout(next));!changed){restore();return changed;}
            if(state->revision!=revision)return {};
            if(auto changed=state->root.set_accessibility_semantics(semantics(next));!changed){restore();return changed;}
            if(state->revision!=revision)return {};
            if(auto result=sync(state);!result){restore();return result;}
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"TabBar update failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> activate(const std::shared_ptr<State>& state,std::uint64_t id){
        Activity activity(state);
        if(auto valid=check(state);!valid)return valid;
        try{
            if(auto collected=collect(state);!collected)return collected;
            const auto found=std::ranges::find_if(state->entries,[&](const auto& entry){return entry.id==id;});
            if(found==state->entries.end())return ii::core::fail(ii::core::ErrorCode::closed,"Tab has retired");
            const auto index=std::size_t(found-state->entries.begin());auto next=state->model;
            const auto activation=next.activate(index);
            if(activation==TabActivation::rejected)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Tab is disabled or hidden");
            if(activation==TabActivation::selection_changed){
                if(auto updated=apply(state,next.configuration());!updated)return updated;
                if(state->model.configuration().auto_select&&state->model.configuration().current_index==int(index)){
                    auto callback=state->changed;if(callback)callback(int(index));
                }
            }
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"TabBar activation failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<bool> input(const std::shared_ptr<State>& state,const ii::gui::WidgetInput& event){
        Activity activity(state);
        try{
        using ii::gui::WidgetInputKind;using ii::gui::WidgetKey;
        if(event.kind!=WidgetInputKind::key_down)return false;
        TabFocusMove move;
        switch(event.key){case WidgetKey::left:move=TabFocusMove::previous;break;case WidgetKey::right:move=TabFocusMove::next;break;
            case WidgetKey::home:move=TabFocusMove::first;break;case WidgetKey::end:move=TabFocusMove::last;break;default:return false;}
        if(auto valid=check(state);!valid)return std::unexpected(valid.error());
        if(auto collected=collect(state);!collected)return std::unexpected(collected.error());
        std::optional<std::size_t> focused;
        for(std::size_t i=0;i<state->entries.size();++i){auto value=state->entries[i].port.snapshot();if(!value)return std::unexpected(value.error());if(value->focused){focused=i;break;}}
        if(!focused)return false;
        auto target=state->model.focus_target(focused,move);if(!target)return true;
        auto result=state->entries[*target].port.focus();if(!result)return std::unexpected(result.error());return true;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"TabBar keyboard handling failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> replace_section(const std::shared_ptr<State>& state,std::uint64_t section_id,
            std::vector<TabGroupPort> ports){
        Activity activity(state);
        if(auto valid=check(state);!valid)return valid;
        struct Attachment final {TabGroupPort port;std::uint64_t id;bool action=false,observer=false;};
        std::vector<Attachment> attached;
        const auto detach=[&]()noexcept{for(auto it=attached.rbegin();it!=attached.rend();++it){
            if(it->observer)try{(void)it->port.changed({});}catch(...){}
            if(it->action)try{(void)it->port.activated({});}catch(...){}
        }};
        try{
            const auto slot=std::ranges::find_if(state->sections,[&](const auto& value){return value.id==section_id;});
            if(slot==state->sections.end())return ii::core::fail(ii::core::ErrorCode::closed,"TabBar section has retired");
            if(ports.size()>4096)return ii::core::fail(ii::core::ErrorCode::out_of_range,"TabBar child limit exceeded");
            const auto host=slot->host;auto host_snapshot=host.snapshot();if(!host_snapshot)return std::unexpected(host_snapshot.error());
            const auto revision=state->revision,old_next=state->next_id;
            if(revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"TabBar revision exhausted");
            const auto unchanged=[&]{return state->revision==revision&&state->next_id==old_next;};
            auto old_entries=state->entries;auto old_model=state->model;
            std::unordered_map<std::string,const typename State::Entry*> existing;
            for(const auto& entry:old_entries)existing.emplace(entry.key,&entry);
            std::unordered_set<std::string> seen;std::vector<typename State::Entry> replacement;
            replacement.reserve(ports.size());attached.reserve(ports.size());auto next_id=old_next;
            for(auto& port:ports){
                if(!port.present||!port.activated||!port.changed||!port.availability||!port.minimum_width||!port.snapshot||!port.focus||!port.retired||!port.widget)
                    return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Incomplete dynamic TabBar child port");
                auto widget=port.widget();if(!widget)return std::unexpected(widget.error());
                auto inside=widget->is_within(host);if(!inside)return std::unexpected(inside.error());
                auto item=port.snapshot();if(!item)return std::unexpected(item.error());
                auto actual=widget->snapshot();if(!actual)return std::unexpected(actual.error());
                if(!*inside||item->id!=actual->id||item->id==host_snapshot->id)
                    return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Dynamic tab is outside its section");
                if(port.layout_groups.size()>64)return ii::core::fail(ii::core::ErrorCode::out_of_range,"TabBar group nesting limit exceeded");
                std::unordered_set<std::string> seen_groups;std::vector<std::string> group_ids;group_ids.reserve(port.layout_groups.size());
                for(const auto& group:port.layout_groups){
                    auto geometry=group.snapshot();if(!geometry)return std::unexpected(geometry.error());
                    auto group_inside=group.is_within(host);if(!group_inside)return std::unexpected(group_inside.error());
                    auto item_inside=widget->is_within(group);if(!item_inside)return std::unexpected(item_inside.error());
                    if(geometry->kind!=ii::gui::WidgetKind::horizontal_stack||!*group_inside||!*item_inside||!seen_groups.insert(geometry->id).second)
                        return ii::core::fail(ii::core::ErrorCode::invalid_argument,"TabBar layout group must be a distinct containing horizontal stack in its section");
                    auto sizing=group.stack_layout();if(!sizing)return std::unexpected(sizing.error());
                    if(sizing->extent.width!=0||sizing->extent.height!=0||sizing->flex!=0)
                        return ii::core::fail(ii::core::ErrorCode::invalid_argument,"TabBar layout group cannot have fixed extent or flex");
                    group_ids.push_back(std::move(geometry->id));
                }
                if(!seen.insert(item->id).second)return ii::core::fail(ii::core::ErrorCode::already_exists,"Duplicate dynamic tab");
                if(auto found=existing.find(item->id);found!=existing.end()){
                    if(found->second->section!=section_id)return ii::core::fail(ii::core::ErrorCode::already_exists,"Tab belongs to another section");
                    if(found->second->group_ids!=group_ids)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Retained tab layout groups changed");
                    replacement.push_back(*found->second);
                }else{
                    if(next_id==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Tab identity exhausted");
                    const auto id=++next_id;replacement.push_back({id,section_id,item->id,port,{},std::move(group_ids)});attached.push_back({port,id});
                }
                if(!unchanged())return ii::core::fail(ii::core::ErrorCode::cancelled,"TabBar changed while inspecting a section");
            }
            std::vector<typename State::Entry> next;next.reserve(old_entries.size()+replacement.size());bool inserted=false;
            for(const auto& entry:old_entries){
                if(entry.section==section_id)continue;
                if(!inserted&&entry.section>section_id){next.insert(next.end(),replacement.begin(),replacement.end());inserted=true;}
                next.push_back(entry);
            }
            if(!inserted)next.insert(next.end(),replacement.begin(),replacement.end());
            if(next.size()>4096)return ii::core::fail(ii::core::ErrorCode::out_of_range,"TabBar child limit exceeded");
            auto planned=old_model;auto config=planned.configuration();const auto previous_index=config.current_index;
            std::optional<std::uint64_t> selected;
            if(previous_index>=0&&std::size_t(previous_index)<old_entries.size())selected=old_entries[std::size_t(previous_index)].id;
            if(selected){const auto found=std::ranges::find_if(next,[&](const auto& entry){return entry.id==*selected;});
                config.current_index=found==next.end()?-1:int(found-next.begin());}
            std::vector<bool> enabled;enabled.reserve(next.size());
            for(const auto& entry:next){auto value=entry.port.availability();if(!value)return std::unexpected(value.error());enabled.push_back(value->enabled&&value->visible);
                if(!unchanged())return ii::core::fail(ii::core::ErrorCode::cancelled,"TabBar changed while preparing a section");}
            if(!planned.set_items(std::move(enabled)))return ii::core::fail(ii::core::ErrorCode::out_of_range,"TabBar child limit exceeded");
            if(state->finalized&&config.auto_select&&(config.current_index<0||!planned.enabled(std::size_t(config.current_index)))){
                auto first=planned.focus_target(std::nullopt,TabFocusMove::first);config.current_index=first?int(*first):-1;}
            (void)planned.set_configuration(config);
            const bool same=next.size()==old_entries.size()&&std::equal(next.begin(),next.end(),old_entries.begin(),[](const auto& a,const auto& b){return a.id==b.id;});
            if(same&&config.current_index==previous_index)return {};
            std::weak_ptr<State> weak=state;
            for(auto& item:attached){const auto id=item.id;
                if(auto r=item.port.activated([weak,id]{auto value=weak.lock();if(!value)return false;auto result=activate(value,id);
                        if(result){value->failure.reset();return true;}value->failure=result.error();return false;});!r){detach();return r;}item.action=true;
                if(auto r=item.port.changed([weak,id]{if(auto value=weak.lock()){
                        if(std::ranges::none_of(value->entries,[&](const auto& entry){return entry.id==id;}))return;
                        if(value->revision==(std::numeric_limits<std::uint64_t>::max)()){value->failure=ii::core::Error{ii::core::ErrorCode::out_of_range,"TabBar revision exhausted"};return;}
                        ++value->revision;auto result=sync(value);if(result)value->failure.reset();else value->failure=result.error();}});!r){detach();return r;}item.observer=true;
                if(!unchanged()){detach();return ii::core::fail(ii::core::ErrorCode::cancelled,"TabBar changed while attaching a section");}
            }
            if(auto valid=check(state);!valid){detach();return valid;}
            if(!unchanged()){detach();return ii::core::fail(ii::core::ErrorCode::cancelled,"TabBar changed before section publication");}
            state->entries=std::move(next);state->model=std::move(planned);state->next_id=next_id;
            const auto applied=++state->revision;
            if(auto result=sync(state);!result){
                if(state->revision==applied){state->entries=std::move(old_entries);state->model=std::move(old_model);state->next_id=old_next;
                    detach();if(auto restored=sync(state);!restored)state->failure=restored.error();}return result;
            }
            if(state->revision==applied&&state->finalized&&config.auto_select&&config.current_index!=previous_index){
                try{auto callback=state->changed;if(callback)callback(config.current_index);}
                catch(...){auto cause=std::current_exception();
                    if(state->revision==applied){state->entries=std::move(old_entries);state->model=std::move(old_model);state->next_id=old_next;
                        detach();attached.clear();if(auto restored=sync(state);!restored)state->failure=restored.error();
                        return ii::core::fail(ii::core::ErrorCode::failed,"TabBar selection callback failed",cause);}
                    state->failure=ii::core::Error{ii::core::ErrorCode::failed,"TabBar selection callback failed",cause};
                }}
            attached.clear();
            return {};
        }catch(...){auto cause=std::current_exception();detach();return ii::core::fail(ii::core::ErrorCode::failed,"TabBar section replacement failed",cause);}
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeTabBar> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,TabBarConfiguration config={},std::function<void(int)> changed={}){
        if(!TabBar::validate(config))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI TabBar configuration");
        try{
            auto state=std::make_shared<State>();(void)state->model.set_configuration(config);state->changed=std::move(changed);
            auto root=builder.hstack(parent,id,layout(config));if(!root)return std::unexpected(root.error());state->root=*root;
            if(auto r=root->set_extent({0,40});!r)return std::unexpected(r.error());
            if(auto r=root->set_accessibility_semantics(semantics(config));!r)return std::unexpected(r.error());
            if(auto r=root->set_input_handler([state](const auto& event){return input(state,event);});!r)return std::unexpected(r.error());
            if(auto r=root->set_layout_handler([state](ii::gui::Rect bounds){
                Activity activity(state);if(state->retired)return;
                if(state->available_width&&*state->available_width==bounds.width)return;
                if(state->revision==(std::numeric_limits<std::uint64_t>::max)()){state->failure=ii::core::Error{ii::core::ErrorCode::out_of_range,"TabBar revision exhausted"};return;}
                ++state->revision;
                state->available_width=bounds.width;auto result=sync(state);if(result)state->failure.reset();else state->failure=result.error();
            });!r)return std::unexpected(r.error());
            std::weak_ptr<State> weak=state;
            if(auto r=builder.on_retire([weak]{if(auto state=weak.lock()){state->retired=true;
                if(state->revision!=(std::numeric_limits<std::uint64_t>::max)())++state->revision;if(!state->activity)release(*state);}});!r)return std::unexpected(r.error());
            return BasicNativeTabBar(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"TabBar creation failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<TabBarConfiguration> options()const{
        auto state=state_;Activity activity(state);
        if(auto valid=check(state);!valid)return std::unexpected(valid.error());
        if(auto collected=collect(state);!collected)return std::unexpected(collected.error());return state->model.configuration();
    }
    [[nodiscard]] ii::core::Result<void> set_options(TabBarConfiguration value)const{auto state=state_;return apply(state,std::move(value));}
    [[nodiscard]] ii::core::Result<void> status()const{
        auto state=state_;Activity activity(state);
        if(auto valid=check(state);!valid)return valid;if(state->failure)return std::unexpected(*state->failure);return {};
    }
    [[nodiscard]] ii::core::Result<void> register_tab(TabGroupPort port)const{
        auto state=state_;Activity activity(state);
        if(auto valid=check(state);!valid)return valid;
        if(!port.present||!port.activated||!port.changed||!port.availability||!port.minimum_width||!port.snapshot||!port.focus||!port.retired)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Incomplete TabBar child port");
        bool action_attached=false,observer_attached=false;
        try{
            if(auto collected=collect(state);!collected)return collected;
            if(state->entries.size()>=4096||state->next_id==(std::numeric_limits<std::uint64_t>::max)()||state->next_section==(std::numeric_limits<std::uint64_t>::max)()||state->revision==(std::numeric_limits<std::uint64_t>::max)())
                return ii::core::fail(ii::core::ErrorCode::out_of_range,"TabBar child or revision limit exceeded");
            auto snapshot=port.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
            auto parent=state->root.snapshot();if(!parent)return std::unexpected(parent.error());
            if(snapshot->parent_id!=parent->id)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"TabBar currently accepts direct Tab children only");
            if(std::ranges::any_of(state->entries,[&](const auto& entry){return entry.key==snapshot->id;}))return ii::core::fail(ii::core::ErrorCode::already_exists,"Tab is already registered");
            const auto id=++state->next_id;std::weak_ptr<State> weak=state;
            if(!port.layout_groups.empty())return ii::core::fail(ii::core::ErrorCode::invalid_argument,"TabBar layout groups require a dynamic section");
            state->entries.reserve(state->entries.size()+1);typename State::Entry entry{id,++state->next_section,snapshot->id,port,{},{}};
            if(auto attached=port.activated([weak,id]{auto state=weak.lock();if(!state)return false;auto result=activate(state,id);
                    if(result){state->failure.reset();return true;}state->failure=result.error();return false;});!attached)return attached;
            action_attached=true;
            if(auto attached=port.changed([weak]{if(auto state=weak.lock()){
                    if(state->revision==(std::numeric_limits<std::uint64_t>::max)()){state->failure=ii::core::Error{ii::core::ErrorCode::out_of_range,"TabBar revision exhausted"};return;}
                    ++state->revision;auto result=sync(state);if(result)state->failure.reset();else state->failure=result.error();}});!attached){(void)port.activated({});return attached;}
            observer_attached=true;
            ++state->revision;
            state->entries.push_back(std::move(entry));
            if(auto result=sync(state);!result){std::erase_if(state->entries,[&](const auto& entry){return entry.id==id;});(void)port.activated({});(void)port.changed({});(void)sync(state);return result;}
            return {};
        }catch(...){auto cause=std::current_exception();try{if(observer_attached)(void)port.changed({});if(action_attached)(void)port.activated({});}catch(...){}
            return ii::core::fail(ii::core::ErrorCode::failed,"TabBar registration failed",cause);}
    }
    [[nodiscard]] ii::core::Result<TabSectionPort> register_section(ii::gui::Widget host)const{
        auto state=state_;Activity activity(state);
        if(auto valid=check(state);!valid)return std::unexpected(valid.error());
        try{
            auto item=host.snapshot(),parent=state->root.snapshot();if(!item)return std::unexpected(item.error());if(!parent)return std::unexpected(parent.error());
            if(item->kind!=ii::gui::WidgetKind::content_host||item->parent_id!=parent->id)
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"TabBar section must be a direct content host");
            if(std::ranges::any_of(state->sections,[&](const auto& value){return value.widget_id==item->id;}))
                return ii::core::fail(ii::core::ErrorCode::already_exists,"TabBar section is already registered");
            if(state->next_section==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"TabBar section identity exhausted");
            const auto id=state->next_section+1;std::weak_ptr<State> weak=state;
            TabSectionPort port=[weak,id](std::vector<TabGroupPort> items)->ii::core::Result<void>{auto state=weak.lock();
                if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"TabBar has retired");return replace_section(state,id,std::move(items));};
            typename State::Section section{id,host,item->id};state->sections.reserve(state->sections.size()+1);
            if(auto collapsed=host.set_collapse_when_empty(true);!collapsed)return std::unexpected(collapsed.error());
            if(auto valid=check(state);!valid)return std::unexpected(valid.error());
            if(state->next_section+1!=id)return ii::core::fail(ii::core::ErrorCode::cancelled,"TabBar changed during section registration");
            state->next_section=id;state->sections.push_back(std::move(section));return port;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"TabBar section registration failed",std::current_exception());}
    }
    // Initial construction may register zero or more children before selection
    // is validated. Later registrations and retirements use their stable IDs.
    [[nodiscard]] ii::core::Result<void> finalize()const{
        auto state=state_;Activity activity(state);
        if(auto valid=check(state);!valid)return valid;
        if(auto collected=collect(state);!collected)return collected;
        auto config=state->model.configuration();
        if(config.current_index>=0){
            const auto index=std::size_t(config.current_index);
            if(index>=state->entries.size()){
                if(!state->entries.empty())return ii::core::fail(ii::core::ErrorCode::out_of_range,"TabBar selection is outside its children");
                config.current_index=-1;
            }
            else{auto candidate=state->entries[index].port.availability();if(!candidate)return std::unexpected(candidate.error());
                if(!candidate->enabled||!candidate->visible){auto first=state->model.focus_target(std::nullopt,TabFocusMove::first);config.current_index=first?int(*first):-1;}}
        }
        const bool previous=state->finalized;state->finalized=true;auto result=apply(state,config);if(!result)state->finalized=previous;return result;
    }
    [[nodiscard]] TabRegistrationPort weak_registration_port()const{
        std::weak_ptr<State> weak=state_;
        return [weak](TabGroupPort child)->ii::core::Result<void>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"TabBar has retired");
            return BasicNativeTabBar(std::move(state)).register_tab(std::move(child));};
    }
    [[nodiscard]] TabSectionAccess weak_section_access()const{
        std::weak_ptr<State> weak=state_;
        return {.register_section=[weak](ii::gui::Widget host)->ii::core::Result<TabSectionPort>{auto state=weak.lock();
                    if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"TabBar has retired");return BasicNativeTabBar(std::move(state)).register_section(std::move(host));},
                .options=[weak]()->ii::core::Result<TabBarConfiguration>{auto state=weak.lock();
                    if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"TabBar has retired");return BasicNativeTabBar(std::move(state)).options();}};
    }
};
using NativeTabBar=BasicNativeTabBar<DefaultTabBarHostPolicy>;
} // namespace ii::gui
