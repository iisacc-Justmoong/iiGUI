#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/Desktop.h>
#include <algorithm>
#include <charconv>
#include <cmath>
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
#include <unordered_set>
#include <utility>
#include <vector>
namespace ii::gui {
// The host owns retained table drawing, input, clipping and scrolling.
// Application data and presentation contracts are supplied by Policy.
template<class Policy>
class BasicNativeTable final {
    using TableInput=typename Policy::TableInput;
    using TableCellEvent=typename Policy::TableCellEvent;
    using NativeTableCallbacks=typename Policy::NativeTableCallbacks;
    using NativeTableClipboard=typename Policy::NativeTableClipboard;
    using NativeTableViews=typename Policy::NativeTableViews;
    using NativeInputField=typename Policy::NativeInputField;
    using NativeMenu=typename Policy::NativeMenu;
    using NativeMenuItem=typename Policy::NativeMenuItem;
    using NativeTableHeaderPresentation=typename Policy::NativeTableHeaderPresentation;
    using NativeTableViewGroup=typename Policy::NativeTableViewGroup;
    using MenuInput=typename Policy::MenuInput;
    using MenuItemInput=typename Policy::MenuItemInput;
    using TableViewItem=typename Policy::TableViewItem;
    using StyleColor=typename Policy::StyleColor;
    using TableAppearance=typename Policy::TableAppearance;
    using InputFieldConfiguration=typename Policy::InputFieldConfiguration;
    using InputFieldPalette=typename Policy::InputFieldPalette;
    using TableError=typename Policy::TableError;
    using TableErrorCode=typename Policy::TableErrorCode;
    using TableSnapshot=typename Policy::TableSnapshot;
    using TableViewportSource=typename Policy::TableViewportSource;
    using TableViewportFrame=typename Policy::TableViewportFrame;
    using TableViewportAnchor=typename Policy::TableViewportAnchor;
    using TableViewportRequest=typename Policy::TableViewportRequest;
    using TableViewportMerge=typename Policy::TableViewportMerge;
    using TableSelection=typename Policy::TableSelection;
    using TableRange=typename Policy::TableRange;
    using TablePosition=typename Policy::TablePosition;
    using TableViewSnapshot=typename Policy::TableViewSnapshot;
    using TableSortOrder=typename Policy::TableSortOrder;
    using TableSelectionMode=typename Policy::TableSelectionMode;
    using TableDirection=typename Policy::TableDirection;
    using TableEditing=typename Policy::TableEditing;
    using TableEdit=typename Policy::TableEdit;
    using TableClipboardData=typename Policy::TableClipboardData;
    using TableIndex=typename Policy::TableIndex;
    using TableTextSorter=typename Policy::TableTextSorter;
    using TableSorter=typename Policy::TableSorter;
    using TableMergeEdit=typename Policy::TableMergeEdit;
    using TableStructureEdit=typename Policy::TableStructureEdit;
    using TableStructureIntent=typename Policy::TableStructureIntent;
    using TableStructureAction=typename Policy::TableStructureAction;
    using TableDefaultStructureFactory=typename Policy::TableDefaultStructureFactory;
    using TableResizeColumns=typename Policy::TableResizeColumns;
    using TableResizeRows=typename Policy::TableResizeRows;
    struct State final {
        struct Resize final {
            bool column = false;
            std::string key;
            std::size_t index = 0;
            double origin = 0, original = 0, preview = 0;
            std::uint64_t source_revision = 0, view_revision = 0;
        };
        struct Context final { std::string row, column; std::uint64_t source_revision = 0, view_revision = 0; };
        struct StructurePress final { TableStructureIntent intent; std::uint64_t view_revision = 0; };
        struct AccessibleAction final {
            std::string key,name;ii::gui::Rect bounds;
            friend bool operator==(const AccessibleAction&,const AccessibleAction&)=default;
        };
        struct AccessibleGroup final {
            bool compact=false;ii::gui::Size viewport;ii::gui::Widget root;
            std::vector<AccessibleAction> actions;std::vector<ii::gui::Widget> widgets;
        };
        TableInput authored;
        std::shared_ptr<const TableSnapshot> snapshot;
        std::shared_ptr<const TableViewportSource> source;
        std::shared_ptr<const TableViewportFrame> frame;
        typename NativeTableHeaderPresentation::Snapshot header_ink;
        std::optional<TableSelection> selection;
        std::optional<TableRange> range;
        std::optional<Resize> resize;
        std::optional<Context> context;
        std::optional<StructurePress> structure_press;
        std::optional<std::string> sorted_column, pressed_header, focused_header;
        TableSortOrder sort_order = TableSortOrder::ascending;
        ii::gui::Widget root, header, scroll, body, edit_host, row_controls, column_controls, first_row, first_column, row_actions, column_actions;
        AccessibleGroup row_accessibility,column_accessibility;
        std::optional<NativeMenu> structure_menu;
        std::optional<NativeMenuItem> delete_row_item, delete_column_item;
        std::shared_ptr<TableEditing> editing;
        std::optional<NativeInputField> field;
        std::optional<TableCellEvent> edit_event;
        ii::gui::Rect edit_frame;
        ii::gui::Size extent;
        ii::gui::ScrollViewport viewport;
        std::function<void(const ii::gui::ScrollViewport&)> viewport_handler;
        TableViewportAnchor anchor;
        NativeTableCallbacks callbacks;
        NativeTableClipboard clipboard;
        std::unique_ptr<NativeTableViewGroup> header_views,cell_views;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner = std::this_thread::get_id();
        double committed_x = 0, committed_y = 0;
        std::uint64_t horizontal_movement = 0, vertical_movement = 0, revision = 0;
        bool live = false, viewport_seen = false, rendering = false, selecting = false, committing = false;
        std::optional<ii::gui::ScrollViewport> pending;
    };
    explicit BasicNativeTable(std::shared_ptr<State> state) : state_(std::move(state)) {}
    [[nodiscard]] static ii::core::Error convert(const TableError& value) {
        const auto code = value.code == TableErrorCode::invalid ? ii::core::ErrorCode::invalid_argument :
                          value.code == TableErrorCode::not_found ? ii::core::ErrorCode::not_found :
                          value.code == TableErrorCode::wrong_thread ? ii::core::ErrorCode::wrong_executor :
                          value.code == TableErrorCode::exhausted ? ii::core::ErrorCode::out_of_range : ii::core::ErrorCode::busy;
        return {code, value.message};
    }
    [[nodiscard]] static bool valid(const TableInput& input) noexcept {
        return input.controller && (!input.editable || input.editor) && (!input.sorting_enabled || input.order_editor) &&
            (!(input.column_resize_enabled || input.row_resize_enabled) || input.structure_editor) &&
            (!input.structure_controls_visible || input.structure_editor) &&
            std::isfinite(input.column_resize_handle) && input.column_resize_handle >= 1 && input.column_resize_handle <= 256 &&
            std::isfinite(input.row_resize_handle) && input.row_resize_handle >= 1 && input.row_resize_handle <= 256 &&
            std::isfinite(input.structure_gutter_width) && input.structure_gutter_width >= 1 && input.structure_gutter_width <= 256 &&
            std::isfinite(input.structure_gutter_height) && input.structure_gutter_height >= 1 && input.structure_gutter_height <= 256 &&
            input.source_revision >= 0 && input.header_icon_revision >= 0 && valid_table_appearance(input.appearance) && valid_table_text(input.text);
    }
    [[nodiscard]] static double row_gutter(const TableInput& input, double width) noexcept {
        return input.structure_controls_visible && input.add_row_controls_visible ? (std::min)(input.structure_gutter_width, (std::max)(0., width - 1)) : 0.;
    }
    [[nodiscard]] static double column_gutter(const TableInput& input, double height, double header) noexcept {
        return input.structure_controls_visible && input.add_column_controls_visible ? (std::min)(input.structure_gutter_height, (std::max)(0., height - header - 1)) : 0.;
    }
    [[nodiscard]] static double content_width(const TableInput& input, double width) noexcept { return width - row_gutter(input, width); }
    [[nodiscard]] static double content_height(const TableInput& input, double height, double header) noexcept {
        return height - header - column_gutter(input, height, header);
    }
    [[nodiscard]] static ii::core::Result<void> initial_controls(const std::shared_ptr<State>& state) {
        const auto width=state->extent.width,height=state->extent.height;
        const auto header=(std::min)(state->authored.appearance.header_height,(std::max)(0.,height-1));
        const auto row=row_gutter(state->authored,width),column=column_gutter(state->authored,height,header);
        const auto view_width=(std::max)(0.,width-row),view_height=(std::max)(0.,height-header-column);
        if(auto result=state->row_actions.set_frame({0,0,float(row),float(view_height)});!result)return result;
        if(auto result=state->column_actions.set_frame({0,0,float(view_width),float(column)});!result)return result;
        if(auto result=state->first_row.set_frame({0,0,float(row),float((std::min)(24.,view_height))});!result)return result;
        if(auto result=state->first_column.set_frame({0,0,float((std::min)(view_width,state->authored.appearance.fallback_column_width)),float(column)});!result)return result;
        if(auto result=state->row_actions.set_visible(row>0&&!state->snapshot->rows.empty());!result)return result;
        if(auto result=state->column_actions.set_visible(column>0&&!state->snapshot->columns.empty());!result)return result;
        if(auto result=state->first_row.set_visible(row>0&&state->snapshot->rows.empty());!result)return result;
        return state->first_column.set_visible(column>0&&state->snapshot->columns.empty());
    }
    [[nodiscard]] static ii::core::Result<void> synchronize_text(const std::shared_ptr<State>& state) {
        const auto revision=state->revision;const auto text=state->authored.text;
        if(auto changed=state->first_row.set_accessibility_name(text.add_first_row);!changed)return changed;
        if(state->revision!=revision)return {};
        if(auto changed=state->first_column.set_accessibility_name(text.add_first_column);!changed)return changed;
        if(state->revision!=revision)return {};
        if(state->structure_menu){
            auto options=state->structure_menu->options();if(!options)return std::unexpected(options.error());
            if(options->configuration.accessibility_name!=text.structure_menu){
                options->configuration.accessibility_name=text.structure_menu;
                if(auto changed=state->structure_menu->set_options(*options);!changed)return changed;
            }
        }
        if(state->revision!=revision)return {};
        for(const bool row:{true,false}){
            auto& item=row?state->delete_row_item:state->delete_column_item;if(!item)continue;
            auto options=item->options();if(!options)return std::unexpected(options.error());
            const auto& label=row?text.delete_row:text.delete_column;
            if(options->configuration.label!=label){options->configuration.label=label;
                if(auto changed=item->set_options(*options);!changed)return changed;}
            if(state->revision!=revision)return {};
        }
        if(state->field)if(auto changed=state->field->set_accessibility_name(text.edit_cell);!changed)return changed;
        return {};
    }
    [[nodiscard]] static ii::gui::Color color(StyleColor value) noexcept {
        const auto channel = [](double c) { return c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4); };
        return {float(channel(value.red)), float(channel(value.green)), float(channel(value.blue)), float(value.alpha)};
    }
    [[nodiscard]] static std::string bounded_text(std::string_view input, std::size_t limit) {
        if (input.size() <= limit) return std::string(input);
        auto end = limit; while (end && (static_cast<unsigned char>(input[end]) & 0xc0) == 0x80) --end;
        return std::string(input.substr(0, end));
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const TableViewportSource>> source(
            std::shared_ptr<const TableSnapshot> snapshot, const TableAppearance& appearance, double width) {
        auto value = TableViewportSource::create(std::move(snapshot), {.width=width,
            .fallback_column_width=appearance.fallback_column_width, .minimum_column_width=appearance.minimum_column_width,
            .fit_flexible_columns=appearance.fit_flexible_columns});
        if (!value) return std::unexpected(convert(value.error())); return std::move(*value);
    }
    [[nodiscard]] static ii::core::Result<void> selection(const std::shared_ptr<State>& state) {
        auto controller = state->authored.controller; const auto revision = state->revision;
        auto selected = controller->selection(); if (!selected) return std::unexpected(convert(selected.error()));
        auto range = controller->range(); if (!range) return std::unexpected(convert(range.error()));
        auto snapshot = controller->source_snapshot(); if (!snapshot) return std::unexpected(convert(snapshot.error()));
        if (state->revision != revision || state->authored.controller != controller || (*snapshot)->revision != state->snapshot->revision)
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed during selection");
        state->selection = std::move(*selected); state->range = std::move(*range); return {};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const TableViewportFrame>> plan(
            const std::shared_ptr<State>& state, const ii::gui::ScrollViewport& viewport) {
        TableViewportRequest request{.height=double(viewport.bounds.height), .header_height=0,
            .x=viewport.horizontal_offset, .y=viewport.offset, .overscan_rows=0, .overscan_columns=0,
            .vertical_movement=state->vertical_movement, .horizontal_movement=state->horizontal_movement, .anchor=state->anchor};
        auto result = state->source->frame(request); if (!result) return std::unexpected(convert(result.error()));
        const auto budget = result->merges.empty() ? std::size_t(3072) : std::size_t(1000);
        if (result->columns.size() && result->rows.size() > budget / result->columns.size())
            return ii::core::fail(ii::core::ErrorCode::out_of_range, "Visible table exceeds the host drawing budget");
        return std::make_shared<const TableViewportFrame>(std::move(*result));
    }
    [[nodiscard]] static ii::core::Result<void> render_views(const std::shared_ptr<State>& state){
        if(!state->frame||!state->snapshot)return {};
        const auto frame=state->frame;const auto snapshot=state->snapshot;const auto revision=state->revision;
        if(state->header_views){
            std::vector<TableViewItem> headers;headers.reserve(frame->columns.size());
            for(const auto& span:frame->columns){
                const auto& column=snapshot->columns[span.index];if(!state->header_views->has(column.key))continue;
                TableViewSnapshot value;value.header=true;value.column_key=column.key;value.column_index=span.index;value.text=column.title;
                value.enabled=state->authored.appearance.enabled&&column.enabled;value.sorted=state->sorted_column==column.key;
                value.descending=value.sorted&&state->sort_order==TableSortOrder::descending;
                value.x=span.offset;value.width=span.extent;value.height=state->authored.appearance.header_height;
                headers.push_back({column.key,std::move(value),{float(span.offset),0,float(span.extent),float(state->authored.appearance.header_height)}});
            }
            if(auto result=state->header_views->update(headers,{frame->width,state->authored.appearance.header_height});!result)return result;
            if(state->revision!=revision||state->frame!=frame||state->snapshot!=snapshot)return {};
        }
        if(state->cell_views){
            std::vector<TableViewItem> cells;cells.reserve(frame->rows.size()*frame->columns.size());
            const auto append=[&](std::size_t row,std::size_t column,std::size_t rows,std::size_t columns,double x,double y,double width,double height){
                const auto& row_data=snapshot->rows[row];const auto& column_data=snapshot->columns[column];
                if(!state->cell_views->has(column_data.key))return;
                const auto& cell=row_data.cells[column];TableViewSnapshot value;
                value.row_key=row_data.key;value.column_key=column_data.key;value.text=cell.text;value.value_type=cell.value_type;
                value.row_index=row;value.column_index=column;value.row_span=rows;value.column_span=columns;
                value.x=x;value.y=y;value.width=width;value.height=height;
                value.enabled=state->authored.appearance.enabled&&row_data.enabled&&column_data.enabled&&cell.enabled;
                value.editable=value.enabled&&state->authored.editable&&column_data.editable&&cell.editable;
                value.selected=state->authored.appearance.selection_enabled&&state->range&&row<=state->range->last_row&&row+rows>state->range->first_row&&column<=state->range->last_column&&column+columns>state->range->first_column;
                value.current=state->selection&&state->selection->current.row==row_data.key&&state->selection->current.column==column_data.key;
                const auto key=std::to_string(row_data.key.size())+":"+row_data.key+column_data.key;
                cells.push_back({key,std::move(value),{float(x),float(y),float(width),float(height)}});
            };
            for(const auto& row:frame->rows)for(const auto& column:frame->columns){
                if(state->source->merge_at(row.index,column.index))continue;
                append(row.index,column.index,1,1,column.offset,row.offset,column.extent,row.extent);
            }
            for(const auto& merge:frame->merges)append(merge.range.row,merge.range.column,merge.range.row_count,merge.range.column_count,merge.x,merge.y,merge.width,merge.height);
            if(auto result=state->cell_views->update(cells,{frame->width,frame->height});!result)return result;
        }
        return {};
    }
    [[nodiscard]] static ii::core::Result<void> viewport(const std::shared_ptr<State>& state, ii::gui::ScrollViewport value) {
        if (state->rendering) { state->pending = value; return {}; }
        for (unsigned pass = 0; pass < 8; ++pass) {
            const auto previous_revision=state->revision;const auto previous_source=state->source;const auto previous_frame=state->frame;
            const auto previous_viewport=state->viewport;const auto previous_seen=state->viewport_seen;const auto previous_anchor=state->anchor;
            const auto previous_x=state->committed_x,previous_y=state->committed_y;
            const auto previous_h=state->horizontal_movement,previous_v=state->vertical_movement;
            if (state->viewport_seen && std::abs(value.offset - state->committed_y) > 1e-6) {
                if (state->vertical_movement == (std::numeric_limits<std::uint64_t>::max)()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Table vertical movement exhausted");
                ++state->vertical_movement;
            }
            if (state->viewport_seen && std::abs(value.horizontal_offset - state->committed_x) > 1e-6) {
                if (state->horizontal_movement == (std::numeric_limits<std::uint64_t>::max)()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Table horizontal movement exhausted");
                ++state->horizontal_movement;
            }
            if (value.bounds.width < 1 || value.bounds.height < 1) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Table viewport is too small");
            if (state->source->configuration().width != value.bounds.width) {
                auto resized = source(state->snapshot, state->authored.appearance, value.bounds.width); if (!resized) return std::unexpected(resized.error());
                state->source = std::move(*resized);
            }
            state->viewport = value; state->viewport_seen = true; state->pending.reset(); state->rendering = true;
            ii::core::Result<void> applied;
            try {
                auto next = plan(state, value);
                if (!next) applied = std::unexpected(next.error());
                else {
                    state->frame = std::move(*next); state->anchor = state->frame->anchor;
                    state->committed_x = state->frame->x; state->committed_y = state->frame->y;
                    applied = state->body.set_extent({value.bounds.width, value.bounds.height});
                    if (applied) applied = state->scroll.set_virtual_scroll_content(
                        {state->frame->content_width, state->frame->content_height}, state->frame->x, state->frame->y);
                    if (applied) applied = structure_accessibility(state);
                    if (applied) applied = header_accessibility(state);
                    if (applied) applied = cell_accessibility(state);
                    if (applied) applied = render_views(state);
                    if (applied && state->editing) applied = position_editor(state);
                    if (applied) applied = state->root.request_repaint();
                    if (applied) applied = state->header.request_repaint();
                    if (applied) applied = state->body.request_repaint();
                    if (applied) applied = state->row_controls.request_repaint();
                    if (applied) applied = state->column_controls.request_repaint();
                }
            } catch (...) { applied = ii::core::fail(ii::core::ErrorCode::failed, "Table viewport update failed", std::current_exception()); }
            if(!applied&&state->revision==previous_revision){
                state->source=previous_source;state->frame=previous_frame;state->viewport=previous_viewport;state->viewport_seen=previous_seen;
                state->anchor=previous_anchor;state->committed_x=previous_x;state->committed_y=previous_y;
                state->horizontal_movement=previous_h;state->vertical_movement=previous_v;
                if(previous_frame){
                    (void)state->scroll.set_virtual_scroll_content({previous_frame->content_width,previous_frame->content_height},previous_x,previous_y);
                    (void)render_views(state);(void)structure_accessibility(state);(void)header_accessibility(state);(void)cell_accessibility(state);
                    (void)state->header.request_repaint();(void)state->body.request_repaint();
                }
                if(state->viewport_handler)(void)state->scroll.set_scroll_handler(state->viewport_handler);
                state->pending.reset();
            }
            state->rendering = false;
            if (!applied) return applied;
            if (!state->pending) return {};
            value = *state->pending; state->pending.reset();
        }
        return ii::core::fail(ii::core::ErrorCode::busy, "Table viewport did not settle");
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint_header(
            const std::shared_ptr<State>& state, const ii::gui::WidgetPaintState& paint) {
        auto output = std::make_shared<ii::gui::WidgetDrawing>(); if (!state->frame) return std::shared_ptr<const ii::gui::WidgetDrawing>(output);
        if (!state->header_ink) return ii::core::fail(ii::core::ErrorCode::failed, "Table header presentation is unavailable");
        output->images_before_text = true;
        const auto& a = state->authored.appearance; const auto background = color(a.palette.background), divider = color(a.palette.divider), ink = color(a.palette.header_text);
        if (paint.size.width > 0 && paint.size.height > 0) output->panels.push_back({{0, 0, float(paint.size.width), float(paint.size.height)}, background, background});
        const auto budget = std::max<std::size_t>(4, 128 * 1024 / std::max<std::size_t>(1, state->frame->columns.size()));
        for (const auto& column : state->frame->columns) {
            const auto& data = state->snapshot->columns[column.index];
            const bool sorted = state->authored.sorting_enabled && state->sorted_column == data.key;
            const auto indicator = sorted && column.extent >= 24 ? 12. : 0.;
            const bool custom=state->header_views&&state->header_views->has(data.key);
            if (!custom)if (auto rendered = state->header_ink->draw(*output, column.index, data.title, column.offset, column.extent,
                    paint.size.height, paint, ink, a.font_size, a.font_size + 3, indicator, budget); !rendered)
                return std::unexpected(rendered.error());
            if (!custom&&indicator && paint.size.height > 0) output->texts.push_back({
                .bounds={float(column.offset + column.extent - indicator), 0, float(indicator), float(paint.size.height)},
                .text=state->sort_order == TableSortOrder::ascending ? "↑" : "↓", .color=ink,
                .font_size=float(a.font_size), .font_weight=500, .alignment=ii::gui::TextAlignment::center,
                .font_family=paint.font_family, .line_height=float(a.font_size + 3),
                .clip=ii::gui::Rect{float(column.offset), 0, float(column.extent), float(paint.size.height)}});
            if (paint.focused && state->authored.sorting_enabled && state->focused_header == data.key && paint.size.height >= 2) {
                const auto left_edge = (std::max)(0., column.offset), right_edge = (std::min)(paint.size.width, column.offset + column.extent);
                if (right_edge - left_edge >= 2) {
                    const auto focus = color(a.palette.current);
                    output->panels.push_back({{float(left_edge), 0, float(right_edge - left_edge), float(paint.size.height)}, focus, focus, 0, 1});
                }
            }
            const auto edge = column.offset + column.extent - 1;
            if (edge >= 0 && edge < paint.size.width && paint.size.height > 0) output->panels.push_back({{float(edge), 0, 1, float(paint.size.height)}, divider, divider});
        }
        if (state->resize && state->resize->column) for (const auto& column : state->frame->columns)
            if (state->snapshot->columns[column.index].key == state->resize->key) {
                const auto edge = column.offset + state->resize->preview; const auto ink = color(a.palette.current);
                if (edge >= 0 && edge < paint.size.width && paint.size.height > 0) output->panels.push_back({{float(edge), 0, 1, float(paint.size.height)}, ink, ink});
                break;
            }
        if (paint.size.width > 0 && paint.size.height >= 1) output->panels.push_back({{0, float(paint.size.height - 1), float(paint.size.width), 1}, divider, divider});
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(output));
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint_background(
            const std::shared_ptr<State>& state, const ii::gui::WidgetPaintState& paint) {
        auto output = std::make_shared<ii::gui::WidgetDrawing>();
        const auto background = color(state->authored.appearance.palette.background);
        if (paint.size.width > 0 && paint.size.height > 0)
            output->panels.push_back({{0, 0, float(paint.size.width), float(paint.size.height)}, background, background});
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(output));
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint_body(
            const std::shared_ptr<State>& state, const ii::gui::WidgetPaintState& paint) {
        auto output = std::make_shared<ii::gui::WidgetDrawing>(); if (!state->frame) return std::shared_ptr<const ii::gui::WidgetDrawing>(output);
        const auto& a = state->authored.appearance; const auto background = color(a.palette.background), divider = color(a.palette.divider);
        const auto selection = color(a.palette.selection), current = color(a.palette.current), ink = color(a.palette.cell_text);
        if (paint.size.width > 0 && paint.size.height > 0) output->panels.push_back({{0, 0, float(paint.size.width), float(paint.size.height)}, background, background});
        const auto count = state->frame->rows.size() * state->frame->columns.size();
        const auto budget = std::max<std::size_t>(4, 800 * 1024 / std::max<std::size_t>(1, count));
        const auto draw_cell = [&](std::size_t row, std::size_t column, std::size_t rows, std::size_t columns,
                                   double x, double y, double width, double height) {
            const auto left = (std::max)(0., x), top = (std::max)(0., y);
            const auto right = (std::min)(paint.size.width, x + width), bottom = (std::min)(paint.size.height, y + height);
            if (right <= left || bottom <= top) return;
            const auto& cell = state->snapshot->rows[row].cells[column];
            const bool selected = a.selection_enabled && state->range && row <= state->range->last_row && row + rows > state->range->first_row &&
                                  column <= state->range->last_column && column + columns > state->range->first_column;
            if (selected) output->panels.push_back({{float(left), float(top), float(right-left), float(bottom-top)}, selection, selection});
            const auto padding = (std::min)(a.cell_padding, width / 2), available = width - 2 * padding;
            const auto text_width = (std::min)(available, 8192.), text_height = (std::min)(height, 8192.);
            const auto text_x = x + padding, text_y = y + (height - text_height) / 2;
            const bool custom=state->cell_views&&state->cell_views->has(state->snapshot->columns[column].key);
            if (!custom&&!cell.text.empty() && text_width > 0 && text_height > 0 && text_x >= -32767 && text_y >= -32767 &&
                text_x < paint.size.width && text_y < paint.size.height && text_x + text_width > 0 && text_y + text_height > 0) output->texts.push_back({
                .bounds={float(text_x), float(text_y), float(text_width), float(text_height)},
                .text=bounded_text(cell.text, budget), .color=ink, .font_size=float(a.font_size), .font_weight=500,
                .alignment=ii::gui::TextAlignment::left, .font_family=paint.font_family, .line_height=float(a.font_size + 3),
                .elide_right=true, .clip=ii::gui::Rect{float(left), float(top), float(right-left), float(bottom-top)}});
            if (a.selection_enabled && state->selection && state->selection->current.row == state->snapshot->rows[row].key &&
                state->selection->current.column == state->snapshot->columns[column].key && right-left >= 2 && bottom-top >= 2)
                output->panels.push_back({{float(left), float(top), float(right-left), float(bottom-top)}, current, current, 0, 1});
        };
        for (const auto& row : state->frame->rows) for (const auto& column : state->frame->columns) {
            if (state->source->merge_at(row.index, column.index)) continue;
            draw_cell(row.index, column.index, 1, 1, column.offset, row.offset, column.extent, row.extent);
        }
        for (const auto& merge : state->frame->merges)
            draw_cell(merge.range.row, merge.range.column, merge.range.row_count, merge.range.column_count,
                      merge.x, merge.y, merge.width, merge.height);
        for (const auto& row : state->frame->rows) {
            const auto edge = row.offset + row.extent - 1;
            if (edge < 0 || edge >= paint.size.height || paint.size.width <= 0) continue;
            if (state->frame->merges.empty()) { output->panels.push_back({{0, float(edge), float(paint.size.width), 1}, divider, divider}); continue; }
            double start = -1, end = -1;
            const auto flush = [&] { if (start >= 0 && end > start) output->panels.push_back({{float(start), float(edge), float(end-start), 1}, divider, divider}); start = end = -1; };
            for (const auto& column : state->frame->columns) {
                const auto left = (std::max)(0., column.offset), right = (std::min)(paint.size.width, column.offset + column.extent);
                if (right <= left) continue;
                const auto merge = state->source->merge_at(row.index, column.index);
                if (merge && row.index + 1 < merge->row + merge->row_count) { flush(); continue; }
                if (start < 0) start = left; end = right;
            }
            flush();
        }
        for (const auto& column : state->frame->columns) {
            const auto edge = column.offset + column.extent - 1;
            if (edge < 0 || edge >= paint.size.width || paint.size.height <= 0) continue;
            if (state->frame->merges.empty()) { output->panels.push_back({{float(edge), 0, 1, float(paint.size.height)}, divider, divider}); continue; }
            double start = -1, end = -1;
            const auto flush = [&] { if (start >= 0 && end > start) output->panels.push_back({{float(edge), float(start), 1, float(end-start)}, divider, divider}); start = end = -1; };
            for (const auto& row : state->frame->rows) {
                const auto top = (std::max)(0., row.offset), bottom = (std::min)(paint.size.height, row.offset + row.extent);
                if (bottom <= top) continue;
                const auto merge = state->source->merge_at(row.index, column.index);
                if (merge && column.index + 1 < merge->column + merge->column_count) { flush(); continue; }
                if (start < 0) start = top; end = bottom;
            }
            flush();
        }
        if (state->resize) {
            const auto ink = color(a.palette.current);
            if (state->resize->column) for (const auto& column : state->frame->columns)
                if (state->snapshot->columns[column.index].key == state->resize->key) {
                    const auto edge = column.offset + state->resize->preview;
                    if (edge >= 0 && edge < paint.size.width && paint.size.height > 0) output->panels.push_back({{float(edge), 0, 1, float(paint.size.height)}, ink, ink});
                    break;
                }
            if (!state->resize->column) for (const auto& row : state->frame->rows)
                if (state->snapshot->rows[row.index].key == state->resize->key) {
                    const auto edge = row.offset + state->resize->preview;
                    if (edge >= 0 && edge < paint.size.height && paint.size.width > 0) output->panels.push_back({{0, float(edge), float(paint.size.width), 1}, ink, ink});
                    break;
                }
        }
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(output));
    }
    [[nodiscard]] static ii::core::Result<void> layout(const std::shared_ptr<State>& state) {
        const auto width = state->extent.width, height = state->extent.height;
        const auto header = (std::min)(state->authored.appearance.header_height, (std::max)(0., height - 1));
        if (width < 1 || height < 2) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Table extent is too small");
        const auto row = row_gutter(state->authored, width), column = column_gutter(state->authored, height, header);
        const auto view_width = width - row, view_height = height - header - column;
        if (auto result = state->header.set_frame({0, 0, float(view_width), float(header)}); !result) return result;
        if (auto result = state->scroll.set_frame({0, float(header), float(view_width), float(view_height)}); !result) return result;
        if (auto result = state->row_controls.set_frame({float(view_width), float(header), float(row), float(view_height)}); !result) return result;
        if (auto result = state->column_controls.set_frame({0, float(header + view_height), float(view_width), float(column)}); !result) return result;
        if (auto result = state->row_controls.set_visible(row > 0); !result) return result;
        if (auto result = state->column_controls.set_visible(column > 0); !result) return result;
        return initial_controls(state);
    }
    [[nodiscard]] static ii::core::Result<void> refresh(const std::shared_ptr<State>& state) {
        auto controller = state->authored.controller; const auto revision = state->revision; const bool was_live = state->live;
        auto snapshot = controller->source_snapshot(); if (!snapshot) return std::unexpected(convert(snapshot.error()));
        if (state->revision != revision || state->authored.controller != controller || (was_live && !state->live)) return {};
        auto next = source(*snapshot, state->authored.appearance, (std::max)(1., content_width(state->authored, state->extent.width))); if (!next) return std::unexpected(next.error());
        auto header = NativeTableHeaderPresentation::prepare((*snapshot)->columns, state->authored.header_presentation,
            state->authored.appearance.cell_padding, state->authored.header_icon_source, state->authored.header_icon_revision, state->header_ink);
        if (state->revision != revision || state->authored.controller != controller || (was_live && !state->live)) return {};
        if (!header) return std::unexpected(header.error());
        auto latest = controller->source_snapshot(); if (!latest) return std::unexpected(convert(latest.error()));
        if (state->revision != revision || state->authored.controller != controller || (was_live && !state->live)) return {};
        if ((*latest)->revision != (*snapshot)->revision) return ii::core::fail(ii::core::ErrorCode::busy, "Table changed during header preparation");
        auto previous_snapshot = state->snapshot; auto previous_source = state->source; auto previous_frame = state->frame; auto previous_header = state->header_ink;
        auto previous_selection = state->selection; auto previous_range = state->range; const auto previous_anchor = state->anchor;
        auto previous_sorted = state->sorted_column;
        const auto previous_viewport = state->viewport; const auto previous_x = state->committed_x, previous_y = state->committed_y;
        const auto previous_h = state->horizontal_movement, previous_v = state->vertical_movement;
        const auto rollback = [&] { if (state->revision == revision && state->authored.controller == controller) {
            state->snapshot = std::move(previous_snapshot); state->source = std::move(previous_source); state->frame = std::move(previous_frame); state->header_ink = std::move(previous_header);
            state->selection = std::move(previous_selection); state->range = std::move(previous_range); state->anchor = previous_anchor;
            state->sorted_column = std::move(previous_sorted);
            state->viewport = previous_viewport; state->committed_x = previous_x; state->committed_y = previous_y;
            state->horizontal_movement = previous_h; state->vertical_movement = previous_v;
            (void)initial_controls(state); (void)render_views(state); (void)structure_accessibility(state); (void)header_accessibility(state); (void)cell_accessibility(state); (void)state->root.request_repaint(); (void)state->header.request_repaint(); (void)state->body.request_repaint();
            (void)state->row_controls.request_repaint(); (void)state->column_controls.request_repaint();
        }};
        if ((*snapshot)->revision != state->snapshot->revision) state->sorted_column.reset();
        state->snapshot = std::move(*snapshot); state->source = std::move(*next); state->header_ink = std::move(*header);
        if (auto selected = selection(state); !selected) { rollback(); return selected; }
        if (state->viewport_seen) {
            auto rendered = viewport(state, state->viewport);
            if (!rendered) { rollback(); return rendered; }
        }
        if (auto controls=initial_controls(state); !controls) { rollback(); return controls; }
        if (auto painted=state->row_controls.request_repaint(); !painted) { rollback(); return painted; }
        if (auto painted=state->column_controls.request_repaint(); !painted) { rollback(); return painted; }
        return {};
    }
    [[nodiscard]] static ii::core::Result<void> publish_selection(const std::shared_ptr<State>& state) {
        if (auto selected = selection(state); !selected) return selected;
        if (auto rendered = render_views(state); !rendered) return rendered;
        if (auto accessible = cell_accessibility(state); !accessible) return accessible;
        if (auto repainted = state->body.request_repaint(); !repainted) return repainted;
        if (state->live && state->callbacks.selection_changed) { auto callback = state->callbacks.selection_changed; callback(state->selection); }
        return {};
    }
    [[nodiscard]] static std::optional<ii::gui::Rect> editor_rect(const std::shared_ptr<State>& state, const TablePosition& position) {
        if (!state->frame) return std::nullopt;
        const auto clipped = [&](double left, double top, double width, double height) -> std::optional<ii::gui::Rect> {
            const auto x = (std::max)(0., left), y = (std::max)(0., top);
            const auto right = (std::min)(state->frame->width, left + width), bottom = (std::min)(state->frame->height, top + height);
            if (right - x < 1 || bottom - y < 1) return std::nullopt;
            return ii::gui::Rect{float(x), float(y), float(right - x), float(bottom - y)};
        };
        for (const auto& merge : state->frame->merges)
            if (state->snapshot->rows[merge.range.row].key == position.row &&
                state->snapshot->columns[merge.range.column].key == position.column)
                return clipped(merge.x, merge.y, merge.width, merge.height);
        const auto row = std::ranges::find_if(state->frame->rows, [&](const auto& value) { return state->snapshot->rows[value.index].key == position.row; });
        const auto column = std::ranges::find_if(state->frame->columns, [&](const auto& value) { return state->snapshot->columns[value.index].key == position.column; });
        if (row == state->frame->rows.end() || column == state->frame->columns.end()) return std::nullopt;
        return clipped(column->offset, row->offset, column->extent, row->extent);
    }
    [[nodiscard]] static std::string accessible_cell_key(const TablePosition& position) {
        return std::to_string(position.row.size()) + ":" + position.row + position.column;
    }
    [[nodiscard]] static std::optional<TablePosition> accessible_cell_position(std::string_view key) {
        const auto separator=key.find(':');if(separator==key.npos)return std::nullopt;
        std::size_t length=0;const auto parsed=std::from_chars(key.data(),key.data()+separator,length);
        if(parsed.ec!=std::errc{}||parsed.ptr!=key.data()+separator||!length||length>key.size()-separator-1)return std::nullopt;
        auto content=key.substr(separator+1);if(length==content.size())return std::nullopt;
        return TablePosition{std::string(content.substr(0,length)),std::string(content.substr(length))};
    }
    [[nodiscard]] static std::string accessibility_excerpt(std::string_view value,std::size_t limit) {
        if(value.size()<=limit)return std::string(value);
        auto end=limit;while(end&&(static_cast<unsigned char>(value[end])&0xc0)==0x80)--end;
        return std::string(value.substr(0,end));
    }
    [[nodiscard]] static ii::core::Result<void> header_accessibility(const std::shared_ptr<State>& state){
        if(!state->frame||!state->snapshot)return {};
        try{
            const auto frame=state->frame;const auto snapshot=state->snapshot;
            std::vector<ii::gui::VirtualAccessibilityNode> children;children.reserve(frame->columns.size());
            for(const auto& span:frame->columns){
                if(span.index>=snapshot->columns.size())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Table header accessibility index is stale");
                const auto left=(std::max)(0.,span.offset),right=(std::min)(frame->width,span.offset+span.extent);if(right<=left)continue;
                const auto& column=snapshot->columns[span.index];const auto number=std::to_string(span.index+1);
                ii::gui::VirtualAccessibilityNode child;child.key=column.key;
                child.name=column.title.empty()?Policy::format_table_text(state->authored.text.unnamed_column,{{"column",number}}):accessibility_excerpt(column.title,256);
                if(state->authored.sorting_enabled&&column.enabled){
                    child.description=state->sorted_column==column.key?
                        (state->sort_order==TableSortOrder::ascending?state->authored.text.sorted_ascending:state->authored.text.sorted_descending):state->authored.text.sortable_column;
                }
                child.bounds={float(left),0,float(right-left),float(state->authored.appearance.header_height)};
                child.enabled=state->authored.appearance.enabled&&column.enabled;child.focused=state->focused_header==column.key;
                child.role=ii::gui::AccessibilityRole::column_header;child.actionable=state->authored.sorting_enabled;
                children.push_back(std::move(child));
            }
            std::weak_ptr<State> weak=state;
            return state->header.set_virtual_accessibility(std::move(children),[weak](std::string_view key,ii::gui::WidgetInputKind action)->ii::core::Result<void>{
                auto owner=weak.lock();if(!owner||!owner->live)return ii::core::fail(ii::core::ErrorCode::closed,"Table header accessibility owner retired");
                try{auto result=header_accessibility_action(owner,key,action);if(!result)owner->failure=result.error();return result;}
                catch(...){owner->failure=ii::core::Error{ii::core::ErrorCode::failed,"Table header accessibility action failed",std::current_exception()};return std::unexpected(*owner->failure);}
            });
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table header accessibility update failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> header_accessibility_action(const std::shared_ptr<State>& state,std::string_view key,ii::gui::WidgetInputKind action){
        if(!state->authored.appearance.enabled||!state->authored.sorting_enabled)return {};
        auto controller=state->authored.controller;const auto revision=state->revision;
        auto latest=controller->source_snapshot();if(!latest)return std::unexpected(convert(latest.error()));
        if(!state->live||state->revision!=revision||state->authored.controller!=controller)return {};
        if((*latest)->revision!=state->snapshot->revision){
            if(auto refreshed=refresh(state);!refreshed)return refreshed;
            return ii::core::fail(ii::core::ErrorCode::busy,"Table changed before header accessibility action");
        }
        const auto visible=std::ranges::find_if(state->frame->columns,[&](const auto& span){return span.index<state->snapshot->columns.size()&&state->snapshot->columns[span.index].key==key;});
        if(visible==state->frame->columns.end())return ii::core::fail(ii::core::ErrorCode::not_found,"Table header is no longer visible");
        if(!state->snapshot->columns[visible->index].enabled)return {};
        const auto previous=state->focused_header;state->focused_header=std::string(key);
        if(auto focused=state->header.focus();!focused){
            if(state->live&&state->revision==revision&&state->authored.controller==controller)state->focused_header=previous;
            return focused;
        }
        if(!state->live||state->revision!=revision||state->authored.controller!=controller)return {};
        if(auto published=header_accessibility(state);!published){
            if(state->live&&state->revision==revision&&state->authored.controller==controller){state->focused_header=previous;(void)header_accessibility(state);}
            return published;
        }
        if(!state->live||state->revision!=revision||state->authored.controller!=controller)return {};
        if(auto painted=state->header.request_repaint();!painted)return painted;
        if(!state->live||state->revision!=revision||state->authored.controller!=controller)return {};
        if(action==ii::gui::WidgetInputKind::activate){
            const auto order=state->sorted_column==key&&state->sort_order==TableSortOrder::ascending?TableSortOrder::descending:TableSortOrder::ascending;
            return sort(state,std::string(key),order);
        }
        return {};
    }
    [[nodiscard]] static ii::core::Result<void> cell_accessibility(const std::shared_ptr<State>& state) {
        if(!state->frame||!state->snapshot||!state->source)return {};
        const auto& frame=*state->frame;const auto& data=*state->snapshot;
        std::vector<ii::gui::VirtualAccessibilityNode> cells;
        cells.reserve(frame.rows.size()*frame.columns.size());
        std::unordered_map<std::size_t,const TableViewportMerge*> merges;
        for(const auto& merge:frame.merges)merges.emplace(merge.range.row*data.columns.size()+merge.range.column,&merge);
        std::unordered_set<std::size_t> seen;
        const auto clip=[&](double x,double y,double width,double height)->std::optional<ii::gui::Rect>{
            const auto left=(std::max)(0.,x),top=(std::max)(0.,y),right=(std::min)(frame.width,x+width),bottom=(std::min)(frame.height,y+height);
            if(right<=left||bottom<=top)return std::nullopt;
            return ii::gui::Rect{float(left),float(top),float(right-left),float(bottom-top)};
        };
        for(const auto& row:frame.rows)for(const auto& column:frame.columns){
            auto row_index=row.index,column_index=column.index;
            const auto merged=state->source->merge_at(row_index,column_index);
            if(merged){row_index=merged->row;column_index=merged->column;}
            const auto identity=row_index*data.columns.size()+column_index;
            if(!seen.insert(identity).second)continue;
            std::optional<ii::gui::Rect> bounds;
            if(merged){
                const auto found=merges.find(identity);if(found==merges.end())return ii::core::fail(ii::core::ErrorCode::failed,"Visible table merge has no geometry");
                const auto& shape=*found->second;bounds=clip(shape.x,shape.y,shape.width,shape.height);
            }else bounds=clip(column.offset,row.offset,column.extent,row.extent);
            if(!bounds)continue;
            const auto& row_data=data.rows[row_index];const auto& column_data=data.columns[column_index];const auto& value=row_data.cells[column_index];
            const TablePosition position{row_data.key,column_data.key};
            const auto column_number=std::to_string(column_index+1),row_number=std::to_string(row_index+1);
            const auto title=column_data.title.empty()?Policy::format_table_text(state->authored.text.unnamed_column,{{"column",column_number}}):accessibility_excerpt(column_data.title,256);
            const auto selected=state->range&&row_index>=state->range->first_row&&row_index<=state->range->last_row&&
                column_index>=state->range->first_column&&column_index<=state->range->last_column;
            const auto current=state->selection&&state->selection->current==position;
            ii::gui::VirtualAccessibilityNode child;
            child.key=accessible_cell_key(position);child.name=Policy::format_table_text(state->authored.text.cell_name,{{"column",title},{"row",row_number}});
            if(merged)child.description=Policy::format_table_text(state->authored.text.merged_description,
                {{"rows",std::to_string(merged->row_count)},{"columns",std::to_string(merged->column_count)}});
            child.value=accessibility_excerpt(value.text,1024);child.bounds=*bounds;
            child.enabled=state->authored.appearance.enabled&&row_data.enabled&&column_data.enabled&&value.enabled;
            child.selected=selected;child.focused=current;cells.push_back(std::move(child));
        }
        std::weak_ptr<State> weak=state;
        return state->body.set_virtual_accessibility(std::move(cells),[weak](std::string_view key,ii::gui::WidgetInputKind action)->ii::core::Result<void>{
            auto owner=weak.lock();if(!owner||!owner->live)return ii::core::fail(ii::core::ErrorCode::closed,"Table accessibility owner retired");
            try{auto result=cell_accessibility_action(owner,key,action);if(!result)owner->failure=result.error();return result;}
            catch(...){owner->failure=ii::core::Error{ii::core::ErrorCode::failed,"Table accessibility action failed",std::current_exception()};return std::unexpected(*owner->failure);}
        });
    }
    [[nodiscard]] static ii::core::Result<void> cell_accessibility_action(const std::shared_ptr<State>& state,std::string_view key,ii::gui::WidgetInputKind action) {
        if(!state->authored.appearance.enabled||!state->authored.appearance.selection_enabled)return {};
        const auto position=accessible_cell_position(key);if(!position)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid accessible table cell key");
        auto controller=state->authored.controller;const auto revision=state->revision;
        auto snapshot=controller->source_snapshot();if(!snapshot)return std::unexpected(convert(snapshot.error()));
        if(!state->live||state->revision!=revision||state->authored.controller!=controller)return {};
        if((*snapshot)->revision!=state->snapshot->revision){
            if(auto refreshed=refresh(state);!refreshed)return refreshed;
            return ii::core::fail(ii::core::ErrorCode::busy,"Table changed before accessibility action");
        }
        if(!editor_rect(state,*position))return ii::core::fail(ii::core::ErrorCode::not_found,"Accessible table cell is no longer visible");
        auto mode=controller->mode();if(!mode)return std::unexpected(convert(mode.error()));if(*mode==TableSelectionMode::none)return {};
        auto changed=controller->select(*position);if(!changed)return std::unexpected(convert(changed.error()));
        if(!state->live||state->revision!=revision||state->authored.controller!=controller)return {};
        if(auto focused=state->body.focus();!focused)return focused;
        if(!state->live||state->revision!=revision||state->authored.controller!=controller)return {};
        if(*changed)if(auto published=publish_selection(state);!published)return published;
        if(!state->live||state->revision!=revision||state->authored.controller!=controller)return {};
        if(action==ii::gui::WidgetInputKind::activate&&state->authored.editable&&!state->editing){
            auto editing=begin_edit(state);
            if(!editing&&editing.error().code!=ii::core::ErrorCode::invalid_argument&&editing.error().code!=ii::core::ErrorCode::not_found)return editing;
        }
        return {};
    }
    [[nodiscard]] static ii::core::Result<void> clear_edit(const std::shared_ptr<State>& state, bool focus) {
        auto editing = std::exchange(state->editing, {}); state->field.reset(); state->edit_event.reset(); state->failure.reset();
        if (editing) (void)editing->cancel();
        if (auto removed = state->body.set_escape_action({}); !removed) return removed;
        if (auto hidden = state->edit_host.set_visible(false); !hidden) return hidden;
        if (auto cleared = state->edit_host.clear_content(); !cleared) return cleared;
        if (focus && state->live) return state->body.focus(); return {};
    }
    [[nodiscard]] static ii::core::Result<void> position_editor(const std::shared_ptr<State>& state) {
        if (!state->editing || !state->edit_event) return {};
        const auto rect = editor_rect(state, state->edit_event->position);
        if (!rect) return clear_edit(state, false);
        state->edit_frame = *rect; return state->edit_host.set_frame(*rect);
    }
    [[nodiscard]] static ii::core::Result<void> commit_edit(const std::shared_ptr<State>& state) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Table requires its owner thread");
        if (!state->editing || !state->edit_event) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "No table cell is being edited");
        if (state->committing) return ii::core::fail(ii::core::ErrorCode::busy, "Table edit is committing");
        auto editing = state->editing; auto editor = state->authored.editor; auto controller = state->authored.controller;
        if (state->field) {
            auto field = state->field->snapshot(); if (!field) return std::unexpected(field.error());
            if (!state->live || state->editing != editing || state->authored.editor != editor || state->authored.controller != controller)
                return ii::core::fail(ii::core::ErrorCode::busy, "Table changed during its edit");
            auto synchronized = editing->set_draft(std::move(field->text)); if (!synchronized) return std::unexpected(convert(synchronized.error()));
        }
        auto value = editing->state(); if (!value) return std::unexpected(convert(value.error()));
        auto event = *state->edit_event; event.text = value->draft;
        state->committing = true; struct Reset final { bool& flag; ~Reset() { flag = false; } } reset{state->committing};
        auto result = editing->commit();
        if (!state->live || state->editing != editing || state->authored.editor != editor || state->authored.controller != controller) return {};
        if (!result) {
            state->failure = convert(result.error());
            if (state->callbacks.rejected) { auto callback = state->callbacks.rejected; callback(event, result.error()); }
            return std::unexpected(convert(result.error()));
        }
        if (auto cleared = clear_edit(state, true); !cleared) return cleared;
        if (!state->live || state->authored.editor != editor || state->authored.controller != controller) return {};
        auto refreshed = refresh(state); if (!refreshed) state->failure = refreshed.error();
        if (state->live && state->authored.editor == editor && state->authored.controller == controller && state->callbacks.submitted) {
            auto callback = state->callbacks.submitted; callback(event);
        }
        if (!refreshed) return refreshed; state->failure.reset(); return {};
    }
    [[nodiscard]] static ii::core::Result<void> begin_edit(const std::shared_ptr<State>& state) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Table requires its owner thread");
        if (auto current = state->root.snapshot(); !current) return std::unexpected(current.error());
        if (!state->live || !state->authored.appearance.enabled || !state->authored.editable || !state->authored.editor)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Table editing is unavailable");
        if (state->editing || state->committing) return ii::core::fail(ii::core::ErrorCode::busy, "A table edit is already active");
        auto controller = state->authored.controller; auto editor = state->authored.editor; const auto revision = state->revision;
        auto snapshot = controller->source_snapshot(); if (!snapshot) return std::unexpected(convert(snapshot.error()));
        if (!state->live || state->revision != revision || state->authored.controller != controller || state->authored.editor != editor)
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before editing");
        if ((*snapshot)->revision != state->snapshot->revision) {
            if (auto refreshed = refresh(state); !refreshed) return refreshed;
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before editing");
        }
        auto selected = controller->selection(); if (!selected) return std::unexpected(convert(selected.error()));
        if (!*selected) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Select a table cell before editing");
        auto candidate = TableEditing::begin(*snapshot, editor, (*selected)->current); if (!candidate) return std::unexpected(convert(candidate.error()));
        auto value = candidate->state(); if (!value) return std::unexpected(convert(value.error()));
        auto row = std::ranges::find_if((*snapshot)->rows, [&](const auto& item) { return item.key == value->position.row; });
        auto column = std::ranges::find_if((*snapshot)->columns, [&](const auto& item) { return item.key == value->position.column; });
        const auto row_index = std::size_t(row - (*snapshot)->rows.begin()), column_index = std::size_t(column - (*snapshot)->columns.begin());
        TableViewportRequest reveal{.height=double(state->viewport.bounds.height), .header_height=0,
            .x=state->viewport.horizontal_offset, .y=state->viewport.offset, .overscan_rows=0, .overscan_columns=0,
            .reveal_row=value->position.row, .reveal_column=value->position.column};
        auto target = state->source->frame(reveal); if (!target) return std::unexpected(convert(target.error()));
        if (auto scrolled = state->scroll.set_scroll_position(target->x, target->y); !scrolled) return scrolled;
        if (!state->live || state->revision != revision || state->authored.controller != controller || state->authored.editor != editor)
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before editing");
        const auto rect = editor_rect(state, value->position); if (!rect) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Table edit cell is not visible");
        if (auto frame = state->edit_host.set_frame(*rect); !frame) return frame;
        InputFieldConfiguration field; field.text = value->draft; field.width = rect->width; field.height = rect->height;
        field.horizontal_padding = (std::min)(state->authored.appearance.cell_padding, double(rect->width) / 2);
        field.corner_radius = 0; field.font_size = state->authored.appearance.font_size; field.accessibility_name = state->authored.text.edit_cell;
        field.palette.background = state->authored.appearance.palette.background; field.palette.text = state->authored.appearance.palette.cell_text;
        field.palette.focus = state->authored.appearance.palette.current;
        field.palette.selection = field.palette.focus; field.palette.selection.alpha *= .38;
        auto staged = std::make_shared<std::optional<NativeInputField>>(); std::weak_ptr<State> weak = state;
        auto mounted = state->edit_host.set_content([weak, field, staged](ii::gui::WidgetBuilder& builder) -> ii::core::Result<void> {
            auto widget = NativeInputField::create(builder, {}, "field", field, {
                .edited = [weak](std::string text) { if (auto state = weak.lock(); state && state->live && state->editing && state->edit_event) try {
                    auto editing = state->editing; auto changed = editing->set_draft(text);
                    if (!changed) { state->failure = convert(changed.error()); return; }
                    if (*changed && state->callbacks.edited) { auto event = *state->edit_event; event.text = std::move(text); auto callback = state->callbacks.edited; callback(event); }
                } catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Table draft callback failed", std::current_exception()}; } },
                .submitted = [weak] { if (auto state = weak.lock(); state && state->live && state->editing) try {
                    if (auto result = commit_edit(state); !result) state->failure = result.error();
                } catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Table commit callback failed", std::current_exception()}; } },
                .focus_changed = [weak](bool focused) { if (!focused) if (auto state = weak.lock(); state && state->live && state->editing && !state->committing) try {
                    if (auto result = clear_edit(state, false); !result) state->failure = result.error();
                } catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Table edit cancellation failed", std::current_exception()}; } }
            });
            if (!widget) return std::unexpected(widget.error()); *staged = *widget; return {};
        });
        if (!mounted) return mounted;
        if (!state->live || state->revision != revision || state->authored.controller != controller || state->authored.editor != editor || !*staged) {
            (void)state->edit_host.clear_content(); return ii::core::fail(ii::core::ErrorCode::busy, "Table changed while opening its editor");
        }
        state->editing = std::make_shared<TableEditing>(std::move(*candidate)); state->field = **staged; state->edit_frame = *rect;
        state->edit_event = TableCellEvent{value->position, row_index, column_index, value->draft, value->value_type};
        if (auto escape = state->body.set_escape_action([weak] { if (auto state = weak.lock(); state && state->live && state->editing) try {
                if (auto result = clear_edit(state, true); !result) state->failure = result.error();
            } catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Table edit cancellation failed", std::current_exception()}; }
        }); !escape) { (void)clear_edit(state, false); return escape; }
        if (auto shown = state->edit_host.set_visible(true); !shown) { (void)clear_edit(state, false); return shown; }
        if (auto focused = state->field->root().focus(); !focused) { (void)clear_edit(state, false); return focused; }
        return {};
    }
    enum class ClipboardOperation { copy, cut, paste };
    [[nodiscard]] static ii::core::Result<void> clipboard(const std::shared_ptr<State>& state, ClipboardOperation operation,
            std::optional<std::string> content = std::nullopt) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Table requires its owner thread");
        if (auto live = state->root.snapshot(); !live) return std::unexpected(live.error());
        if (!state->live || !state->authored.appearance.enabled || !state->authored.appearance.selection_enabled || !state->authored.clipboard_enabled)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Table clipboard is unavailable");
        if (state->editing || state->committing) return ii::core::fail(ii::core::ErrorCode::busy, "A table edit is already active");
        if (operation != ClipboardOperation::copy && (!state->authored.editable || !state->authored.editor))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Table clipboard editing is unavailable");
        const auto revision = state->revision; auto controller = state->authored.controller; auto editor = state->authored.editor;
        auto snapshot = controller->source_snapshot(); if (!snapshot) return std::unexpected(convert(snapshot.error()));
        auto range = controller->range(); if (!range) return std::unexpected(convert(range.error()));
        auto after_selection = controller->source_snapshot(); if (!after_selection) return std::unexpected(convert(after_selection.error()));
        const auto current = [&] { return state->live && state->revision == revision && state->authored.controller == controller &&
            (operation == ClipboardOperation::copy || (state->authored.editor == editor && state->authored.editable)); };
        if (!current()) return ii::core::fail(ii::core::ErrorCode::busy, "Table changed during clipboard access");
        if ((*snapshot)->revision != (*after_selection)->revision || (*snapshot)->revision != state->snapshot->revision) {
            if (auto refreshed = refresh(state); !refreshed) return refreshed;
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before clipboard access");
        }
        if (!*range) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Select table cells before using the clipboard");
        state->committing = true; struct Reset final { bool& flag; ~Reset() { flag = false; } } reset{state->committing};
        std::optional<TableEdit> edit; TableRange affected = **range;
        if (operation == ClipboardOperation::cut) {
            auto prepared = prepare_table_paste(*snapshot, affected, TableClipboardData{{{std::string{}}}});
            if (!prepared) return std::unexpected(convert(prepared.error())); edit = std::move(*prepared);
        }
        if (operation != ClipboardOperation::paste) {
            auto text = serialize_table_clipboard(*snapshot, affected); if (!text) return std::unexpected(convert(text.error()));
            auto write = state->clipboard.write;
            auto result = write ? write(std::move(*text)) : ii::gui::write_clipboard({.text=std::move(*text)});
            if (!result) return result;
            if (!current()) return ii::core::fail(ii::core::ErrorCode::busy, "Table changed while copying");
            if (operation == ClipboardOperation::copy) return {};
        } else {
            if (!content) {
                auto read = state->clipboard.read;
                if (read) { auto result = read(); if (!result) return std::unexpected(result.error()); content = std::move(*result); }
                else { auto result = ii::gui::read_clipboard(); if (!result) return std::unexpected(result.error()); content = std::move(result->text); }
            }
            if (!current()) return ii::core::fail(ii::core::ErrorCode::busy, "Table changed while reading the clipboard");
            auto parsed = Policy::parse_table_clipboard(*content); if (!parsed) return std::unexpected(convert(parsed.error()));
            auto prepared = prepare_table_paste(*snapshot, affected, *parsed); if (!prepared) return std::unexpected(convert(prepared.error()));
            auto index = TableIndex::create(*snapshot); if (!index) return std::unexpected(convert(index.error()));
            affected.last_row = *index->row(prepared->cells.back().row);
            affected.last_column = *index->column(prepared->cells.back().column);
            edit = std::move(*prepared);
        }
        auto submitted = editor->edit(*edit);
        if (!submitted) return std::unexpected(convert(submitted.error()));
        const auto compatible = [&] { return state->live && state->authored.controller == controller && state->authored.editor == editor; };
        if (!compatible() || *submitted == (*snapshot)->revision) return {};
        auto refreshed = refresh(state); if (!refreshed) state->failure = refreshed.error();
        auto published = controller->source_snapshot();
        if (compatible() && published && (*published)->revision == *submitted && state->callbacks.range_values_changed) {
            auto callback = state->callbacks.range_values_changed; callback(affected);
        }
        if (!refreshed) return refreshed; state->failure.reset(); return {};
    }
    [[nodiscard]] static ii::core::Result<void> sort(const std::shared_ptr<State>& state, std::string column, TableSortOrder direction) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Table requires its owner thread");
        if (auto live = state->root.snapshot(); !live) return std::unexpected(live.error());
        if (!state->live || !state->authored.appearance.enabled || !state->authored.sorting_enabled || !state->authored.order_editor)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Table sorting is unavailable");
        if (state->editing || state->committing) return ii::core::fail(ii::core::ErrorCode::busy, "A table edit is already active");
        auto controller = state->authored.controller; auto editor = state->authored.order_editor; auto sorter = state->authored.sorter;
        const auto revision = state->revision;
        const auto current = [&] { return state->live && state->revision == revision && state->authored.controller == controller &&
            state->authored.order_editor == editor && state->authored.sorter == sorter && state->authored.sorting_enabled; };
        auto snapshot = controller->source_snapshot(); if (!snapshot) return std::unexpected(convert(snapshot.error()));
        if (!current()) return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before sorting");
        if ((*snapshot)->revision != state->snapshot->revision) {
            if (auto refreshed = refresh(state); !refreshed) return refreshed;
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before sorting");
        }
        state->committing = true; struct Reset final { bool& flag; ~Reset() { flag = false; } } reset{state->committing};
        TableTextSorter fallback; const auto& policy = sorter ? *sorter : static_cast<const TableSorter&>(fallback);
        auto prepared = prepare_table_sort(*snapshot, std::move(column), direction, policy);
        if (!prepared) return std::unexpected(convert(prepared.error()));
        if (!current()) return ii::core::fail(ii::core::ErrorCode::busy, "Table changed while preparing a sort");
        auto index = TableIndex::create(*snapshot); if (!index) return std::unexpected(convert(index.error()));
        const auto column_index = *index->column(prepared->column);
        auto submitted = editor->reorder(*prepared); if (!submitted) return std::unexpected(convert(submitted.error()));
        const auto compatible = [&] { return state->live && state->authored.controller == controller && state->authored.order_editor == editor &&
            state->authored.sorter == sorter && state->authored.sorting_enabled; };
        if (!compatible()) return {};
        auto refreshed = refresh(state); if (!refreshed) state->failure = refreshed.error();
        auto published = controller->source_snapshot();
        if (compatible() && published && (*published)->revision == *submitted) {
            state->sorted_column = prepared->column; state->sort_order = direction;
            if (auto rendered = render_views(state); !rendered && refreshed) refreshed = rendered;
            if (auto accessible = header_accessibility(state); !accessible && refreshed) refreshed = accessible;
            if (!compatible()) return {};
            if (auto repainted = state->header.request_repaint(); !repainted && refreshed) refreshed = repainted;
            if (compatible() && state->callbacks.sorted) { auto callback = state->callbacks.sorted; callback(column_index, direction); }
        }
        if (!refreshed) return refreshed; state->failure.reset(); return {};
    }
    [[nodiscard]] static ii::core::Result<std::uint64_t> merge(const std::shared_ptr<State>& state, TableMergeEdit edit) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Table requires its owner thread");
        if (auto live = state->root.snapshot(); !live) return std::unexpected(live.error());
        if (!state->live || !state->authored.appearance.enabled || !state->authored.merge_editor)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Table merge editing is unavailable");
        if (state->editing || state->resize || state->committing) return ii::core::fail(ii::core::ErrorCode::busy, "A table edit is already active");
        auto controller = state->authored.controller; auto editor = state->authored.merge_editor; const auto revision = state->revision;
        auto snapshot = controller->source_snapshot(); if (!snapshot) return std::unexpected(convert(snapshot.error()));
        if (!state->live || state->revision != revision || state->authored.controller != controller || state->authored.merge_editor != editor)
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before its merge edit");
        if ((*snapshot)->revision != state->snapshot->revision) {
            if (auto refreshed = refresh(state); !refreshed) return std::unexpected(refreshed.error());
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before its merge edit");
        }
        { auto candidate = prepare_table_merge(*snapshot, edit); if (!candidate) return std::unexpected(convert(candidate.error())); }
        state->committing = true; struct Reset final { bool& flag; ~Reset() { flag = false; } } reset{state->committing};
        auto submitted = editor->apply(edit); if (!submitted) return std::unexpected(convert(submitted.error()));
        const auto compatible = [&] { return state->live && state->authored.controller == controller && state->authored.merge_editor == editor; };
        if (!compatible() || *submitted == (*snapshot)->revision) return *submitted;
        auto refreshed = refresh(state); if (!refreshed) state->failure = refreshed.error();
        auto published = controller->source_snapshot();
        if (compatible() && published && (*published)->revision == *submitted && state->callbacks.merge_changed) {
            auto callback = state->callbacks.merge_changed; callback(edit);
        }
        if (!refreshed) return std::unexpected(refreshed.error()); state->failure.reset(); return *submitted;
    }
    [[nodiscard]] static ii::core::Result<std::uint64_t> structure(const std::shared_ptr<State>& state, TableStructureEdit edit) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Table requires its owner thread");
        if (auto live = state->root.snapshot(); !live) return std::unexpected(live.error());
        if (!state->live || !state->authored.appearance.enabled || !state->authored.structure_editor)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Table structure editing is unavailable");
        if (state->editing || state->resize || state->committing) return ii::core::fail(ii::core::ErrorCode::busy, "A table edit is already active");
        auto controller = state->authored.controller; auto editor = state->authored.structure_editor; const auto revision = state->revision;
        auto snapshot = controller->source_snapshot(); if (!snapshot) return std::unexpected(convert(snapshot.error()));
        if (!state->live || state->revision != revision || state->authored.controller != controller || state->authored.structure_editor != editor)
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before its structure edit");
        if ((*snapshot)->revision != state->snapshot->revision) {
            if (auto refreshed = refresh(state); !refreshed) return std::unexpected(refreshed.error());
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before its structure edit");
        }
        { auto candidate = prepare_table_structure(*snapshot, edit); if (!candidate) return std::unexpected(convert(candidate.error())); }
        const auto previous_selection = state->selection;
        state->committing = true; struct Reset final { bool& flag; ~Reset() { flag = false; } } reset{state->committing};
        auto submitted = editor->apply(edit); if (!submitted) return std::unexpected(convert(submitted.error()));
        const auto compatible = [&] { return state->live && state->authored.controller == controller && state->authored.structure_editor == editor; };
        if (!compatible() || *submitted == (*snapshot)->revision) return *submitted;
        auto refreshed = refresh(state); if (!refreshed) state->failure = refreshed.error();
        auto published = controller->source_snapshot();
        if (refreshed && compatible() && published && (*published)->revision == *submitted &&
            previous_selection != state->selection && state->callbacks.selection_changed) {
            auto callback = state->callbacks.selection_changed; callback(state->selection);
            if (compatible()) published = controller->source_snapshot();
        }
        if (compatible() && published && (*published)->revision == *submitted && state->callbacks.structure_changed) {
            auto callback = state->callbacks.structure_changed; callback(edit);
        }
        if (!refreshed) return std::unexpected(refreshed.error()); state->failure.reset(); return *submitted;
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint_structure_controls(
            const std::shared_ptr<State>& state, const ii::gui::WidgetPaintState& paint, bool row) {
        auto output = std::make_shared<ii::gui::WidgetDrawing>();
        const auto background = color(state->authored.appearance.palette.background);
        auto ink = color(state->authored.appearance.palette.cell_text);
        if (!state->authored.appearance.enabled) ink.alpha *= .4f;
        if (paint.size.width > 0 && paint.size.height > 0)
            output->panels.push_back({{0, 0, float(paint.size.width), float(paint.size.height)}, background, background});
        const auto plus = [&](double x, double y) {
            const auto horizontal_left = (std::max)(0., x - 4), horizontal_right = (std::min)(paint.size.width, x + 4);
            const auto vertical_top = (std::max)(0., y - 4), vertical_bottom = (std::min)(paint.size.height, y + 4);
            if (horizontal_right > horizontal_left && y >= 0 && y + 1 <= paint.size.height)
                output->panels.push_back({{float(horizontal_left), float(y), float(horizontal_right - horizontal_left), 1}, ink, ink});
            if (vertical_bottom > vertical_top && x >= 0 && x + 1 <= paint.size.width)
                output->panels.push_back({{float(x), float(vertical_top), 1, float(vertical_bottom - vertical_top)}, ink, ink});
        };
        if (state->frame) {
            if (row) {
                if(state->snapshot->rows.empty())plus(std::floor(paint.size.width/2),std::floor((std::min)(24.,paint.size.height)/2));
                else for (const auto& item : state->frame->rows) plus(std::floor(paint.size.width / 2), std::floor(item.offset + item.extent / 2));
            } else {
                if(state->snapshot->columns.empty())plus(std::floor((std::min)(paint.size.width,state->authored.appearance.fallback_column_width)/2),std::floor(paint.size.height/2));
                else for (const auto& item : state->frame->columns) plus(std::floor(item.offset + item.extent / 2), std::floor(paint.size.height / 2));
            }
        }
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(output));
    }
    [[nodiscard]] static ii::core::Result<void> perform_structure_action(const std::shared_ptr<State>& state,
            TableStructureIntent intent, std::uint64_t expected_view) {
        if (!state->live || state->revision != expected_view || !state->authored.appearance.enabled || !state->authored.structure_editor)
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before the structure action");
        auto controller = state->authored.controller; auto editor = state->authored.structure_editor;
        auto factory = state->authored.structure_factory; auto snapshot = controller->source_snapshot();
        if (!snapshot) return std::unexpected(convert(snapshot.error()));
        if (!state->live || state->revision != expected_view || state->authored.controller != controller ||
            state->authored.structure_editor != editor || state->authored.structure_factory != factory)
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before the structure action");
        if ((*snapshot)->revision != intent.revision || (*snapshot)->revision != state->snapshot->revision) {
            if (auto refreshed = refresh(state); !refreshed) return refreshed;
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before the structure action");
        }
        const TableDefaultStructureFactory fallback;
        auto edit = plan_table_structure_action(*snapshot, intent, factory ? *factory : fallback);
        if (!edit) return std::unexpected(convert(edit.error()));
        if (!state->live || state->revision != expected_view || state->authored.controller != controller ||
            state->authored.structure_editor != editor || state->authored.structure_factory != factory)
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed while preparing the structure action");
        auto committed = structure(state, std::move(*edit)); if (!committed) return std::unexpected(committed.error());
        return {};
    }
    [[nodiscard]] static ii::core::Result<std::optional<typename State::StructurePress>> prepare_structure_press(
            const std::shared_ptr<State>& state,TableStructureAction action,std::string key) {
        const auto revision=state->revision;auto controller=state->authored.controller;auto snapshot=state->snapshot;
        if(state->editing) {if(auto canceled=clear_edit(state,false);!canceled)return std::unexpected(canceled.error());}
        if(state->resize) {if(auto canceled=clear_resize(state);!canceled)return std::unexpected(canceled.error());}
        if(!state->live||state->revision!=revision||state->authored.controller!=controller||state->snapshot!=snapshot)
            return std::optional<typename State::StructurePress>{};
        return std::optional<typename State::StructurePress>{typename State::StructurePress{TableStructureIntent{action,std::move(key),snapshot->revision},revision}};
    }
    static void activate_first_structure(const std::weak_ptr<State>& weak,bool row) {
        auto state=weak.lock();if(!state||!state->live||!state->authored.appearance.enabled||!state->authored.structure_controls_visible||
            !state->authored.structure_editor||(row?!state->authored.add_row_controls_visible:!state->authored.add_column_controls_visible)||
            (row?!state->snapshot->rows.empty():!state->snapshot->columns.empty()))return;
        try {
            auto press=prepare_structure_press(state,row?TableStructureAction::insert_first_row:TableStructureAction::insert_first_column,{});
            if(!press){state->failure=press.error();return;}if(!*press)return;
            auto result=perform_structure_action(state,std::move((*press)->intent),(*press)->view_revision);
            if(!result)state->failure=result.error();
        }catch(...){state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Table initial structure action failed",std::current_exception()};}
    }
    static void activate_structure(const std::weak_ptr<State>& weak,bool row,std::string key) {
        auto state=weak.lock();if(!state||!state->live||!state->authored.appearance.enabled||!state->authored.structure_controls_visible||
            !state->authored.structure_editor||(row?!state->authored.add_row_controls_visible:!state->authored.add_column_controls_visible))return;
        try{
            auto press=prepare_structure_press(state,row?TableStructureAction::insert_row_after:TableStructureAction::insert_column_after,std::move(key));
            if(!press){state->failure=press.error();return;}if(!*press)return;
            auto result=perform_structure_action(state,std::move((*press)->intent),(*press)->view_revision);
            if(result)state->failure.reset();else state->failure=result.error();
        }catch(...){state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Table structure accessibility action failed",std::current_exception()};}
    }
    [[nodiscard]] static ii::core::Result<void> structure_accessibility(const std::shared_ptr<State>& state){
        if(!state->frame||!state->authored.structure_controls_visible||!state->authored.structure_editor)return {};
        try{
            const auto revision=state->revision;const auto frame=state->frame;const auto snapshot=state->snapshot;
            const auto sync=[&](bool row)->ii::core::Result<void>{
                const auto thickness=row?row_gutter(state->authored,state->extent.width):
                    column_gutter(state->authored,state->extent.height,state->authored.appearance.header_height);
                if(thickness<=0||(row?!state->authored.add_row_controls_visible:!state->authored.add_column_controls_visible))return {};
                const auto length=row?frame->height:frame->width;
                if(length<=0)return {};
                std::vector<typename State::AccessibleAction> actions;
                const auto& spans=row?frame->rows:frame->columns;actions.reserve(spans.size());
                std::weak_ptr<State> weak=state;
                for(const auto& span:spans){
                    if((row&&span.index>=snapshot->rows.size())||(!row&&span.index>=snapshot->columns.size()))
                        return ii::core::fail(ii::core::ErrorCode::out_of_range,"Table structure accessibility index is stale");
                    const auto start=(std::max)(0.,span.offset),end=(std::min)(length,span.offset+span.extent);if(end<=start)continue;
                    auto key=row?snapshot->rows[span.index].key:snapshot->columns[span.index].key;
                    const auto number=std::to_string(span.index+1);
                    auto name=Policy::format_table_text(row?state->authored.text.add_row_after:state->authored.text.add_column_after,
                        {{row?"row":"column",number}});
                    const ii::gui::Rect bounds=row?ii::gui::Rect{0,float(start),float(thickness),float(end-start)}:
                        ii::gui::Rect{float(start),0,float(end-start),float(thickness)};
                    actions.push_back({std::move(key),std::move(name),bounds});
                }
                auto target=row?state->row_actions:state->column_actions;
                auto& cache=row?state->row_accessibility:state->column_accessibility;
                const ii::gui::Size viewport=row?ii::gui::Size{thickness,length}:ii::gui::Size{length,thickness};
                if(cache.compact){
                    if(cache.actions==actions&&cache.viewport==viewport)return {};
                    const bool same=cache.widgets.size()==actions.size()&&cache.actions.size()==actions.size()&&
                        std::equal(actions.begin(),actions.end(),cache.actions.begin(),[](const auto& a,const auto& b){return a.key==b.key;});
                    if(same){
                        if(auto resized=cache.root.set_extent(viewport);!resized)return resized;
                        for(std::size_t i=0;i<actions.size();++i){
                            if(actions[i].bounds!=cache.actions[i].bounds)if(auto moved=cache.widgets[i].set_frame(actions[i].bounds);!moved)return moved;
                            if(actions[i].name!=cache.actions[i].name)if(auto renamed=cache.widgets[i].set_accessibility_name(actions[i].name);!renamed)return renamed;
                            if(state->revision!=revision||state->frame!=frame||state->snapshot!=snapshot)return {};
                        }
                        cache.actions=std::move(actions);cache.viewport=viewport;return {};
                    }
                }else{
                    std::vector<ii::gui::KeyedContentItem> items;items.reserve(actions.size());
                    for(const auto& item:actions)items.push_back({item.key,[weak,row,key=item.key,name=item.name](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
                        ii::gui::ActionOptions options;options.accessibility_name=name;options.pointer_focusable=false;
                        auto action=builder.action_surface({},"action",options,[weak,row,key]{activate_structure(weak,row,key);});
                        if(!action)return std::unexpected(action.error());return {};
                    },item.bounds});
                    auto updated=target.set_keyed_content(std::move(items),{.viewport=viewport});
                    if(updated){
                        if(state->revision!=revision||state->frame!=frame||state->snapshot!=snapshot)return {};
                        for(const auto& item:actions){
                            auto action=target.keyed_content_item(item.key,"action");if(!action)return std::unexpected(action.error());
                            auto options=action->action_options();if(!options)return std::unexpected(options.error());
                            if(options->accessibility_name!=item.name)if(auto renamed=action->set_accessibility_name(item.name);!renamed)return renamed;
                            if(state->revision!=revision||state->frame!=frame||state->snapshot!=snapshot)return {};
                        }
                        return {};
                    }
                    if(updated.error().code!=ii::core::ErrorCode::out_of_range)return updated;
                }
                // A deeply nested host or a nearly full widget domain may not
                // have room for a separate keyed generation per action. A
                // single positioned generation uses one level and fewer nodes.
                ii::gui::Widget compact_root;std::vector<ii::gui::Widget> compact_widgets;compact_widgets.reserve(actions.size());
                auto compact=target.set_content([&](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
                    auto root=builder.absolute_stack({},"actions",viewport);if(!root)return std::unexpected(root.error());compact_root=*root;
                    for(std::size_t i=0;i<actions.size();++i){const auto& item=actions[i];
                        ii::gui::ActionOptions options;options.accessibility_name=item.name;options.pointer_focusable=false;
                        auto action=builder.action_surface(*root,"action-"+std::to_string(i),options,
                            [weak,row,key=item.key]{activate_structure(weak,row,key);});
                        if(!action)return std::unexpected(action.error());
                        if(auto placed=action->set_frame(item.bounds);!placed)return placed;compact_widgets.push_back(*action);
                    }return {};
                });
                if(!compact)return compact;
                if(state->revision!=revision||state->frame!=frame||state->snapshot!=snapshot)return {};
                cache.compact=true;cache.root=std::move(compact_root);cache.widgets=std::move(compact_widgets);cache.actions=std::move(actions);cache.viewport=viewport;
                return {};
            };
            if(auto updated=sync(true);!updated)return updated;
            if(state->revision!=revision||state->frame!=frame||state->snapshot!=snapshot)return {};
            return sync(false);
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table structure accessibility update failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<bool> structure_control_input(const std::shared_ptr<State>& state,
            const ii::gui::WidgetInput& event, bool row) {
        using ii::gui::WidgetInputKind;
        if (!state->live || !state->authored.appearance.enabled || !state->authored.structure_controls_visible ||
            !state->authored.structure_editor || (row ? !state->authored.add_row_controls_visible : !state->authored.add_column_controls_visible)) return false;
        if (Policy::pointer_canceled(event.kind)) { state->structure_press.reset(); return false; }
        if (event.kind != WidgetInputKind::pointer_down && event.kind != WidgetInputKind::pointer_up) return false;
        std::optional<std::string> key;
        auto action=row?TableStructureAction::insert_row_after:TableStructureAction::insert_column_after;
        if (event.local_position && state->frame) {
            const auto x = event.local_position->x, y = event.local_position->y;
            const auto gutter = row ? row_gutter(state->authored, state->extent.width) :
                column_gutter(state->authored, state->extent.height, state->authored.appearance.header_height);
            if (row && x >= 0 && x < gutter && y >= 0 && y < state->frame->height) {
                if(state->snapshot->rows.empty()&&y<(std::min)(24.,state->frame->height)){
                    key=std::string{};action=TableStructureAction::insert_first_row;
                }else for (const auto& visible : state->frame->rows) if (y >= visible.offset && y < visible.offset + visible.extent) {
                    key = state->snapshot->rows[visible.index].key; break;
                }
            }
            if (!row && y >= 0 && y < gutter && x >= 0 && x < state->frame->width) {
                if(state->snapshot->columns.empty()&&x<(std::min)(state->authored.appearance.fallback_column_width,state->frame->width)){
                    key=std::string{};action=TableStructureAction::insert_first_column;
                }else for (const auto& visible : state->frame->columns) if (x >= visible.offset && x < visible.offset + visible.extent) {
                    key = state->snapshot->columns[visible.index].key; break;
                }
            }
        }
        if (event.kind == WidgetInputKind::pointer_down) {
            state->structure_press.reset(); if (!key) return true;
            auto press=prepare_structure_press(state,action,std::move(*key));
            if(!press)return std::unexpected(press.error());state->structure_press=std::move(*press);
            return true;
        }
        auto pressed = std::exchange(state->structure_press, {});
        if (!pressed || !key || pressed->intent.key != *key || pressed->intent.action != action) return true;
        if (auto result = perform_structure_action(state, std::move(pressed->intent), pressed->view_revision); !result)
            return std::unexpected(result.error());
        return true;
    }
    [[nodiscard]] static ii::core::Result<bool> context_request(const std::shared_ptr<State>& state, const ii::gui::WidgetInput& event) {
        if (!state->authored.delete_context_menu_enabled || !state->authored.structure_editor || !event.local_position || !state->frame) return false;
        const auto observed = state->revision;
        if (state->editing) { if (auto canceled = clear_edit(state, false); !canceled) return std::unexpected(canceled.error()); }
        if (state->resize) { if (auto canceled = clear_resize(state); !canceled) return std::unexpected(canceled.error()); }
        if (!state->live || state->revision != observed) return true;
        auto controller = state->authored.controller; const auto revision = state->revision;
        auto latest = controller->source_snapshot(); if (!latest) return std::unexpected(convert(latest.error()));
        if (!state->live || state->revision != revision || state->authored.controller != controller) return true;
        if ((*latest)->revision != state->snapshot->revision) { if (auto updated = refresh(state); !updated) return std::unexpected(updated.error()); return true; }
        const auto x = event.local_position->x, y = event.local_position->y;
        const auto row = std::ranges::find_if(state->frame->rows, [&](const auto& value) { return y >= value.offset && y < value.offset + value.extent; });
        const auto column = std::ranges::find_if(state->frame->columns, [&](const auto& value) { return x >= value.offset && x < value.offset + value.extent; });
        if (row == state->frame->rows.end() || column == state->frame->columns.end()) return true;
        auto row_index = row->index, column_index = column->index;
        if (const auto merge = state->source->merge_at(row_index, column_index)) { row_index = merge->row; column_index = merge->column; }
        const auto& row_data = state->snapshot->rows[row_index]; const auto& column_data = state->snapshot->columns[column_index];
        if (!row_data.enabled || !column_data.enabled || !row_data.cells[column_index].enabled) return true;
        const TablePosition position{row_data.key, column_data.key};
        if (state->authored.select_on_context_click && state->authored.appearance.selection_enabled) {
            auto mode = controller->mode(); if (!mode) return std::unexpected(convert(mode.error()));
            if (*mode != TableSelectionMode::none) {
                auto changed = controller->select(position); if (!changed) return std::unexpected(convert(changed.error()));
                if (!state->live || state->revision != revision || state->authored.controller != controller) return true;
                if (*changed) if (auto published = publish_selection(state); !published) return std::unexpected(published.error());
                if (!state->live || state->revision != revision || state->authored.controller != controller) return true;
            }
        }
        auto row_options = state->delete_row_item->options(); if (!row_options) return std::unexpected(row_options.error());
        auto column_options = state->delete_column_item->options(); if (!column_options) return std::unexpected(column_options.error());
        row_options->configuration.enabled = !state->snapshot->rows.empty();
        column_options->configuration.enabled = state->snapshot->columns.size() > 1;
        if (auto changed = state->delete_row_item->set_options(*row_options); !changed) return std::unexpected(changed.error());
        if (auto changed = state->delete_column_item->set_options(*column_options); !changed) return std::unexpected(changed.error());
        if (!state->live || state->revision != revision || state->authored.controller != controller) return true;
        state->context = typename State::Context{position.row, position.column, (*latest)->revision, revision};
        if (auto opened = state->structure_menu->root().open_at(event.x, event.y); !opened) { state->context.reset(); return std::unexpected(opened.error()); }
        return true;
    }
    static void context_action(const std::shared_ptr<State>& state, bool row) {
        if (!state->context || !state->authored.delete_context_menu_enabled) return;
        auto context = std::exchange(state->context, {});
        try {
            auto result = perform_structure_action(state, {row ? TableStructureAction::delete_row : TableStructureAction::delete_column,
                row ? context->row : context->column, context->source_revision}, context->view_revision);
            if (!result) state->failure = result.error();
        } catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Table context action failed", std::current_exception()}; }
    }
    [[nodiscard]] static ii::core::Result<void> repaint_resize(const std::shared_ptr<State>& state) {
        if (auto header = state->header.request_repaint(); !header) return header;
        return state->body.request_repaint();
    }
    [[nodiscard]] static ii::core::Result<void> clear_resize(const std::shared_ptr<State>& state) {
        if (!state->resize) return {};
        const bool column = state->resize->column; state->resize.reset();
        if (auto removed = (column ? state->header : state->body).set_escape_action({}); !removed) return removed;
        return repaint_resize(state);
    }
    [[nodiscard]] static ii::core::Result<void> begin_resize(const std::shared_ptr<State>& state, bool column,
            std::string key, std::size_t index, double origin, double size) {
        if (state->editing || state->resize || state->committing) return ii::core::fail(ii::core::ErrorCode::busy, "A table edit is already active");
        auto controller = state->authored.controller; const auto revision = state->revision;
        auto snapshot = controller->source_snapshot(); if (!snapshot) return std::unexpected(convert(snapshot.error()));
        if (!state->live || state->revision != revision || state->authored.controller != controller)
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before resizing");
        if ((*snapshot)->revision != state->snapshot->revision) {
            if (auto refreshed = refresh(state); !refreshed) return refreshed;
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before resizing");
        }
        state->resize = typename State::Resize{column, std::move(key), index, origin, size, size, (*snapshot)->revision, revision};
        std::weak_ptr<State> weak = state;
        if (auto escape = (column ? state->header : state->body).set_escape_action([weak] { if (auto state = weak.lock(); state && state->live && state->resize) try {
                if (auto result = clear_resize(state); !result) state->failure = result.error();
            } catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Table resize cancellation failed", std::current_exception()}; }
        }); !escape) { state->resize.reset(); return escape; }
        auto painted = repaint_resize(state); if (!painted) (void)clear_resize(state); return painted;
    }
    [[nodiscard]] static ii::core::Result<bool> resize_input(const std::shared_ptr<State>& state,
            const ii::gui::WidgetInput& event, bool column) {
        using ii::gui::WidgetInputKind; using ii::gui::WidgetKey;
        if (!state->resize || state->resize->column != column) return false;
        if (Policy::pointer_canceled(event.kind) || (event.kind == WidgetInputKind::key_down && event.key == WidgetKey::escape) ||
            state->revision != state->resize->view_revision) {
            if (auto cleared = clear_resize(state); !cleared) return std::unexpected(cleared.error()); return true;
        }
        if (event.kind != WidgetInputKind::pointer_move && event.kind != WidgetInputKind::pointer_up) return true;
        if (event.local_position) {
            const auto position = column ? event.local_position->x : event.local_position->y;
            state->resize->preview = std::clamp(state->resize->original + position - state->resize->origin, column ? 32. : 18., 8192.);
        }
        if (event.kind == WidgetInputKind::pointer_move) {
            if (auto painted = repaint_resize(state); !painted) return std::unexpected(painted.error()); return true;
        }
        auto completed = *state->resize;
        if (auto cleared = clear_resize(state); !cleared) return std::unexpected(cleared.error());
        if (std::abs(completed.preview - completed.original) < 1e-6) return true;
        TableStructureEdit edit{.revision=completed.source_revision};
        if (column) edit.operations.emplace_back(TableResizeColumns{{{completed.key, completed.preview}}});
        else edit.operations.emplace_back(TableResizeRows{{{completed.key, completed.preview}}});
        auto controller = state->authored.controller; auto editor = state->authored.structure_editor;
        auto submitted = structure(state, std::move(edit)); if (!submitted) return std::unexpected(submitted.error());
        auto published = controller->source_snapshot();
        if (state->live && state->authored.controller == controller && state->authored.structure_editor == editor &&
            published && (*published)->revision == *submitted && *submitted != completed.source_revision) {
            auto callback = column ? state->callbacks.column_resized : state->callbacks.row_resized;
            if (callback) callback(completed.index, completed.preview);
        }
        return true;
    }
    [[nodiscard]] static ii::core::Result<bool> header_input(const std::shared_ptr<State>& state, const ii::gui::WidgetInput& event) {
        using ii::gui::WidgetInputKind; using ii::gui::WidgetKey;
        if (!state->live) return false;
        if (state->resize && state->resize->column) return resize_input(state, event, true);
        if (Policy::pointer_canceled(event.kind) || event.kind == WidgetInputKind::pointer_leave) {
            state->pressed_header.reset(); return false;
        }
        if(event.native_content_control_active||(event.kind==WidgetInputKind::key_down&&event.native_control_active))return false;
        if (event.kind == WidgetInputKind::key_down && state->authored.appearance.enabled && state->authored.sorting_enabled &&
            !event.alt && !event.control && !event.meta) {
            const auto& columns = state->snapshot->columns;
            std::optional<std::size_t> selected;
            for (std::size_t i = 0; i < columns.size(); ++i) if (columns[i].enabled && state->focused_header == columns[i].key) { selected = i; break; }
            if (!selected) for (std::size_t i = 0; i < columns.size(); ++i) if (columns[i].enabled) { selected = i; break; }
            if (!selected) return false;
            if (event.key == WidgetKey::left || event.key == WidgetKey::right || event.key == WidgetKey::home || event.key == WidgetKey::end) {
                const auto direction = event.key == WidgetKey::left || event.key == WidgetKey::end ? -1 : 1;
                auto position = event.key == WidgetKey::home ? std::ptrdiff_t(-1) :
                    event.key == WidgetKey::end ? std::ptrdiff_t(columns.size()) : std::ptrdiff_t(*selected);
                for (position += direction; position >= 0 && position < std::ptrdiff_t(columns.size()); position += direction)
                    if (columns[std::size_t(position)].enabled) { selected = std::size_t(position); break; }
                state->focused_header = columns[*selected].key;
                TableViewportRequest reveal{.height=double(state->viewport.bounds.height), .header_height=0,
                    .x=state->viewport.horizontal_offset, .y=state->viewport.offset, .overscan_rows=0, .overscan_columns=0,
                    .reveal_column=*state->focused_header};
                auto target = state->source->frame(reveal); if (!target) return std::unexpected(convert(target.error()));
                if (auto scrolled = state->scroll.set_scroll_position(target->x, target->y); !scrolled) return std::unexpected(scrolled.error());
                if (auto accessible = header_accessibility(state); !accessible) return std::unexpected(accessible.error());
                if (auto painted = state->header.request_repaint(); !painted) return std::unexpected(painted.error()); return true;
            }
            if (event.key == WidgetKey::enter || event.key == WidgetKey::space) {
                const auto key = columns[*selected].key; state->focused_header = key;
                if (auto accessible = header_accessibility(state); !accessible) return std::unexpected(accessible.error());
                const auto order = state->sorted_column == key && state->sort_order == TableSortOrder::ascending ?
                    TableSortOrder::descending : TableSortOrder::ascending;
                if (auto result = sort(state, key, order); !result) return std::unexpected(result.error()); return true;
            }
            return false;
        }
        if (event.kind != WidgetInputKind::pointer_down && event.kind != WidgetInputKind::pointer_up) return false;
        if (event.kind == WidgetInputKind::pointer_down && state->authored.appearance.enabled && state->authored.column_resize_enabled &&
            event.local_position && state->frame) {
            const auto x = event.local_position->x, y = event.local_position->y;
            if (y >= 0 && y < state->authored.appearance.header_height) for (const auto& column : state->frame->columns) {
                const auto edge = column.offset + column.extent;
                if (x >= edge - state->authored.column_resize_handle && x < edge && state->snapshot->columns[column.index].enabled) {
                    state->pressed_header.reset();
                    if (auto started = begin_resize(state, true, state->snapshot->columns[column.index].key, column.index, x, column.extent); !started)
                        return std::unexpected(started.error());
                    if (auto focused = state->header.focus(); !focused) { (void)clear_resize(state); return std::unexpected(focused.error()); }
                    return true;
                }
            }
        }
        if (!state->authored.appearance.enabled || !state->authored.sorting_enabled) { state->pressed_header.reset(); return true; }
        std::optional<std::string> key;
        if (event.local_position && state->frame) {
            const auto x = event.local_position->x, y = event.local_position->y;
            if (y >= 0 && y < state->authored.appearance.header_height)
                for (const auto& visible : state->frame->columns) if (x >= visible.offset && x < visible.offset + visible.extent) {
                    const auto& column = state->snapshot->columns[visible.index]; if (column.enabled) key = column.key; break;
                }
        }
        if (event.kind == WidgetInputKind::pointer_down) {
            state->pressed_header = key;
            if (key) { state->focused_header = key; if (auto focused = state->header.focus(); !focused) return std::unexpected(focused.error());
                if(state->live)if(auto accessible=header_accessibility(state);!accessible)return std::unexpected(accessible.error()); }
            return true;
        }
        auto pressed = std::exchange(state->pressed_header, {});
        if (!pressed || pressed != key) return true;
        const auto order = state->sorted_column == key && state->sort_order == TableSortOrder::ascending ?
            TableSortOrder::descending : TableSortOrder::ascending;
        if (auto result = sort(state, *key, order); !result) return std::unexpected(result.error());
        return true;
    }
    [[nodiscard]] static ii::core::Result<bool> input(const std::shared_ptr<State>& state, const ii::gui::WidgetInput& event) {
        using ii::gui::WidgetInputKind; using ii::gui::WidgetKey;
        if (!state->live || !state->authored.appearance.enabled) return false;
        if (event.kind == WidgetInputKind::context_request) return event.native_content_control_active?ii::core::Result<bool>{false}:context_request(state,event);
        if (state->resize && !state->resize->column) return resize_input(state, event, false);
        if (Policy::pointer_canceled(event.kind)) { state->selecting = false; return false; }
        if (state->editing) {
            if (event.kind == WidgetInputKind::key_down && event.key == WidgetKey::escape) {
                if (auto canceled = clear_edit(state, true); !canceled) return std::unexpected(canceled.error()); return true;
            }
            if (event.kind == WidgetInputKind::pointer_down && event.local_position) {
                const auto x = event.local_position->x, y = event.local_position->y; const auto frame = state->edit_frame;
                if (x < frame.x || y < frame.y || x >= frame.x + frame.width || y >= frame.y + frame.height) {
                    if (auto canceled = clear_edit(state, false); !canceled) return std::unexpected(canceled.error());
                } else return false;
            } else return false;
        }
        if(event.native_content_control_active||(event.kind==WidgetInputKind::key_down&&event.native_control_active))return false;
        const bool pointer = event.kind == WidgetInputKind::pointer_down || event.kind == WidgetInputKind::pointer_move || event.kind == WidgetInputKind::pointer_up;
        const bool copy_key = event.kind == WidgetInputKind::key_down && (event.key == WidgetKey::copy ||
            ((event.control || event.meta) && (event.text == "c" || event.text == "C")));
        const bool cut_key = event.kind == WidgetInputKind::key_down && (event.key == WidgetKey::cut ||
            ((event.control || event.meta) && (event.text == "x" || event.text == "X")));
        const bool paste_key = event.kind == WidgetInputKind::key_down && (event.control || event.meta) &&
            (event.text == "v" || event.text == "V");
        const bool paste_content = event.kind == WidgetInputKind::text && bool(event.content);
        const bool keyboard = event.kind == WidgetInputKind::key_down &&
            (state->authored.appearance.keyboard_navigation || (state->authored.editable && event.key == WidgetKey::enter) ||
             (state->authored.clipboard_enabled && (copy_key || cut_key || paste_key)));
        if (!pointer && !keyboard && !(state->authored.clipboard_enabled && paste_content)) return false;
        auto controller = state->authored.controller; const auto revision = state->revision;
        auto latest = controller->source_snapshot(); if (!latest) return std::unexpected(convert(latest.error()));
        if (!state->live || state->revision != revision || state->authored.controller != controller) return true;
        if ((*latest)->revision != state->snapshot->revision) {
            state->selecting = false; if (auto result = refresh(state); !result) return std::unexpected(result.error()); return true;
        }
        if (event.kind == WidgetInputKind::pointer_down && state->authored.row_resize_enabled && event.local_position && state->frame) {
            const auto x = event.local_position->x, y = event.local_position->y;
            if (x >= 0 && x < state->frame->width) for (const auto& row : state->frame->rows) {
                const auto edge = row.offset + row.extent;
                if (y >= edge - state->authored.row_resize_handle && y < edge && state->snapshot->rows[row.index].enabled) {
                    if (auto started = begin_resize(state, false, state->snapshot->rows[row.index].key, row.index, y, row.extent); !started)
                        return std::unexpected(started.error());
                    if (auto focused = state->body.focus(); !focused) { (void)clear_resize(state); return std::unexpected(focused.error()); }
                    return true;
                }
            }
        }
        if (!state->authored.appearance.selection_enabled) return pointer && event.kind == WidgetInputKind::pointer_down;
        if (state->authored.clipboard_enabled && (copy_key || cut_key || paste_key || paste_content)) {
            const auto operation = copy_key ? ClipboardOperation::copy : cut_key ? ClipboardOperation::cut : ClipboardOperation::paste;
            auto result = clipboard(state, operation, paste_content ? std::optional<std::string>(event.content->text) : std::nullopt);
            if (!result) return std::unexpected(result.error()); return true;
        }
        if (pointer) {
            if (event.kind == WidgetInputKind::pointer_up) { const bool had = state->selecting; state->selecting = false; return had; }
            if (event.kind == WidgetInputKind::pointer_move && !state->selecting) return false;
            if (!event.local_position || !state->frame) return event.kind == WidgetInputKind::pointer_down || state->selecting;
            const auto x = event.local_position->x, y = event.local_position->y;
            auto row = std::ranges::find_if(state->frame->rows, [&](const auto& value) { return y >= value.offset && y < value.offset + value.extent; });
            auto column = std::ranges::find_if(state->frame->columns, [&](const auto& value) { return x >= value.offset && x < value.offset + value.extent; });
            if (row == state->frame->rows.end() || column == state->frame->columns.end()) return event.kind == WidgetInputKind::pointer_down || state->selecting;
            auto row_index = row->index, column_index = column->index;
            if (const auto merge = state->source->merge_at(row_index, column_index)) { row_index = merge->row; column_index = merge->column; }
            const auto& row_data = state->snapshot->rows[row_index]; const auto& column_data = state->snapshot->columns[column_index];
            if (!row_data.enabled || !column_data.enabled || !row_data.cells[column_index].enabled) return event.kind == WidgetInputKind::pointer_down || state->selecting;
            const TablePosition position{row_data.key, column_data.key};
            auto changed = controller->select(position, event.kind == WidgetInputKind::pointer_move || event.shift);
            if (!changed) return std::unexpected(convert(changed.error()));
            if (!state->live || state->revision != revision || state->authored.controller != controller) return true;
            state->selecting = true;
            if (auto focused = state->body.focus(); !focused) return std::unexpected(focused.error());
            if (!state->live || state->revision != revision || state->authored.controller != controller) return true;
            if (*changed) if (auto published = publish_selection(state); !published) return std::unexpected(published.error());
            return true;
        }
        if (event.alt || event.control || event.meta) return false;
        std::optional<TableDirection> direction;
        if (event.key == WidgetKey::up) direction = TableDirection::up; else if (event.key == WidgetKey::down) direction = TableDirection::down;
        else if (event.key == WidgetKey::left) direction = TableDirection::left; else if (event.key == WidgetKey::right) direction = TableDirection::right;
        else if (event.key == WidgetKey::home) direction = TableDirection::home; else if (event.key == WidgetKey::end) direction = TableDirection::end;
        if (!direction) {
            if (event.key == WidgetKey::enter && state->authored.editable) {
                auto started = begin_edit(state);
                if (!started && started.error().code != ii::core::ErrorCode::invalid_argument && started.error().code != ii::core::ErrorCode::not_found)
                    return std::unexpected(started.error());
                return true;
            }
            return false;
        }
        auto moved = controller->navigate(*direction, event.shift); if (!moved) return std::unexpected(convert(moved.error()));
        if (!state->live || state->revision != revision || state->authored.controller != controller) return true;
        if (*moved) {
            if (auto published = publish_selection(state); !published) return std::unexpected(published.error());
            if (!state->live || state->revision != revision || state->authored.controller != controller || !state->selection) return true;
            TableViewportRequest reveal{.height=double(state->viewport.bounds.height), .header_height=0,
                .x=state->viewport.horizontal_offset, .y=state->viewport.offset, .overscan_rows=0, .overscan_columns=0,
                .reveal_row=state->selection->current.row, .reveal_column=state->selection->current.column};
            auto target = state->source->frame(reveal); if (!target) return std::unexpected(convert(target.error()));
            if (auto scrolled = state->scroll.set_scroll_position(target->x, target->y); !scrolled) return std::unexpected(scrolled.error());
        }
        return true;
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state, TableInput input) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Table requires its owner thread");
        if (!valid(input)) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid table configuration");
        try {
            const auto observed = state->revision; const bool was_live = state->live;
            if (auto live = state->root.snapshot(); !live) return std::unexpected(live.error());
            if (state->revision != observed || (was_live && !state->live)) return {};
            if (state->revision == (std::numeric_limits<std::uint64_t>::max)()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Table revision exhausted");
            auto controller = input.controller;
            auto snapshot = controller->source_snapshot(); if (!snapshot) return std::unexpected(convert(snapshot.error()));
            if (state->revision != observed || (was_live && !state->live)) return {};
            if (state->authored == input && state->snapshot == *snapshot) return {};
            // A palette-only update must not cancel a resize, dismiss a context
            // menu, or replace an active cell editor. Those interactions retain
            // their document revision and only advance their view revision.
            auto comparison = input; comparison.appearance.palette = state->authored.appearance.palette;
            if (comparison == state->authored && state->snapshot == *snapshot) {
                std::optional<InputFieldPalette> old_field_palette;
                if (state->field) {
                    auto options = state->field->options(); if (!options) return std::unexpected(options.error());
                    if (state->revision != observed || (was_live && !state->live)) return {};
                    old_field_palette = options->palette;
                }
                const auto old_palette = state->authored.appearance.palette;
                state->authored.appearance.palette = input.appearance.palette;
                const auto revision = ++state->revision;
                if (state->resize && state->resize->view_revision == observed) state->resize->view_revision = revision;
                if (state->context && state->context->view_revision == observed) state->context->view_revision = revision;
                if (state->structure_press && state->structure_press->view_revision == observed) state->structure_press->view_revision = revision;
                const auto rollback = [&] {
                    if (state->revision != revision) return;
                    state->authored.appearance.palette = old_palette;
                    if (old_field_palette && state->field) (void)state->field->set_palette(*old_field_palette);
                };
                if (old_field_palette && state->field) {
                    auto field_palette = *old_field_palette;
                    field_palette.background = input.appearance.palette.background;
                    field_palette.text = input.appearance.palette.cell_text;
                    field_palette.focus = input.appearance.palette.current;
                    field_palette.selection = field_palette.focus; field_palette.selection.alpha *= .38;
                    if (auto changed = state->field->set_palette(field_palette); !changed) { rollback(); return changed; }
                    if (state->revision != revision || (was_live && !state->live)) return {};
                }
                for (const auto& widget : {state->root, state->header, state->body}) {
                    if (auto painted = widget.request_repaint(); !painted) { rollback(); return painted; }
                    if (state->revision != revision || (was_live && !state->live)) return {};
                }
                state->failure.reset(); return {};
            }
            auto next = source(*snapshot, input.appearance, content_width(input, input.appearance.width)); if (!next) return std::unexpected(next.error());
            auto header = NativeTableHeaderPresentation::prepare((*snapshot)->columns, input.header_presentation,
                input.appearance.cell_padding, input.header_icon_source, input.header_icon_revision, state->header_ink);
            if (state->revision != observed || (was_live && !state->live)) return {};
            if (!header) return std::unexpected(header.error());
            auto latest = controller->source_snapshot(); if (!latest) return std::unexpected(convert(latest.error()));
            if (state->revision != observed || (was_live && !state->live)) return {};
            if ((*latest)->revision != (*snapshot)->revision) return ii::core::fail(ii::core::ErrorCode::busy, "Table changed during header preparation");
            auto candidate = (*next)->frame({.height=content_height(input, input.appearance.height, input.appearance.header_height), .header_height=0,
                .x=state->viewport.horizontal_offset, .y=state->viewport.offset, .overscan_rows=0, .overscan_columns=0});
            if (!candidate) return std::unexpected(convert(candidate.error()));
            const auto candidate_budget=candidate->merges.empty()?std::size_t(3072):std::size_t(1000);
            if (candidate->columns.size() && candidate->rows.size() > candidate_budget / candidate->columns.size())
                return ii::core::fail(ii::core::ErrorCode::out_of_range, "Visible table exceeds the host drawing budget");
            if (state->editing && (state->authored.controller != input.controller || state->authored.editor != input.editor ||
                                   !input.editable || !input.appearance.enabled)) {
                if (auto canceled = clear_edit(state, false); !canceled) return canceled;
                if (state->revision != observed) return {};
            }
            if (state->resize) { if (auto cleared = clear_resize(state); !cleared) return cleared; if (state->revision != observed) return {}; }
            state->structure_press.reset(); state->context.reset();
            if (state->structure_menu) {
                auto popup = state->structure_menu->root().snapshot(); if (!popup) return std::unexpected(popup.error());
                if (popup->opened) {
                    if (auto closed = state->structure_menu->root().close_popup(); !closed) return closed;
                    if (state->revision != observed) return {};
                }
            }
            auto previous = state->authored; auto previous_snapshot = state->snapshot; auto previous_source = state->source; auto previous_frame = state->frame; auto previous_header = state->header_ink;
            auto previous_selection = state->selection; auto previous_range = state->range; const auto previous_extent = state->extent;
            auto previous_sorted = state->sorted_column; auto previous_pressed = state->pressed_header;
            const auto previous_anchor = state->anchor; const auto previous_viewport = state->viewport;
            const auto previous_x = state->committed_x, previous_y = state->committed_y;
            const auto previous_h = state->horizontal_movement, previous_v = state->vertical_movement;
            const auto revision = ++state->revision; state->authored = std::move(input); state->snapshot = std::move(*snapshot); state->source = std::move(*next); state->header_ink = std::move(*header);
            if (previous_snapshot->revision != state->snapshot->revision || previous.controller != state->authored.controller ||
                previous.order_editor != state->authored.order_editor || previous.sorter != state->authored.sorter || !state->authored.sorting_enabled) {
                state->sorted_column.reset(); state->pressed_header.reset();
            }
            state->extent = {state->authored.appearance.width, state->authored.appearance.height};
            auto rollback = [&] { if (state->revision == revision) { state->authored = std::move(previous); state->snapshot = std::move(previous_snapshot);
                state->source = std::move(previous_source); state->frame = std::move(previous_frame); state->header_ink = std::move(previous_header); state->selection = std::move(previous_selection);
                state->range = std::move(previous_range); state->extent = previous_extent; state->anchor = previous_anchor; state->viewport = previous_viewport;
                state->sorted_column = std::move(previous_sorted); state->pressed_header = std::move(previous_pressed);
                state->committed_x = previous_x; state->committed_y = previous_y; state->horizontal_movement = previous_h; state->vertical_movement = previous_v;
                (void)state->root.set_extent(state->extent); (void)layout(state); (void)synchronize_text(state); (void)render_views(state); (void)structure_accessibility(state); (void)header_accessibility(state); (void)cell_accessibility(state); (void)state->root.set_enabled(state->authored.appearance.enabled);
                (void)state->root.set_accessibility_semantics(ii::gui::AccessibilitySemantics{.role=ii::gui::AccessibilityRole::table,.name=state->authored.appearance.accessibility_name});
                if (state->frame) (void)state->scroll.set_scroll_options({.width=content_width(state->authored, state->extent.width),
                    .height=content_height(state->authored, state->extent.height, state->authored.appearance.header_height), .enabled=true,
                    .show_indicator=state->authored.appearance.show_indicator, .virtual_content_height=state->frame->content_height,
                    .virtual_content_width=state->frame->content_width, .horizontal_enabled=true});
                (void)state->root.request_repaint(); (void)state->header.request_repaint(); (void)state->body.request_repaint(); } };
            if (auto selected = selection(state); !selected) { rollback(); return selected; }
            if (auto resized = state->root.set_extent(state->extent); !resized) { rollback(); return resized; }
            if (auto arranged = layout(state); !arranged) { rollback(); return arranged; }
            if (state->revision != revision) return {};
            if (state->viewport_seen) if (auto rendered = viewport(state, state->viewport); !rendered) { rollback(); return rendered; }
            if (!state->viewport_seen) state->frame=std::make_shared<const TableViewportFrame>(std::move(*candidate));
            auto options = ii::gui::ScrollOptions{.width=content_width(state->authored, state->extent.width),
                .height=content_height(state->authored, state->extent.height, state->authored.appearance.header_height), .enabled=true,
                .show_indicator=state->authored.appearance.show_indicator,
                .virtual_content_height=state->frame ? state->frame->content_height : 0,
                .virtual_content_width=state->frame ? state->frame->content_width : 0, .horizontal_enabled=true};
            if (auto changed = state->scroll.set_scroll_options(options); !changed) { rollback(); return changed; }
            if (auto enabled = state->root.set_enabled(state->authored.appearance.enabled); !enabled) { rollback(); return enabled; }
            if (previous.text != state->authored.text) {
                if (auto labels = synchronize_text(state); !labels) { rollback(); return labels; }
                if (state->revision != revision) return {};
                if (auto accessible = structure_accessibility(state); !accessible) { rollback(); return accessible; }
                if (state->revision != revision) return {};
            }
            if (auto semantics = state->root.set_accessibility_semantics(ii::gui::AccessibilitySemantics{
                    .role=ii::gui::AccessibilityRole::table,.name=state->authored.appearance.accessibility_name}); !semantics) { rollback(); return semantics; }
            if (auto accessible = header_accessibility(state); !accessible) { rollback(); return accessible; }
            if (auto accessible = cell_accessibility(state); !accessible) { rollback(); return accessible; }
            if (auto painted = state->root.request_repaint(); !painted) { rollback(); return painted; }
            if (!state->authored.appearance.enabled || !state->authored.appearance.selection_enabled) state->selecting = false;
            state->failure.reset(); return {};
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Table update failed", std::current_exception()); }
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeTable> create(ii::gui::WidgetBuilder& builder, ii::gui::Widget parent,
            std::string_view id, TableInput input, NativeTableCallbacks callbacks = {}, NativeTableClipboard clipboard = {}, NativeTableViews views = {}) {
        if (!valid(input)) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid table configuration");
        try {
            auto snapshot = input.controller->source_snapshot(); if (!snapshot) return std::unexpected(convert(snapshot.error()));
            auto prepared = source(*snapshot, input.appearance, content_width(input, input.appearance.width)); if (!prepared) return std::unexpected(prepared.error());
            auto header_ink = NativeTableHeaderPresentation::prepare((*snapshot)->columns, input.header_presentation,
                input.appearance.cell_padding, input.header_icon_source, input.header_icon_revision);
            if (!header_ink) return std::unexpected(header_ink.error());
            auto latest = input.controller->source_snapshot(); if (!latest) return std::unexpected(convert(latest.error()));
            if ((*latest)->revision != (*snapshot)->revision) return ii::core::fail(ii::core::ErrorCode::busy, "Table changed during header preparation");
            auto state = std::make_shared<State>(); state->authored = std::move(input); state->snapshot = std::move(*snapshot);
            state->source = std::move(*prepared); state->header_ink = std::move(*header_ink); state->callbacks = std::move(callbacks); state->clipboard = std::move(clipboard);
            state->extent = {state->authored.appearance.width, state->authored.appearance.height};
            if (auto selected = selection(state); !selected) return std::unexpected(selected.error());
            std::weak_ptr<State> weak = state;
            auto root = builder.absolute_stack(parent, id, state->extent); if (!root) return std::unexpected(root.error()); state->root = *root;
            if (auto paint = root->set_paint_handler([weak](const auto& view) -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> {
                    auto state = weak.lock(); if (!state) return ii::core::fail(ii::core::ErrorCode::closed, "Table retired"); return paint_background(state, view);
                }); !paint) return std::unexpected(paint.error());
            const auto initial_width = content_width(state->authored, state->extent.width);
            const auto initial_height = content_height(state->authored, state->extent.height, state->authored.appearance.header_height);
            auto header = builder.absolute_stack(*root, std::string(id) + ".header", {initial_width, state->authored.appearance.header_height});
            if (!header) return std::unexpected(header.error()); state->header = *header;
            auto initial = state->source->frame({.height=initial_height, .header_height=0,
                                                  .overscan_rows=0, .overscan_columns=0});
            if (!initial) return std::unexpected(convert(initial.error())); state->frame = std::make_shared<const TableViewportFrame>(std::move(*initial));
            const auto budget = state->frame->merges.empty() ? std::size_t(3072) : std::size_t(1000);
            if (state->frame->columns.size() && state->frame->rows.size() > budget / state->frame->columns.size())
                return ii::core::fail(ii::core::ErrorCode::out_of_range, "Visible table exceeds the host drawing budget");
            auto scroll = builder.scroll_view(*root, std::string(id) + ".scroll", {.width=initial_width,
                .height=initial_height, .enabled=true,
                .show_indicator=state->authored.appearance.show_indicator, .virtual_content_height=state->frame->content_height,
                .virtual_content_width=state->frame->content_width, .horizontal_enabled=true});
            if (!scroll) return std::unexpected(scroll.error()); state->scroll = *scroll;
            auto body = builder.absolute_stack(*scroll, std::string(id) + ".body", {initial_width, initial_height});
            if (!body) return std::unexpected(body.error()); state->body = *body;
            if(views.header||!views.headers.empty()){
                auto host=builder.content_host(*header,std::string(id)+".header-views",{});if(!host)return std::unexpected(host.error());
                if(auto passthrough=host->set_content_input_passthrough(true);!passthrough)return std::unexpected(passthrough.error());
                state->header_views=std::make_unique<NativeTableViewGroup>(*host,std::move(views.header),std::move(views.headers));
            }
            if(views.cell||!views.columns.empty()){
                auto host=builder.content_host(*body,std::string(id)+".cell-views",{});if(!host)return std::unexpected(host.error());
                if(auto passthrough=host->set_content_input_passthrough(true);!passthrough)return std::unexpected(passthrough.error());
                state->cell_views=std::make_unique<NativeTableViewGroup>(*host,std::move(views.cell),std::move(views.columns));
            }
            auto rows = builder.absolute_stack(*root, std::string(id) + ".row-controls", {1, 1});
            if (!rows) return std::unexpected(rows.error()); state->row_controls = *rows;
            auto columns = builder.absolute_stack(*root, std::string(id) + ".column-controls", {1, 1});
            if (!columns) return std::unexpected(columns.error()); state->column_controls = *columns;
            auto row_actions=builder.content_host(*rows,std::string(id)+".row-actions",{1,1});
            if(!row_actions)return std::unexpected(row_actions.error());state->row_actions=*row_actions;
            auto column_actions=builder.content_host(*columns,std::string(id)+".column-actions",{1,1});
            if(!column_actions)return std::unexpected(column_actions.error());state->column_actions=*column_actions;
            ii::gui::ActionOptions first; first.accessibility_name=state->authored.text.add_first_row;
            auto first_row=builder.action_surface(*rows,std::string(id)+".first-row",first,[weak]{activate_first_structure(weak,true);});
            if(!first_row)return std::unexpected(first_row.error());state->first_row=*first_row;
            first.accessibility_name=state->authored.text.add_first_column;
            auto first_column=builder.action_surface(*columns,std::string(id)+".first-column",first,[weak]{activate_first_structure(weak,false);});
            if(!first_column)return std::unexpected(first_column.error());state->first_column=*first_column;
            MenuInput menu_options; menu_options.configuration.accessibility_name = state->authored.text.structure_menu;
            menu_options.configuration.show_icon_slot = false;
            auto menu = NativeMenu::create(builder, *root, std::string(id) + ".structure-menu", menu_options,
                {.closed=[weak] { if (auto state = weak.lock()) state->context.reset(); }});
            if (!menu) return std::unexpected(menu.error()); state->structure_menu = *menu;
            MenuItemInput item; item.configuration.label = state->authored.text.delete_row; item.configuration.has_children = false;
            item.configuration.show_icon_slot = false; item.configuration.show_chevron = false;
            auto delete_row = NativeMenuItem::create(builder, menu->content_root(), std::string(id) + ".delete-row", item,
                {.clicked=[weak] { if (auto state = weak.lock()) context_action(state, true); }});
            if (!delete_row) return std::unexpected(delete_row.error()); state->delete_row_item = *delete_row;
            if (auto registered = menu->register_item(delete_row->weak_group_port()); !registered) return std::unexpected(registered.error());
            item.configuration.label = state->authored.text.delete_column;
            auto delete_column = NativeMenuItem::create(builder, menu->content_root(), std::string(id) + ".delete-column", item,
                {.clicked=[weak] { if (auto state = weak.lock()) context_action(state, false); }});
            if (!delete_column) return std::unexpected(delete_column.error()); state->delete_column_item = *delete_column;
            if (auto registered = menu->register_item(delete_column->weak_group_port()); !registered) return std::unexpected(registered.error());
            if (auto finalized = menu->finalize(); !finalized) return std::unexpected(finalized.error());
            auto edit_host = builder.content_host(*body, std::string(id) + ".editor", {1, 1});
            if (!edit_host) return std::unexpected(edit_host.error()); state->edit_host = *edit_host;
            if (auto frame = edit_host->set_frame({0, 0, 1, 1}); !frame) return std::unexpected(frame.error());
            if (auto hidden = edit_host->set_visible(false); !hidden) return std::unexpected(hidden.error());
            for (const bool row : {true, false}) {
                auto control = row ? *rows : *columns;
                if (auto paint = control.set_paint_handler([weak, row](const auto& view) -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> {
                        auto state = weak.lock(); if (!state) return ii::core::fail(ii::core::ErrorCode::closed, "Table retired");
                        return paint_structure_controls(state, view, row);
                    }); !paint) return std::unexpected(paint.error());
                if (auto handler = control.set_input_handler([weak, row](const ii::gui::WidgetInput& event) -> ii::core::Result<bool> {
                        auto state = weak.lock(); if (!state) return false;
                        try { auto result = structure_control_input(state, event, row); if (!result) state->failure = result.error(); return result; }
                        catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Table structure input failed", std::current_exception()}; return std::unexpected(*state->failure); }
                    }); !handler) return std::unexpected(handler.error());
            }
            if (auto paint = header->set_paint_handler([weak](const auto& view) -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> {
                    auto state = weak.lock(); if (!state) return ii::core::fail(ii::core::ErrorCode::closed, "Table retired"); return paint_header(state, view);
                }); !paint) return std::unexpected(paint.error());
            if (auto handler = header->set_input_handler([weak](const ii::gui::WidgetInput& event) -> ii::core::Result<bool> {
                    auto state = weak.lock(); if (!state) return false;
                    try { auto result = BasicNativeTable::header_input(state, event); if (!result) state->failure = result.error(); return result; }
                    catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Table header input failed", std::current_exception()}; return std::unexpected(*state->failure); }
                }); !handler) return std::unexpected(handler.error());
            if (auto paint = body->set_paint_handler([weak](const auto& view) -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> {
                    auto state = weak.lock(); if (!state) return ii::core::fail(ii::core::ErrorCode::closed, "Table retired"); return paint_body(state, view);
                }); !paint) return std::unexpected(paint.error());
            if (auto handler = body->set_input_handler([weak](const ii::gui::WidgetInput& event) -> ii::core::Result<bool> {
                    auto state = weak.lock(); if (!state) return false;
                    try { auto result = BasicNativeTable::input(state, event); if (!result) state->failure = result.error(); return result; }
                    catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Table input failed", std::current_exception()}; return std::unexpected(*state->failure); }
                }); !handler) return std::unexpected(handler.error());
            if (auto name = root->set_accessibility_semantics(ii::gui::AccessibilitySemantics{
                    .role=ii::gui::AccessibilityRole::table,.name=state->authored.appearance.accessibility_name}); !name) return std::unexpected(name.error());
            const auto on_viewport = [weak](const ii::gui::ScrollViewport& view) {
                if (auto state = weak.lock(); state && state->live) {
                    auto result = viewport(state, view); if (!result) { state->failure = result.error(); throw std::runtime_error(result.error().message); }
                    state->failure.reset();
                }
            };
            state->viewport_handler=on_viewport;
            if (auto handler = scroll->set_scroll_handler(on_viewport); !handler) return std::unexpected(handler.error());
            if (auto arranged = layout(state); !arranged) return std::unexpected(arranged.error());
            if (auto rendered = render_views(state); !rendered) return std::unexpected(rendered.error());
            if (auto accessible = structure_accessibility(state); !accessible) return std::unexpected(accessible.error());
            if (auto accessible = header_accessibility(state); !accessible) return std::unexpected(accessible.error());
            if (auto accessible = cell_accessibility(state); !accessible) return std::unexpected(accessible.error());
            if (auto enabled = root->set_enabled(state->authored.appearance.enabled); !enabled) return std::unexpected(enabled.error());
            if (auto layout_handler = root->set_layout_handler([weak](ii::gui::Rect bounds) { if (auto state = weak.lock()) {
                    if (state->extent.width == bounds.width && state->extent.height == bounds.height) return;
                    state->extent = {bounds.width, bounds.height}; if (auto result = layout(state); !result) state->failure = result.error();
                }}); !layout_handler) return std::unexpected(layout_handler.error());
            if (auto retained = builder.retain(state); !retained) return std::unexpected(retained.error());
            if (auto published = builder.on_publish([weak, on_viewport] { if (auto state = weak.lock()) {
                    state->live = true; if (auto handler = state->scroll.set_scroll_handler(on_viewport); !handler) state->failure = handler.error();
                }}); !published) return std::unexpected(published.error());
            if (auto retired = builder.on_retire([weak] { if (auto state = weak.lock()) { state->live = false; state->selecting = false; state->resize.reset(); if (state->editing) (void)state->editing->cancel(); state->editing.reset(); state->field.reset(); } }); !retired) return std::unexpected(retired.error());
            return BasicNativeTable(std::move(state));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Table construction failed", std::current_exception()); }
    }
    [[nodiscard]] ii::gui::Widget root() const { return state_->root; }
    [[nodiscard]] ii::gui::Widget scroll() const { return state_->scroll; }
    [[nodiscard]] ii::gui::Widget body() const { return state_->body; }
    [[nodiscard]] ii::core::Result<std::shared_ptr<const TableViewportFrame>> frame() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Table requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error()); return state_->frame;
    }
    [[nodiscard]] ii::core::Result<void> set_options(TableInput input) const { return update(state_, std::move(input)); }
    [[nodiscard]] ii::core::Result<void> begin_edit() const {
        try { return begin_edit(state_); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Opening a table edit failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> commit_edit() const {
        try { return commit_edit(state_); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Committing a table edit failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> copy_selection() const {
        try { return clipboard(state_, ClipboardOperation::copy); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Copying table cells failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> cut_selection() const {
        try { return clipboard(state_, ClipboardOperation::cut); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Cutting table cells failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> paste_clipboard() const {
        try { return clipboard(state_, ClipboardOperation::paste); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Pasting table cells failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> paste_text(std::string text) const {
        try { return clipboard(state_, ClipboardOperation::paste, std::move(text)); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Pasting table cells failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> sort(std::string column, TableSortOrder direction) const {
        try { return sort(state_, std::move(column), direction); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Sorting table rows failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<std::uint64_t> apply_structure(TableStructureEdit edit) const {
        try { return structure(state_, std::move(edit)); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Changing table structure failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<std::uint64_t> apply_merge(TableMergeEdit edit) const {
        try { return merge(state_, std::move(edit)); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Changing table merges failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> cancel_edit() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Table requires its owner thread");
        if (!state_->editing) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "No table cell is being edited");
        try { return clear_edit(state_, true); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Canceling a table edit failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Table requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error()); return state_->failure;
    }
    [[nodiscard]] ii::core::Result<NativeInputField> editor() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Table requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error());
        if (!state_->field) return ii::core::fail(ii::core::ErrorCode::not_found, "No table cell is being edited"); return *state_->field;
    }
private:
    std::shared_ptr<State> state_;
};

} // namespace ii::gui
