#pragma once
#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <utility>

namespace ii::gui::detail {
// One owner-thread relay. Pre-open transitions coalesce; later transitions are
// ordered and bounded. Unknown initial state is never replaced with a guess.
class HostActivity final {
public:
    static constexpr std::size_t capacity = 64;
    void set_handler(std::function<void(bool)> handler) { handler_ = std::move(handler); }
    [[nodiscard]] bool has_handler() const noexcept { return bool(handler_); }
    void observe(bool value) noexcept {
        if (closed_ || overflow_) return;
        if (!opened_) { initial_ = value; return; }
        const auto previous = size_ ? std::optional<bool>{queue_[(head_+size_-1)%capacity]} : delivered_;
        if (previous && *previous==value) return;
        if (size_==capacity) { overflow_ = true; return; }
        queue_[(head_+size_)%capacity] = value;
        ++size_;
    }
    void open() noexcept {
        if (closed_ || opened_) return;
        opened_ = true;
        if (auto initial=std::exchange(initial_,std::nullopt)) observe(*initial);
    }
    [[nodiscard]] std::optional<bool> take() noexcept {
        if (!opened_ || closed_ || !size_) return {};
        const bool value=queue_[head_];
        head_=(head_+1)%capacity;
        --size_;
        delivered_ = value;
        return value;
    }
    void deliver() {
        if (delivering_ || closed_) return;
        delivering_=true;
        struct Reset final { bool& value; ~Reset(){value=false;} } reset{delivering_};
        for (std::size_t count=0; count!=capacity && !closed_; ++count) {
            if (overflow_) throw std::runtime_error("Native activity queue overflow");
            auto value=take();
            if (!value) return;
            if (handler_) { auto handler=handler_; handler(*value); }
        }
        if (!closed_ && (overflow_ || size_)) throw std::runtime_error("Native activity reentry limit exceeded");
    }
    void close() noexcept {
        closed_ = true;
        initial_.reset(); size_=0;
        handler_ = {};
    }
private:
    std::optional<bool> initial_, delivered_;
    std::array<bool,capacity> queue_{};
    std::function<void(bool)> handler_;
    std::size_t head_=0, size_=0;
    bool opened_=false, closed_=false, overflow_=false, delivering_=false;
};
} // namespace ii::gui::detail
