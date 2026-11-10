#pragma once

#include <iiGUI/NativeWidgets.h>
#include <iiGUI/SrgbColor.h>
#include <algorithm>
#include <cmath>
#include <exception>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

namespace ii::gui {
// Encoded sRGB values and logical-pixel geometry are supplied by the application.
struct PanelColors final {
    std::optional<WidgetColor> background, border;
    double width = 0, radius = 0;
    friend bool operator==(const PanelColors&, const PanelColors&) = default;
};

// Retains only the most recent drawing; a color change or resize invalidates it.
class PanelPainter final {
public:
    [[nodiscard]] static core::Result<void> validate(const PanelColors& value) {
        const auto channel = [](double n) { return std::isfinite(n) && n >= 0 && n <= 1; };
        const auto color = [&](const std::optional<WidgetColor>& c) {
            return !c || (channel(c->red) && channel(c->green) && channel(c->blue) && channel(c->alpha));
        };
        if (!color(value.background) || !color(value.border) || !std::isfinite(value.width) ||
            !std::isfinite(value.radius) || value.width < 0 || value.width > 8192 ||
            value.radius < 0 || value.radius > 8192 || (value.width > 0 && !value.border))
            return core::fail(core::ErrorCode::invalid_argument, "Invalid panel colors or geometry");
        return {};
    }

    [[nodiscard]] core::Result<void> set_colors(const PanelColors& value) {
        if (owner_ != std::this_thread::get_id())
            return core::fail(core::ErrorCode::wrong_executor, "Panel painter requires its owner thread");
        if (auto valid = validate(value); !valid) return valid;
        if (value_ != value) { value_ = value; cached_.reset(); }
        return {};
    }

    [[nodiscard]] core::Result<std::shared_ptr<const WidgetDrawing>> operator()(const WidgetPaintState& state) {
        if (owner_ != std::this_thread::get_id())
            return core::fail(core::ErrorCode::wrong_executor, "Panel painter requires its owner thread");
        if (!std::isfinite(state.size.width) || !std::isfinite(state.size.height) || state.size.width < 0 ||
            state.size.height < 0 || state.size.width > 32767 || state.size.height > 32767)
            return core::fail(core::ErrorCode::invalid_argument, "Invalid panel paint extent");
        if (cached_ && size_ == state.size) return cached_;
        try {
            auto drawing = std::make_shared<WidgetDrawing>();
            const Rect bounds{0, 0, static_cast<float>(state.size.width), static_cast<float>(state.size.height)};
            if (value_.background) {
                const auto color = srgb_to_linear(*value_.background);
                drawing->panels.push_back({bounds, color, color, static_cast<float>(value_.radius)});
            }
            if (value_.border && value_.width > 0) {
                const auto color = srgb_to_linear(*value_.border);
                drawing->panels.push_back({bounds, color, color, static_cast<float>(value_.radius), static_cast<float>(value_.width)});
            }
            size_ = state.size; cached_ = std::move(drawing); return cached_;
        } catch (...) {
            return core::fail(core::ErrorCode::failed, "Panel drawing failed", std::current_exception());
        }
    }
private:
    const std::thread::id owner_ = std::this_thread::get_id();
    PanelColors value_;
    Size size_;
    std::shared_ptr<const WidgetDrawing> cached_;
};

[[nodiscard]] inline core::Result<void> install_panel_paint(const Widget& target, const PanelColors& value) {
    try {
        auto painter = std::make_shared<PanelPainter>();
        if (auto configured = painter->set_colors(value); !configured) return configured;
        return target.set_paint_handler([painter](const WidgetPaintState& state) { return (*painter)(state); });
    } catch (...) {
        return core::fail(core::ErrorCode::failed, "Panel paint installation failed", std::current_exception());
    }
}
} // namespace ii::gui
