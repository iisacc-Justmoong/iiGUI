#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <expected>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace ii::gui {

enum class WidgetMotionCurve { linear, out_back, out_cubic, css_ease_out };

// A value supplied by an application, not a dependency on a host scheduler.
struct WidgetMotion final {
    std::chrono::milliseconds duration{360};
    WidgetMotionCurve curve = WidgetMotionCurve::out_back;
    double overshoot = 1.45;
    bool reduced_motion = false;
    friend bool operator==(const WidgetMotion&, const WidgetMotion&) = default;
};

[[nodiscard]] inline bool valid_widget_motion(const WidgetMotion& value) noexcept {
    return value.duration >= std::chrono::milliseconds::zero() &&
           value.duration <= std::chrono::hours(24) &&
           (value.curve == WidgetMotionCurve::linear || value.curve == WidgetMotionCurve::out_back ||
            value.curve == WidgetMotionCurve::out_cubic || value.curve == WidgetMotionCurve::css_ease_out) &&
           std::isfinite(value.overshoot) && value.overshoot >= 0 && value.overshoot <= 10;
}

[[nodiscard]] inline WidgetMotion widget_motion_value(const WidgetMotion& value) noexcept { return value; }
template<class T> requires std::same_as<std::remove_const_t<T>, WidgetMotion>
[[nodiscard]] WidgetMotion widget_motion_value(const std::shared_ptr<T>& value) noexcept {
    return value ? *value : WidgetMotion{};
}
template<class T> requires std::same_as<std::remove_const_t<T>, WidgetMotion>
[[nodiscard]] WidgetMotion widget_motion_value(const std::weak_ptr<T>& value) noexcept {
    return widget_motion_value(value.lock());
}

// Applications may supply a structurally equivalent policy without iiGUI
// depending on the module that defines it. The enum is a validated contract:
// linear, out-back, out-cubic, and ease-out are 0 through 3 respectively.
template<class T> requires requires(const T& value) {
    value.duration; value.curve; value.overshoot; value.reduced_motion;
}
[[nodiscard]] WidgetMotion widget_motion_value(const T& value) noexcept {
    return {value.duration, static_cast<WidgetMotionCurve>(value.curve),
            value.overshoot, value.reduced_motion};
}
template<class T> requires (!std::same_as<std::remove_const_t<T>, WidgetMotion>)
[[nodiscard]] WidgetMotion widget_motion_value(const std::shared_ptr<T>& value) noexcept {
    return value ? widget_motion_value(*value) : WidgetMotion{};
}
template<class T> requires (!std::same_as<std::remove_const_t<T>, WidgetMotion>)
[[nodiscard]] WidgetMotion widget_motion_value(const std::weak_ptr<T>& value) noexcept {
    return widget_motion_value(value.lock());
}

enum class WidgetMotionError { invalid_value, invalid_policy };

// Retained interpolation with an application-supplied monotonic frame time.
// It owns no timer, widget, callback or external SDK object.
template<std::size_t N> class WidgetMotionTransition final {
    static_assert(N > 0);
public:
    using TimePoint = std::chrono::steady_clock::time_point;
    using Value = std::array<double, N>;

    explicit WidgetMotionTransition(Value initial = {}) : value_(initial), from_(initial), target_(initial) {
        if (!valid_value(initial)) throw std::invalid_argument("Invalid motion initial value");
    }

    [[nodiscard]] const Value& value() const noexcept { return value_; }
    [[nodiscard]] const Value& target() const noexcept { return target_; }
    [[nodiscard]] bool active() const noexcept { return active_; }

    // Invalid candidates leave both the visible value and the active motion intact.
    [[nodiscard]] std::expected<void, WidgetMotionError> retarget(Value target, WidgetMotion policy,
                                                           bool enabled, TimePoint now) noexcept {
        if (!valid_value(target)) return std::unexpected(WidgetMotionError::invalid_value);
        if (!valid_widget_motion(policy)) return std::unexpected(WidgetMotionError::invalid_policy);
        (void)advance(now);
        if (target == target_ && policy == policy_ && enabled == enabled_) return {};
        from_ = value_;
        target_ = target;
        policy_ = policy;
        enabled_ = enabled;
        started_ = last_time_;
        active_ = enabled && !policy.reduced_motion && policy.duration.count() > 0 && from_ != target_;
        if (!active_) value_ = target_;
        return {};
    }

    // Returns whether another frame is required. Earlier timestamps never undo
    // an already-observed frame, including after a retarget.
    [[nodiscard]] bool advance(TimePoint now) noexcept {
        if (!observed_ || now > last_time_) last_time_ = now;
        observed_ = true;
        if (!active_) return false;
        using ClockDuration = typename TimePoint::duration;
        using ClockTicks = std::chrono::duration<long double, typename ClockDuration::period>;
        // Subtract counts before converting to seconds: subtracting two rounded
        // fractional seconds can leave the exact final frame just below 1.
        const auto elapsed = static_cast<long double>(last_time_.time_since_epoch().count()) -
                             static_cast<long double>(started_.time_since_epoch().count());
        const auto duration = ClockTicks(policy_.duration).count();
        const double t = static_cast<double>(std::clamp(elapsed / duration, 0.L, 1.L));
        if (t >= 1) { value_ = target_; active_ = false; return false; }
        const auto u = t - 1;
        double factor;
        if (policy_.curve == WidgetMotionCurve::css_ease_out) {
            // CSS ease-out is cubic-bezier(0,0,.58,1). Its x coordinate is
            // time, so invert x before evaluating y; the parameter is not t.
            double low=0,high=1;
            for (int iteration=0;iteration<40;++iteration) {
                const double parameter=(low+high)/2;
                const double x=3*(1-parameter)*parameter*parameter*.58+parameter*parameter*parameter;
                if (x<t) low=parameter; else high=parameter;
            }
            const double parameter=(low+high)/2;
            factor=3*(1-parameter)*parameter*parameter+parameter*parameter*parameter;
        } else if (policy_.curve == WidgetMotionCurve::linear) factor=t;
        else if (policy_.curve == WidgetMotionCurve::out_cubic) factor=1+u*u*u;
        else factor=1+(policy_.overshoot+1)*u*u*u+policy_.overshoot*u*u;
        for (std::size_t i = 0; i < N; ++i) value_[i] = from_[i] + (target_[i] - from_[i]) * factor;
        return true;
    }

    void settle() noexcept { value_ = from_ = target_; active_ = false; }
    [[nodiscard]] std::expected<void, WidgetMotionError> reset(Value value) noexcept {
        if (!valid_value(value)) return std::unexpected(WidgetMotionError::invalid_value);
        value_ = from_ = target_ = value;
        active_ = false;
        return {};
    }

private:
    static bool valid_value(const Value& value) noexcept {
        for (double channel : value) if (!std::isfinite(channel) || std::abs(channel) > 1e12) return false;
        return true;
    }
    Value value_, from_, target_;
    WidgetMotion policy_;
    TimePoint started_{}, last_time_{};
    bool enabled_ = true, active_ = false, observed_ = false;
};

} // namespace ii::gui
