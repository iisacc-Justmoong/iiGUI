#pragma once

#include <iiGUI/NativeWidgets.h>
#include <iiGUI/Text.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace ii::gui {
template<class Policy>
class BasicNativeHierarchyToolbar final {
    using HierarchyToolbarInput=typename Policy::Input;
    using HierarchyToolbar=typename Policy::Model;
    using HierarchyToolbarLayout=typename Policy::Layout;
    using ToolbarButtonModel=typename Policy::ButtonModel;
    using NativeToolbarButton=typename Policy::Button;
    using ToolbarButtonInput=typename Policy::ButtonInput;
    using ButtonTone=typename Policy::ButtonTone;
    using NativeIconCache=typename Policy::IconCache;
    using ToolbarButtonId=typename Policy::Id;
    using HierarchyToolbarSnapshot=typename Policy::Snapshot;
    using HierarchyToolbarSelection=typename Policy::Selection;
    using HierarchyToolbarError=typename Policy::Error;
    using HierarchyToolbarErrorCode=typename Policy::ErrorCode;
    using HierarchyToolbarDirection=typename Policy::Direction;
    using HierarchyToolbarGeometry=typename Policy::Geometry;
    using HierarchyToolbarRect=typename Policy::Rect;
    using StyleColor=typename Policy::Color;
    using NativeHierarchyToolbarCallbacks=typename Policy::Callbacks;
    struct Row final {
        ToolbarButtonId id;
        std::string key;
        NativeIconCache cache;
        std::optional<NativeToolbarButton> button;
    };
    struct Prepared final {
        HierarchyToolbarInput input;
        std::shared_ptr<HierarchyToolbar> model;
        std::shared_ptr<const HierarchyToolbarSnapshot> snapshot;
        std::vector<std::shared_ptr<Row>> rows;
        std::vector<std::shared_ptr<const typename Policy::Image>> icons;
        std::vector<double> text_widths;
    };
    using Snapshot = std::shared_ptr<const Prepared>;
    struct State final {
        Snapshot prepared;
        std::map<ToolbarButtonId, std::shared_ptr<Row>> rows;
        ii::gui::Widget root, content;
        ii::gui::Size extent;
        NativeHierarchyToolbarCallbacks callbacks;
        std::function<void(ii::gui::Rect)> layout_handler;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner = std::this_thread::get_id();
        std::uint64_t revision = 0, serial = 0;
        bool live = false, mounted = false;
    };
    explicit BasicNativeHierarchyToolbar(std::shared_ptr<State> state) : state_(std::move(state)) {}
    [[nodiscard]] static ii::core::Error convert(const HierarchyToolbarError& value) {
        const auto code = value.code == HierarchyToolbarErrorCode::invalid ? ii::core::ErrorCode::invalid_argument :
            value.code == HierarchyToolbarErrorCode::not_found ? ii::core::ErrorCode::not_found :
            value.code == HierarchyToolbarErrorCode::wrong_thread ? ii::core::ErrorCode::wrong_executor : ii::core::ErrorCode::out_of_range;
        return {code, value.message};
    }
    [[nodiscard]] static ii::gui::Color color(StyleColor value, double opacity = 1) noexcept {
        const auto channel = [](double c) { return c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4); };
        return {float(channel(value.red)), float(channel(value.green)), float(channel(value.blue)), float(value.alpha * opacity)};
    }
    [[nodiscard]] static ii::gui::Rect rect(HierarchyToolbarRect value) noexcept {
        return {float(value.x), float(value.y), float(value.width), float(value.height)};
    }
    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const std::shared_ptr<State>& state, HierarchyToolbarInput input) {
        if (!HierarchyToolbarLayout::validate(input.appearance) || input.icon_revision < 0)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid hierarchy toolbar appearance");
        try {
            auto result = std::make_shared<Prepared>(); result->input = std::move(input);
            result->model = state->prepared ? std::make_shared<HierarchyToolbar>(*state->prepared->model) : std::make_shared<HierarchyToolbar>();
            auto changed = result->model->set_items(result->input.items); if (!changed) return std::unexpected(convert(changed.error()));
            if (result->input.active_id) { auto active = result->model->set_active(*result->input.active_id); if (!active) return std::unexpected(convert(active.error())); }
            auto snapshot = result->model->snapshot(); if (!snapshot) return std::unexpected(convert(snapshot.error())); result->snapshot = std::move(*snapshot);
            result->rows.reserve(result->snapshot->items.size()); result->icons.resize(result->snapshot->items.size());
            result->text_widths.resize(result->snapshot->items.size());
            for (std::size_t i = 0; i < result->snapshot->items.size(); ++i) {
                const auto& id = result->snapshot->ids[i]; const auto& item = result->snapshot->items[i];
                const auto existing = state->rows.find(id); std::shared_ptr<Row> row;
                if (existing != state->rows.end()) row = existing->second;
                else {
                    if (state->serial == (std::numeric_limits<std::uint64_t>::max)()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Toolbar slot identities exhausted");
                    row = std::make_shared<Row>(); row->id = id; row->key = "slot-" + std::to_string(++state->serial);
                }
                result->rows.push_back(row);
                if (!item.icon_name.empty()) {
                    auto icon = row->cache.resolve(item.icon_name, result->input.icon_source, result->input.appearance.icon_size);
                    if (!icon) return std::unexpected(icon.error()); result->icons[i] = std::move(*icon);
                }
                if (!item.text.empty()) {
                    auto measured = ii::gui::measure_text_layout({.text=item.text, .font_size=float(result->input.appearance.font_size),
                        .font_weight=500, .line_height=float(result->input.appearance.font_size + 2), .elide_right=true});
                    if (!measured) return std::unexpected(measured.error()); result->text_widths[i] = (std::min)(8192., double(measured->width));
                }
            }
            auto geometry = HierarchyToolbarLayout::geometry(*result->snapshot, result->input.appearance, result->text_widths);
            if (!geometry)
                return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Hierarchy toolbar exceeds its geometry limit");
            for (std::size_t i = 0; i < result->rows.size(); ++i) {
                auto input = button_input(result, i, geometry->slots[i]); auto configuration = input.configuration;
                configuration.has_icon = bool(input.icon);
                if (!ToolbarButtonModel::validate(configuration)) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid hierarchy toolbar button");
            }
            return Snapshot(std::move(result));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Hierarchy toolbar preparation failed", std::current_exception()); }
    }
    [[nodiscard]] static ToolbarButtonInput button_input(const Snapshot& value, std::size_t index, HierarchyToolbarRect frame) {
        const auto& a = value->input.appearance; const auto& item = value->snapshot->items[index]; ToolbarButtonInput input;
        auto& c = input.configuration; c.id = value->snapshot->ids[index]; c.text = item.text;
        c.accessibility_name = item.accessibility_name.empty() ? (item.text.empty() ? item.icon_name : item.text) : item.accessibility_name;
        c.enabled = a.enabled && item.enabled; c.visible = item.visible && frame.width > 0 && frame.height > 0;
        c.width = (std::max)(1., frame.width); c.height = (std::max)(1., frame.height); c.icon_size = a.icon_size;
        c.corner_radius = a.corner_radius; c.font_size = a.font_size; c.palette = a.palette;
        c.tone = value->snapshot->active && value->snapshot->active->id == c.id ? ButtonTone::normal : ButtonTone::borderless;
        input.icon = value->icons[index]; return input;
    }
    [[nodiscard]] static bool current(const std::shared_ptr<State>& state, const std::shared_ptr<Row>& row) {
        const auto found = state->rows.find(row->id); return state->live && found != state->rows.end() && found->second == row;
    }
    static void activated(const std::shared_ptr<State>& state, const std::shared_ptr<Row>& row) noexcept;
    [[nodiscard]] static ii::core::Result<bool> render(const std::shared_ptr<State>& state, std::uint64_t revision) {
        auto appearance = state->prepared->input.appearance;
        std::optional<HierarchyToolbarGeometry> geometry;
        if (state->extent.width <= 0 || state->extent.height <= 0) {
            geometry = HierarchyToolbarGeometry{}; geometry->slots.resize(state->prepared->rows.size());
        } else {
            appearance.width = state->extent.width; appearance.height = state->extent.height;
            geometry = HierarchyToolbarLayout::geometry(*state->prepared->snapshot, appearance, state->prepared->text_widths);
            if (!geometry) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid hierarchy toolbar geometry");
        }
        std::vector<ii::gui::KeyedContentItem> content; content.reserve(state->prepared->rows.size());
        std::vector<ToolbarButtonInput> inputs; inputs.reserve(state->prepared->rows.size());
        std::map<ToolbarButtonId, std::shared_ptr<Row>> next; std::weak_ptr<State> weak = state;
        for (std::size_t i = 0; i < state->prepared->rows.size(); ++i) {
            auto row = state->prepared->rows[i]; auto input = button_input(state->prepared, i, geometry->slots[i]);
            inputs.push_back(input); next.emplace(row->id, row); std::weak_ptr<Row> weak_row = row;
            content.push_back({row->key, [weak, weak_row, input](ii::gui::WidgetBuilder& builder) -> ii::core::Result<void> {
                auto state = weak.lock(); auto row = weak_row.lock();
                if (!state || !row || !state->live) return ii::core::fail(ii::core::ErrorCode::closed, "Toolbar item owner retired");
                auto button = NativeToolbarButton::create(builder, {}, "button", input, {.clicked = [weak, weak_row] {
                    if (auto state = weak.lock()) if (auto row = weak_row.lock()) activated(state, row);
                }});
                if (!button) return std::unexpected(button.error()); row->button = *button; return {};
            }, rect(geometry->slots[i])});
        }
        if (auto frame = state->content.set_frame({0, 0, float(state->extent.width), float(state->extent.height)}); !frame) return std::unexpected(frame.error());
        if (state->revision != revision) return false;
        if (auto mounted = state->content.set_keyed_content(std::move(content), {.viewport=state->extent}); !mounted) return std::unexpected(mounted.error());
        if (state->revision != revision) return false;
        for (std::size_t i = 0; i < state->prepared->rows.size(); ++i) {
            if (!state->prepared->rows[i]->button) return ii::core::fail(ii::core::ErrorCode::failed, "Toolbar button did not publish");
            if (auto changed = state->prepared->rows[i]->button->set_options(inputs[i]); !changed) return std::unexpected(changed.error());
            if (state->revision != revision) return false;
        }
        state->rows = std::move(next); state->mounted = true;
        if (auto painted = state->root.request_repaint(); !painted) return std::unexpected(painted.error());
        return true;
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state, HierarchyToolbarInput input) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Hierarchy toolbar requires its owner thread");
        try {
            if (auto live = state->root.snapshot(); !live) return std::unexpected(live.error());
            if (state->prepared->input == input) return {};
            if (state->revision == (std::numeric_limits<std::uint64_t>::max)()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Hierarchy toolbar revision exhausted");
            const auto observed = state->revision; auto next = prepare(state, std::move(input)); if (!next) return std::unexpected(next.error());
            if (state->revision != observed || !state->live) return {};
            auto geometry = HierarchyToolbarLayout::geometry(*(*next)->snapshot, (*next)->input.appearance, (*next)->text_widths);
            auto previous = state->prepared; const auto extent = state->extent;
            const auto revision = ++state->revision; state->prepared = std::move(*next); state->extent = {geometry->width, geometry->height};
            auto rollback = [&] { if (state->revision == revision) { state->prepared = previous; state->extent = extent;
                (void)state->root.set_extent(extent); (void)render(state, revision); (void)state->root.set_enabled(previous->input.appearance.enabled); } };
            if (auto resized = state->root.set_extent(state->extent); !resized) { rollback(); return resized; }
            if (state->revision != revision) return {};
            auto rendered = render(state, revision); if (!rendered) { rollback(); return std::unexpected(rendered.error()); }
            if (!*rendered || state->revision != revision) return {};
            if (auto enabled = state->root.set_enabled(state->prepared->input.appearance.enabled); !enabled) { rollback(); return enabled; }
            state->failure.reset(); return {};
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Hierarchy toolbar update failed", std::current_exception()); }
    }
    [[nodiscard]] static ii::core::Result<bool> keyboard(const std::shared_ptr<State>& state, const ii::gui::WidgetInput& event) {
        using ii::gui::WidgetInputKind; using ii::gui::WidgetKey;
        if (!state->live || !state->prepared->input.appearance.enabled || event.kind != WidgetInputKind::key_down || event.alt || event.control || event.meta) return false;
        std::optional<HierarchyToolbarDirection> direction;
        if (event.key == WidgetKey::left) direction = HierarchyToolbarDirection::previous;
        else if (event.key == WidgetKey::right) direction = HierarchyToolbarDirection::next;
        else if (event.key == WidgetKey::home) direction = HierarchyToolbarDirection::first;
        else if (event.key == WidgetKey::end) direction = HierarchyToolbarDirection::last;
        if (!direction) return false;
        auto model = std::make_shared<HierarchyToolbar>(*state->prepared->model); auto changed = model->navigate(*direction);
        if (!changed) return std::unexpected(convert(changed.error())); if (!*changed) return true;
        auto snapshot = model->snapshot(); if (!snapshot) return std::unexpected(convert(snapshot.error()));
        if (state->revision == (std::numeric_limits<std::uint64_t>::max)()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Hierarchy toolbar revision exhausted");
        auto previous = state->prepared; auto next = std::make_shared<Prepared>(*previous); next->model = std::move(model); next->snapshot = *snapshot;
        const auto revision = ++state->revision; state->prepared = std::move(next);
        auto rendered = render(state, revision);
        if (!rendered) { if (state->revision == revision) { state->prepared = previous; (void)render(state, revision); } return std::unexpected(rendered.error()); }
        if (!*rendered || state->revision != revision || !state->live || !state->prepared->snapshot->active) return true;
        const auto selected = *state->prepared->snapshot->active; auto row = state->rows.find(selected.id);
        if (row != state->rows.end() && row->second->button) if (auto focused = row->second->button->root().focus(); !focused) return std::unexpected(focused.error());
        if (state->live && state->revision == revision && state->callbacks.active_changed) { auto callback = state->callbacks.active_changed; callback(selected); }
        return true;
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeHierarchyToolbar> create(ii::gui::WidgetBuilder& builder, ii::gui::Widget parent,
            std::string_view id, HierarchyToolbarInput input = {}, NativeHierarchyToolbarCallbacks callbacks = {}) {
        try {
            auto state = std::make_shared<State>(); auto prepared = prepare(state, std::move(input)); if (!prepared) return std::unexpected(prepared.error());
            state->prepared = std::move(*prepared); state->callbacks = std::move(callbacks);
            const auto geometry = *HierarchyToolbarLayout::geometry(*state->prepared->snapshot, state->prepared->input.appearance, state->prepared->text_widths);
            state->extent = {geometry.width, geometry.height}; std::weak_ptr<State> weak = state;
            auto root = builder.absolute_stack(parent, id, state->extent); if (!root) return std::unexpected(root.error()); state->root = *root;
            auto content = builder.content_host(*root, std::string(id) + ".items", state->extent); if (!content) return std::unexpected(content.error()); state->content = *content;
            if (auto painted = root->set_paint_handler([weak](const ii::gui::WidgetPaintState& view) -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> {
                    auto state = weak.lock(); if (!state || !state->prepared) return ii::core::fail(ii::core::ErrorCode::closed, "Hierarchy toolbar retired");
                    auto drawing = std::make_shared<ii::gui::WidgetDrawing>(); const auto& a = state->prepared->input.appearance;
                    const auto fill = color(a.background, a.background_opacity);
                    if (fill.alpha > 0 && view.size.width > 0 && view.size.height > 0) drawing->panels.push_back({
                        {0, 0, float(view.size.width), float(view.size.height)}, fill, fill, float((std::min)({a.corner_radius, view.size.width / 2, view.size.height / 2}))});
                    return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
                }); !painted) return std::unexpected(painted.error());
            if (auto handler = root->set_input_handler([weak](const ii::gui::WidgetInput& event) -> ii::core::Result<bool> {
                    auto state = weak.lock(); if (!state) return false;
                    try { auto result = keyboard(state, event); if (!result) state->failure = result.error(); return result; }
                    catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Hierarchy toolbar input failed", std::current_exception()}; return std::unexpected(*state->failure); }
                }); !handler) return std::unexpected(handler.error());
            if (auto enabled = root->set_enabled(state->prepared->input.appearance.enabled); !enabled) return std::unexpected(enabled.error());
            state->layout_handler=[weak](ii::gui::Rect bounds) { if (auto state = weak.lock(); state && state->live) {
                    if (state->mounted && state->extent.width == bounds.width && state->extent.height == bounds.height) return;
                    state->extent = {bounds.width, bounds.height}; auto result = render(state, state->revision);
                    if (!result) {
                        state->failure = result.error();
                        if (result.error().code == ii::core::ErrorCode::busy) {
                            (void)state->root.set_layout_handler(state->layout_handler);
                            throw std::runtime_error(result.error().message);
                        }
                    } else if (*result) state->failure.reset();
                }};
            if (auto layout = root->set_layout_handler(state->layout_handler); !layout) return std::unexpected(layout.error());
            if (auto retained = builder.retain(state); !retained) return std::unexpected(retained.error());
            if (auto published = builder.on_publish([weak] { if (auto state = weak.lock()) state->live = true; }); !published) return std::unexpected(published.error());
            if (auto retired = builder.on_retire([weak] { if (auto state = weak.lock()) { state->live = false; if (state->revision != (std::numeric_limits<std::uint64_t>::max)()) ++state->revision; state->rows.clear(); state->prepared.reset(); state->callbacks = {}; state->layout_handler = {}; } }); !retired) return std::unexpected(retired.error());
            return BasicNativeHierarchyToolbar(std::move(state));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Hierarchy toolbar construction failed", std::current_exception()); }
    }
    [[nodiscard]] ii::gui::Widget root() const { return state_->root; }
    [[nodiscard]] ii::core::Result<ii::gui::Widget> slot(const ToolbarButtonId& id) const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Hierarchy toolbar requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error());
        if (!state_->mounted) { auto mounted = render(state_, state_->revision); if (!mounted) return std::unexpected(mounted.error()); }
        const auto found = state_->rows.find(id); if (found == state_->rows.end() || !found->second->button) return ii::core::fail(ii::core::ErrorCode::not_found, "Toolbar item was not found");
        return found->second->button->root();
    }
    [[nodiscard]] ii::core::Result<std::shared_ptr<const HierarchyToolbarSnapshot>> snapshot() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Hierarchy toolbar requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error()); return state_->prepared->snapshot;
    }
    [[nodiscard]] ii::core::Result<void> set_options(HierarchyToolbarInput input) const { return update(state_, std::move(input)); }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Hierarchy toolbar requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error()); return state_->failure;
    }
private:
    std::shared_ptr<State> state_;
};

template<class Policy>
inline void BasicNativeHierarchyToolbar<Policy>::activated(const std::shared_ptr<State>& state, const std::shared_ptr<Row>& row) noexcept {
    if (!current(state, row) || !state->prepared->input.appearance.enabled) return;
    try {
        if (state->revision == (std::numeric_limits<std::uint64_t>::max)()) {
            state->failure = ii::core::Error{ii::core::ErrorCode::out_of_range, "Hierarchy toolbar revision exhausted"}; return;
        }
        auto model = std::make_shared<HierarchyToolbar>(*state->prepared->model); auto activation = model->activate(row->id);
        if (!activation) { state->failure = convert(activation.error()); return; }
        auto snapshot = model->snapshot(); if (!snapshot) { state->failure = convert(snapshot.error()); return; }
        auto previous = state->prepared; auto next = std::make_shared<Prepared>(*previous); next->model = std::move(model); next->snapshot = *snapshot;
        const auto revision = ++state->revision; state->prepared = std::move(next);
        auto rendered = render(state, revision);
        if (!rendered) { if (state->revision == revision) { state->prepared = previous; (void)render(state, revision); } state->failure = rendered.error(); return; }
        if (!*rendered || !current(state, row) || state->revision != revision) return;
        const auto value = activation->selection;
        const auto committed = state->prepared;
        const auto compatible = [&] {
            if (!current(state, row) || !state->prepared->snapshot->active || state->prepared->snapshot->active->id != row->id) return false;
            const auto& before = committed->input; const auto& now = state->prepared->input;
            return before.items == now.items && before.appearance == now.appearance && before.icon_source == now.icon_source &&
                before.icon_revision == now.icon_revision && (!now.active_id || *now.active_id == row->id);
        };
        if (activation->changed && state->callbacks.active_changed) { auto callback = state->callbacks.active_changed; callback(value); }
        if (!compatible()) return;
        if (state->callbacks.triggered) { auto callback = state->callbacks.triggered; callback(value); }
        if (!compatible()) return;
        if (activation->command) activation->command->invoke(value.id);
        if (compatible()) state->failure.reset();
    } catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Hierarchy toolbar action failed", std::current_exception()}; }
}


} // namespace ii::gui
