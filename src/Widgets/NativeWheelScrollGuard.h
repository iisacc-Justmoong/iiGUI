#pragma once
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>

namespace ii::gui {
struct WheelScrollOptions final {
    double width=0,height=0;
    bool enabled=true,consume_inside=true;
    friend bool operator==(const WheelScrollOptions&,const WheelScrollOptions&)=default;
};
struct WheelScrollObservation final {
    double delta_x=0,delta_y=0,previous_x=0,previous_y=0,next_x=0,next_y=0;
};
class NativeWheelScrollGuard final {
    struct State final {
        WheelScrollOptions options;
        Widget root,target;
        std::function<void(const WheelScrollObservation&)> routed;
        std::uint64_t revision=0;
        bool live=false,retired=false;
        const std::thread::id owner=std::this_thread::get_id();
    };
    std::shared_ptr<State> state_;
    explicit NativeWheelScrollGuard(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static bool valid(const WheelScrollOptions& options)noexcept{
        return std::isfinite(options.width)&&options.width>=0&&options.width<=8192&&
               std::isfinite(options.height)&&options.height>=0&&options.height<=8192;
    }
    [[nodiscard]] static core::Result<void> check(const std::shared_ptr<State>& state){
        if(state->owner!=std::this_thread::get_id())return core::fail(core::ErrorCode::wrong_executor,"Wheel guard requires its owner thread");
        if(state->retired)return core::fail(core::ErrorCode::closed,"Wheel guard retired");
        if(auto root=state->root.snapshot();!root)return std::unexpected(root.error());return {};
    }
    [[nodiscard]] static core::Result<bool> route(const std::shared_ptr<State>& state,const WidgetInput& event){
        if(!state->live||state->retired||!state->options.enabled||state->target.is_null()||event.kind!=WidgetInputKind::scroll)return false;
        if(event.device==PointerDevice::touch&&event.control)return false;
        auto interaction=state->target.interaction();if(!interaction)return std::unexpected(interaction.error());
        if(!interaction->alive||!interaction->visible||!interaction->enabled||!interaction->contains(event.x,event.y))return false;
        auto before=state->target.snapshot();if(!before)return std::unexpected(before.error());
        double dx=event.scroll_x,dy=event.scroll_y;if(event.shift&&dx==0){dx=dy;dy=0;}
        const auto x=std::clamp(before->horizontal_scroll_offset+dx,0.,(std::max)(0.,before->content_width-before->bounds.width));
        const auto y=std::clamp(before->scroll_offset+dy,0.,(std::max)(0.,before->content_height-before->bounds.height));
        const bool moved=x!=before->horizontal_scroll_offset||y!=before->scroll_offset;
        const auto revision=state->revision;const bool consumed=moved||state->options.consume_inside;
        if(moved)if(auto changed=state->target.set_scroll_position(x,y);!changed)return std::unexpected(changed.error());
        if(state->retired||state->revision!=revision)return consumed;
        auto callback=state->routed;
        if(callback)callback({dx,dy,before->horizontal_scroll_offset,before->scroll_offset,x,y});
        return consumed;
    }
public:
    [[nodiscard]] static core::Result<NativeWheelScrollGuard> create(WidgetBuilder& builder,Widget parent,std::string_view id,
            const WheelScrollOptions& options={},std::function<void(const WheelScrollObservation&)> routed={}){
        if(!valid(options))return core::fail(core::ErrorCode::invalid_argument,"Invalid wheel-guard extent");
        try{
            auto state=std::make_shared<State>();state->options=options;state->routed=std::move(routed);
            auto root=builder.zstack(parent,id,{.spacing=0,.alignment=StackAlignment::stretch});if(!root)return std::unexpected(root.error());state->root=*root;
            if(auto sized=root->set_extent({options.width,options.height});!sized)return std::unexpected(sized.error());
            if(auto retained=builder.retain(state);!retained)return std::unexpected(retained.error());std::weak_ptr<State> weak=state;
            if(auto installed=root->set_input_handler([weak](const WidgetInput& event)->core::Result<bool>{
                auto state=weak.lock();if(!state)return false;
                try{return route(state,event);}catch(...){return core::fail(core::ErrorCode::failed,"Wheel-guard callback failed",std::current_exception());}
            });!installed)return std::unexpected(installed.error());
            if(auto published=builder.on_publish([weak]{if(auto state=weak.lock();state&&!state->retired)state->live=true;});!published)return std::unexpected(published.error());
            if(auto retired=builder.on_retire([weak]{if(auto state=weak.lock()){state->live=false;state->retired=true;state->target={};state->routed={};}});!retired)return std::unexpected(retired.error());
            return NativeWheelScrollGuard(std::move(state));
        }catch(...){return core::fail(core::ErrorCode::failed,"Wheel-guard creation failed",std::current_exception());}
    }
    [[nodiscard]] Widget root()const{return state_->root;}
    [[nodiscard]] core::Result<void> set_target(Widget target)const{
        if(auto ready=check(state_);!ready)return ready;
        if(target==state_->target)return {};
        if(state_->revision==(std::numeric_limits<std::uint64_t>::max)())return core::fail(core::ErrorCode::out_of_range,"Wheel-guard revision exhausted");
        if(target.is_null()){++state_->revision;state_->target={};return {};}
        auto snapshot=target.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
        if(snapshot->kind!=WidgetKind::scroll_view)return core::fail(core::ErrorCode::invalid_argument,"Wheel guard target must be a scroll view");
        auto descendant=target.is_within(state_->root);if(!descendant)return std::unexpected(descendant.error());
        if(!*descendant)return core::fail(core::ErrorCode::invalid_argument,"Wheel guard target must be inside its view");
        ++state_->revision;state_->target=std::move(target);return {};
    }
    [[nodiscard]] core::Result<void> set_options(const WheelScrollOptions& options)const{
        if(auto ready=check(state_);!ready)return ready;
        if(!valid(options))return core::fail(core::ErrorCode::invalid_argument,"Invalid wheel-guard extent");
        if(options==state_->options)return {};
        if(state_->revision==(std::numeric_limits<std::uint64_t>::max)())return core::fail(core::ErrorCode::out_of_range,"Wheel-guard revision exhausted");
        auto previous=state_->options;const auto revision=++state_->revision;state_->options=options;
        if(auto sized=state_->root.set_extent({options.width,options.height});!sized){if(state_->revision==revision)state_->options=previous;return sized;}
        return {};
    }
};
} // namespace ii::gui
