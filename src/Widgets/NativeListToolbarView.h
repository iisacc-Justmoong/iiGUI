#pragma once
#include <iiGUI/ListToolbarModel.h>
#include <iiGUI/NativeButtonView.h>
#include <array>
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
struct ListBarInput final {
    ListBarOptions configuration;
    std::shared_ptr<const ButtonIconSource> icon_source;
    std::int64_t icon_revision=0;
    friend bool operator==(const ListBarInput&,const ListBarInput&)=default;
};
struct ListBarCallbacks final {std::function<void(std::size_t,std::string)> clicked;};
struct DefaultListToolbarHostPolicy final {
    using Input=ListBarInput;using Model=ListToolbarModel;using Rect=ListBarRect;using Color=WidgetColor;
    using IconCache=ButtonIconCache;using Callbacks=ListBarCallbacks;
};
template<class Policy>
class BasicNativeListToolbar final {
    using ListToolbarInput=typename Policy::Input;
    using ListToolbar=typename Policy::Model;
    using ListToolbarRect=typename Policy::Rect;
    using StyleColor=typename Policy::Color;
    using NativeIconCache=typename Policy::IconCache;
    using NativeListToolbarCallbacks=typename Policy::Callbacks;
    struct Prepared final {
        ListToolbarInput input;
        std::array<std::shared_ptr<const ii::gui::RasterImage>, 3> icons;
    };
    using Snapshot = std::shared_ptr<const Prepared>;
    struct State final {
        Snapshot prepared;
        std::shared_ptr<std::array<NativeIconCache, 3>> icons=std::make_shared<std::array<NativeIconCache, 3>>();
        ii::gui::Widget root;
        std::array<ii::gui::Widget, 3> slots;
        ii::gui::Size extent;
        NativeListToolbarCallbacks callbacks;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner = std::this_thread::get_id();
        std::uint64_t revision = 0, request_revision = 0;
        bool live = false, retired = false;
    };
    explicit BasicNativeListToolbar(std::shared_ptr<State> state) : state_(std::move(state)) {}
    [[nodiscard]] static ii::gui::Color color(StyleColor value) noexcept {
        const auto channel = [](double c) { return c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4); };
        return {float(channel(value.red)), float(channel(value.green)), float(channel(value.blue)), float(value.alpha)};
    }
    [[nodiscard]] static ii::gui::Rect rect(ListToolbarRect value) noexcept {
        return {float(value.x), float(value.y), float(value.width), float(value.height)};
    }
    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const ListToolbarInput& input, std::array<NativeIconCache, 3>& caches,
                                                            const std::shared_ptr<State>& state) {
        if (!ListToolbar::validate(input.configuration) || input.icon_revision < 0)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid list toolbar configuration");
        try {
            auto result = std::make_shared<Prepared>(); result->input = input;
            for (std::size_t i = 0; i < result->icons.size(); ++i) if (!input.configuration.icons[i].empty()) {
                if (state->retired) return ii::core::fail(ii::core::ErrorCode::closed, "List toolbar retired during icon resolution");
                auto icon = caches[i].resolve(input.configuration.icons[i], input.icon_source, input.configuration.icon_size);
                if (state->retired) return ii::core::fail(ii::core::ErrorCode::closed, "List toolbar retired during icon resolution");
                if (!icon) return std::unexpected(icon.error()); result->icons[i] = std::move(*icon);
            }
            return Snapshot(std::move(result));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "List toolbar preparation failed", std::current_exception()); }
    }
    [[nodiscard]] static ii::gui::ActionOptions slot_options(const Snapshot& value, std::size_t index) {
        const auto& c = value->input.configuration;
        return {.accessibility_name = c.icons[index], .enabled = c.enabled && c.interactive && bool(value->icons[index]),
                .focus_ring = false, .width = c.button_size, .height = c.button_size};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint_root(
            const Snapshot& value, const ii::gui::WidgetPaintState& state) {
        auto drawing = std::make_shared<ii::gui::WidgetDrawing>(); const auto& c = value->input.configuration;
        const auto fill = color(c.palette.background);
        if (fill.alpha > 0 && state.size.width > 0 && state.size.height > 0) drawing->panels.push_back({
            {0, 0, float(state.size.width), float(state.size.height)}, fill, fill,
            float((std::min)({c.corner_radius, state.size.width / 2, state.size.height / 2}))});
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint_slot(
            const Snapshot& value, std::size_t index, const ii::gui::WidgetPaintState& state) {
        auto drawing = std::make_shared<ii::gui::WidgetDrawing>(); const auto& c = value->input.configuration;
        const auto side = (std::min)(state.size.width, state.size.height);
        const auto background = color(state.pressed ? c.palette.pressed : c.palette.hover);
        if (state.enabled && (state.hovered || state.pressed) && side > 0 && background.alpha > 0)
            drawing->panels.push_back({{0, 0, float(state.size.width), float(state.size.height)}, background, background,
                                       float((std::min)(c.corner_radius, side / 2))});
        if (state.enabled && state.focused && side > 0) {
            const auto focus = color(c.palette.focus);
            drawing->panels.push_back({{0, 0, float(state.size.width), float(state.size.height)}, focus, focus,
                                       float((std::min)(c.corner_radius, side / 2)), 1.5f});
        }
        const auto icon = (std::min)(side, c.icon_size);
        if (value->icons[index] && icon > 0) drawing->images.push_back({
            .bounds = {float((state.size.width - icon) / 2), float((state.size.height - icon) / 2), float(icon), float(icon)},
            .image = value->icons[index], .opacity = state.enabled ? 1.f : .35f});
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
    }
    [[nodiscard]] static ii::core::Result<void> frames(const std::shared_ptr<State>& state, std::uint64_t revision) {
        if (state->retired || state->revision != revision) return {};
        auto value = state->prepared->input.configuration; value.width = state->extent.width; value.height = state->extent.height;
        if (value.width <= 0 || value.height <= 0) {
            for (auto& slot : state->slots) {
                if (auto result = slot.set_frame({0, 0, 0, 0}); !result) return result;
                if (state->retired || state->revision != revision) return {};
                if (auto result = slot.set_visible(false); !result) return result;
                if (state->retired || state->revision != revision) return {};
            }
            return {};
        }
        auto geometry = ListToolbar::geometry(value); if (!geometry) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid list toolbar extent");
        for (std::size_t i = 0; i < state->slots.size(); ++i) {
            if (auto result = state->slots[i].set_frame(rect(geometry->slots[i])); !result) return result;
            if (state->retired || state->revision != revision) return {};
            const bool shown = bool(state->prepared->icons[i]) && geometry->slots[i].width > 0 && geometry->slots[i].height > 0;
            if (auto result = state->slots[i].set_visible(shown); !result) return result;
            if (state->retired || state->revision != revision) return {};
        }
        return {};
    }
    [[nodiscard]] static ii::core::Result<bool> apply(const std::shared_ptr<State>& state, std::uint64_t revision) {
        const auto step = [&](auto&& action) -> ii::core::Result<bool> {
            if (state->retired || state->revision != revision) return false;
            auto result = action(); if (!result) return std::unexpected(result.error()); return !state->retired && state->revision == revision;
        };
        auto result = step([&] { return state->root.set_extent(state->extent); }); if (!result || !*result) return result;
        for (std::size_t i = 0; i < state->slots.size(); ++i) {
            result = step([&] { return state->slots[i].set_action_options(slot_options(state->prepared, i)); }); if (!result || !*result) return result;
        }
        result = step([&] { return frames(state, revision); }); if (!result || !*result) return result;
        result = step([&] { return state->root.request_repaint(); }); if (!result || !*result) return result;
        for (auto& slot : state->slots) { result = step([&] { return slot.request_repaint(); }); if (!result || !*result) return result; }
        return true;
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state, ListToolbarInput input) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "List toolbar requires its owner thread");
        try {
            if (auto live = state->root.snapshot(); !live) return std::unexpected(live.error());
            if (state->retired || !state->prepared || !state->icons) return ii::core::fail(ii::core::ErrorCode::closed, "List toolbar has retired");
            if (state->request_revision == (std::numeric_limits<std::uint64_t>::max)()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "List toolbar request revision exhausted");
            const auto request=++state->request_revision;
            if (state->prepared->input == input) return {};
            if (state->revision == (std::numeric_limits<std::uint64_t>::max)()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "List toolbar revision exhausted");
            auto icons=state->icons;auto next=prepare(input, *icons, state);
            if (state->retired || state->request_revision != request) return {};
            if (!next) return std::unexpected(next.error());
            auto previous = state->prepared; const auto extent = state->extent;
            const auto layout = *ListToolbar::geometry(input.configuration);
            const auto revision = ++state->revision; state->prepared = std::move(*next); state->extent = {layout.width, layout.height};
            auto applied = apply(state, revision);
            if (!applied) {
                if (state->revision == revision) { state->prepared = std::move(previous); state->extent = extent; (void)apply(state, revision); }
                return std::unexpected(applied.error());
            }
            state->failure.reset(); return {};
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "List toolbar update failed", std::current_exception()); }
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeListToolbar> create(ii::gui::WidgetBuilder& builder, ii::gui::Widget parent,
            std::string_view id, ListToolbarInput input = {}, NativeListToolbarCallbacks callbacks = {}) {
        try {
            auto state = std::make_shared<State>(); auto prepared = prepare(input, *state->icons, state); if (!prepared) return std::unexpected(prepared.error());
            state->prepared = std::move(*prepared); state->callbacks = std::move(callbacks);
            const auto geometry = *ListToolbar::geometry(input.configuration); state->extent = {geometry.width, geometry.height};
            std::weak_ptr<State> weak = state;
            auto root = builder.absolute_stack(parent, id, state->extent); if (!root) return std::unexpected(root.error()); state->root = *root;
            if (auto paint = root->set_paint_handler([weak](const auto& view) -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> {
                    auto state = weak.lock(); if (!state || !state->prepared) return ii::core::fail(ii::core::ErrorCode::closed, "List toolbar retired");
                    return paint_root(state->prepared, view);
                }); !paint) return std::unexpected(paint.error());
            for (std::size_t i = 0; i < state->slots.size(); ++i) {
                auto slot = builder.action_surface(*root, std::string(id) + ".slot" + std::to_string(i), slot_options(state->prepared, i), [weak, i] {
                    if (auto state = weak.lock(); state && state->live) try {
                        const auto& c = state->prepared->input.configuration;
                        if (c.enabled && c.interactive && state->prepared->icons[i] && state->callbacks.clicked) {
                            auto callback = state->callbacks.clicked; auto name = c.icons[i]; callback(i, std::move(name));
                        }
                    } catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "List toolbar callback failed", std::current_exception()}; }
                });
                if (!slot) return std::unexpected(slot.error()); state->slots[i] = *slot;
                if (auto paint = slot->set_paint_handler([weak, i](const auto& view) -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> {
                        auto state = weak.lock(); if (!state || !state->prepared) return ii::core::fail(ii::core::ErrorCode::closed, "List toolbar retired");
                        return paint_slot(state->prepared, i, view);
                    }); !paint) return std::unexpected(paint.error());
            }
            if (auto layout = root->set_layout_handler([weak](ii::gui::Rect bounds) { if (auto state = weak.lock(); state && !state->retired) {
                    if (state->extent.width == bounds.width && state->extent.height == bounds.height) return;
                    state->extent = {bounds.width, bounds.height}; if (auto result = frames(state, state->revision); !result) state->failure = result.error();
                }}); !layout) return std::unexpected(layout.error());
            auto applied = apply(state, state->revision); if (!applied) return std::unexpected(applied.error());
            if (auto retained = builder.retain(state); !retained) return std::unexpected(retained.error());
            if (auto published = builder.on_publish([weak] { if (auto state = weak.lock()) state->live = true; }); !published) return std::unexpected(published.error());
            if (auto retired = builder.on_retire([weak] { if (auto state = weak.lock()) {
                    state->live=false;state->retired=true;state->prepared.reset();state->icons.reset();state->callbacks={};
                }}); !retired) return std::unexpected(retired.error());
            return BasicNativeListToolbar(std::move(state));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "List toolbar construction failed", std::current_exception()); }
    }
    [[nodiscard]] ii::gui::Widget root() const { return state_->root; }
    [[nodiscard]] ii::core::Result<ii::gui::Widget> slot(std::size_t index) const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "List toolbar requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error());
        if (index >= state_->slots.size()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "List toolbar slot index is invalid");
        return state_->slots[index];
    }
    [[nodiscard]] ii::core::Result<ListToolbarInput> options() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "List toolbar requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error());
        if (!state_->prepared) return ii::core::fail(ii::core::ErrorCode::closed, "List toolbar has retired");return state_->prepared->input;
    }
    [[nodiscard]] ii::core::Result<void> set_options(ListToolbarInput input) const { auto state=state_;return update(state, std::move(input)); }
private:
    std::shared_ptr<State> state_;
};

using ListToolbarView=BasicNativeListToolbar<DefaultListToolbarHostPolicy>;
} // namespace ii::gui
