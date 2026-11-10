#pragma once
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
namespace ii::gui {
template<class Policy>
class BasicNativeHierarchyList final {
    using HierarchyListInput=typename Policy::Input;
    using HierarchyItemInput=typename Policy::ItemInput;
    using HierarchyItem=typename Policy::ItemModel;
    using NativeHierarchyItem=typename Policy::Item;
    using IconSource=typename Policy::IconSource;
    using NativeIconCache=typename Policy::IconCache;
    using HierarchyViewportSource=typename Policy::ViewportSource;
    using HierarchyViewportAnchor=typename Policy::ViewportAnchor;
    using HierarchyViewportRequest=typename Policy::ViewportRequest;
    using HierarchyProjection=typename Policy::Projection;
    using HierarchyVisibleEntry=typename Policy::VisibleEntry;
    using HierarchyRowContext=typename Policy::RowContext;
    using HierarchyRowSnapshot=typename Policy::RowSnapshot;
    using HierarchyDrag=typename Policy::Drag;
    using HierarchyDragPoint=typename Policy::DragPoint;
    using HierarchyError=typename Policy::Error;
    using HierarchyErrorCode=typename Policy::ErrorCode;
    using NativeHierarchyListCallbacks=typename Policy::Callbacks;
    using HierarchyCountViewFactory=typename Policy::ViewFactory;
    using HierarchyRowViewFactory=typename Policy::ViewFactory;
    struct Row final {
        std::string key;
        HierarchyItemInput input;
        std::shared_ptr<const IconSource> provider;
        std::int64_t provider_revision = 0;
        std::optional<NativeHierarchyItem> item;
        std::shared_ptr<HierarchyRowContext> context;
    };
    struct State final {
        HierarchyListInput authored;
        std::shared_ptr<const HierarchyViewportSource> source;
        std::unordered_map<std::string, std::shared_ptr<Row>> rows;
        std::optional<NativeIconCache> icons{std::in_place};
        ii::gui::Widget root, content;
        NativeHierarchyListCallbacks callbacks;
        HierarchyCountViewFactory count_view;
        HierarchyRowViewFactory row_view;
        std::function<void(const ii::gui::ScrollViewport&)> viewport_handler;
        ii::gui::ScrollViewport viewport;
        std::optional<ii::gui::ScrollViewport> pending;
        HierarchyViewportAnchor anchor;
        std::string selected, drag_key, drag_target;
        std::optional<HierarchyDrag> drag;
        bool dragging = false;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner = std::this_thread::get_id();
        double committed_offset = 0;
        std::uint64_t revision = 0, movement = 0;
        unsigned updates = 0;
        bool live = false, retired = false, viewport_seen = false, rendering = false;
    };
    static void release(const std::shared_ptr<State>& state)noexcept {
        if(state->drag)state->drag->cancel();state->drag.reset();state->rows.clear();state->source.reset();
        state->callbacks={};state->count_view={};state->row_view={};state->viewport_handler={};
        state->authored={};state->icons.reset();state->pending.reset();
    }
    struct UpdateGuard final {
        std::shared_ptr<State> state;
        explicit UpdateGuard(std::shared_ptr<State> value):state(std::move(value)){++state->updates;}
        ~UpdateGuard(){if(--state->updates==0&&state->retired)release(state);}
    };
    explicit BasicNativeHierarchyList(std::shared_ptr<State> state) : state_(std::move(state)) {}
    [[nodiscard]] static ii::core::Error convert(const HierarchyError& error) {
        const auto code = error.code == HierarchyErrorCode::invalid ? ii::core::ErrorCode::invalid_argument :
                          error.code == HierarchyErrorCode::not_found ? ii::core::ErrorCode::not_found :
                          error.code == HierarchyErrorCode::wrong_thread ? ii::core::ErrorCode::wrong_executor :
                          error.code == HierarchyErrorCode::exhausted ? ii::core::ErrorCode::out_of_range : ii::core::ErrorCode::busy;
        return {code, error.message};
    }
    [[nodiscard]] static bool valid(const HierarchyListInput& value) noexcept {
        const auto bound = [](double n, double low, double high) { return std::isfinite(n) && n >= low && n <= high; };
        return value.controller && (!value.editable || value.editor) && value.source_revision >= 0 && value.view_revision >= 0 && value.icon_revision >= 0 &&
            bound(value.width, 1, 8192) && bound(value.height, 1, 8192) && bound(value.row_height, 18, 256) &&
            value.overscan <= 256 && HierarchyItem::validate(value.row_appearance);
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const HierarchyViewportSource>> projection(const HierarchyListInput& value) {
        if (!valid(value)) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid hierarchy list configuration");
        auto projected = value.controller->projection(); if (!projected) return std::unexpected(convert(projected.error()));
        auto source = HierarchyViewportSource::create(std::make_shared<const HierarchyProjection>(std::move(*projected)));
        if (!source) return std::unexpected(convert(source.error())); return std::move(*source);
    }
    [[nodiscard]] static ii::gui::ScrollOptions scroll_options(const State& state) {
        return {.width = state.authored.width, .height = state.authored.height, .enabled = true,
                .show_indicator = state.authored.show_indicator,
                .virtual_content_height = *state.source->content_extent(state.authored.row_height)};
    }
    [[nodiscard]] static bool current(const std::shared_ptr<State>& state, const std::shared_ptr<Row>& row) {
        const auto found = state->rows.find(row->key);
        return state->live && found != state->rows.end() && found->second == row;
    }
    static void activation(const std::shared_ptr<State>& state, const std::shared_ptr<Row>& row) noexcept;
    static void expansion(const std::shared_ptr<State>& state, const std::shared_ptr<Row>& row, bool expanded) noexcept;

    [[nodiscard]] static ii::core::Result<HierarchyItemInput> row_input(
            const std::shared_ptr<State>& state, const HierarchyVisibleEntry& visible,
            const std::shared_ptr<Row>& existing, double width, double height) {
        auto configuration = Policy::project(visible, state->authored.row_appearance);
        if (!configuration) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid projected hierarchy row");
        configuration->width = width; configuration->height = height;
        configuration->enabled = configuration->enabled && state->authored.enabled;
        configuration->selected = state->selected == visible.entry.key;
        configuration->drag_preview = configuration->drag_preview || (state->dragging &&
            (state->drag_key == visible.entry.key || state->drag_target == visible.entry.key));
        if (!HierarchyItem::validate(*configuration)) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid hierarchy row extent");
        HierarchyItemInput result; result.configuration = std::move(*configuration); result.icon_revision = state->authored.icon_revision;
        if (!state->row_view && !result.configuration.icon_name.empty() && result.configuration.icon_size > 0) {
            if (existing && existing->input.icon && existing->provider == state->authored.icon_source &&
                existing->provider_revision == state->authored.icon_revision &&
                existing->input.configuration.icon_name == result.configuration.icon_name &&
                existing->input.configuration.icon_size == result.configuration.icon_size) result.icon = existing->input.icon;
            else {
                auto icon = state->icons->resolve(result.configuration.icon_name, state->authored.icon_source, result.configuration.icon_size);
                if (!icon) return std::unexpected(icon.error()); result.icon = std::move(*icon);
            }
        }
        return result;
    }
    [[nodiscard]] static HierarchyRowSnapshot row_snapshot(const HierarchyVisibleEntry& visible,
            const HierarchyItemInput& input) {
        return {.entry=visible.entry, .source_index=visible.source_index, .depth=visible.depth,
                .child_count=visible.child_count, .expanded=visible.expanded,
                .selected=input.configuration.selected, .enabled=input.configuration.enabled,
                .height=input.configuration.height};
    }
    [[nodiscard]] static ii::core::Result<void> frame(const std::shared_ptr<State>& state, const ii::gui::ScrollViewport& viewport) {
        const auto revision = state->revision;
        const auto width = double(viewport.bounds.width), height = double(viewport.bounds.height);
        HierarchyViewportRequest request{.width = width, .height = height, .offset = viewport.offset,
            .row_height = state->authored.row_height, .overscan = state->authored.overscan,
            .movement = state->movement, .anchor = state->anchor};
        auto planned = state->source->frame(request); if (!planned) return std::unexpected(convert(planned.error()));
        struct Change final { std::shared_ptr<Row> row; HierarchyItemInput input; std::optional<HierarchyRowSnapshot> snapshot; };
        std::unordered_map<std::string, std::shared_ptr<Row>> next; next.reserve(planned->rows.size());
        std::vector<Change> changes; changes.reserve(planned->rows.size());
        std::vector<ii::gui::KeyedContentItem> content; content.reserve(planned->rows.size());
        std::weak_ptr<State> weak = state;
        for (const auto& cell : planned->rows) {
            const auto& visible = state->source->projection().entries[cell.index];
            auto found = state->rows.find(visible.entry.key); auto row = found == state->rows.end() ? std::make_shared<Row>() : found->second;
            if (row->key.empty()) row->key = visible.entry.key;
            auto input = row_input(state, visible, found == state->rows.end() ? nullptr : row, width, cell.height);
            if (!input) return std::unexpected(input.error());
            std::optional<HierarchyRowSnapshot> snapshot;
            if (state->count_view || state->row_view) {
                snapshot = row_snapshot(visible, *input);
                if (!row->context) row->context = Policy::make_context(*snapshot);
            }
            next.emplace(row->key, row); changes.push_back({row, *input, std::move(snapshot)}); std::weak_ptr<Row> weak_row = row;
            content.push_back({row->key, [weak, weak_row, input = *input](ii::gui::WidgetBuilder& builder) -> ii::core::Result<void> {
                auto state = weak.lock(); auto row = weak_row.lock();
                if (!state || !row || !state->live) return ii::core::fail(ii::core::ErrorCode::closed, "Hierarchy list row owner retired");
                ii::gui::WidgetFactory count_view, row_view;
                if (state->count_view || state->row_view) {
                    auto context = row->context; auto factory = state->count_view ? state->count_view : state->row_view;
                    if (auto retired = builder.on_retire([context] { Policy::close_context(*context); }); !retired) return retired;
                    auto view = [factory=std::move(factory), context=std::move(context)](ii::gui::WidgetBuilder& content) {
                        return factory(content, context);
                    };
                    if (state->row_view) row_view = std::move(view); else count_view = std::move(view);
                }
                auto item = NativeHierarchyItem::create(builder, {}, "item", input,
                    {.activated = [weak, weak_row] { if (auto state = weak.lock()) if (auto row = weak_row.lock()) activation(state, row); },
                     .expansion_changed = [weak, weak_row](bool value) { if (auto state = weak.lock()) if (auto row = weak_row.lock()) expansion(state, row, value); }},
                    std::move(count_view), std::move(row_view));
                if (!item) return std::unexpected(item.error()); row->item = *item; return {};
            }, ii::gui::Rect{0, float(cell.y), float(width), float(cell.height)}});
        }
        if (auto mounted = state->content.set_keyed_content(std::move(content), {.viewport = ii::gui::Size{width, height}}); !mounted) return mounted;
        if (!state->live) return {};
        for (auto& change : changes) {
            if (!change.row->item) return ii::core::fail(ii::core::ErrorCode::failed, "Hierarchy row did not publish");
            if (auto updated = change.row->item->set_options(change.input); !updated) return updated;
            if (!state->live) return {};
            change.row->input = std::move(change.input); change.row->provider = state->authored.icon_source;
            change.row->provider_revision = state->authored.icon_revision;
        }
        state->rows = std::move(next); state->anchor = planned->anchor; state->committed_offset = planned->offset;
        for (auto& change : changes) {
            if (!state->live) return ii::core::fail(ii::core::ErrorCode::closed, "Hierarchy list retired during row publication");
            if (state->revision != revision) return {};
            if (change.row->context && change.snapshot && current(state, change.row))
                Policy::publish_context(*change.row->context,std::move(*change.snapshot));
        }
        if (state->revision != revision) return {};
        return state->root.set_virtual_scroll_content({width, planned->content_height}, 0, planned->offset);
    }
    [[nodiscard]] static ii::core::Result<void> viewport(const std::shared_ptr<State>& state, ii::gui::ScrollViewport value) {
        if (state->rendering) { state->pending = value; return {}; }
        for (unsigned pass = 0; pass < 8; ++pass) {
            if (state->viewport_seen && std::abs(value.offset - state->committed_offset) > 1e-6) {
                if (state->movement == (std::numeric_limits<std::uint64_t>::max)())
                    return ii::core::fail(ii::core::ErrorCode::out_of_range, "Hierarchy viewport movement exhausted");
                ++state->movement;
            }
            state->viewport = value; state->viewport_seen = true; state->pending.reset(); state->rendering = true;
            ii::core::Result<void> result;
            try { result = frame(state, value); }
            catch (...) { result = ii::core::fail(ii::core::ErrorCode::failed, "Hierarchy viewport construction failed", std::current_exception()); }
            state->rendering = false;
            if (!result) return result;
            if (!state->pending) return {};
            value = *state->pending; state->pending.reset();
        }
        return ii::core::fail(ii::core::ErrorCode::busy, "Hierarchy viewport did not settle");
    }
    [[nodiscard]] static ii::core::Result<void> refresh_projection(const std::shared_ptr<State>& state) {
        auto next = projection(state->authored); if (!next) return std::unexpected(next.error());
        auto selected = state->authored.controller->selected_key(); if (!selected) return std::unexpected(convert(selected.error()));
        auto old = state->source; auto previous = state->selected;
        state->source = std::move(*next); state->selected = std::move(*selected);
        if (state->viewport_seen) {
            auto rendered = viewport(state, state->viewport);
            if (!rendered) { state->source = std::move(old); state->selected = std::move(previous); (void)viewport(state, state->viewport); return rendered; }
        }
        return {};
    }
    [[nodiscard]] static ii::core::Result<void> selection(const std::shared_ptr<State>& state, std::string key) {
        auto selected = state->authored.controller->select(key); if (!selected) return std::unexpected(convert(selected.error()));
        if (!*selected) return {};
        const auto previous = state->selected; state->selected = std::move(key); const auto revision = state->revision;
        for (const auto& name : {previous, state->selected}) {
            auto found = state->rows.find(name); if (found == state->rows.end() || !found->second->item) continue;
            auto row = found->second; auto input = row->input; input.configuration.selected = name == state->selected;
            if (auto changed = row->item->set_options(input); !changed) return changed;
            if (current(state, row)) {
                row->input = std::move(input);
                if (row->context) { auto snapshot = row->context->get(); snapshot.selected = name == state->selected;
                    Policy::publish_context(*row->context,std::move(snapshot)); }
            }
            if (state->revision != revision || !state->live) return {};
        }
        return {};
    }
    [[nodiscard]] static ii::core::Result<void> drag_visuals(const std::shared_ptr<State>& state,
            std::string_view old_source, std::string_view old_target) {
        const std::array<std::string, 4> names{std::string(old_source), std::string(old_target), state->drag_key, state->drag_target};
        for (const auto& name : names) {
            if (name.empty()) continue; auto found = state->rows.find(name);
            if (found == state->rows.end() || !found->second->item) continue;
            auto row = found->second; auto input = row->input;
            input.configuration.drag_preview = state->authored.row_appearance.drag_preview ||
                (state->dragging && (name == state->drag_key || name == state->drag_target));
            if (auto updated = row->item->set_options(input); !updated) return updated;
            if (current(state, row)) row->input = std::move(input);
        }
        return {};
    }
    [[nodiscard]] static ii::core::Result<void> clear_drag(const std::shared_ptr<State>& state) {
        const auto source = state->drag_key, target = state->drag_target;
        if (state->drag) state->drag->cancel();
        state->drag.reset(); state->drag_key.clear(); state->drag_target.clear(); state->dragging = false;
        return drag_visuals(state, source, target);
    }
    [[nodiscard]] static ii::core::Result<bool> pointer(const std::shared_ptr<State>& state, const ii::gui::WidgetInput& event) {
        using ii::gui::WidgetInputKind;
        if (!state->live) return false;
        if (Policy::pointer_canceled(event.kind) && state->drag) {
            if (auto cleared = clear_drag(state); !cleared) return std::unexpected(cleared.error()); return true;
        }
        if (event.kind == WidgetInputKind::pointer_down) {
            if (!state->authored.enabled || !state->authored.editable || !state->authored.editor || !event.local_position || event.native_content_control_active ||
                (event.buttons && !(event.buttons & 1)) || state->drag || !state->viewport_seen) return false;
            const auto x = event.local_position->x, y = event.local_position->y;
            if (x < 0 || y < 0 || x >= state->viewport.bounds.width || y >= state->viewport.bounds.height) return false;
            auto source = state->authored.controller->source_snapshot(); if (!source) return std::unexpected(convert(source.error()));
            if ((*source)->revision != state->source->projection().source_revision) {
                if (auto refreshed = refresh_projection(state); !refreshed) return std::unexpected(refreshed.error()); return true;
            }
            const auto& entries = state->source->projection().entries;
            const auto logical = state->viewport.offset + y;
            const auto index = state->source->row_at(logical, state->authored.row_height); if (!index) return false;
            const auto& entry = entries[*index].entry; if (!entry.enabled || !entry.draggable) return false;
            auto row = state->rows.find(entry.key); if (row == state->rows.end() || !row->second->item) return false;
            HierarchyItem model; if (!model.set_configuration(row->second->input.configuration)) return false;
            auto geometry = model.geometry(state->viewport.bounds.width, *state->source->row_extent(*index, state->authored.row_height));
            if (!geometry) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid hierarchy drag hit geometry");
            if (geometry->show_chevron && x >= geometry->chevron.x && x < geometry->chevron.x + geometry->chevron.width) return false;
            auto drag = HierarchyDrag::begin(*source, entry.key, {x, y}); if (!drag) return std::unexpected(convert(drag.error()));
            state->drag = std::move(*drag); state->drag_key = entry.key;
            if (auto focused = row->second->item->row().focus(); !focused) { (void)clear_drag(state); return std::unexpected(focused.error()); }
            return true;
        }
        if (!state->drag || (event.kind != WidgetInputKind::pointer_move && event.kind != WidgetInputKind::pointer_up &&
                            event.kind != WidgetInputKind::pointer_leave)) return false;
        if (event.kind == WidgetInputKind::pointer_leave) return true;
        if (!event.local_position) { (void)clear_drag(state); return true; }
        auto revision = state->authored.controller->source_revision(); if (!revision) { (void)clear_drag(state); return std::unexpected(convert(revision.error())); }
        if (*revision != state->source->projection().source_revision) {
            if (auto cleared = clear_drag(state); !cleared) return std::unexpected(cleared.error());
            if (auto refreshed = refresh_projection(state); !refreshed) return std::unexpected(refreshed.error()); return true;
        }
        const HierarchyDragPoint position{event.local_position->x, event.local_position->y};
        const auto calculate = [&] { return state->drag->update(position, {state->source, double(state->viewport.bounds.width),
            double(state->viewport.bounds.height), state->viewport.offset, state->authored.row_height}); };
        auto planned = calculate(); if (!planned) { (void)clear_drag(state); return std::unexpected(convert(planned.error())); }
        if (event.kind == WidgetInputKind::pointer_move && planned->dragging && planned->scroll_direction) {
            const auto height = *state->source->content_extent(state->authored.row_height);
            const auto logical = std::clamp(state->viewport.offset + position.y, 0., std::nextafter(height, 0.));
            const auto row = state->source->row_at(logical, state->authored.row_height);
            const auto step = row ? *state->source->row_extent(*row, state->authored.row_height) : state->authored.row_height;
            const auto offset = (std::max)(0., state->viewport.offset + planned->scroll_direction * step);
            if (auto scrolled = state->root.set_scroll_offset(offset); !scrolled) { (void)clear_drag(state); return std::unexpected(scrolled.error()); }
            if (!state->live || !state->drag) return true;
            planned = calculate(); if (!planned) { (void)clear_drag(state); return std::unexpected(convert(planned.error())); }
        }
        const auto old_target = state->drag_target; state->dragging = planned->dragging;
        state->drag_target = planned->proposal ? planned->proposal->request.target : std::string{};
        if (auto painted = drag_visuals(state, state->drag_key, old_target); !painted) { (void)clear_drag(state); return std::unexpected(painted.error()); }
        if (event.kind == WidgetInputKind::pointer_move) return true;
        const auto key = state->drag_key; const bool was_dragging = state->dragging;
        auto proposal = state->drag->finish(); if (!proposal) { (void)clear_drag(state); return std::unexpected(convert(proposal.error())); }
        if (auto cleared = clear_drag(state); !cleared) return std::unexpected(cleared.error());
        if (!state->live) return true;
        if (!was_dragging) {
            auto row = state->rows.find(key); if (row != state->rows.end()) activation(state, row->second);
        } else if (*proposal) {
            auto editor = state->authored.editor; auto controller = state->authored.controller;
            auto committed = editor->move((*proposal)->request);
            if (!committed) { if (state->live) if (auto refreshed = refresh_projection(state); !refreshed) state->failure = refreshed.error();
                return std::unexpected(convert(committed.error())); }
            if (!state->live || state->authored.editor != editor || state->authored.controller != controller) return true;
            auto refreshed = refresh_projection(state);
            if (state->live && state->authored.editor == editor && state->authored.controller == controller && state->callbacks.moved) {
                auto callback = state->callbacks.moved; callback((*proposal)->plan);
            }
            if (!refreshed) return std::unexpected(refreshed.error());
        }
        return true;
    }
    [[nodiscard]] static ii::core::Result<bool> navigate(const std::shared_ptr<State>& state, const ii::gui::WidgetInput& event) {
        using ii::gui::WidgetInputKind; using ii::gui::WidgetKey;
        if (!state->live || !state->authored.enabled || !state->authored.keyboard_navigation ||
            event.kind != WidgetInputKind::key_down || event.alt || event.control || event.meta || event.shift) return false;
        const auto key = event.key;
        if (key != WidgetKey::up && key != WidgetKey::down && key != WidgetKey::home && key != WidgetKey::end &&
            key != WidgetKey::page_up && key != WidgetKey::page_down && key != WidgetKey::left && key != WidgetKey::right) return false;
        auto revision = state->authored.controller->source_revision(); if (!revision) return std::unexpected(convert(revision.error()));
        if (*revision != state->source->projection().source_revision) {
            if (auto refreshed = refresh_projection(state); !refreshed) return std::unexpected(refreshed.error());
            return true;
        }
        const auto& entries = state->source->projection().entries; if (entries.empty()) return true;
        std::optional<std::size_t> current;
        for (const auto& [name, row] : state->rows) if (row->item) {
            auto focused = row->item->row().interaction(); if (!focused) return std::unexpected(focused.error());
            auto arrow = row->item->chevron().interaction(); if (!arrow) return std::unexpected(arrow.error());
            if (focused->focused || arrow->focused) { current = state->source->find(name); break; }
        }
        if (!current && !state->selected.empty()) current = state->source->find(state->selected);
        const auto count = std::ptrdiff_t(entries.size());
        std::ptrdiff_t index = -1;
        if (key == WidgetKey::left || key == WidgetKey::right) {
            if (!current) return false;
            const auto& selected = entries[*current];
            // Let the row own collapse/expand and its accompanying callbacks.
            if ((key == WidgetKey::left && selected.expanded) ||
                (key == WidgetKey::right && selected.child_count && !selected.expanded)) return false;
            if (key == WidgetKey::left) {
                auto parent = selected.entry.parent;
                while (!parent.empty()) {
                    const auto found = state->source->find(parent); if (!found) break;
                    const auto& candidate = entries[*found];
                    if (candidate.entry.enabled && candidate.entry.selectable) { index = std::ptrdiff_t(*found); break; }
                    parent = candidate.entry.parent;
                }
            } else if (selected.expanded) {
                for (auto next = *current + 1; next < entries.size() && entries[next].depth > selected.depth; ++next)
                    if (entries[next].depth == selected.depth + 1 && entries[next].entry.enabled && entries[next].entry.selectable) {
                        index = std::ptrdiff_t(next); break;
                    }
            }
            if (index < 0) return true;
        } else {
            const bool backward = key == WidgetKey::up || key == WidgetKey::page_up || key == WidgetKey::end;
            const auto direction = backward ? std::ptrdiff_t(-1) : std::ptrdiff_t(1);
            if (key == WidgetKey::home) index = 0;
            else if (key == WidgetKey::end) index = count - 1;
            else if (!current) index = backward ? count - 1 : 0;
            else if (key == WidgetKey::page_up || key == WidgetKey::page_down) {
                const auto total = *state->source->content_extent(state->authored.row_height);
                const auto current_offset = *state->source->row_offset(*current, state->authored.row_height);
                const auto logical = std::clamp(current_offset + direction * double(state->viewport.bounds.height), 0., std::nextafter(total, 0.));
                index = std::ptrdiff_t(*state->source->row_at(logical, state->authored.row_height));
                if (index == std::ptrdiff_t(*current)) index = std::clamp(index + direction, std::ptrdiff_t(0), count - 1);
            }
            else index = std::ptrdiff_t(*current) + direction;
            while (index >= 0 && index < count && (!entries[std::size_t(index)].entry.enabled || !entries[std::size_t(index)].entry.selectable)) index += direction;
            if (index < 0 || index >= count) return true;
        }
        const auto target = entries[std::size_t(index)]; const auto previous = state->selected;
        if (auto selected = selection(state, target.entry.key); !selected) return std::unexpected(selected.error());
        if (!state->live) return true;
        HierarchyViewportRequest request{.width = double(state->viewport.bounds.width), .height = double(state->viewport.bounds.height),
            .offset = state->viewport.offset, .row_height = state->authored.row_height, .overscan = state->authored.overscan,
            .movement = state->movement, .anchor = state->anchor, .reveal_key = target.entry.key};
        auto plan = state->source->frame(request); if (!plan) return std::unexpected(convert(plan.error()));
        if (auto scrolled = state->root.set_scroll_offset(plan->offset); !scrolled) return std::unexpected(scrolled.error());
        if (!state->live) return true;
        const auto found = state->rows.find(target.entry.key);
        if (found == state->rows.end() || !found->second->item)
            return ii::core::fail(ii::core::ErrorCode::not_found, "Keyboard target is not mounted");
        if (auto focused = found->second->item->row().focus(); !focused) return std::unexpected(focused.error());
        if (state->live && previous != target.entry.key && state->selected == target.entry.key && state->callbacks.activated) {
            auto callback = state->callbacks.activated; callback({target.entry.key, target.entry.id, target.source_index});
        }
        return true;
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state, HierarchyListInput input) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Hierarchy list requires its owner thread");
        UpdateGuard guard(state);if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Hierarchy list retired");
        try {
            if (auto live = state->root.snapshot(); !live) return std::unexpected(live.error());
            if (state->revision == (std::numeric_limits<std::uint64_t>::max)()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Hierarchy list revision exhausted");
            const auto observed=state->revision;auto next = projection(input); if (!state->live || state->revision!=observed) return {};if (!next) return std::unexpected(next.error());
            if (state->drag && (state->authored.controller != input.controller || state->authored.editor != input.editor ||
                                !input.enabled || !input.editable)) {
                if (auto cleared = clear_drag(state); !cleared) return cleared;
            }
            auto selected = input.controller->selected_key(); if (!selected) return std::unexpected(convert(selected.error()));
            auto old_input = state->authored; auto old_source = state->source; auto old_selected = state->selected;
            const auto revision = ++state->revision; state->authored = std::move(input); state->source = std::move(*next); state->selected = std::move(*selected);
            auto rollback = [&] { if (state->revision == revision) {
                state->authored = std::move(old_input); state->source = std::move(old_source); state->selected = std::move(old_selected);
                if (state->viewport_seen) (void)viewport(state, state->viewport);
                (void)state->root.set_scroll_options(scroll_options(*state)); (void)state->root.set_enabled(state->authored.enabled);
            }};
            if (state->viewport_seen) { if (auto rendered = viewport(state, state->viewport); !rendered) { rollback(); return rendered; } }
            if (state->revision != revision) return {};
            if (auto changed = state->root.set_scroll_options(scroll_options(*state)); !changed) { rollback(); return changed; }
            if (state->revision != revision) return {};
            if (auto enabled = state->root.set_enabled(state->authored.enabled); !enabled) { rollback(); return enabled; }
            state->failure.reset(); return {};
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Hierarchy list update failed", std::current_exception()); }
    }

public:
    [[nodiscard]] static ii::core::Result<BasicNativeHierarchyList> create(ii::gui::WidgetBuilder& builder, ii::gui::Widget parent,
            std::string_view id, HierarchyListInput input, NativeHierarchyListCallbacks callbacks = {},
            HierarchyCountViewFactory count_view = {}, HierarchyRowViewFactory row_view = {}) {
        try {
            if (count_view && row_view) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "A hierarchy row delegate cannot also have a count View");
            auto source = projection(input); if (!source) return std::unexpected(source.error());
            auto selected = input.controller->selected_key(); if (!selected) return std::unexpected(convert(selected.error()));
            auto state = std::make_shared<State>(); state->authored = std::move(input); state->source = std::move(*source);
            state->selected = std::move(*selected); state->callbacks = std::move(callbacks);
            state->count_view = std::move(count_view); state->row_view = std::move(row_view); std::weak_ptr<State> weak = state;
            auto root = builder.scroll_view(parent, id, scroll_options(*state)); if (!root) return std::unexpected(root.error()); state->root = *root;
            auto content = builder.content_host(*root, std::string(id) + ".rows", {state->authored.width, state->authored.height});
            if (!content) return std::unexpected(content.error()); state->content = *content;
            state->viewport_handler = [weak](const ii::gui::ScrollViewport& value) {
                if (auto state = weak.lock(); state && state->live) {
                    UpdateGuard guard(state);auto result = viewport(state, value);
                    if (!result) {
                        state->failure = result.error();
                        // A surrounding content generation can temporarily forbid
                        // keyed publication. Mark this delivery pending so the
                        // host retries after the outer generation has committed.
                        if (result.error().code == ii::core::ErrorCode::busy)
                            (void)state->root.set_scroll_handler(state->viewport_handler);
                        throw std::runtime_error(result.error().message);
                    }
                    state->failure.reset();
                }
            };
            const auto on_viewport = state->viewport_handler;
            if (auto handler = root->set_input_handler([weak](const ii::gui::WidgetInput& event) -> ii::core::Result<bool> {
                    auto state = weak.lock(); if (!state) return false;
                    UpdateGuard guard(state);try {
                        auto handled = pointer(state, event); if (!handled) { state->failure = handled.error(); return handled; }
                        if (*handled) return true;
                        auto result = navigate(state, event); if (!result) state->failure = result.error(); return result;
                    }
                    catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Hierarchy keyboard navigation failed", std::current_exception()};
                        return std::unexpected(*state->failure); }
                }); !handler) return std::unexpected(handler.error());
            if (auto handler = root->set_scroll_handler(on_viewport); !handler) return std::unexpected(handler.error());
            if (auto enabled = root->set_enabled(state->authored.enabled); !enabled) return std::unexpected(enabled.error());
            if (auto retained = builder.retain(state); !retained) return std::unexpected(retained.error());
            if (auto published = builder.on_publish([weak, on_viewport] { if (auto state = weak.lock()) {
                    state->live = true;
                    if (auto observed = state->root.set_scroll_handler(on_viewport); !observed) state->failure = observed.error();
                }}); !published) return std::unexpected(published.error());
            if (auto retired = builder.on_retire([weak] { if (auto state = weak.lock()) { state->live = false; state->retired = true;
                if(state->revision!=(std::numeric_limits<std::uint64_t>::max)())++state->revision;
                if(state->updates==0)release(state); } }); !retired) return std::unexpected(retired.error());
            return BasicNativeHierarchyList(std::move(state));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Hierarchy list construction failed", std::current_exception()); }
    }
    [[nodiscard]] ii::gui::Widget root() const { return state_->root; }
    [[nodiscard]] ii::core::Result<NativeHierarchyItem> row(std::string_view key) const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Hierarchy list requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error());
        auto found = state_->rows.find(std::string(key));
        if (found == state_->rows.end() || !found->second->item) return ii::core::fail(ii::core::ErrorCode::not_found, "Hierarchy row is not mounted");
        return *found->second->item;
    }
    [[nodiscard]] ii::core::Result<std::vector<std::string>> mounted_keys() const { return state_->content.keyed_content_keys(); }
    [[nodiscard]] ii::core::Result<void> set_options(HierarchyListInput input) const { return update(state_, std::move(input)); }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Hierarchy list requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error()); return state_->failure;
    }
private:
    std::shared_ptr<State> state_;
};

template<class Policy>
inline void BasicNativeHierarchyList<Policy>::activation(const std::shared_ptr<State>& state, const std::shared_ptr<Row>& row) noexcept {
    UpdateGuard guard(state);
    try {
        if (!current(state, row) || !state->authored.enabled) return;
        auto revision = state->authored.controller->source_revision();
        if (!revision) { state->failure = convert(revision.error()); return; }
        if (*revision != state->source->projection().source_revision) {
            if (auto refreshed = refresh_projection(state); !refreshed) state->failure = refreshed.error();
            return;
        }
        auto index = state->source->find(row->key); if (!index) return;
        const auto visible = state->source->projection().entries[*index];
        if (auto selected = selection(state, row->key); !selected) { state->failure = selected.error(); return; }
        if (current(state, row) && state->callbacks.activated) {
            auto callback = state->callbacks.activated; callback({row->key, visible.entry.id, visible.source_index});
        }
    } catch (...) { try { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Hierarchy list activation failed", std::current_exception()}; } catch (...) {} }
}

template<class Policy>
inline void BasicNativeHierarchyList<Policy>::expansion(const std::shared_ptr<State>& state, const std::shared_ptr<Row>& row, bool expanded) noexcept {
    UpdateGuard guard(state);
    try {
        if (!current(state, row) || !state->authored.enabled) return;
        auto revision = state->authored.controller->source_revision();
        if (!revision) { state->failure = convert(revision.error()); return; }
        if (*revision != state->source->projection().source_revision) {
            if (auto refreshed = refresh_projection(state); !refreshed) state->failure = refreshed.error();
            return;
        }
        auto changed = state->authored.controller->set_expanded(row->key, expanded);
        if (!changed) { state->failure = convert(changed.error()); return; }
        if (*changed) {
            if (auto refreshed = refresh_projection(state); !refreshed) {
                (void)state->authored.controller->set_expanded(row->key, !expanded); state->failure = refreshed.error(); return;
            }
            if (current(state, row) && state->callbacks.expansion_changed) {
                auto callback = state->callbacks.expansion_changed; callback(row->key, expanded);
            }
        }
    } catch (...) { try { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Hierarchy list expansion failed", std::current_exception()}; } catch (...) {} }
}

} // namespace ii::gui
