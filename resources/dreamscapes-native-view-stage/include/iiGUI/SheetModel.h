#pragma once

#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Text/Utf8.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace ii::gui {
[[nodiscard]] inline bool valid_sheet_text(std::string_view text) noexcept {
    return text.size() <= 65536 && text.find('\0') == std::string_view::npos && ii::core::valid_utf8(text);
}
[[nodiscard]] inline bool valid_sheet_color(WidgetColor c) noexcept {
    const auto channel = [](double v) { return std::isfinite(v) && v >= 0 && v <= 1; };
    return channel(c.red) && channel(c.green) && channel(c.blue) && channel(c.alpha);
}

enum class SheetPresentation { automatic, desktop, mobile };

struct SheetPalette final {
    WidgetColor surface{0, 0, 0, 1};
    WidgetColor backdrop{0, 0, 0, .48};
    friend bool operator==(const SheetPalette&, const SheetPalette&) = default;
};

struct SheetConfiguration final {
    std::string title, description, accessibility_name = "Sheet";
    SheetPresentation presentation = SheetPresentation::automatic;
    SheetDetent detent = SheetDetent::fit;
    std::optional<bool> show_grabber;
    std::optional<double> corner_radius;
    bool show_header = true, show_description = true, show_close_button = true;
    bool scroll_content = true, dismiss_on_background = true, dismiss_on_escape = true;
    bool dismiss_on_drag = true, motion_enabled = true;
    double preferred_width = 560, preferred_height = 420, desktop_margin = 24;
    double content_padding = 20, top_safe_inset = 0, bottom_safe_inset = 0;
    double animation_duration_ms = 420;
    SheetPalette palette;
    friend bool operator==(const SheetConfiguration&, const SheetConfiguration&) = default;
};

struct SheetRect final {
    double x = 0, y = 0, width = 0, height = 0;
    friend bool operator==(const SheetRect&, const SheetRect&) = default;
};

struct SheetLayout final {
    SheetRect frame;
    double radius = 0;
    bool mobile = false, show_grabber = false;
};

class Sheet final {
public:
    [[nodiscard]] static bool validate(const SheetConfiguration& value) noexcept {
        const auto range = [](double n, double low, double high) {
            return std::isfinite(n) && n >= low && n <= high;
        };
        if (!valid_sheet_text(value.title) || !valid_sheet_text(value.description) ||
            !valid_sheet_text(value.accessibility_name)) return false;
        if (value.presentation != SheetPresentation::automatic && value.presentation != SheetPresentation::desktop &&
            value.presentation != SheetPresentation::mobile) return false;
        if (value.detent != SheetDetent::fit && value.detent != SheetDetent::medium && value.detent != SheetDetent::large) return false;
        return range(value.preferred_width, 0, 8192) && range(value.preferred_height, 0, 8192) &&
               range(value.desktop_margin, 0, 8192) && range(value.content_padding, 0, 8192) &&
               range(value.top_safe_inset, 0, 8192) && range(value.bottom_safe_inset, 0, 8192) &&
               range(value.animation_duration_ms, 0, 86400000) &&
               (!value.corner_radius || range(*value.corner_radius, 0, 8192)) &&
               valid_sheet_color(value.palette.surface) && valid_sheet_color(value.palette.backdrop);
    }

    bool set_configuration(SheetConfiguration value) {
        if (!validate(value)) return false;
        configuration_ = std::move(value);
        return true;
    }
    [[nodiscard]] const SheetConfiguration& configuration() const noexcept { return configuration_; }

    // The host supplies intrinsic body/chrome measurements and target platform;
    // the Sheet decides its frame and does not infer mobile from window width.
    [[nodiscard]] std::optional<SheetLayout> layout(double viewport_width, double viewport_height,
                                                     double body_height, double chrome_height,
                                                     bool mobile_platform) const noexcept {
        const auto extent = [](double n, double limit) { return std::isfinite(n) && n >= 0 && n <= limit; };
        if (!extent(viewport_width, 8192) || !extent(viewport_height, 8192) ||
            !extent(body_height, 1e15) || !extent(chrome_height, 1e15)) {
            return std::nullopt;
        }
        const auto& c = configuration_;
        const bool mobile = c.presentation == SheetPresentation::mobile ||
            (c.presentation == SheetPresentation::automatic && mobile_platform);
        double width = 0, height = 0, x = 0, y = 0;
        if (mobile) {
            width = viewport_width;
            const auto maximum = (std::max)(0., viewport_height - (std::max)(24., c.top_safe_inset));
            const auto natural = body_height + chrome_height + c.bottom_safe_inset;
            const auto desired = c.detent == SheetDetent::large ? std::round(viewport_height * .9) :
                                 c.detent == SheetDetent::medium ? std::round(viewport_height * .57) : natural;
            height = (std::min)(maximum, desired);
            y = viewport_height - height;
        } else {
            width = (std::min)(c.preferred_width, (std::max)(0., viewport_width - 2 * c.desktop_margin));
            height = (std::min)((std::max)(0., viewport_height - 2 * c.desktop_margin),
                              (std::max)(chrome_height, c.preferred_height));
            x = (viewport_width - width) / 2;
            y = (viewport_height - height) / 2;
        }
        const auto radius = (std::min)(c.corner_radius.value_or(mobile ? 24. : 16.), (std::min)(width, height) / 2);
        return SheetLayout{{x, y, width, height}, radius, mobile, c.show_grabber.value_or(mobile)};
    }

private:
    SheetConfiguration configuration_;
};

enum class SheetDragResult { restore, dismiss };

// Pointer capture and animation belong to the host. This state machine owns
// the direction, bounded displacement and dismissal decision.
class SheetDrag final {
public:
    [[nodiscard]] bool active() const noexcept { return active_; }
    [[nodiscard]] double offset() const noexcept { return offset_; }
    bool begin(double y, double height, bool mobile, bool enabled) noexcept {
        if (active_ || !mobile || !enabled || !std::isfinite(y) || !std::isfinite(height) || height <= 0 || height > 8192) return false;
        start_ = y; height_ = height; offset_ = 0; active_ = true; return true;
    }
    [[nodiscard]] std::optional<double> move(double y) noexcept {
        if (!active_ || !std::isfinite(y)) return std::nullopt;
        offset_ = std::clamp(y - start_, 0., height_); return offset_;
    }
    [[nodiscard]] std::optional<SheetDragResult> end(double y) noexcept {
        if (!move(y)) return std::nullopt;
        const auto result = offset_ >= (std::min)(120., height_ * .2) ? SheetDragResult::dismiss : SheetDragResult::restore;
        active_ = false; offset_ = 0; return result;
    }
    bool cancel() noexcept {
        if (!active_) return false;
        active_ = false; offset_ = 0; return true;
    }

private:
    double start_ = 0, height_ = 0, offset_ = 0;
    bool active_ = false;
};

} // namespace ii::gui
