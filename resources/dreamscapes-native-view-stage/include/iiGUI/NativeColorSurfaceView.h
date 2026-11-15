#pragma once
#include <iiGUI/ColorSurfaceModel.h>
#include <iiGUI/Image.h>
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
namespace ii::gui {
struct ColorSurfaceInput final {ColorSurfaceConfiguration configuration;std::shared_ptr<ColorSurfaceSource> source;std::int64_t source_revision=0;
 friend bool operator==(const ColorSurfaceInput&,const ColorSurfaceInput&)=default;};
struct DefaultColorSurfaceHostPolicy final {
 using Model=ColorSurfaceModel;using Input=ColorSurfaceInput;using Frame=ColorSurfaceFrame;using Kind=ColorSurfaceKind;
 using ErrorCode=ColorSurfaceErrorCode;using Color=WidgetColor;
};
template<class Policy>class BasicNativeColorSurface final {
    using ColorSurface=typename Policy::Model;using ColorSurfaceInput=typename Policy::Input;
    using ColorSurfaceFrame=typename Policy::Frame;using ColorSurfaceKind=typename Policy::Kind;
    using ColorSurfaceErrorCode=typename Policy::ErrorCode;using StyleColor=typename Policy::Color;
    struct Prepared final {
        ColorSurface model;
        ColorSurfaceFrame frame;
        std::shared_ptr<const ii::gui::RasterImage> image;
    };
    using Snapshot = std::shared_ptr<const Prepared>;
    struct State final {
        ColorSurfaceInput authored;
        Snapshot prepared;
        ii::gui::Widget root;
        std::function<void(ii::gui::WidgetColor)> edited;
        const std::thread::id owner = std::this_thread::get_id();
        std::uint64_t revision = 0;
        bool dragging = false, live = false, retired = false;
    };

    explicit BasicNativeColorSurface(std::shared_ptr<State> state) : state_(std::move(state)) {}

    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const ColorSurfaceInput& input, Snapshot previous = {}) {
        if (!ColorSurface::validate(input.configuration) || !input.source || input.source_revision < 0) {
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid LVUI color surface configuration");
        }
        try {
            const auto width = std::uint32_t(std::ceil(input.configuration.width));
            const auto height = std::uint32_t(std::ceil(input.configuration.height));
            for (unsigned attempt = 0; attempt < 4; ++attempt) {
                const auto before = input.source->revision();
                auto frame = input.source->frame(input.configuration.kind, width, height);
                const auto after = input.source->revision();
                if (before != after || (!frame && frame.error().code == ColorSurfaceErrorCode::stale)) continue;
                if (!frame) return ii::core::fail(ii::core::ErrorCode::failed, frame.error().message);
                if (frame->revision != after) continue;
                if (!ColorSurface::valid_frame(*frame) || frame->width != width || frame->height != height) {
                    return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Color source returned an invalid frame");
                }
                auto result = std::make_shared<Prepared>();
                if (!result->model.set_configuration(input.configuration)) {
                    return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid LVUI color surface configuration");
                }
                if (previous && previous->image && previous->image->width()==width && previous->image->height()==height &&
                    std::ranges::equal(previous->image->rgba(), frame->rgba)) {
                    result->image=previous->image;
                    std::vector<std::uint8_t>{}.swap(frame->rgba);
                } else {
                    auto image = ii::gui::rgba_image(width, height, std::move(frame->rgba));
                    if (!image) return std::unexpected(image.error());
                    result->image=std::move(*image);
                }
                result->frame = std::move(*frame);
                return Snapshot(std::move(result));
            }
            return ii::core::fail(ii::core::ErrorCode::busy, "Color source changed repeatedly while preparing the surface");
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Color surface preparation failed", std::current_exception());
        }
    }

    [[nodiscard]] static std::string description(StyleColor value) {
        constexpr char digits[] = "0123456789ABCDEF";
        std::string result = "#";
        for (double channel : {value.red, value.green, value.blue, value.alpha}) {
            const auto byte = static_cast<unsigned>(std::lround(channel * 255));
            result += digits[byte >> 4];
            result += digits[byte & 15];
        }
        return result;
    }

    [[nodiscard]] static ii::gui::ActionOptions action(const Snapshot& prepared) {
        const auto& config = prepared->model.configuration();
        return {.accessibility_name = config.accessibility_name, .enabled = config.enabled,
                .focus_ring = false, .width = config.width, .height = config.height,
                .accessibility_description = description(prepared->frame.color)};
    }

    [[nodiscard]] static ii::gui::AccessibilitySemantics semantics(const Snapshot& prepared) {
        const auto& config = prepared->model.configuration();
        ii::gui::AccessibilitySemantics result;
        result.name = config.accessibility_name;
        result.description = description(prepared->frame.color);
        if (config.kind == ColorSurfaceKind::hue_rail || config.kind == ColorSurfaceKind::brightness_rail) {
            result.role = ii::gui::AccessibilityRole::slider;
            result.range = ii::gui::AccessibilityRange{0, 1, 1 - prepared->frame.marker.y};
        } else if (config.kind == ColorSurfaceKind::spectrum) {
            result.role = ii::gui::AccessibilityRole::slider;
            result.range = ii::gui::AccessibilityRange{0, 1, prepared->frame.marker.x};
        }
        return result;
    }

    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> marker() {
        // The marker is a separate image so it remains visible over every
        // surface color without depending on host-specific drawing order.
        constexpr std::uint32_t side = 32;
        std::vector<std::uint8_t> rgba(side * side * 4, 0);
        for (std::uint32_t y = 0; y < side; ++y) {
            for (std::uint32_t x = 0; x < side; ++x) {
                const auto radius = std::hypot(double(x) + .5 - side / 2., double(y) + .5 - side / 2.);
                if (radius >= 15 || radius < 9) continue;
                const auto index = (std::size_t(y) * side + x) * 4;
                const auto color = radius >= 13 ? std::uint8_t{0} : std::uint8_t{255};
                rgba[index] = rgba[index + 1] = rgba[index + 2] = color;
                rgba[index + 3] = 255;
            }
        }
        return ii::gui::rgba_image(side, side, std::move(rgba));
    }

    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> shared_marker() {
        static std::mutex mutex;static std::weak_ptr<const ii::gui::RasterImage> weak;
        std::lock_guard lock(mutex);if(auto image=weak.lock())return image;
        auto image=marker();if(!image)return std::unexpected(image.error());weak=*image;return *image;
    }

    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint(
            const Snapshot& prepared, const ii::gui::WidgetPaintState& state) {
        try {
            if (!std::isfinite(state.size.width) || !std::isfinite(state.size.height) ||
                state.size.width < 0 || state.size.height < 0 ||
                state.size.width > 8192 || state.size.height > 8192) {
                return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid color surface extent");
            }
            auto drawing = std::make_shared<ii::gui::WidgetDrawing>();
            const auto width = float(state.size.width), height = float(state.size.height);
            const auto opacity = state.enabled ? 1.f : .5f;
            if (width > 0 && height > 0) {
                drawing->images.push_back({.bounds = {0, 0, width, height}, .image = prepared->image, .opacity = opacity});
                auto indicator = shared_marker();
                if (!indicator) return std::unexpected(indicator.error());
                const auto size = float((std::min)({prepared->model.configuration().marker_diameter,
                                                  state.size.width, state.size.height}));
                const auto x = float(prepared->frame.marker.x * width) - size / 2;
                const auto y = float(prepared->frame.marker.y * height) - size / 2;
                drawing->images.push_back({.bounds = {x, y, size, size}, .image = *indicator, .opacity = opacity});
            }
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Color surface paint failed", std::current_exception());
        }
    }

    [[nodiscard]] static ii::core::Result<void> replace(const std::shared_ptr<State>& state,
                                                        ColorSurfaceInput input, Snapshot next) {
        if (state->retired) return ii::core::fail(ii::core::ErrorCode::closed, "Color surface retired");
        if (state->revision == (std::numeric_limits<std::uint64_t>::max)()) {
            return ii::core::fail(ii::core::ErrorCode::out_of_range, "Color surface revision exhausted");
        }
        auto previous_input = state->authored;
        auto previous = state->prepared;
        const bool previous_dragging = state->dragging;
        const bool cancel_drag = state->authored.source != input.source ||
            state->authored.configuration.kind != input.configuration.kind || !input.configuration.enabled;
        const auto revision = ++state->revision;
        state->authored = std::move(input);
        state->prepared = std::move(next);
        if (cancel_drag) state->dragging = false;
        if (auto updated = state->root.set_action_options(action(state->prepared)); !updated) {
            if (!state->retired && state->revision == revision) {
                state->authored = std::move(previous_input);
                state->prepared = std::move(previous);
                state->dragging = previous_dragging;
            }
            return updated;
        }
        if (state->retired || state->revision != revision) return {};
        if (auto updated = state->root.set_accessibility_semantics(semantics(state->prepared)); !updated) {
            if (!state->retired && state->revision == revision) {
                state->authored = std::move(previous_input);
                state->prepared = std::move(previous);
                state->dragging = previous_dragging;
            }
            return updated;
        }
        if (state->retired || state->revision != revision) return {};
        return state->root.request_repaint();
    }

    [[nodiscard]] static ii::core::Result<bool> input(const std::shared_ptr<State>& state,
                                                       const ii::gui::WidgetInput& event) {
        using Kind = ii::gui::WidgetInputKind;
        using Key = ii::gui::WidgetKey;
        if (state->owner != std::this_thread::get_id()) {
            return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Color surface requires its owner thread");
        }
        try {
            if (state->retired || !state->live || !state->prepared) return false;
            if (event.kind == Kind::focus_lost
#if defined(IIGUI_POINTER_CANCEL_API)
                || event.kind == Kind::pointer_cancel
#endif
            ) { state->dragging = false; return false; }
            if (!state->prepared->model.configuration().enabled) return false;
            auto point = state->prepared->frame.marker;
            bool handled = false;
            if (event.kind == Kind::pointer_down ||
                (state->dragging && (event.kind == Kind::pointer_move || event.kind == Kind::pointer_up))) {
                const auto observed = state->revision;
                if (event.kind == Kind::pointer_down) {
                    if (auto focused = state->root.focus(); !focused) return std::unexpected(focused.error());
                }
                if (state->retired || state->revision != observed) return true;
                auto bounds = state->root.snapshot();
                if (!bounds) return std::unexpected(bounds.error());
                if (state->retired || state->revision != observed) return true;
                if (!event.local_position) {
                    return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Color surface pointer requires local coordinates");
                }
                auto mapped = state->prepared->model.position(event.local_position->x, event.local_position->y,
                                                               bounds->bounds.width, bounds->bounds.height);
                if (!mapped) return false;
                point = *mapped;
                state->dragging = event.kind != Kind::pointer_up;
                handled = true;
            } else {
                const auto kind = state->prepared->model.configuration().kind;
                const bool vertical = kind == ColorSurfaceKind::hue_rail || kind == ColorSurfaceKind::brightness_rail;
                const double step = event.shift ? .1 : .01;
                if (event.kind == Kind::increment) { (vertical ? point.y : point.x) += vertical ? -step : step; handled = true; }
                else if (event.kind == Kind::decrement) { (vertical ? point.y : point.x) += vertical ? step : -step; handled = true; }
                else if (event.kind == Kind::key_down) {
                    if (event.key == Key::left) { point.x -= step; handled = true; }
                    else if (event.key == Key::right) { point.x += step; handled = true; }
                    else if (event.key == Key::up) { point.y -= step; handled = true; }
                    else if (event.key == Key::down) { point.y += step; handled = true; }
                }
                point.x = std::clamp(point.x, 0., 1.);
                point.y = std::clamp(point.y, 0., 1.);
            }
            if (!handled) return false;
            const auto observed = state->revision;
            const auto prior = state->prepared->frame.revision;
            auto authored = state->authored;
            // Keep the source alive if it reenters and replaces the options.
            auto changed = authored.source->edit(authored.configuration.kind, point, prior);
            if (state->retired || state->revision != observed) return true;
            if (!changed && changed.error().code != ColorSurfaceErrorCode::stale) {
                return ii::core::fail(ii::core::ErrorCode::failed, changed.error().message);
            }
            if (!changed) state->dragging = false;
            auto next = prepare(authored,state->prepared);
            if (state->retired || state->revision != observed) return true;
            if (!next) return std::unexpected(next.error());
            const bool notify = changed && *changed != prior && (*next)->frame.revision == *changed;
            const auto color = (*next)->frame.color;
            if (auto replaced = replace(state, std::move(authored), std::move(*next)); !replaced) {
                return std::unexpected(replaced.error());
            }
            if (state->revision == observed + 1 && notify && state->live && state->edited) {
                auto callback = state->edited;
                callback({color.red, color.green, color.blue, color.alpha});
            }
            return true;
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Color surface input failed", std::current_exception());
        }
    }

public:
    [[nodiscard]] static ii::core::Result<BasicNativeColorSurface> create(
            ii::gui::WidgetBuilder& builder, ii::gui::Widget parent, std::string_view id,
            ColorSurfaceInput input, std::function<void(ii::gui::WidgetColor)> edited = {}) {
        try {
            auto prepared = prepare(input);
            if (!prepared) return std::unexpected(prepared.error());
            auto state = std::make_shared<State>();
            state->authored = std::move(input);
            state->prepared = std::move(*prepared);
            state->edited = std::move(edited);
            auto root = builder.action_surface(parent, id, action(state->prepared));
            if (!root) return std::unexpected(root.error());
            state->root = *root;
            if (auto result = root->set_accessibility_semantics(semantics(state->prepared)); !result) {
                return std::unexpected(result.error());
            }
            if (auto result = root->set_paint_handler([state](const auto& value) -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> {
                if(state->retired||!state->prepared)return ii::core::fail(ii::core::ErrorCode::closed,"Color surface retired");
                return paint(state->prepared, value); }); !result) {
                return std::unexpected(result.error());
            }
            if (auto result = root->set_input_handler([state](const auto& value) { return BasicNativeColorSurface::input(state, value); }); !result) {
                return std::unexpected(result.error());
            }
            std::weak_ptr<State> weak = state;
            if (auto result = builder.on_publish([weak] { if (auto current = weak.lock()) current->live = true; }); !result) {
                return std::unexpected(result.error());
            }
            if (auto result = builder.on_retire([weak] { if (auto current = weak.lock()) {
                current->live=false;current->retired=true;current->dragging=false;current->authored.source.reset();
                current->prepared.reset();current->edited={};
            } }); !result) {
                return std::unexpected(result.error());
            }
            return BasicNativeColorSurface(std::move(state));
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Color surface construction failed", std::current_exception());
        }
    }

    [[nodiscard]] ii::gui::Widget root() const { return state_->root; }

    [[nodiscard]] ii::core::Result<ColorSurfaceInput> options() const {
        if (state_->owner != std::this_thread::get_id()) {
            return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Color surface requires its owner thread");
        }
        if (auto valid = state_->root.snapshot(); !valid) return std::unexpected(valid.error());
        try { return state_->authored; }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Color surface options failed", std::current_exception()); }
    }

    [[nodiscard]] ii::core::Result<void> set_options(ColorSurfaceInput input) const {
        auto state=state_;
        if (state->owner != std::this_thread::get_id()) {
            return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Color surface requires its owner thread");
        }
        try {
            if (auto valid = state->root.snapshot(); !valid) return std::unexpected(valid.error());
            const auto observed = state->revision;
            auto previous=state->prepared;
            if(input==state->authored && input.source && previous){
                const auto source_revision=input.source->revision();
                if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Color surface retired");
                if(state->revision!=observed||source_revision==previous->frame.revision)return {};
            }
            auto next = prepare(input,previous);
            if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Color surface retired");
            if (state->revision != observed) return {};
            if (!next) return std::unexpected(next.error());
            return replace(state, std::move(input), std::move(*next));
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Color surface update failed", std::current_exception());
        }
    }

private:
    std::shared_ptr<State> state_;
};

using NativeColorSurface=BasicNativeColorSurface<DefaultColorSurfaceHostPolicy>;
} // namespace ii::gui
