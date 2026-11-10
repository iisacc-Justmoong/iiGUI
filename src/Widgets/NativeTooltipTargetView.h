#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/WidgetInteraction.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

namespace ii::gui {
// Observation never replaces the target's input handlers. The controller owns
// the state policy, while iiGUI supplies weak, generation-specific metadata.
template<class Policy>
class BasicNativeTooltipTarget final {
    using NativeTooltipPresentation=typename Policy::Presentation;using TooltipController=typename Policy::Controller;
    using NativeTooltipScheduler=typename Policy::Scheduler;using Tooltip=typename Policy::Model;
    using TooltipTargetOptions=typename Policy::Options;using TooltipInput=typename Policy::Input;
    using TooltipPresentationStyle=typename Policy::Style;using TooltipDiagnostic=typename Policy::Diagnostic;
    using TooltipConfiguration=typename Policy::Configuration;using TooltipGeometryInput=typename Policy::GeometryInput;
    using TooltipInteraction=typename Policy::Interaction;using TooltipPhase=typename Policy::Phase;
    using TooltipPoint=typename Policy::Point;using TooltipRect=typename Policy::Rect;
    struct State final {
        std::shared_ptr<NativeTooltipPresentation> presentation;
        TooltipController controller;
        NativeTooltipScheduler scheduler;
        TooltipTargetOptions options;
        ii::gui::Widget target;
        std::shared_ptr<ii::gui::WidgetInteractionPort> target_port,popup_port;
        std::optional<ii::gui::WidgetInteraction> target_info,popup_info;
        std::optional<ii::core::Error> failure;
        std::shared_ptr<ii::core::Observable<TooltipDiagnostic>> diagnostic=
            std::make_shared<ii::core::Observable<TooltipDiagnostic>>(TooltipDiagnostic{});
        std::optional<TooltipInput> pending_options;
        std::optional<ii::gui::Widget> pending_target;
        std::optional<TooltipPresentationStyle> restore_style;
        std::optional<std::chrono::milliseconds> restore_duration;
        std::optional<TooltipConfiguration> restore_configuration;
        std::optional<double> restore_offset;
        const std::thread::id owner=std::this_thread::get_id();
        std::atomic_bool foreign_delivery=false;
        std::uint64_t target_revision=0;
        bool live=false,retired=false,processing=false,pending=false;
        State(std::shared_ptr<NativeTooltipPresentation> p,TooltipController c,NativeTooltipScheduler s,TooltipTargetOptions o)
            :presentation(std::move(p)),controller(std::move(c)),scheduler(std::move(s)),options(std::move(o)){}
    };
    std::shared_ptr<State> state_;
    explicit BasicNativeTooltipTarget(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static bool valid(const TooltipTargetOptions& value)noexcept {
        const auto coordinate=[](double n){return std::isfinite(n)&&std::abs(n)<=1000000000.;};
        const auto metric=[](double n){return std::isfinite(n)&&n>=0&&n<=8192;};
        return (!value.anchor||(coordinate(value.anchor->x)&&coordinate(value.anchor->y)))&&
            (!value.available||(coordinate(value.available->x)&&coordinate(value.available->y)&&
                metric(value.available->width)&&metric(value.available->height)));
    }
    [[nodiscard]] static ii::core::Result<void> ready(const std::shared_ptr<State>& state,bool recovering=false){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Tooltip target requires its owner thread");
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Tooltip target has retired");
        if(state->restore_style&&!recovering)return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip options need reconciliation");
        return {};
    }
    [[nodiscard]] static bool valid(const TooltipInput& value)noexcept {
        return Tooltip::validate(value.configuration)&&value.configuration.animation_duration<=std::chrono::hours(24)&&NativeTooltipPresentation::valid_style(value.style)&&
            value.configuration.content_padding==value.style.content_padding&&valid(value.target);
    }
    static void report(const std::shared_ptr<State>& state,std::optional<ii::core::Error> error){
        if(!error&&!state->failure)return;
        state->failure=std::move(error);
        auto diagnostic=state->diagnostic->get();
        diagnostic.revision=diagnostic.revision==(std::numeric_limits<std::uint64_t>::max)()?0:diagnostic.revision+1;
        diagnostic.failure=state->failure;state->diagnostic->set(std::move(diagnostic));
    }
    [[nodiscard]] static ii::core::Result<bool> position(const std::shared_ptr<State>& state){
        const bool targeted=!state->target.is_null();ii::gui::Size size;TooltipPoint anchor;
        if(targeted){
            if(!state->target_info)return false;
            const auto& target=*state->target_info;
            if(!target.alive||!target.visible||!target.enabled||target.input_blocked)return false;
            size=target.surface;
            anchor={target.bounds.x+(state->options.anchor?state->options.anchor->x:target.bounds.width/2),
                target.bounds.y+(state->options.anchor?state->options.anchor->y:target.bounds.height/2)};
            const auto& clip=target.clip;
            if(anchor.x<clip.x||anchor.y<clip.y||anchor.x>clip.x+clip.width||anchor.y>clip.y+clip.height)return false;
        }else{
            auto popup=state->presentation->root().interaction();if(!popup)return std::unexpected(popup.error());
            size=popup->surface;anchor=state->options.anchor.value_or(TooltipPoint{});
        }
        if(!std::isfinite(size.width)||!std::isfinite(size.height)||size.width<=0||size.height<=0||
                size.width>8192||size.height>8192)return false;
        const TooltipRect surface{0,0,size.width,size.height};
        const auto requested=state->options.available.value_or(surface);
        const auto left=(std::max)(surface.x,requested.x),top=(std::max)(surface.y,requested.y);
        const auto right=(std::min)(surface.x+surface.width,requested.x+requested.width);
        const auto bottom=(std::min)(surface.y+surface.height,requested.y+requested.height);
        if(right<=left||bottom<=top)return false;
        const TooltipRect available{left,top,right-left,bottom-top};
        auto config=state->controller.configuration();if(!config)return std::unexpected(config.error());
        const auto max_width=std::clamp(config->maximum_width-2*config->content_padding,1.,8192.);
        auto measured=state->presentation->content_root().measure(max_width);if(!measured)return std::unexpected(measured.error());
        const TooltipGeometryInput geometry{available,anchor,{measured->width,measured->height},targeted?std::optional{surface}:std::nullopt};
        auto positioned=state->controller.set_geometry(geometry);
        if(!positioned){if(positioned.error().code==ii::core::ErrorCode::invalid_argument)return false;return std::unexpected(positioned.error());}
        return true;
    }
    [[nodiscard]] static ii::core::Result<void> drive(const std::shared_ptr<State>& state){
        auto now=state->scheduler.elapsed();if(!now)return std::unexpected(now.error());
        TooltipInteraction input;input.target_present=!state->target.is_null();
        input.bubble_hovered=state->popup_info&&state->popup_info->alive&&state->popup_info->hovered;
        if(input.target_present&&state->target_info){
            const auto& target=*state->target_info;
            input.target_available=target.alive&&target.visible&&target.enabled&&!target.input_blocked;
            input.target_engaged=target.hovered||target.pressed||target.focused;
            if(input.target_available){auto positioned=position(state);if(!positioned)return std::unexpected(positioned.error());input.target_available=*positioned;}
        }else if(!input.target_present){
            auto snapshot=state->controller.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
            if(snapshot->phase==TooltipPhase::shown||snapshot->phase==TooltipPhase::waiting_to_hide){
                auto positioned=position(state);if(!positioned)return std::unexpected(positioned.error());
                if(!*positioned)return state->controller.close(*now);
            }
        }
        return state->controller.advance(*now,input);
    }
    [[nodiscard]] static ii::core::Result<TooltipInput> current_options(const std::shared_ptr<State>& state){
        auto configuration=state->controller.configuration();if(!configuration)return std::unexpected(configuration.error());
        auto style=state->presentation->style();if(!style)return std::unexpected(style.error());
        return TooltipInput{std::move(*configuration),std::move(*style),state->options};
    }
    [[nodiscard]] static ii::core::Result<void> apply_options(const std::shared_ptr<State>& state,const TooltipInput& value){
        if(!valid(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid tooltip options");
        auto previous=current_options(state);if(!previous)return std::unexpected(previous.error());
        if(*previous==value)return {};
        auto staged_style=value.style;
        staged_style.motion_enabled=previous->style.motion_enabled;staged_style.motion=previous->style.motion;
        const bool style_changed=previous->style!=staged_style;
        const bool motion_changed=staged_style!=value.style||previous->configuration.animation_duration!=value.configuration.animation_duration;
        const bool configuration_changed=previous->configuration!=value.configuration;
        std::optional<double> previous_offset;
        if(style_changed||motion_changed){
            auto checkpoint=state->presentation->scroll_checkpoint();if(!checkpoint)return std::unexpected(checkpoint.error());
            previous_offset=*checkpoint;
        }
        const auto record_restore=[&]{state->restore_style=previous->style;state->restore_duration=previous->configuration.animation_duration;
            state->restore_configuration=previous->configuration;state->restore_offset=previous_offset;};
        const auto restore_style=[&]()->ii::core::Result<void>{
            auto restored=state->presentation->set_style_with_duration(previous->style,previous->configuration.animation_duration);
            if(restored)restored=state->presentation->restore_scroll(previous_offset);return restored;
        };
        if(style_changed){
            if(auto changed=state->presentation->set_style_with_duration(staged_style,previous->configuration.animation_duration);!changed){
                if(state->presentation->needs_reconciliation())record_restore();
                return changed;
            }
            if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Tooltip retired during options update");
        }
        if(configuration_changed){
            if(auto changed=state->controller.set_configuration(value.configuration);!changed){
                if(style_changed&&!state->retired){
                    auto restored=restore_style();
                    if(!restored){record_restore();
                        return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip option rollback needs reconciliation",restored.error().cause);}
                }
                return changed;
            }
            if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Tooltip retired during options update");
        }
        // Motion may immediately complete an in-flight transition. Apply it
        // only after scheduling and geometry have accepted the candidate.
        if(motion_changed){
            if(auto changed=state->presentation->set_style_with_duration(value.style,value.configuration.animation_duration);!changed){
                const auto error=changed.error();
                auto restored=configuration_changed?state->controller.set_configuration(previous->configuration):ii::core::Result<void>{};
                if(restored)restored=restore_style();
                if(!restored||state->presentation->needs_reconciliation()){record_restore();
                    return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip motion rollback needs reconciliation",!restored?restored.error().cause:error.cause);}
                return std::unexpected(error);
            }
            if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Tooltip retired during options update");
        }
        state->options=value.target;
        return {};
    }
    [[nodiscard]] static ii::core::Result<void> validate_target(const std::shared_ptr<State>& state,const ii::gui::Widget& target){
        if(target.is_null())return {};
        auto target_place=target.placement();if(!target_place)return std::unexpected(target_place.error());
        auto popup_place=state->presentation->root().placement();if(!popup_place)return std::unexpected(popup_place.error());
        auto target_domain=target_place->domain.lock(),popup_domain=popup_place->domain.lock();
        if(!target_domain||target_domain!=popup_domain)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Tooltip target belongs to another widget domain");
        return {};
    }
    [[nodiscard]] static ii::core::Result<void> apply_target(const std::shared_ptr<State>& state,ii::gui::Widget target){
        if(auto checked=validate_target(state,target);!checked)return checked;
        if(target==state->target){if(state->live)state->pending=true;return {};}
        if(state->target_revision==(std::numeric_limits<std::uint64_t>::max)())
            return ii::core::fail(ii::core::ErrorCode::out_of_range,"Tooltip target revision exhausted");
        if(target.is_null()){
            if(state->live&&!state->target.is_null()){
                auto now=state->scheduler.elapsed();if(!now)return std::unexpected(now.error());
                if(auto closed=state->controller.close(*now);!closed)return closed;
                if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Tooltip retired during target update");
            }
            ++state->target_revision;state->target={};state->target_info.reset();state->target_port.reset();
            if(state->live)state->pending=true;return {};
        }
        std::optional<ii::gui::WidgetInteraction> current;
        if(state->live){auto info=target.interaction();if(!info)return std::unexpected(info.error());current=*info;}
        const auto revision=state->target_revision+1;std::weak_ptr<State> weak=state;
        auto port=std::make_shared<ii::gui::WidgetInteractionPort>([weak,revision](const ii::gui::WidgetInteraction& value){
            if(auto state=weak.lock()){
                if(state->owner!=std::this_thread::get_id()){state->foreign_delivery.store(true,std::memory_order_relaxed);return;}
                if(!state->retired&&state->target_revision==revision){state->target_info=value;enqueue(state);}
            }
        });
        if(auto observed=target.observe_interaction(port);!observed)return observed;
        state->target_revision=revision;state->target=std::move(target);state->target_info=std::move(current);state->target_port=std::move(port);
        if(state->live)state->pending=true;return {};
    }
    static void enqueue(const std::shared_ptr<State>& state)noexcept {
        if(!state->live||state->retired)return;state->pending=true;if(state->processing)return;
        state->processing=true;struct Reset final{State& state;~Reset(){state.processing=false;}} reset{*state};
        try {
            std::optional<ii::gui::Widget> failed_target;
            std::optional<TooltipInput> failed_options;
            std::optional<ii::core::Error> target_failure,options_failure;
            for(unsigned pass=0;state->pending&&pass<8;++pass){
                state->pending=false;
                if(auto permitted=ready(state);!permitted){report(state,permitted.error());return;}
                if(state->pending_target&&(!failed_target||*state->pending_target!=*failed_target)){
                    auto target=std::move(*state->pending_target);state->pending_target.reset();
                    if(auto changed=apply_target(state,target);!changed){
                        failed_target=target;target_failure=changed.error();
                        if(!state->pending_target)state->pending_target=std::move(target);
                    }else{failed_target.reset();target_failure.reset();}
                    if(state->retired||!state->live)return;
                }
                if(state->pending_options&&(!failed_options||*state->pending_options!=*failed_options)){
                    auto options=std::move(*state->pending_options);state->pending_options.reset();
                    if(auto changed=apply_options(state,options);!changed){
                        failed_options=options;options_failure=changed.error();
                        if(!state->pending_options)state->pending_options=std::move(options);
                    }else{failed_options.reset();options_failure.reset();}
                    if(state->retired||!state->live)return;
                }
                auto result=drive(state);
                if(!result){report(state,result.error());return;}
                if(state->retired||!state->live)return;
                if(!state->pending_target){failed_target.reset();target_failure.reset();}
                if(!state->pending_options){failed_options.reset();options_failure.reset();}
                const bool newer_target=state->pending_target&&(!failed_target||*state->pending_target!=*failed_target);
                const bool newer_options=state->pending_options&&(!failed_options||*state->pending_options!=*failed_options);
                if(newer_target||newer_options){state->pending=true;continue;}
                if(target_failure||options_failure){report(state,target_failure?target_failure:options_failure);return;}
                report(state,std::nullopt);
            }
            if(state->pending)report(state,ii::core::Error{ii::core::ErrorCode::busy,"Tooltip observation did not settle"});
        }catch(...){try{report(state,ii::core::Error{ii::core::ErrorCode::failed,"Tooltip observation failed",std::current_exception()});}catch(...) {}}
    }
    template<class Function>
    [[nodiscard]] static ii::core::Result<void> coordinated(const std::shared_ptr<State>& state,Function function,bool recovering=false){
        if(auto valid=ready(state,recovering);!valid)return valid;
        if(!state->live||state->processing)return ii::core::fail(ii::core::ErrorCode::busy,"Tooltip observation is not ready");
        state->processing=true;ii::core::Result<void> result;
        try{result=function();}catch(...){result=ii::core::fail(ii::core::ErrorCode::failed,"Tooltip operation failed",std::current_exception());}
        state->processing=false;
        const bool deliver=state->pending&&!state->retired;if(deliver)enqueue(state);
        if(result&&deliver&&state->failure)return std::unexpected(*state->failure);return result;
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeTooltipTarget> create(ii::gui::WidgetBuilder& builder,
            std::shared_ptr<NativeTooltipPresentation> presentation,TooltipController controller,
            NativeTooltipScheduler scheduler,TooltipTargetOptions options={}){
        if(!presentation||!valid(options))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid tooltip target options");
        auto matching=controller.uses_port(presentation.get());if(!matching)return std::unexpected(matching.error());
        if(!*matching)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Tooltip controller uses another presentation port");
        auto configuration=controller.configuration();if(!configuration)return std::unexpected(configuration.error());
        auto style=presentation->style();if(!style)return std::unexpected(style.error());
        if(auto configured=presentation->set_style_with_duration(*style,configuration->animation_duration);!configured)return std::unexpected(configured.error());
        try {
            auto state=std::make_shared<State>(std::move(presentation),std::move(controller),std::move(scheduler),std::move(options));
            std::weak_ptr<State> weak=state;
            state->popup_port=std::make_shared<ii::gui::WidgetInteractionPort>([weak](const ii::gui::WidgetInteraction& value){
                if(auto state=weak.lock()){
                    if(state->owner!=std::this_thread::get_id()){state->foreign_delivery.store(true,std::memory_order_relaxed);return;}
                    if(!state->retired){state->popup_info=value;enqueue(state);}
                }
            });
            if(auto observed=state->presentation->root().observe_interaction(state->popup_port);!observed)return std::unexpected(observed.error());
            if(auto measured=state->presentation->content_root().set_measure_handler([weak](const ii::gui::WidgetMeasurement&){
                if(auto state=weak.lock()){
                    if(state->owner!=std::this_thread::get_id()){state->foreign_delivery.store(true,std::memory_order_relaxed);return;}
                    if(!state->retired)enqueue(state);
                }
            });!measured)return std::unexpected(measured.error());
            if(auto retained=builder.retain(state);!retained)return std::unexpected(retained.error());
            if(auto registered=builder.on_publish([weak]{if(auto state=weak.lock();state&&!state->retired){
                state->live=true;if(!state->target.is_null()){
                    auto current=state->target.interaction();
                    if(!current)report(state,current.error());else{state->target_info=*current;enqueue(state);}
                }
            }});!registered)return std::unexpected(registered.error());
            if(auto registered=builder.on_retire([weak]{if(auto state=weak.lock()){
                state->retired=true;state->live=false;state->target_port.reset();state->popup_port.reset();
            }});!registered)return std::unexpected(registered.error());
            return BasicNativeTooltipTarget(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip observation construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_target(ii::gui::Widget target)const {
        if(auto valid=ready(state_);!valid)return valid;
        try {
            if(auto checked=validate_target(state_,target);!checked){state_->pending_target.reset();return checked;}
            if(state_->processing){state_->pending_target=std::move(target);state_->pending=true;return {};}
            state_->pending_target.reset();
            if(!state_->live){
                state_->processing=true;struct Reset final{State& state;~Reset(){state.processing=false;}} reset{*state_};
                return apply_target(state_,std::move(target));
            }
            return coordinated(state_,[&]{return apply_target(state_,std::move(target));});
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip target update failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> last_failure()const {
        if(auto valid=ready(state_,true);!valid)return std::unexpected(valid.error());
        if(state_->foreign_delivery.exchange(false,std::memory_order_relaxed))
            return std::optional{ii::core::Error{ii::core::ErrorCode::wrong_executor,"Tooltip observation arrived on another thread"}};
        return state_->failure;
    }
    [[nodiscard]] ii::core::Result<void> status()const{return ready(state_);}
    [[nodiscard]] ii::core::Result<std::shared_ptr<const ii::core::Observable<TooltipDiagnostic>>> diagnostic_source()const {
        if(auto permitted=ready(state_,true);!permitted)return std::unexpected(permitted.error());
        return std::shared_ptr<const ii::core::Observable<TooltipDiagnostic>>(state_->diagnostic);
    }
    [[nodiscard]] ii::core::Result<TooltipInput> options()const {
        if(auto permitted=ready(state_);!permitted)return std::unexpected(permitted.error());
        return current_options(state_);
    }
    [[nodiscard]] ii::core::Result<void> set_options(TooltipInput value)const {
        if(auto permitted=ready(state_);!permitted)return permitted;
        if(!valid(value)){state_->pending_options.reset();return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid tooltip options");}
        if(state_->processing){
            try{state_->pending_options=std::move(value);state_->pending=true;return {};}
            catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip option queue failed",std::current_exception());}
        }
        state_->pending_options.reset();
        if(!state_->live){
            state_->processing=true;struct Reset final{State& state;~Reset(){state.processing=false;}} reset{*state_};
            try{return apply_options(state_,value);}
            catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip options failed",std::current_exception());}
        }
        return coordinated(state_,[&]()->ii::core::Result<void>{
            auto changed=apply_options(state_,value);if(changed)state_->pending=true;return changed;
        });
    }
    [[nodiscard]] ii::core::Result<void> reconcile()const {
        return coordinated(state_,[&]()->ii::core::Result<void>{
            if(auto repaired=state_->controller.reconcile();!repaired)return repaired;
            if(state_->restore_configuration){
                if(auto restored=state_->controller.set_configuration(*state_->restore_configuration);!restored)return restored;
            }
            if(state_->restore_style){
                if(!state_->restore_duration)return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip motion duration needs reconciliation");
                if(auto restored=state_->presentation->set_style_with_duration(*state_->restore_style,*state_->restore_duration);!restored)return restored;
                if(auto restored=state_->presentation->restore_scroll(state_->restore_offset);!restored)return restored;
                state_->restore_style.reset();
                state_->restore_duration.reset();
                state_->restore_configuration.reset();
                state_->restore_offset.reset();
            }
            state_->pending=true;return {};
        },true);
    }
    [[nodiscard]] ii::core::Result<void> refresh()const {
        if(auto valid=ready(state_);!valid)return valid;
        // Binding evaluation may precede publication, and opened/closed handlers
        // may change content while an observation is being delivered.
        if(!state_->live)return {};
        if(state_->processing){state_->pending=true;return {};}
        enqueue(state_);if(state_->failure)return std::unexpected(*state_->failure);return {};
    }
    [[nodiscard]] ii::core::Result<void> open()const {
        if(auto valid=ready(state_);!valid)return valid;auto now=state_->scheduler.elapsed();if(!now)return std::unexpected(now.error());
        return coordinated(state_,[&]()->ii::core::Result<void>{
            if(!state_->target.is_null()){
                auto current=state_->target.interaction();if(!current)return std::unexpected(current.error());state_->target_info=*current;
            }
            auto positioned=position(state_);if(!positioned)return std::unexpected(positioned.error());
            if(!*positioned)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Tooltip anchor or available area is not visible");
            return state_->controller.open(*now);
        });
    }
    [[nodiscard]] ii::core::Result<void> close()const {
        if(auto valid=ready(state_);!valid)return valid;auto now=state_->scheduler.elapsed();if(!now)return std::unexpected(now.error());
        return coordinated(state_,[&]{return state_->controller.close(*now);});
    }
    [[nodiscard]] typename NativeTooltipScheduler::Wake weak_tick()const {
        std::weak_ptr<State> weak=state_;
        return [weak](std::int64_t now)->ii::core::Result<void>{
            auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Tooltip observer expired");
            auto result=coordinated(state,[&]{return state->controller.tick(now);});
            if(!result&&!state->retired)report(state,result.error());return result;
        };
    }
};
} // namespace ii::gui
