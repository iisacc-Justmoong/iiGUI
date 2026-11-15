#pragma once

#include <iiGUI/WidgetMotion.h>
#include <iiGUI/ProgressDrawing.h>
#include <algorithm>
#include <cmath>
#include <concepts>
#include <expected>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace ii::gui {

// Backend-independent state and drawing geometry. All values are logical units;
// the embedding application is responsible for submitting the drawing to its host.
struct ProgressValues final {
    double minimum = 0;
    double maximum = 100;
    double start = 0;
    double current = 0;
    friend bool operator==(const ProgressValues&, const ProgressValues&) = default;
};

// A state provider is an application-owned input, not a dependency on a
// particular model implementation. Observable adapters can publish snapshots.
class ProgressStateSource {
public:
    virtual ~ProgressStateSource() = default;
    [[nodiscard]] virtual std::optional<double> number(std::string_view key) const = 0;
};

struct ProgressState final : ProgressStateSource {
    std::map<std::string, double, std::less<>> numbers;

    [[nodiscard]] std::optional<double> number(std::string_view key) const override {
        const auto found = numbers.find(key);
        return found == numbers.end() ? std::nullopt : std::optional{found->second};
    }
    friend bool operator==(const ProgressState& a, const ProgressState& b) {
        return a.numbers == b.numbers;
    }
};

struct ProgressStateKeys final {
    std::string minimum = "minimumValue", maximum = "maximumValue";
    std::string start = "startValue", current = "currentValue";
};

inline ProgressValues progress_values(ProgressValues fallback, const ProgressStateKeys& keys,
                                      const ProgressStateSource& source) {
    const auto read = [&](const std::string& key, std::string_view default_key, double value) {
        const auto first = key.find_first_not_of(" \t\r\n");
        const auto last = key.find_last_not_of(" \t\r\n");
        const auto normalized = first == std::string::npos ? default_key
            : std::string_view(key).substr(first, last - first + 1);
        return source.number(normalized).value_or(value);
    };
    return {read(keys.minimum, "minimumValue", fallback.minimum),
            read(keys.maximum, "maximumValue", fallback.maximum),
            read(keys.start, "startValue", fallback.start),
            read(keys.current, "currentValue", fallback.current)};
}

inline ProgressValues progress_values(ProgressValues fallback, const ProgressStateKeys& keys,
                                      const std::optional<ProgressState>& source) {
    return source ? progress_values(fallback, keys, *source) : fallback;
}

template<class T> requires std::derived_from<T, ProgressStateSource>
ProgressValues progress_values(ProgressValues fallback, const ProgressStateKeys& keys,
                               const std::shared_ptr<T>& source) {
    return source ? progress_values(fallback, keys, *source) : fallback;
}

template<class T> requires std::derived_from<T, ProgressStateSource>
ProgressValues progress_values(ProgressValues fallback, const ProgressStateKeys& keys,
                               const std::weak_ptr<T>& source) {
    return progress_values(fallback, keys, source.lock());
}

enum class ProgressSize { large, regular };
enum class ProgressShape { round_rect, cylinder };

inline ProgressSize progress_size(ProgressSize value) {
    if (value != ProgressSize::large && value != ProgressSize::regular)
        throw std::invalid_argument("Invalid progress size");
    return value;
}
template<std::integral T> requires (!std::same_as<std::remove_cv_t<T>, bool>)
ProgressSize progress_size(T value) {
    if (value == 0) return ProgressSize::large;
    if (value == 1) return ProgressSize::regular;
    throw std::invalid_argument("Invalid progress size");
}
inline ProgressShape progress_shape(ProgressShape value) {
    if (value != ProgressShape::round_rect && value != ProgressShape::cylinder)
        throw std::invalid_argument("Invalid progress shape");
    return value;
}
template<std::integral T> requires (!std::same_as<std::remove_cv_t<T>, bool>)
ProgressShape progress_shape(T value) {
    if (value == 0) return ProgressShape::round_rect;
    if (value == 1) return ProgressShape::cylinder;
    throw std::invalid_argument("Invalid progress shape");
}

struct ProgressStyle final {
    ProgressSize size = ProgressSize::large;
    ProgressShape shape = ProgressShape::round_rect;
    double width = 100;
    std::optional<double> height;
    double large_height = 6;
    double regular_height = 3;
    double corner_radius = 100;
    WidgetColor track{0, 0, 0, 13.0 / 255};
    WidgetColor fill{0, 0, 0, 1};
    WidgetColor outline{0, 0, 0, 20.0 / 255};
    std::optional<double> displayed_start;
    std::optional<double> displayed_progress;
    friend bool operator==(const ProgressStyle&, const ProgressStyle&) = default;
};

struct ProgressConfiguration final {
    ProgressValues values;
    ProgressStyle style;
    bool motion_enabled = true;
    WidgetMotion motion;
    friend bool operator==(const ProgressConfiguration&, const ProgressConfiguration&) = default;
};

struct ProgressExtent final {
    double width, height;
    friend bool operator==(const ProgressExtent&, const ProgressExtent&) = default;
};

struct ProgressGeometry final {
    double width = 0;
    double height = 0;
    double fill_x = 0;
    double fill_width = 0;
    double track_radius = 0;
    double fill_radius = 0;
    double stroke_width = 0;
    friend bool operator==(const ProgressGeometry&, const ProgressGeometry&) = default;
};

enum class ProgressError { invalid_value, invalid_style, invalid_extent, invalid_widget_motion };

class ProgressModel final {
public:
    [[nodiscard]] const ProgressValues& values() const noexcept { return configuration_.values; }
    [[nodiscard]] const ProgressConfiguration& configuration() const noexcept { return configuration_; }
    // Accessibility uses the target value rather than the animation frame.
    [[nodiscard]] double normalized_current() const noexcept {
        return normalize(configuration_.values, configuration_.values.current);
    }
    [[nodiscard]] ProgressExtent extent() const noexcept {
        const auto& style = configuration_.style;
        return {style.width, style.height.value_or(style.size == ProgressSize::regular
                                                       ? style.regular_height : style.large_height)};
    }

    [[nodiscard]] static std::expected<void, ProgressError> validate(const ProgressConfiguration& candidate) noexcept {
        constexpr double value_limit = 1e12;
        const auto valid_value = [](double v) { return std::isfinite(v) && std::abs(v) <= value_limit; };
        const auto& values = candidate.values;
        if (!valid_value(values.minimum) || !valid_value(values.maximum) ||
            !valid_value(values.start) || !valid_value(values.current))
            return std::unexpected(ProgressError::invalid_value);
        const auto& style = candidate.style;
        const auto metric = [](double value, double minimum) {
            return std::isfinite(value) && value >= minimum && value <= 8192;
        };
        const auto color = [](WidgetColor c) {
            const auto channel = [](double n) { return std::isfinite(n) && n >= 0 && n <= 1; };
            return channel(c.red) && channel(c.green) && channel(c.blue) && channel(c.alpha);
        };
        if ((style.size != ProgressSize::large && style.size != ProgressSize::regular) ||
            (style.shape != ProgressShape::round_rect && style.shape != ProgressShape::cylinder) ||
            !metric(style.width, 1) || (style.height && !metric(*style.height, 1)) ||
            !metric(style.large_height, 0) || !metric(style.regular_height, 0) ||
            !metric(style.corner_radius, 0) || !color(style.track) || !color(style.fill) || !color(style.outline) ||
            (style.displayed_start && !valid_value(*style.displayed_start)) ||
            (style.displayed_progress && !valid_value(*style.displayed_progress)))
            return std::unexpected(ProgressError::invalid_style);
        if (!valid_widget_motion(candidate.motion)) return std::unexpected(ProgressError::invalid_widget_motion);
        return {};
    }

    [[nodiscard]] std::expected<void, ProgressError> set_configuration(ProgressConfiguration candidate) noexcept {
        if (auto valid = validate(candidate); !valid) return std::unexpected(valid.error());
        (void)displayed_.reset(target(candidate));
        configuration_ = candidate;
        return {};
    }

    // The host supplies frame timestamps. Configuration and target validation
    // happen before any visible or model state changes.
    [[nodiscard]] std::expected<void, ProgressError> set_animated_configuration(
            ProgressConfiguration candidate, WidgetMotionTransition<2>::TimePoint now) noexcept {
        if (auto valid = validate(candidate); !valid) return std::unexpected(valid.error());
        if (!displayed_.retarget(target(candidate), candidate.motion, candidate.motion_enabled, now))
            return std::unexpected(ProgressError::invalid_widget_motion);
        configuration_ = candidate;
        return {};
    }
    [[nodiscard]] bool advance(WidgetMotionTransition<2>::TimePoint now) noexcept { return displayed_.advance(now); }
    [[nodiscard]] bool animating() const noexcept { return displayed_.active(); }
    void finish_animation() noexcept { displayed_.settle(); }

    // Validate the whole candidate before committing any part of it.
    [[nodiscard]] std::expected<void, ProgressError> set_values(ProgressValues value) noexcept {
        auto candidate = configuration_;
        candidate.values = value;
        return set_configuration(candidate);
    }

    [[nodiscard]] std::expected<void, ProgressError> set_current(double current) noexcept {
        auto next = configuration_.values;
        next.current = current;
        return set_values(next);
    }

    [[nodiscard]] std::expected<ProgressGeometry, ProgressError> geometry(double width, double height) const noexcept {
        if (!std::isfinite(width) || !std::isfinite(height) || width < 0 || height < 0 ||
            width > 32767 || height > 32767)
            return std::unexpected(ProgressError::invalid_extent);
        const auto& style = configuration_.style;
        const auto fill_start = std::clamp(displayed_.value()[0], 0.0, 1.0);
        const auto fill_fraction = std::clamp(displayed_.value()[1], 0.0, 1.0 - fill_start);
        const auto fill_width = fill_fraction * width;
        const auto radius = [&](double available_width) {
            const auto maximum = (std::min)(available_width, height) / 2;
            return style.shape == ProgressShape::cylinder ? maximum : (std::min)(style.corner_radius, maximum);
        };
        return ProgressGeometry{
            .width = width,
            .height = height,
            .fill_x = fill_start * width,
            .fill_width = fill_width,
            .track_radius = radius(width),
            .fill_radius = radius(fill_width),
            .stroke_width = (std::min)({1.0, width / 2, height / 2})
        };
    }

private:
    [[nodiscard]] static double normalize(const ProgressValues& values, double value) noexcept {
        const auto span = values.maximum - values.minimum;
        if (std::abs(span) < .000001) return value >= values.maximum ? 1.0 : 0.0;
        return std::clamp((value - values.minimum) / span, 0.0, 1.0);
    }
    [[nodiscard]] static WidgetMotionTransition<2>::Value target(const ProgressConfiguration& config) noexcept {
        const auto start = normalize(config.values, config.values.start);
        const auto current = normalize(config.values, config.values.current);
        const auto fill_start = std::clamp(config.style.displayed_start.value_or((std::min)(start, current)), 0.0, 1.0);
        const auto fill_fraction = std::clamp(config.style.displayed_progress.value_or(std::abs(current-start)),
                                              0.0, 1.0-fill_start);
        return {fill_start, fill_fraction};
    }
    ProgressConfiguration configuration_;
    WidgetMotionTransition<2> displayed_;
};

// Convert an application's encoded-sRGB color without linking its module.
template<class T> requires requires(const T& value) {
    value.red; value.green; value.blue; value.alpha;
}
[[nodiscard]] inline WidgetColor progress_color(const T& value) noexcept {
    return {value.red, value.green, value.blue, value.alpha};
}

// Owns progress presentation and schedules frames only while motion is active.
// Widget is a weak generation handle; neither handler keeps its tree alive.
class ProgressIndicator final {
public:
    [[nodiscard]] static core::Result<ProgressIndicator> create(
            WidgetBuilder& widgets, Widget parent, std::string_view id,
            ProgressConfiguration configuration, bool allow_animation = true) {
        auto state = std::make_shared<ProgressModel>();
        if (!state->set_configuration(configuration))
            return core::fail(core::ErrorCode::invalid_argument, "Invalid progress configuration");
        const auto extent = state->extent();
        auto widget = widgets.absolute_stack(parent, id, {extent.width, extent.height});
        if (!widget) return std::unexpected(widget.error());
        ProgressIndicator result(*widget, std::move(state), allow_animation);
        if (auto installed = result.install(); !installed) return std::unexpected(installed.error());
        return result;
    }

    [[nodiscard]] Widget root() const noexcept { return widget_; }

    [[nodiscard]] core::Result<void> set_configuration(const ProgressConfiguration& value) const {
        // Even an unchanged value must honor the widget's thread and generation.
        if (auto current = widget_.snapshot(); !current) return std::unexpected(current.error());
        if (!ProgressModel::validate(value))
            return core::fail(core::ErrorCode::invalid_argument, "Invalid progress configuration");
        if (state_->configuration() == value) return {};
        const auto previous_extent = state_->extent();
        ProgressModel next;
        (void)next.set_configuration(value);
        const auto extent = next.extent();
        if (extent != previous_extent) {
            if (auto resized = widget_.set_extent({extent.width, extent.height}); !resized) return resized;
        }
        const auto previous_current = state_->normalized_current();
        if (allow_animation_) {
            if (!state_->set_animated_configuration(value, runtime::Clock::now()))
                return core::fail(core::ErrorCode::invalid_argument, "Invalid progress motion");
        } else if (!state_->set_configuration(value)) {
            return core::fail(core::ErrorCode::invalid_argument, "Invalid progress configuration");
        }
        if (previous_current != state_->normalized_current()) {
            if (auto accessible = publish_accessibility(); !accessible) return accessible;
        }
        if (allow_animation_) {
            if (state_->animating()) {
                if (auto started = widget_.request_frames(); !started) {
                    state_->finish_animation();
                    (void)widget_.set_frame_handler(frame_handler(), false);
                    (void)widget_.request_repaint();
                    return started;
                }
            } else if (auto stopped = widget_.set_frame_handler(frame_handler(), false); !stopped) {
                return stopped;
            }
        }
        return widget_.request_repaint();
    }

private:
    ProgressIndicator(Widget widget, std::shared_ptr<ProgressModel> state, bool allow_animation)
        : widget_(std::move(widget)), state_(std::move(state)), allow_animation_(allow_animation) {}

    [[nodiscard]] core::Result<void> publish_accessibility() const {
        return widget_.set_accessibility_semantics(AccessibilitySemantics{
            .role = AccessibilityRole::progress, .name = "Progress",
            .range = AccessibilityRange{0, 1, state_->normalized_current()}});
    }

    [[nodiscard]] WidgetFrameHandler frame_handler() const {
        return [target = widget_, state = state_](runtime::Clock::time_point now) -> core::Result<bool> {
            const bool active = state->advance(now);
            if (auto painted = target.request_repaint(); !painted) return std::unexpected(painted.error());
            return active;
        };
    }

    [[nodiscard]] core::Result<void> install() const {
        if (auto accessible = publish_accessibility(); !accessible) return accessible;
        if (auto installed = widget_.set_paint_handler(
                [state = state_](const WidgetPaintState& paint) -> core::Result<std::shared_ptr<const WidgetDrawing>> {
                    auto geometry = state->geometry(paint.size.width, paint.size.height);
                    if (!geometry) return core::fail(core::ErrorCode::invalid_argument, "Invalid progress extent");
                    const auto& style = state->configuration().style;
                    return progress_drawing({.width = geometry->width, .height = geometry->height,
                        .fill_x = geometry->fill_x, .fill_width = geometry->fill_width,
                        .track_radius = geometry->track_radius, .fill_radius = geometry->fill_radius,
                        .stroke_width = geometry->stroke_width,
                        .track = style.track, .fill = style.fill, .outline = style.outline});
                }); !installed) return installed;
        return allow_animation_ ? widget_.set_frame_handler(frame_handler(), false) : core::Result<void>{};
    }

    Widget widget_;
    std::shared_ptr<ProgressModel> state_;
    bool allow_animation_;
};

} // namespace ii::gui
