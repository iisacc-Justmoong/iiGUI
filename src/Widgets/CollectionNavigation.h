#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
namespace ii::gui {
template<class Policy>
class BasicCollectionNavigation final {
    using CollectionSelectionMode=typename Policy::CollectionSelectionMode;
    using CollectionCommand=typename Policy::CollectionCommand;
    using CollectionNavigationOptions=typename Policy::CollectionNavigationOptions;
    using CollectionNavigationLayout=typename Policy::CollectionNavigationLayout;
    using CollectionModifiers=typename Policy::CollectionModifiers;
    using CollectionSelectionState=typename Policy::CollectionSelectionState;
    using CollectionNavigationReply=typename Policy::CollectionNavigationReply;
    using AnyCollectionItem=typename Policy::AnyCollectionItem;

public:
    class StateSource final {
    public:
        StateSource()=default;
        StateSource(const StateSource&)=delete;
        StateSource& operator=(const StateSource&)=delete;
        const CollectionSelectionState& get()const{check();return value_->get();}
        std::uint64_t revision()const{check();return value_->revision();}
        ii::core::ObservableSubscription observe(std::function<void(const CollectionSelectionState&,std::uint64_t)> callback,bool initial=false){check();return value_->observe(std::move(callback),initial);}
    private:
        friend class BasicCollectionNavigation;
        void check()const{if(!value_)throw std::logic_error("Collection navigation source retired");}
        std::optional<ii::core::Observable<CollectionSelectionState>> value_{std::in_place,CollectionSelectionState{}};
    } state;
    BasicCollectionNavigation()=default;
    BasicCollectionNavigation(const BasicCollectionNavigation&)=delete;
    BasicCollectionNavigation& operator=(const BasicCollectionNavigation&)=delete;
    void configure(CollectionNavigationOptions options){
        ready();if(static_cast<unsigned>(options.mode)>2)throw std::invalid_argument("Invalid collection selection mode");
        auto next=state.get();
        if(options.mode==CollectionSelectionMode::none){next.selected_keys.clear();next.anchor_key.clear();}
        if(options.mode==CollectionSelectionMode::single&&next.selected_keys.size()>1)
            next.selected_keys={std::binary_search(next.selected_keys.begin(),next.selected_keys.end(),next.current_key)?next.current_key:next.selected_keys.front()};
        options_=options;(void)publish(std::move(next));
    }
    bool is_selected(std::string_view key)const{ready();const auto& keys=state.get().selected_keys;return std::binary_search(keys.begin(),keys.end(),key);}
    bool set_current(std::string key){ready();if(!key.empty())key=Policy::collection_key(key);auto next=state.get();next.current_key=std::move(key);return publish(std::move(next));}
    // Replace a product-owned snapshot atomically; keys may be outside the
    // current projection because filtering must preserve hidden selection.
    bool set_state(CollectionSelectionState next){
        ready();
        if(!next.current_key.empty())next.current_key=Policy::collection_key(next.current_key);
        if(!next.anchor_key.empty())next.anchor_key=Policy::collection_key(next.anchor_key);
        if(next.selected_keys.size()>Policy::maximum_items)throw std::length_error("Collection selection exceeds 65536 keys");
        std::size_t bytes=0;
        for(auto& key:next.selected_keys){
            key=Policy::collection_key(key);
            if(key.size()>Policy::maximum_key_bytes-bytes)throw std::length_error("Collection selection exceeds 16 MiB");
            bytes+=key.size();
        }
        canonicalize(next);
        if((options_.mode==CollectionSelectionMode::none&&!next.selected_keys.empty())||
           (options_.mode==CollectionSelectionMode::single&&next.selected_keys.size()>1))
            throw std::invalid_argument("Collection selection does not match its configured mode");
        return state.value_->set(std::move(next));
    }
    template<class Source> std::optional<std::size_t> current_index(const Source& source)const{ready();return source.find(state.get().current_key);}
    template<class Source,class Eligible=AnyCollectionItem> CollectionNavigationReply focus_at(const Source& source,std::size_t index,CollectionModifiers modifiers={},bool select=true,Eligible eligible={}){
        ready();if(index>=source.items().size())throw std::out_of_range("Collection focus index is out of range");
        if(!std::invoke(eligible,source.items()[index].value,index))return {};
        const auto& before=state.get();const auto& key=source.items()[index].key;
        const bool range=modifiers.range&&options_.mode==CollectionSelectionMode::multiple;
        const bool replace=options_.mode!=CollectionSelectionMode::none&&select&&!range&&!(modifiers.additive&&options_.mode==CollectionSelectionMode::multiple);
        auto next=replace?CollectionSelectionState{key,key,{key}}:range&&!modifiers.additive?CollectionSelectionState{key,before.anchor_key,{}}:before;
        if(replace)return commit(before,std::move(next),index);
        next.current_key=key;
        if(options_.mode!=CollectionSelectionMode::none){
            if(modifiers.range&&options_.mode==CollectionSelectionMode::multiple){
                const auto anchor=source.find(before.anchor_key).value_or(modifiers.anchor_index.value_or(source.find(before.current_key).value_or(index)));
                if(anchor>=source.items().size())throw std::out_of_range("Collection range anchor is out of range");
                if(!modifiers.additive)next.selected_keys.clear();
                for(auto n=(std::min)(anchor,index);n<=(std::max)(anchor,index);++n)if(std::invoke(eligible,source.items()[n].value,n))next.selected_keys.push_back(source.items()[n].key);
                if(next.anchor_key.empty()||!source.find(next.anchor_key))next.anchor_key=source.items()[anchor].key;
            }else if(select){
                if(modifiers.additive&&options_.mode==CollectionSelectionMode::multiple){
                    const auto found=std::ranges::find(next.selected_keys,key);
                    if(found==next.selected_keys.end())next.selected_keys.push_back(key);else next.selected_keys.erase(found);
                }else next.selected_keys={key};
                next.anchor_key=key;
            }else if(options_.anchor_on_unselected_focus){
                next.anchor_key=key;
            }
        }
        return commit(before,std::move(next),index);
    }
    template<class Source,class Eligible=AnyCollectionItem>
    CollectionNavigationReply command(const Source& source,CollectionCommand command,
                                      CollectionNavigationLayout layout={},CollectionModifiers modifiers={},Eligible eligible={}){
        ready();if(static_cast<unsigned>(command)>static_cast<unsigned>(CollectionCommand::activate)||!layout.columns||layout.columns>65536||!layout.page_rows||layout.page_rows>65536)
            throw std::invalid_argument("Invalid collection navigation command or geometry");
        const auto count=source.items().size();
        const auto accepted=[&](std::size_t n){return bool(std::invoke(eligible,source.items()[n].value,n));};
        const auto& before=state.get();auto current=source.find(before.current_key);
        if(layout.current_index){if(*layout.current_index>=count)throw std::out_of_range("Collection focus hint is out of range");current=layout.current_index;}
        if(modifiers.range&&before.anchor_key.empty()&&layout.current_index)modifiers.anchor_index=layout.current_index;
        if(command==CollectionCommand::clear)return commit(before,{.current_key=before.current_key},current);
        if(command==CollectionCommand::select_all){
            if(options_.mode!=CollectionSelectionMode::multiple||!count)return {};
            auto next=before;for(std::size_t n=0;n<count;++n)if(accepted(n))next.selected_keys.push_back(source.items()[n].key);
            return commit(before,std::move(next),current);
        }
        if(command==CollectionCommand::activate){if(!current||!accepted(*current))return {};return {.handled=true,.activated=true,.index=current};}
        if(command==CollectionCommand::toggle){
            if(!current||!accepted(*current)||options_.mode==CollectionSelectionMode::none)return {};
            auto next=before;const auto& key=source.items()[*current].key;
            if(options_.mode==CollectionSelectionMode::single)next.selected_keys=is_selected(key)?std::vector<std::string>{}:std::vector<std::string>{key};
            else{const auto found=std::ranges::find(next.selected_keys,key);if(found==next.selected_keys.end())next.selected_keys.push_back(key);else next.selected_keys.erase(found);}
            next.anchor_key=key;return commit(before,std::move(next),current);
        }
        if(!count)return {};
        const bool reverse=command==CollectionCommand::previous||command==CollectionCommand::left||command==CollectionCommand::up||command==CollectionCommand::page_previous||command==CollectionCommand::end;
        const bool boundary=command==CollectionCommand::home||command==CollectionCommand::end;
        const auto step=command==CollectionCommand::up||command==CollectionCommand::down?layout.horizontal?1:layout.columns:
            command==CollectionCommand::page_previous||command==CollectionCommand::page_next?
                std::size_t(std::min<std::uint64_t>(std::uint64_t(layout.columns)*layout.page_rows,count)):1;
        std::size_t target=boundary||!current?(reverse?count-1:0):reverse?(*current>step?*current-step:0):*current+(std::min)(step,count-1-*current);
        for(std::size_t attempts=0;attempts<count;++attempts){
            if(accepted(target))return focus_at(source,target,modifiers,options_.select_on_move&&!modifiers.additive,eligible);
            if(reverse){if(!target)break;--target;}else{if(target==count-1)break;++target;}
        }
        return {};
    }
    void close(){if(owner_!=std::this_thread::get_id())throw std::logic_error("Collection navigation requires its owner thread");if(active_){active_=false;state.value_.reset();}}
private:
    void ready()const{if(owner_!=std::this_thread::get_id())throw std::logic_error("Collection navigation requires its owner thread");if(!active_)throw std::logic_error("Collection navigation retired");}
    static void canonicalize(CollectionSelectionState& next){
        std::sort(next.selected_keys.begin(),next.selected_keys.end());next.selected_keys.erase(std::unique(next.selected_keys.begin(),next.selected_keys.end()),next.selected_keys.end());
        if(next.selected_keys.size()>Policy::maximum_items)throw std::length_error("Collection selection exceeds 65536 keys");
        std::size_t bytes=0;for(const auto& key:next.selected_keys){if(key.size()>Policy::maximum_key_bytes-bytes)throw std::length_error("Collection selection exceeds 16 MiB");bytes+=key.size();}
    }
    bool publish(CollectionSelectionState next){canonicalize(next);return state.value_->set(std::move(next));}
    CollectionNavigationReply commit(const CollectionSelectionState& before,CollectionSelectionState next,std::optional<std::size_t> index){
        canonicalize(next);
        CollectionNavigationReply reply{true,before.current_key!=next.current_key,before.selected_keys!=next.selected_keys,false,index};
        (void)state.value_->set(std::move(next));return reply;
    }
    const std::thread::id owner_=std::this_thread::get_id();
    bool active_=true;
    CollectionNavigationOptions options_;
};
} // namespace ii::gui
