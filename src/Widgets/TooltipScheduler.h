#pragma once
#include <iiSDKCore/Core/Result/Result.h>
#include <iiSDKCore/Runtime/EventLoop.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

namespace ii::gui {
// One event-loop deadline at a time. The receiver is weak and all UI-facing
// calls are owner-thread only; cancellation cannot address a later generation.
class TooltipScheduler final {
public:
    using Wake=std::function<ii::core::Result<void>(std::int64_t)>;
private:
    struct State final {
        ii::runtime::EventLoop::Sender sender;
        ii::runtime::TaskToken token;
        const ii::runtime::Clock::time_point origin=ii::runtime::Clock::now();
        const std::thread::id owner=std::this_thread::get_id();
        Wake wake;
        std::optional<ii::core::Error> failure;
        std::atomic_bool foreign_delivery=false;
        std::uint64_t generation=0;
        bool retired=false;
        explicit State(ii::runtime::EventLoop::Sender value):sender(std::move(value)){}
    };
    std::shared_ptr<State> state_;
    explicit TooltipScheduler(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::core::Result<void> ready(const std::shared_ptr<State>& state){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Tooltip scheduler requires its owner thread");
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Tooltip scheduler has retired");return {};
    }
    [[nodiscard]] static std::int64_t now(const std::shared_ptr<State>& state)noexcept {
        const auto current=ii::runtime::Clock::now();if(current<=state->origin)return 0;
        using FloatingMilliseconds=std::chrono::duration<long double,std::milli>;
        const auto elapsed=FloatingMilliseconds(current.time_since_epoch()).count()-FloatingMilliseconds(state->origin.time_since_epoch()).count();
        const auto limit=(std::numeric_limits<std::int64_t>::max)()-86400000;
        return elapsed>=static_cast<long double>(limit)?limit:static_cast<std::int64_t>(elapsed);
    }
    static void deliver(const std::weak_ptr<State>& weak,std::uint64_t generation)noexcept {
        auto state=weak.lock();if(!state)return;
        if(state->owner!=std::this_thread::get_id()){state->foreign_delivery.store(true,std::memory_order_relaxed);return;}
        if(state->retired||state->generation!=generation)return;
        state->token={};
        try {
            auto wake=state->wake;if(!wake){state->failure=ii::core::Error{ii::core::ErrorCode::closed,"Tooltip wake receiver is missing"};return;}
            auto result=wake(now(state));
            if(!state->retired){if(result)state->failure.reset();else state->failure=result.error();}
        }catch(...){
            const auto cause=std::current_exception();
            if(!state->retired)try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Tooltip wake failed",cause};}catch(...){}
        }
    }
public:
    [[nodiscard]] static ii::core::Result<TooltipScheduler> create(ii::runtime::EventLoop::Sender sender){
        try{return TooltipScheduler(std::make_shared<State>(std::move(sender)));}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip scheduler construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_wake(Wake wake)const {
        if(auto valid=ready(state_);!valid)return valid;
        if(!wake)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Tooltip wake receiver is required");
        if(state_->token.pending())return ii::core::fail(ii::core::ErrorCode::busy,"Cancel the active tooltip deadline before replacing its receiver");
        state_->wake=std::move(wake);return {};
    }
    [[nodiscard]] ii::core::Result<std::int64_t> elapsed()const {
        if(auto valid=ready(state_);!valid)return std::unexpected(valid.error());return now(state_);
    }
    [[nodiscard]] ii::core::Result<void> schedule(std::optional<std::int64_t> deadline)const {
        if(auto valid=ready(state_);!valid)return valid;
        if(state_->generation==(std::numeric_limits<std::uint64_t>::max)())
            return ii::core::fail(ii::core::ErrorCode::out_of_range,"Tooltip scheduler generation exhausted");
        const auto generation=state_->generation+1;
        if(!deadline){state_->generation=generation;(void)state_->token.cancel();state_->token={};return {};}
        if(*deadline<0||!state_->wake)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid tooltip deadline or missing wake receiver");
        using FloatingMilliseconds=std::chrono::duration<long double,std::milli>;
        const auto max_duration=FloatingMilliseconds((ii::runtime::Clock::duration::max)()).count();
        const auto available=FloatingMilliseconds((ii::runtime::Clock::time_point::max)().time_since_epoch()).count()-
            FloatingMilliseconds(state_->origin.time_since_epoch()).count();
        if(static_cast<long double>(*deadline)>=(std::min)(max_duration,available)-1)
            return ii::core::fail(ii::core::ErrorCode::out_of_range,"Tooltip deadline is out of range");
        try {
            const auto delay=std::chrono::duration_cast<ii::runtime::Clock::duration>(std::chrono::milliseconds(*deadline));
            std::weak_ptr<State> weak=state_;auto task=state_->sender.at(state_->origin+delay,
                [weak,generation]{deliver(weak,generation);});
            if(!task){const auto code=task.error==ii::runtime::SubmitError::stopped?ii::core::ErrorCode::closed:
                task.error==ii::runtime::SubmitError::invalid?ii::core::ErrorCode::invalid_argument:ii::core::ErrorCode::full;
                return ii::core::fail(code,"Tooltip deadline was not admitted");}
            (void)state_->token.cancel();state_->token=task.token;state_->generation=generation;return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip deadline scheduling failed",std::current_exception());}
    }
    [[nodiscard]] std::function<ii::core::Result<void>(std::optional<std::int64_t>)> weak_schedule()const {
        std::weak_ptr<State> weak=state_;
        return [weak](std::optional<std::int64_t> deadline)->ii::core::Result<void>{
            auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Tooltip scheduler owner expired");
            return TooltipScheduler(std::move(state)).schedule(deadline);
        };
    }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> last_failure()const {
        if(auto valid=ready(state_);!valid)return std::unexpected(valid.error());
        if(state_->foreign_delivery.exchange(false,std::memory_order_relaxed))
            return std::optional{ii::core::Error{ii::core::ErrorCode::wrong_executor,"Tooltip wake arrived on another thread"}};
        return state_->failure;
    }
    [[nodiscard]] ii::core::Result<void> retire()const {
        if(auto valid=ready(state_);!valid)return valid;
        state_->retired=true;(void)state_->token.cancel();state_->token={};state_->wake={};return {};
    }
};
} // namespace ii::gui
