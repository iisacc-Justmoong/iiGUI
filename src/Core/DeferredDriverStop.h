#pragma once
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

namespace ii::gui::detail {
// A close observer can run a nested native loop. Do not make completion
// available to that loop until the observer has returned, even on failure.
template<class Notify, class Schedule>
void notify_closed_then_schedule(Notify&& notify, Schedule&& schedule) {
    std::exception_ptr failure;
    try { std::forward<Notify>(notify)(); }
    catch (...) { failure=std::current_exception(); }
    try { std::forward<Schedule>(schedule)(); }
    catch (...) { if (failure) std::rethrow_exception(failure); throw; }
    if (failure) std::rethrow_exception(failure);
}
// Owner-thread token for an OS-queued driver completion. Queued deliveries
// borrow the token weakly and cannot outlive the driver or repeat completion.
class DeferredDriverStop final : public std::enable_shared_from_this<DeferredDriverStop> {
public:
    using Task = std::function<void()>;
    explicit DeferredDriverStop(Task stop) : stop_(std::move(stop)) {
        if (!stop_) throw std::invalid_argument("Driver completion requires a callback");
    }
    DeferredDriverStop(const DeferredDriverStop&) = delete;
    DeferredDriverStop& operator=(const DeferredDriverStop&) = delete;
    [[nodiscard]] Task delivery() {
        auto weak=weak_from_this();
        if (weak.expired()) throw std::logic_error("Driver completion requires shared ownership");
        return [weak] { if (auto self=weak.lock()) self->deliver(); };
    }
    void cancel() noexcept { active_=false; stop_={}; }
private:
    void deliver() {
        if (!active_) return;
        active_=false;
        auto stop=std::move(stop_);
        if (stop) stop();
    }
    Task stop_;
    bool active_=true;
};
} // namespace ii::gui::detail
