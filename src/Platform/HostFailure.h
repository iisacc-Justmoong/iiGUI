#pragma once

#include <iiSDKCore/Core/Result/Result.h>
#include <functional>
#include <optional>
#include <utility>

namespace ii::gui::detail {

// Owner-thread first-error storage. Notification cannot replace the original
// error, including when a handler clears itself, reenters, or throws.
class HostFailure final {
public:
    using Handler = std::function<void(const core::Error&)>;
    HostFailure() = default;
    HostFailure(const HostFailure&) = delete;
    HostFailure& operator=(const HostFailure&) = delete;

    void set_handler(Handler handler) { handler_ = std::move(handler); }
    [[nodiscard]] Handler handler_snapshot() const { return handler_; }
    HostFailure& operator=(core::Error error) {
        if (first_) return *this;
        first_ = std::move(error);
        try {
            auto handler = handler_;
            if (handler) {
                auto snapshot = *first_;
                handler(snapshot);
            }
        } catch (...) {
            // Handler failures must not replace the original host failure.
        }
        return *this;
    }
    [[nodiscard]] explicit operator bool() const noexcept { return first_.has_value(); }
    [[nodiscard]] const core::Error& operator*() const { return *first_; }
    [[nodiscard]] const core::Error* operator->() const { return &*first_; }

private:
    std::optional<core::Error> first_;
    Handler handler_;
};

} // namespace ii::gui::detail
