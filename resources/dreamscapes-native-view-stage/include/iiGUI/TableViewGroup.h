#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
namespace ii::gui {
// A keyed viewport retains only visible cells and delegates each context's
// data and lifetime to an application-supplied policy.
template<class Policy>
class BasicNativeTableViewGroup final {
    using TableViewFactory=typename Policy::Factory;using TableViewItem=typename Policy::Item;
    using TableViewContext=typename Policy::Context;using TableViewSnapshot=typename Policy::Snapshot;
    struct State final {
        ii::gui::Widget host;
        TableViewFactory fallback;
        std::map<std::string,TableViewFactory,std::less<>> factories;
        std::unordered_map<std::string,std::shared_ptr<TableViewContext>> contexts;
        std::unordered_map<std::string,std::string> native_keys;
        std::uint64_t generation=0,serial=0;
        const std::thread::id owner=std::this_thread::get_id();
        std::atomic_bool retired=false;
        State(ii::gui::Widget value,TableViewFactory default_factory,
              std::map<std::string,TableViewFactory,std::less<>> named)
            :host(value),fallback(std::move(default_factory)),factories(std::move(named)){}
    };
    std::shared_ptr<State> state_;
public:
    BasicNativeTableViewGroup(ii::gui::Widget host,TableViewFactory fallback,std::map<std::string,TableViewFactory,std::less<>> factories)
        :state_(std::make_shared<State>(host,std::move(fallback),std::move(factories))){}
    BasicNativeTableViewGroup(const BasicNativeTableViewGroup&)=delete;
    BasicNativeTableViewGroup& operator=(const BasicNativeTableViewGroup&)=delete;
    ~BasicNativeTableViewGroup(){if(state_)state_->retired=true;}
    [[nodiscard]] bool has(std::string_view key)const{
        const auto state=state_;if(!state||state->retired||state->owner!=std::this_thread::get_id())return false;
        auto found=state->factories.find(key);return found!=state->factories.end()&&found->second?true:bool(state->fallback);
    }
    [[nodiscard]] ii::core::Result<void> update(const std::vector<TableViewItem>& items,ii::gui::Size viewport){
        const auto state=state_;
        if(!state||state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Table View group retired");
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Table View group requires its owner thread");
        try{
            if(state->generation==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Table View generation exhausted");
            const auto admission=state->generation;
            std::unordered_map<std::string,std::shared_ptr<TableViewContext>> next;next.reserve(items.size());
            std::unordered_map<std::string,std::string> next_keys;next_keys.reserve(items.size());
            std::vector<ii::gui::KeyedContentItem> content;content.reserve(items.size());
            for(const auto& item:items){
                auto selected=state->factories.find(item.value.column_key);
                auto factory=selected!=state->factories.end()&&selected->second?selected->second:state->fallback;
                if(!factory)continue;
                auto found=state->contexts.find(item.key);
                auto context=found==state->contexts.end()?Policy::make_context(item.value):found->second;
                if(state->retired||state->generation!=admission)return {};
                if(!next.emplace(item.key,context).second)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Duplicate table View key");
                auto old_key=state->native_keys.find(item.key);std::string native_key;
                if(old_key!=state->native_keys.end())native_key=old_key->second;
                else{if(state->serial==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Table View keys exhausted");native_key=std::to_string(++state->serial);}
                next_keys.emplace(item.key,native_key);
                content.push_back({std::move(native_key),[context,factory=std::move(factory)](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
                    if(auto retired=builder.on_retire([context]{Policy::close(*context);});!retired)return retired;
                    auto clip=builder.absolute_stack({},"clip",{});if(!clip)return std::unexpected(clip.error());
                    if(auto clipped=clip->set_clip_content(true);!clipped)return clipped;
                    if(auto enabled=clip->set_enabled(context->get().enabled);!enabled)return enabled;
                    auto enabled=std::make_shared<ii::core::ObservableSubscription>(context->observe([clip=*clip](const TableViewSnapshot& value,std::uint64_t){
                        (void)clip.set_enabled(value.enabled);
                    }));
                    if(auto retained=builder.retain(std::move(enabled));!retained)return retained;
                    auto host=builder.content_host(*clip,"content",{});if(!host)return std::unexpected(host.error());
                    if(auto layout=clip->set_layout_handler([host=*host](ii::gui::Rect bounds){(void)host.set_frame({0,0,bounds.width,bounds.height});});!layout)return layout;
                    return host->set_content([context,factory](ii::gui::WidgetBuilder& child){return factory(child,context);});
                },item.frame});
            }
            if(state->retired||state->generation!=admission)return {};
            if(auto frame=state->host.set_frame({0,0,float(viewport.width),float(viewport.height)});!frame)return frame;
            if(state->retired||state->generation!=admission)return {};
            if(auto mounted=state->host.set_keyed_content(std::move(content),{.viewport=viewport});!mounted)return mounted;
            if(state->retired||state->generation!=admission)return {};
            const auto generation=++state->generation;
            state->contexts=std::move(next);
            state->native_keys=std::move(next_keys);
            for(const auto& item:items)if(auto found=state->contexts.find(item.key);found!=state->contexts.end()){
                auto context=found->second;
                Policy::publish(*context,item.value);
                if(state->retired||state->generation!=generation||!state->host.snapshot())return {};
            }
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table View update failed",std::current_exception());}
    }
};
} // namespace ii::gui
