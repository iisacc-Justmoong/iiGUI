#pragma once
#include <cstddef>
#include <functional>
#include <utility>

namespace ii::gui::detail {
// Hold the primary close observer until all nested native callbacks have
// returned; callers deliver the returned task outside the outermost callback.
class HostCallbackGate final {
public:
    using Task = std::function<void()>;
    void enter() noexcept { ++depth_; }
    [[nodiscard]] bool defer(Task& callback) noexcept {
        if (!depth_ || !callback) return false;
        if (!pending_) pending_=std::move(callback);
        else callback={};
        return true;
    }
    [[nodiscard]] Task leave() noexcept {
        if (!depth_) return {};
        if (--depth_) return {};
        return std::move(pending_);
    }
private:
    std::size_t depth_=0;
    Task pending_;
};
// A callback may close its host and clear the original callback storage.
inline void deliver_host_opened(std::function<void()> opened) {
    if (opened) opened();
}
// A child may be released by its close observer. Submit that observer to the
// parent pump, so native destruction returns before the child can disappear.
template<class Submit>
void defer_host_closed(std::function<void()> closed, Submit&& submit) {
    if (closed) std::forward<Submit>(submit)(std::move(closed));
}
// Return admission failure to the owner so it can stop rather than silently
// lose a child generation's close notification.
template<class Submit>
[[nodiscard]] bool try_defer_host_closed(std::function<void()> closed, Submit&& submit) {
    return !closed || bool(std::forward<Submit>(submit)(std::move(closed)));
}
} // namespace ii::gui::detail
