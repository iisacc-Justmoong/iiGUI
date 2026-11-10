#pragma once

#include <iiGUI/SheetModel.h>

#include <iiGUI/NativeWidgets.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace ii::gui {

[[nodiscard]] constexpr bool sheet_mobile_platform() noexcept {
#if defined(__ANDROID__) || (defined(__APPLE__) && TARGET_OS_IPHONE)
    return true;
#else
    return false;
#endif
}

struct SheetInput final {
    SheetConfiguration configuration;
    bool mobile_platform = sheet_mobile_platform();
    friend bool operator==(const SheetInput&, const SheetInput&) = default;
};

struct NativeSheetCallbacks final { std::function<void()> opened, closed; };
using SheetContentFactory = std::function<ii::core::Result<void>(ii::gui::WidgetBuilder&, ii::gui::Widget)>;
// The app can supply a themed close control; iiGUI owns the sheet itself.
using SheetCloseFactory = std::function<ii::core::Result<ii::gui::Widget>(
    ii::gui::WidgetBuilder&, ii::gui::Widget, std::string_view, ii::runtime::Task)>;

class NativeSheet final {
    struct Prepared final { Sheet model; bool mobile = false, grabber = false; };
    using Snapshot = std::shared_ptr<const Prepared>;
    struct State final {
        SheetInput authored;
        Snapshot prepared;
        SheetDrag drag;
        ii::gui::Widget root, chrome, grabber, header, header_row, title, description, viewport, body, safe_area;
        ii::gui::Widget close;
        NativeSheetCallbacks callbacks;
        std::string viewport_id;
        const std::thread::id owner = std::this_thread::get_id();
        std::uint64_t revision = 0;
        bool live = false;
    };

    explicit NativeSheet(std::shared_ptr<State> state) : state_(std::move(state)) {}

    [[nodiscard]] static ii::gui::WidgetColor host_color(ii::gui::WidgetColor value) noexcept {
        return {value.red, value.green, value.blue, value.alpha};
    }

    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const SheetInput& input) {
        if (!Sheet::validate(input.configuration)) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid LVUI Sheet configuration");
        try {
            auto result = std::make_shared<Prepared>();
            if (!result->model.set_configuration(input.configuration)) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid LVUI Sheet configuration");
            const auto layout = result->model.layout(100, 100, 0, 0, input.mobile_platform);
            result->mobile = layout->mobile; result->grabber = layout->show_grabber;
            return Snapshot(std::move(result));
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Sheet preparation failed", std::current_exception());
        }
    }

    [[nodiscard]] static ii::gui::PopoverOptions host_options(const std::shared_ptr<State>& state) {
        const auto& c = state->prepared->model.configuration();
        ii::gui::PopoverOptions options;
        options.width = c.preferred_width; options.padding = 0; options.margins = c.desktop_margin;
        options.corner_radius = c.corner_radius.value_or(state->prepared->mobile ? 24 : 16);
        options.surface_opacity = 1; options.surface_color = host_color(c.palette.surface);
        options.modal = true; options.dim = true; options.focus_on_open = true; options.clip_content = true;
        options.dismiss_on_escape = c.dismiss_on_escape; options.dismiss_on_outside_press = c.dismiss_on_background;
        options.accessibility_name = c.accessibility_name; options.placement = ii::gui::PopoverPlacement::centered;
        options.backdrop_color = host_color(c.palette.backdrop); options.opaque_base = false;
        options.vertical_offset = state->drag.offset();
        ii::gui::PopoverTransition transition;
        transition.enter_duration = transition.exit_duration = std::chrono::duration_cast<ii::runtime::Clock::duration>(
            std::chrono::duration<double, std::milli>(c.animation_duration_ms));
        transition.enter_easing = ii::animation::Easing::out_back; transition.exit_easing = ii::animation::Easing::in_cubic;
        transition.overshoot = 1.45; transition.collapsed_scale = .94; transition.enabled = c.motion_enabled;
        options.transition = std::move(transition);
        std::weak_ptr<State> weak = state;
        options.adaptive_layout = ii::gui::PopoverAdaptiveLayout{.viewport_id = state->viewport_id,
            .layout = [weak](const ii::gui::PopoverLayoutMeasure& measure) -> ii::core::Result<ii::gui::Rect> {
                auto state = weak.lock();
                if (!state) return ii::core::fail(ii::core::ErrorCode::closed, "Sheet layout owner retired");
                const auto& prepared = *state->prepared;
                const auto bottom = prepared.mobile ? prepared.model.configuration().bottom_safe_inset : 0.;
                const auto chrome = (std::max)(0., measure.content.height - measure.viewport_content_height - bottom);
                const auto frame = prepared.model.layout(measure.viewport.width, measure.viewport.height,
                    measure.viewport_content_height, chrome, state->authored.mobile_platform);
                if (!frame) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Sheet cannot fit the supplied viewport");
                return ii::gui::Rect{float(frame->frame.x), float(frame->frame.y), float(frame->frame.width), float(frame->frame.height)};
            }, .slide_from_bottom = state->prepared->mobile};
        return options;
    }

    [[nodiscard]] static ii::gui::LabelOptions title_options(const SheetConfiguration& c) {
        return {.text = c.title, .font_size = 20, .selectable = false, .role = ii::gui::TextRole::title2,
                .color = ii::gui::WidgetColor{1, 1, 1, .9}, .line_height = 22, .font_weight = 700};
    }
    [[nodiscard]] static ii::gui::LabelOptions description_options(const SheetConfiguration& c) {
        return {.text = c.description, .font_size = 13, .selectable = false, .role = ii::gui::TextRole::body,
                .color = ii::gui::WidgetColor{1, 1, 1, .8}, .line_height = 18, .font_weight = 500};
    }

    [[nodiscard]] static ii::core::Result<bool> apply_host(const std::shared_ptr<State>& state,
                                                            Snapshot prepared, std::uint64_t revision) {
        const auto& c = prepared->model.configuration();
        const auto step = [&](auto&& operation) -> ii::core::Result<bool> {
            if (state->revision != revision) return false;
            auto result = operation(); if (!result) return std::unexpected(result.error());
            return state->revision == revision;
        };
        auto result = step([&] { return state->root.set_popover_options(host_options(state)); });
        if (!result || !*result) return result;
        result = step([&] { return state->title.set_label_options(title_options(c)); });
        if (!result || !*result) return result;
        result = step([&] { return state->description.set_label_options(description_options(c)); });
        if (!result || !*result) return result;
        result = step([&] { return state->header.set_visible(c.show_header); });
        if (!result || !*result) return result;
        result = step([&] { return state->title.set_visible(!c.title.empty()); });
        if (!result || !*result) return result;
        result = step([&] { return state->description.set_visible(c.show_description && !c.description.empty()); });
        if (!result || !*result) return result;
        result = step([&] { return state->close.set_visible(c.show_close_button); });
        if (!result || !*result) return result;
        result = step([&] { return state->grabber.set_visible(prepared->grabber); });
        if (!result || !*result) return result;
        result = step([&] { return state->viewport.set_scroll_options({.enabled = c.scroll_content, .show_indicator = c.scroll_content}); });
        if (!result || !*result) return result;
        result = step([&] { return state->body.set_stack_options({.spacing = 0, .padding = c.content_padding, .alignment = ii::gui::StackAlignment::stretch}); });
        if (!result || !*result) return result;
        const auto bottom = prepared->mobile ? c.bottom_safe_inset : 0.;
        result = step([&] { return state->safe_area.set_extent({0, bottom}); });
        if (!result || !*result) return result;
        return step([&] { return state->safe_area.set_visible(bottom > 0); });
    }

    [[nodiscard]] static ii::core::Result<bool> drag_event(const std::shared_ptr<State>& state, const ii::gui::WidgetInput& event) {
        if (!state->live) return false;
        const auto& config = state->prepared->model.configuration();
        if (event.kind == ii::gui::WidgetInputKind::pointer_down) {
            auto popup = state->root.snapshot(); if (!popup) return std::unexpected(popup.error());
            if (!popup->opened || !state->drag.begin(event.y, popup->bounds.height, state->prepared->mobile, config.dismiss_on_drag)) return false;
            return true;
        }
        if (!state->drag.active()) return false;
        if (event.kind == ii::gui::WidgetInputKind::pointer_move) {
            if (!state->drag.move(event.y)) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid Sheet drag position");
            if (auto updated = state->root.set_popover_options(host_options(state)); !updated) return std::unexpected(updated.error());
            return true;
        }
        if (event.kind == ii::gui::WidgetInputKind::pointer_up) {
            auto result = state->drag.end(event.y);
            if (!result) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid Sheet drag position");
            if (auto updated = state->root.set_popover_options(host_options(state)); !updated) return std::unexpected(updated.error());
            if (*result == SheetDragResult::dismiss) if (auto closed = state->root.close_popup(); !closed) return std::unexpected(closed.error());
            return true;
        }
        if (event.kind == ii::gui::WidgetInputKind::focus_lost
#if defined(IIGUI_POINTER_CANCEL_API)
            || event.kind == ii::gui::WidgetInputKind::pointer_cancel
#endif
        ) {
            (void)state->drag.cancel();
            if (auto updated = state->root.set_popover_options(host_options(state)); !updated) return std::unexpected(updated.error());
            return true;
        }
        return true;
    }

public:
    [[nodiscard]] static ii::core::Result<NativeSheet> create(
            ii::gui::WidgetBuilder& builder, ii::gui::Widget parent, std::string_view id,
            SheetInput input = {}, NativeSheetCallbacks callbacks = {}, SheetContentFactory content = {},
            SheetCloseFactory close_factory = {}) {
        try {
            auto state = std::make_shared<State>(); auto prepared = prepare(input);
            if (!prepared) return std::unexpected(prepared.error());
            state->authored = std::move(input); state->prepared = std::move(*prepared); state->callbacks = std::move(callbacks);
            const auto name = [&](std::string_view suffix) { return std::string(id) + "." + std::string(suffix); };
            state->viewport_id = name("viewport"); std::weak_ptr<State> weak = state;
            const auto event = [weak](auto member, bool opened) {
                return [weak, member, opened] {
                    if (auto state = weak.lock(); state && state->live) {
                        auto current = state->root.snapshot(); if (!current || current->opened != opened) return;
                        if (!opened && state->drag.cancel()) (void)state->root.set_popover_options(host_options(state));
                        auto callback = state->callbacks.*member; if (callback) callback();
                    }
                };
            };
            auto root = builder.popover(parent, id, host_options(state),
                {.opened = event(&NativeSheetCallbacks::opened, true), .closed = event(&NativeSheetCallbacks::closed, false)});
            if (!root) return std::unexpected(root.error()); state->root = *root;
            auto chrome = builder.vstack(*root, name("chrome"), {.spacing = 0, .alignment = ii::gui::StackAlignment::stretch});
            if (!chrome) return std::unexpected(chrome.error()); state->chrome = *chrome;
            auto grabber = builder.absolute_stack(*chrome, name("grabber"), {36, 20});
            if (!grabber) return std::unexpected(grabber.error()); state->grabber = *grabber;
            if (auto painted = grabber->set_paint_handler([](const ii::gui::WidgetPaintState& paint) -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> {
                auto drawing = std::make_shared<ii::gui::WidgetDrawing>();
                const auto width = float((std::min)(36., paint.size.width));
                const ii::gui::Color color{.48f, .48f, .48f, .7f};
                drawing->panels.push_back({{float((paint.size.width - width) / 2), 8, width, 4}, color, color, 2});
                return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
            }); !painted) return std::unexpected(painted.error());
            if (auto installed = grabber->set_input_handler([weak](const auto& input) -> ii::core::Result<bool> {
                auto state = weak.lock(); if (!state) return false; return drag_event(state, input);
            }); !installed) return std::unexpected(installed.error());
            auto header = builder.vstack(*chrome, name("header"), {.spacing = 8, .padding = 20, .alignment = ii::gui::StackAlignment::stretch});
            if (!header) return std::unexpected(header.error()); state->header = *header;
            auto row = builder.hstack(*header, name("header-row"), {.spacing = 8, .alignment = ii::gui::StackAlignment::center});
            if (!row) return std::unexpected(row.error()); state->header_row = *row;
            auto title = builder.label(*row, name("title"), title_options(state->prepared->model.configuration()));
            if (!title) return std::unexpected(title.error()); state->title = *title;
            if (auto flex = title->set_flex(1); !flex) return std::unexpected(flex.error());
            auto close_action = [weak] {
                if (auto state = weak.lock(); state && state->live) (void)state->root.close_popup();
            };
            ii::core::Result<ii::gui::Widget> close = [&]() -> ii::core::Result<ii::gui::Widget> {
                if (close_factory) return close_factory(builder, *row, name("close"), close_action);
                ii::gui::ButtonOptions options;
                options.text = "×"; options.accessibility_name = "Close";
                options.tone = ii::gui::ButtonTone::borderless;
                options.width = 26; options.height = 26; options.horizontal_padding = 0;
                return builder.button(*row, name("close"), std::move(options), std::move(close_action));
            }();
            if (!close) return std::unexpected(close.error()); state->close = *close;
            auto description = builder.label(*header, name("description"), description_options(state->prepared->model.configuration()));
            if (!description) return std::unexpected(description.error()); state->description = *description;
            auto viewport = builder.scroll_view(*chrome, state->viewport_id, {});
            if (!viewport) return std::unexpected(viewport.error()); state->viewport = *viewport;
            if (auto flex = viewport->set_flex(1); !flex) return std::unexpected(flex.error());
            auto body = builder.vstack(*viewport, name("body"), {.spacing = 0, .padding = 20, .alignment = ii::gui::StackAlignment::stretch});
            if (!body) return std::unexpected(body.error()); state->body = *body;
            if (content) if (auto built = content(builder, *body); !built) return std::unexpected(built.error());
            auto safe = builder.absolute_stack(*chrome, name("safe-area"), {0, 0});
            if (!safe) return std::unexpected(safe.error()); state->safe_area = *safe;
            if (auto applied = apply_host(state, state->prepared, state->revision); !applied) return std::unexpected(applied.error());
            if (auto retained = builder.retain(state); !retained) return std::unexpected(retained.error());
            if (auto published = builder.on_publish([weak] { if (auto state = weak.lock()) state->live = true; }); !published) return std::unexpected(published.error());
            if (auto retired = builder.on_retire([weak] { if (auto state = weak.lock()) state->live = false; }); !retired) return std::unexpected(retired.error());
            return NativeSheet(std::move(state));
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Sheet construction failed", std::current_exception());
        }
    }

    [[nodiscard]] ii::gui::Widget root() const { return state_->root; }
    [[nodiscard]] ii::gui::Widget viewport() const { return state_->viewport; }
    [[nodiscard]] ii::gui::Widget content_root() const { return state_->body; }

    [[nodiscard]] ii::core::Result<SheetInput> options() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Sheet requires its owner thread");
        if (auto valid = state_->root.snapshot(); !valid) return std::unexpected(valid.error());
        try { return state_->authored; }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Sheet options failed", std::current_exception()); }
    }

    [[nodiscard]] ii::core::Result<void> set_options(SheetInput input) const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Sheet requires its owner thread");
        try {
            if (auto valid = state_->root.snapshot(); !valid) return std::unexpected(valid.error());
            if (state_->authored == input) return {};
            if (state_->revision == (std::numeric_limits<std::uint64_t>::max)()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Sheet revision exhausted");
            const auto observed = state_->revision; auto next = prepare(input);
            if (!next) return std::unexpected(next.error());
            if (state_->revision != observed) return {};
            auto old_input = state_->authored; auto old = state_->prepared;
            const auto revision = ++state_->revision; (void)state_->drag.cancel();
            state_->authored = std::move(input); state_->prepared = std::move(*next);
            auto applied = apply_host(state_, state_->prepared, revision);
            if (!applied) {
                if (state_->revision == revision) {
                    state_->authored = std::move(old_input); state_->prepared = std::move(old);
                    (void)apply_host(state_, state_->prepared, revision);
                }
                return std::unexpected(applied.error());
            }
            return {};
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Sheet update failed", std::current_exception());
        }
    }

private:
    std::shared_ptr<State> state_;
};

} // namespace ii::gui
