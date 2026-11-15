#pragma once
#include <iiGUI/MobileTabPainter.h>
#include <iiGUI/WidgetMotion.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>

namespace ii::gui {
template<class Policy>class BasicNativeMobileTab final {
    using Input=typename Policy::Input;using Platform=typename Policy::Platform;using IconCache=typename Policy::IconCache;
    struct State final {
        Input input;
        Widget root;
        std::shared_ptr<IconCache> cache=std::make_shared<IconCache>();
        std::shared_ptr<const RasterImage> image;
        std::shared_ptr<MotionPreferences> motion;
        WidgetMotionTransition<1> selection;
        WidgetFrameHandler frame;
        runtime::Task activated;
        std::uint64_t revision=0,motion_revision=0;
        bool retired=false;
        const std::thread::id owner=std::this_thread::get_id();
    };
    std::shared_ptr<State> state_;
    explicit BasicNativeMobileTab(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static bool valid(const Input& value)noexcept{
        const auto metric=[](double n,double lo,double hi){return std::isfinite(n)&&n>=lo&&n<=hi;};
        const auto& item=value.item;
        if(!metric(value.width,1,8192)||!metric(value.height,1,8192)||value.icon_revision<0||item.icon_name.empty()||item.icon_name.size()>256||
           item.icon_name.find_first_not_of(" \t\r\n")==item.icon_name.npos||item.badge.size()>256||
           (!item.text.size()&&!item.accessibility_name.size())||
           !Policy::valid_text(item.icon_name)||!Policy::valid_text(item.text)||!Policy::valid_text(item.badge)||
           !Policy::valid_text(item.accessibility_name)||!Policy::valid_text(item.badge_accessibility_name)||
           (value.platform!=Platform::automatic&&value.platform!=Platform::ios&&value.platform!=Platform::android))return false;
        for(auto c:{value.focus_color.red,value.focus_color.green,value.focus_color.blue,value.focus_color.alpha})if(!metric(c,0,1))return false;
        return true;
    }
    [[nodiscard]] static bool android(const Input& input)noexcept{
        if(input.platform!=Platform::automatic)return input.platform==Platform::android;
#if defined(__ANDROID__)
        return true;
#else
        return false;
#endif
    }
    [[nodiscard]] static ActionOptions action(const Input& input){
        return {.accessibility_name=input.item.accessibility_name.empty()?input.item.text:input.item.accessibility_name,
            .enabled=input.enabled&&input.item.enabled,.focus_ring=false,.role=ActionRole::tab,
            .width=input.width,.height=input.height,.checked=input.selected,
            .accessibility_description=input.item.badge_accessibility_name.empty()?input.item.badge:input.item.badge_accessibility_name};
    }
    [[nodiscard]] static core::Result<void> check(const std::shared_ptr<State>& state){
        if(state->owner!=std::this_thread::get_id())return core::fail(core::ErrorCode::wrong_executor,"Mobile tab requires its owner thread");
        if(state->retired)return core::fail(core::ErrorCode::closed,"Mobile tab retired");
        if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());return {};
    }
    [[nodiscard]] static core::Result<WidgetMotion> motion_policy(const std::shared_ptr<State>& state){
        auto preferences=state->motion->state();if(!preferences)return std::unexpected(preferences.error());
        WidgetMotion policy;auto duration=state->motion->duration(policy.duration);if(!duration)return std::unexpected(duration.error());
        policy.duration=std::chrono::duration_cast<std::chrono::milliseconds>(*duration);
        policy.reduced_motion=!preferences->animated();state->motion_revision=preferences->revision;return policy;
    }
public:
    [[nodiscard]] static core::Result<BasicNativeMobileTab> create(WidgetBuilder& builder,Widget parent,std::string_view id,
                                                                    const Input& input,runtime::Task activated={}){
        try{
            if(!valid(input))return core::fail(core::ErrorCode::invalid_argument,"Invalid mobile-tab options");
            auto state=std::make_shared<State>();auto image=state->cache->resolve(input.item.icon_name,input.icon_source,Policy::Tokens::icon_size);
            if(!image)return std::unexpected(image.error());state->image=*image;state->input=input;state->activated=std::move(activated);
            auto motion=builder.motion_preferences();if(!motion)return std::unexpected(motion.error());state->motion=*motion;
            if(!state->selection.reset({input.selected&&input.draw_selection?1.:0.}))return core::fail(core::ErrorCode::invalid_argument,"Invalid mobile-tab selection");
            std::weak_ptr<State> weak=state;
            auto root=builder.action_surface(parent,id,action(input),[weak]{if(auto s=weak.lock();s&&!s->retired&&s->activated){auto callback=s->activated;callback();}});
            if(!root)return std::unexpected(root.error());state->root=*root;
            if(auto painted=root->set_paint_handler([state](const WidgetPaintState& paint)->core::Result<std::shared_ptr<const WidgetDrawing>>{
                if(state->retired)return core::fail(core::ErrorCode::closed,"Mobile tab retired");
                return BasicMobileTabPainter<Policy>::standalone(state->input.item,state->input.selected,state->input.enabled,
                    android(state->input),state->input.show_label,state->selection.value()[0],state->input.focus_color,state->image,paint);
            });!painted)return std::unexpected(painted.error());
            state->frame=[weak](runtime::Clock::time_point now)->core::Result<bool>{
                auto state=weak.lock();if(!state||state->retired)return false;
                auto visible=state->root.snapshot();if(!visible)return std::unexpected(visible.error());
                if(!visible->visible||!visible->enabled){state->selection.settle();return false;}
                auto preferences=state->motion->state();if(!preferences)return std::unexpected(preferences.error());
                if(preferences->revision!=state->motion_revision){
                    auto policy=motion_policy(state);if(!policy)return std::unexpected(policy.error());
                    if(!state->selection.retarget(state->selection.target(),*policy,state->input.motion_enabled&&state->input.enabled,now))
                        return core::fail(core::ErrorCode::invalid_argument,"Invalid mobile-tab motion");
                }
                const auto more=state->selection.advance(now);
                if(auto painted=state->root.request_repaint();!painted){if(state->retired)return false;return std::unexpected(painted.error());}
                return more&&!state->retired;
            };
            if(auto frame=root->set_frame_handler(state->frame,false);!frame)return std::unexpected(frame.error());
            if(auto retired=builder.on_retire([weak]{if(auto state=weak.lock()){state->retired=true;state->cache.reset();state->image.reset();state->motion.reset();state->frame={};state->input.icon_source.reset();state->activated={};}});!retired)return std::unexpected(retired.error());
            return BasicNativeMobileTab(std::move(state));
        }catch(...){return core::fail(core::ErrorCode::failed,"Mobile-tab creation failed",std::current_exception());}
    }
    [[nodiscard]] Widget root()const{return state_->root;}
    [[nodiscard]] core::Result<void> set_options(const Input& input)const{
        if(auto ready=check(state_);!ready)return ready;
        if(input==state_->input)return {};
        if(!valid(input))return core::fail(core::ErrorCode::invalid_argument,"Invalid mobile-tab options");
        try{
            if(state_->revision==(std::numeric_limits<std::uint64_t>::max)())return core::fail(core::ErrorCode::out_of_range,"Mobile-tab revision exhausted");
            const auto revision=++state_->revision;
            auto cache=state_->cache;
            if(input.icon_revision!=state_->input.icon_revision||input.icon_source!=state_->input.icon_source||input.item.icon_name!=state_->input.item.icon_name)
                cache=std::make_shared<IconCache>();
            auto image=cache->resolve(input.item.icon_name,input.icon_source,Policy::Tokens::icon_size);
            if(state_->retired)return core::fail(core::ErrorCode::closed,"Mobile tab retired during icon lookup");
            if(state_->revision!=revision)return {};
            if(!image)return std::unexpected(image.error());
            auto next=state_->selection;
            auto policy=motion_policy(state_);if(!policy)return std::unexpected(policy.error());
            if(!next.retarget({input.selected&&input.draw_selection?1.:0.},*policy,input.motion_enabled&&input.enabled,runtime::Clock::now()))
                return core::fail(core::ErrorCode::invalid_argument,"Invalid mobile-tab motion");
            const auto old=state_->input;auto old_image=state_->image;auto old_cache=state_->cache;auto old_selection=state_->selection;
            state_->input=input;state_->image=*image;state_->cache=std::move(cache);state_->selection=std::move(next);
            if(auto updated=state_->root.set_action_options(action(input));!updated){
                if(state_->revision==revision){state_->input=old;state_->image=std::move(old_image);state_->cache=std::move(old_cache);state_->selection=std::move(old_selection);}return updated;
            }
            if(state_->revision!=revision||state_->retired)return {};
            if(state_->selection.active()){
                if(auto requested=state_->root.request_frames();!requested)state_->selection.settle();
            }else if(auto stopped=state_->root.set_frame_handler(state_->frame,false);!stopped)return stopped;
            return state_->root.request_repaint();
        }catch(...){return core::fail(core::ErrorCode::failed,"Mobile-tab update failed",std::current_exception());}
    }
};
} // namespace ii::gui
