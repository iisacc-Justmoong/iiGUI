#pragma once
#include <iiGUI/FocusRing.h>
#include <iiGUI/NativeWidgets.h>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>

namespace ii::gui {
class NativeFocusRing final {
    struct State final {
        FocusRing ring;
        FocusRingOptions authored;
        Widget root;
        std::shared_ptr<MotionPreferences> motion;
        WidgetFrameHandler frame;
        std::uint64_t revision=0,motion_revision=0;
        bool retired=false;
        const std::thread::id owner=std::this_thread::get_id();
    };
    std::shared_ptr<State> state_;
    explicit NativeFocusRing(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static core::Result<void> check(const std::shared_ptr<State>& state){
        if(state->owner!=std::this_thread::get_id())return core::fail(core::ErrorCode::wrong_executor,"Focus ring requires its owner thread");
        if(state->retired)return core::fail(core::ErrorCode::closed,"Focus ring retired");
        if(auto snapshot=state->root.snapshot();!snapshot)return std::unexpected(snapshot.error());
        return {};
    }
    [[nodiscard]] static core::Result<FocusRingOptions> effective_options(const std::shared_ptr<State>& state,const FocusRingOptions& authored){
        auto preferences=state->motion->state();if(!preferences)return std::unexpected(preferences.error());
        auto duration=state->motion->duration(authored.motion.duration);if(!duration)return std::unexpected(duration.error());
        auto effective=authored;effective.motion.duration=std::chrono::duration_cast<std::chrono::milliseconds>(*duration);
        effective.motion.reduced_motion=authored.motion.reduced_motion||!preferences->animated();
        state->motion_revision=preferences->revision;return effective;
    }
public:
    [[nodiscard]] static core::Result<NativeFocusRing> create(WidgetBuilder& builder,Widget parent,std::string_view id,const FocusRingOptions& options){
        try{
            auto state=std::make_shared<State>();
            if(!state->ring.update(options,runtime::Clock::now()))return core::fail(core::ErrorCode::invalid_argument,"Invalid focus-ring options");
            state->authored=options;auto motion=builder.motion_preferences();if(!motion)return std::unexpected(motion.error());state->motion=*motion;
            auto root=builder.absolute_stack(parent,id,{options.width,options.height});if(!root)return std::unexpected(root.error());
            state->root=*root;std::weak_ptr<State> weak=state;
            if(auto paint=root->set_paint_handler([state](const WidgetPaintState& input)->core::Result<std::shared_ptr<const WidgetDrawing>>{
                if(state->retired)return core::fail(core::ErrorCode::closed,"Focus ring retired");
                return std::make_shared<const WidgetDrawing>(state->ring.drawing(input.size));
            });!paint)return std::unexpected(paint.error());
            state->frame=[weak](runtime::Clock::time_point now)->core::Result<bool>{
                auto state=weak.lock();if(!state||state->retired)return false;
                auto preferences=state->motion->state();if(!preferences)return std::unexpected(preferences.error());
                if(preferences->revision!=state->motion_revision){
                    auto options=effective_options(state,state->authored);if(!options)return std::unexpected(options.error());
                    if(!state->ring.update(*options,now))return core::fail(core::ErrorCode::invalid_argument,"Invalid focus-ring motion");
                }
                const auto more=state->ring.advance(now);
                if(auto painted=state->root.request_repaint();!painted){if(state->retired)return false;return std::unexpected(painted.error());}
                return more&&!state->retired;
            };
            if(auto frame=root->set_frame_handler(state->frame,false);!frame)return std::unexpected(frame.error());
            if(auto retiring=builder.on_retire([weak]{if(auto state=weak.lock()){state->retired=true;state->motion.reset();state->frame={};}});!retiring)return std::unexpected(retiring.error());
            return NativeFocusRing(std::move(state));
        }catch(...){return core::fail(core::ErrorCode::failed,"Focus-ring creation failed",std::current_exception());}
    }
    [[nodiscard]] Widget root()const{return state_->root;}
    [[nodiscard]] core::Result<void> set_options(const FocusRingOptions& options)const{
        if(auto active=check(state_);!active)return active;
        if(options==state_->authored)return {};
        try{
            auto effective=effective_options(state_,options);if(!effective)return std::unexpected(effective.error());
            auto next=state_->ring;
            if(!next.update(*effective,runtime::Clock::now()))return core::fail(core::ErrorCode::invalid_argument,"Invalid focus-ring options");
            if(state_->revision==(std::numeric_limits<std::uint64_t>::max)())return core::fail(core::ErrorCode::out_of_range,"Focus-ring revision exhausted");
            auto previous=state_->ring;auto previous_authored=state_->authored;
            const auto revision=++state_->revision;state_->ring=std::move(next);state_->authored=options;
            if(auto resized=state_->root.set_extent({options.width,options.height});!resized){
                if(state_->revision==revision){state_->ring=std::move(previous);state_->authored=std::move(previous_authored);}return resized;
            }
            if(state_->revision!=revision||state_->retired)return {};
            if(state_->ring.animating()){
                if(auto started=state_->root.request_frames();!started)state_->ring.settle();
            }else if(auto stopped=state_->root.set_frame_handler(state_->frame,false);!stopped)return stopped;
            return state_->root.request_repaint();
        }catch(...){return core::fail(core::ErrorCode::failed,"Focus-ring update failed",std::current_exception());}
    }
};
} // namespace ii::gui
