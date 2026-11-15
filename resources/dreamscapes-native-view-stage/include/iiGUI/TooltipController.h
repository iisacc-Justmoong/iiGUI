#pragma once
#include <iiSDKCore/Core/Result/Result.h>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
namespace ii::gui {
template<class Policy>
class BasicTooltipController final {
    using Tooltip=typename Policy::Model;using TooltipInteraction=typename Policy::Interaction;using TooltipGeometryInput=typename Policy::GeometryInput;
    using TooltipLayout=typename Policy::Layout;using TooltipPresentationPort=typename Policy::Port;using TooltipCallbacks=typename Policy::Callbacks;
    using TooltipTransition=typename Policy::Transition;using TooltipConfiguration=typename Policy::Configuration;using TooltipSnapshot=typename Policy::Snapshot;
    struct State final {
        Tooltip model;
        TooltipInteraction interaction;
        std::optional<TooltipGeometryInput> geometry;
        std::optional<TooltipLayout> presentation;
        std::shared_ptr<TooltipPresentationPort> port;
        TooltipCallbacks callbacks;
        const std::thread::id owner=std::this_thread::get_id();
        bool busy=false,retired=false,needs_reconciliation=false;
    };
    std::shared_ptr<State> state_;
    explicit BasicTooltipController(std::shared_ptr<State> state):state_(std::move(state)){}
    struct Guard final {bool& value;explicit Guard(bool& target):value(target){value=true;}~Guard(){value=false;}};
    [[nodiscard]] static ii::core::Result<void> ready(const std::shared_ptr<State>& state,bool allow_reconciliation=false){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Tooltip requires its owner thread");
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Tooltip has retired");
        if(state->busy)return ii::core::fail(ii::core::ErrorCode::busy,"Tooltip update is already in progress");
        if(state->needs_reconciliation&&!allow_reconciliation)return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip host needs reconciliation");
        return {};
    }
    [[nodiscard]] static ii::core::Result<void> schedule(const std::shared_ptr<State>& state,std::optional<std::int64_t> deadline){
        try{return state->port->set_deadline(deadline);}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip scheduler threw",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> present(const std::shared_ptr<State>& state,std::optional<TooltipLayout> layout){
        try{return state->port->present(std::move(layout));}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip presentation threw",std::current_exception());}
    }
    template<class Mutation>
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state,Mutation mutation){
        if(auto valid=ready(state);!valid)return valid;
        TooltipCallbacks callbacks;TooltipTransition changed;
        try {
            {
                Guard guard(state->busy);
                callbacks=state->callbacks;
                auto model=state->model;auto interaction=state->interaction;auto geometry=state->geometry;
                auto applied=mutation(model,interaction,geometry);if(!applied)return std::unexpected(applied.error());changed=*applied;
                std::optional<TooltipLayout> presentation;
                if(model.visible()){
                    if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Visible tooltip needs geometry");
                    presentation=model.layout(geometry->available,geometry->anchor,geometry->content,geometry->anchor_bounds);
                    if(!presentation)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Tooltip geometry is unavailable");
                }
                const auto old_deadline=state->model.next_deadline(),new_deadline=model.next_deadline();
                if(old_deadline!=new_deadline){if(auto scheduled=schedule(state,new_deadline);!scheduled){
                    state->needs_reconciliation=state->port->needs_reconciliation();return scheduled;
                }}
                if(presentation!=state->presentation){
                    if(auto shown=present(state,presentation);!shown){
                        state->needs_reconciliation=state->port->needs_reconciliation();
                        if(old_deadline!=new_deadline){
                            auto restored=schedule(state,old_deadline);
                            if(!restored){state->needs_reconciliation=true;return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip presentation and deadline rollback failed",restored.error().cause);}
                        }
                        return shown;
                    }
                }
                state->model=std::move(model);state->interaction=interaction;state->geometry=geometry;
                state->presentation=std::move(presentation);
            }
            if(changed.opened&&callbacks.opened)callbacks.opened();
            if(changed.closed&&callbacks.closed)callbacks.closed();
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip update callback failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<BasicTooltipController> create(std::shared_ptr<TooltipPresentationPort> port,
            TooltipConfiguration configuration={},TooltipCallbacks callbacks={}){
        if(!port||!Tooltip::validate(configuration))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid tooltip port or configuration");
        try {
            auto state=std::make_shared<State>();(void)state->model.set_configuration(configuration);
            state->port=std::move(port);state->callbacks=std::move(callbacks);
            if(auto hidden=present(state,std::nullopt);!hidden)return std::unexpected(hidden.error());
            if(auto unscheduled=schedule(state,std::nullopt);!unscheduled)return std::unexpected(unscheduled.error());
            return BasicTooltipController(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<TooltipSnapshot> snapshot()const {
        if(auto valid=ready(state_,true);!valid)return std::unexpected(valid.error());
        return TooltipSnapshot{state_->model.phase(),state_->presentation,state_->model.next_deadline(),state_->needs_reconciliation};
    }
    [[nodiscard]] ii::core::Result<TooltipConfiguration> configuration()const {
        if(auto valid=ready(state_);!valid)return std::unexpected(valid.error());return state_->model.configuration();
    }
    [[nodiscard]] ii::core::Result<bool> uses_port(const TooltipPresentationPort* port)const {
        if(auto valid=ready(state_);!valid)return std::unexpected(valid.error());return state_->port.get()==port;
    }
    [[nodiscard]] ii::core::Result<void> set_configuration(TooltipConfiguration value)const {
        return update(state_,[&](Tooltip& model,auto&,auto&)->ii::core::Result<TooltipTransition>{
            if(!model.set_configuration(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid tooltip configuration");return TooltipTransition{};
        });
    }
    [[nodiscard]] ii::core::Result<void> set_geometry(TooltipGeometryInput value)const {
        return update(state_,[&](Tooltip& model,auto&,auto& geometry)->ii::core::Result<TooltipTransition>{
            if(!model.layout(value.available,value.anchor,value.content,value.anchor_bounds))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid tooltip geometry");
            geometry=value;return TooltipTransition{};
        });
    }
    [[nodiscard]] ii::core::Result<void> advance(std::int64_t now,TooltipInteraction value)const {
        return update(state_,[&](Tooltip& model,auto& interaction,auto&)->ii::core::Result<TooltipTransition>{
            auto result=model.advance(now,value);if(!result)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid tooltip time");
            interaction=value;return *result;
        });
    }
    [[nodiscard]] ii::core::Result<void> tick(std::int64_t now)const {
        return update(state_,[&](Tooltip& model,auto& interaction,auto&)->ii::core::Result<TooltipTransition>{
            auto result=model.advance(now,interaction);if(!result)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid tooltip time");return *result;
        });
    }
    [[nodiscard]] ii::core::Result<void> open(std::int64_t now)const {
        return update(state_,[&](Tooltip& model,auto&,auto&)->ii::core::Result<TooltipTransition>{
            auto result=model.open(now);if(!result)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid tooltip time");return *result;
        });
    }
    [[nodiscard]] ii::core::Result<void> close(std::int64_t now)const {
        return update(state_,[&](Tooltip& model,auto&,auto&)->ii::core::Result<TooltipTransition>{
            auto result=model.close(now);if(!result)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid tooltip time");return *result;
        });
    }
    [[nodiscard]] std::function<ii::core::Result<void>(std::int64_t)> weak_tick()const {
        std::weak_ptr<State> weak=state_;
        return [weak](std::int64_t now)->ii::core::Result<void>{
            auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Tooltip owner expired");
            return BasicTooltipController(std::move(state)).tick(now);
        };
    }
    [[nodiscard]] ii::core::Result<void> reconcile()const {
        if(auto valid=ready(state_,true);!valid)return valid;
        Guard guard(state_->busy);
        if(auto scheduled=schedule(state_,state_->model.next_deadline());!scheduled){state_->needs_reconciliation=true;return scheduled;}
        if(auto shown=present(state_,state_->presentation);!shown){state_->needs_reconciliation=true;return shown;}
        state_->needs_reconciliation=false;return {};
    }
    [[nodiscard]] ii::core::Result<void> retire()const {
        if(auto valid=ready(state_,true);!valid)return valid;
        try {
            Guard guard(state_->busy);const auto deadline=state_->model.next_deadline();
            if(auto canceled=schedule(state_,std::nullopt);!canceled){
                state_->needs_reconciliation=state_->needs_reconciliation||state_->port->needs_reconciliation();return canceled;
            }
            if(auto hidden=present(state_,std::nullopt);!hidden){
                state_->needs_reconciliation=state_->needs_reconciliation||state_->port->needs_reconciliation();
                auto restored=schedule(state_,deadline);if(!restored){state_->needs_reconciliation=true;
                    return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip retirement rollback failed",restored.error().cause);}
                return hidden;
            }
            state_->retired=true;state_->callbacks={};state_->presentation.reset();state_->port.reset();return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip retirement failed",std::current_exception());}
    }
};
} // namespace ii::gui
