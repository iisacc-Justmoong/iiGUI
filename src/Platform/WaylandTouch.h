#pragma once

#include <iiGUI/NativeWidgets.h>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>

namespace ii::gui::detail {

// Wayland ids are valid only until up/cancel. WidgetState has one capture;
// another contact cancels it and no remaining contact is promoted to a press.
class WaylandTouchState final {
    struct Point { double x, y; };
    std::map<std::int32_t, Point> contacts_;
    std::optional<std::int32_t> captured_;

    static void validate(double x, double y) {
        if (!std::isfinite(x) || !std::isfinite(y))
            throw std::invalid_argument("Invalid Wayland touch coordinates");
    }
    static WidgetInput event(WidgetInputKind kind, std::int32_t id, Point point, std::uint32_t time) {
        WidgetInput value;
        value.kind = kind;
        value.device = PointerDevice::touch;
        value.device_id = std::uint32_t(id);
        value.x = point.x; value.y = point.y;
        value.timestamp = double(time) / 1000.;
        value.buttons = kind == WidgetInputKind::pointer_down || kind == WidgetInputKind::pointer_move ? 1 : 0;
        return value;
    }
public:
    std::optional<WidgetInput> down(std::int32_t id, double x, double y, std::uint32_t time) {
        validate(x, y);
        if (contacts_.contains(id)) throw std::invalid_argument("Duplicate Wayland touch id");
        if (contacts_.size() >= 32) throw std::length_error("Too many Wayland touch contacts");
        contacts_.emplace(id, Point{x, y});
        if (contacts_.size() == 1) {
            captured_ = id;
            return event(WidgetInputKind::pointer_down, id, {x, y}, time);
        }
        if (!captured_) return {};
        const auto original = *captured_;
        captured_.reset();
        return event(WidgetInputKind::pointer_cancel, original, contacts_.at(original), time);
    }
    std::optional<WidgetInput> motion(std::int32_t id, double x, double y, std::uint32_t time) {
        validate(x, y);
        const auto it = contacts_.find(id);
        if (it == contacts_.end()) return {};
        it->second = {x, y};
        if (!captured_ || *captured_ != id) return {};
        return event(WidgetInputKind::pointer_move, id, it->second, time);
    }
    std::optional<WidgetInput> up(std::int32_t id, std::uint32_t time) {
        const auto it = contacts_.find(id);
        if (it == contacts_.end()) return {};
        std::optional<WidgetInput> result;
        if (captured_ && *captured_ == id) {
            result = event(WidgetInputKind::pointer_up, id, it->second, time);
            captured_.reset();
        }
        contacts_.erase(it);
        return result;
    }
    std::optional<WidgetInput> cancel() {
        std::optional<WidgetInput> result;
        if (captured_) result = event(WidgetInputKind::pointer_cancel, *captured_, contacts_.at(*captured_), 0);
        captured_.reset();
        contacts_.clear();
        return result;
    }
};
} // namespace ii::gui::detail
