#pragma once
#include <cstddef>
#include <utility>

namespace ii::gui::detail {
// A foreign event loop may be nested inside one of our native callbacks.
// Retain close until the outermost callback returns, without reposting while
// that callback is still active.
class ExternalCloseBarrier final {
public:
    void enter() noexcept { ++depth_; }
    [[nodiscard]] bool admit() noexcept {
        if(!depth_) return true;
        deferred_=true;
        return false;
    }
    [[nodiscard]] bool leave() noexcept {
        if(!depth_ || --depth_) return false;
        return std::exchange(deferred_,false);
    }
private:
    std::size_t depth_=0;
    bool deferred_=false;
};
} // namespace ii::gui::detail
