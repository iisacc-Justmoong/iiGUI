#include "DispatchState.h"

namespace ii::gui {
namespace {
template<class Submit>
runtime::Submission submit(const std::weak_ptr<detail::DispatchState>& endpoint, Submit&& work) {
    auto state = endpoint.lock();
    if (!state || !state->accepting.load(std::memory_order_acquire))
        return {{}, runtime::SubmitError::stopped};
    auto producer=state->loop.sender();
    return work(producer);
}
}
runtime::Submission UiExecutor::post(runtime::Task task, runtime::Priority priority) const {
    return submit(state_, [&](auto& loop) { return loop.post(std::move(task), priority); });
}
runtime::Submission UiExecutor::at(runtime::Clock::time_point time, runtime::Task task,
                                  runtime::Priority priority) const {
    return submit(state_, [&](auto& loop) { return loop.at(time, std::move(task), priority); });
}
runtime::Submission UiExecutor::after(runtime::Clock::duration delay, runtime::Task task,
                                     runtime::Priority priority) const {
    return submit(state_, [&](auto& loop) { return loop.after(delay, std::move(task), priority); });
}
runtime::Submission UiExecutor::every(runtime::Clock::duration interval, runtime::Task task,
                                     runtime::Priority priority) const {
    return submit(state_, [&](auto& loop) { return loop.every(interval, std::move(task), priority); });
}
bool UiExecutor::cancel(runtime::TaskToken token) const {
    auto state = state_.lock();
    if (!state) return false;
    return state->loop.cancel(token);
}
runtime::Submission UiExecutor::watch(io::Channel channel,io::Interest interest,
    runtime::IOReady ready,runtime::IOWatchOptions options) const {
    return submit(state_,[&](auto& loop) { return loop.watch(std::move(channel),interest,std::move(ready),std::move(options)); });
}
runtime::Submission UiExecutor::read(io::Channel channel,io::Read request,
    runtime::IOCompletion completion,runtime::Priority priority) const {
    return submit(state_,[&](auto& loop) { return loop.read(std::move(channel),std::move(request),std::move(completion),priority); });
}
runtime::Submission UiExecutor::write(io::Channel channel,io::Write request,
    runtime::IOCompletion completion,runtime::Priority priority) const {
    return submit(state_,[&](auto& loop) { return loop.write(std::move(channel),std::move(request),std::move(completion),priority); });
}
} // namespace ii::gui
