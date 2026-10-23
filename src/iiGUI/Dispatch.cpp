#include "DispatchState.h"

namespace ii::gui {
namespace {
template<class Submit>
runtime::Submission submit(const std::weak_ptr<detail::DispatchState>& endpoint, Submit&& work) {
    auto state = endpoint.lock();
    if (!state || !state->accepting.load(std::memory_order_acquire))
        return {{}, runtime::SubmitError::stopped};
    auto producer=state->loop.sender();
    return state->track(work(producer));
}
template<class Callback>auto guarded(std::weak_ptr<detail::DispatchState> endpoint,Callback callback){
    return [endpoint,callback=std::move(callback)](auto&&... arguments)mutable{
        auto state=endpoint.lock();if(!state||!state->accepting.load(std::memory_order_acquire))return;
        (void)state->tasks.run([&]{std::invoke(callback,std::forward<decltype(arguments)>(arguments)...);});
    };
}
}
runtime::Submission UiExecutor::post(runtime::Task task, runtime::Priority priority) const {
    if(!task)return {{},runtime::SubmitError::invalid};
    return submit(state_, [&](auto& loop) { return loop.post(guarded(state_,std::move(task)), priority); });
}
runtime::Submission UiExecutor::at(runtime::Clock::time_point time, runtime::Task task,
                                  runtime::Priority priority) const {
    if(!task)return {{},runtime::SubmitError::invalid};
    return submit(state_, [&](auto& loop) { return loop.at(time, guarded(state_,std::move(task)), priority); });
}
runtime::Submission UiExecutor::after(runtime::Clock::duration delay, runtime::Task task,
                                     runtime::Priority priority) const {
    if(!task)return {{},runtime::SubmitError::invalid};
    return submit(state_, [&](auto& loop) { return loop.after(delay, guarded(state_,std::move(task)), priority); });
}
runtime::Submission UiExecutor::every(runtime::Clock::duration interval, runtime::Task task,
                                     runtime::Priority priority) const {
    if(!task)return {{},runtime::SubmitError::invalid};
    return submit(state_, [&](auto& loop) { return loop.every(interval, guarded(state_,std::move(task)), priority); });
}
bool UiExecutor::cancel(runtime::TaskToken token) const {
    auto state = state_.lock();
    if (!state) return false;
    return state->loop.cancel(token);
}
runtime::Submission UiExecutor::watch(io::Channel channel,io::Interest interest,
    runtime::IOReady ready,runtime::IOWatchOptions options) const {
    if(!ready)return {{},runtime::SubmitError::invalid};
    return submit(state_,[&](auto& loop) { return loop.watch(std::move(channel),interest,guarded(state_,std::move(ready)),std::move(options)); });
}
runtime::Submission UiExecutor::read(io::Channel channel,io::Read request,
    runtime::IOCompletion completion,runtime::Priority priority) const {
    if(!completion)return {{},runtime::SubmitError::invalid};
    return submit(state_,[&](auto& loop) { return loop.read(std::move(channel),std::move(request),guarded(state_,std::move(completion)),priority); });
}
runtime::Submission UiExecutor::write(io::Channel channel,io::Write request,
    runtime::IOCompletion completion,runtime::Priority priority) const {
    if(!completion)return {{},runtime::SubmitError::invalid};
    return submit(state_,[&](auto& loop) { return loop.write(std::move(channel),std::move(request),guarded(state_,std::move(completion)),priority); });
}
} // namespace ii::gui
