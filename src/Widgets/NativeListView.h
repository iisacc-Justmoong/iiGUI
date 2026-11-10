#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/ListItemDrawing.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
namespace ii::gui {
template<class Policy>
class BasicNativeListView final {
    using ListViewInput=typename Policy::Input;using NativeListViewCallbacks=typename Policy::Callbacks;using ListRowViewFactory=typename Policy::ViewFactory;
    using ListItemDrawing=typename Policy::Drawing;using ListItemConfiguration=typename Policy::ItemConfiguration;using NativeInputField=typename Policy::InputField;
    using InputFieldConfiguration=typename Policy::InputFieldConfiguration;using NativeIconCache=typename Policy::IconCache;using StyleColor=typename Policy::Color;
    using ListSnapshot=typename Policy::Snapshot;using ListEntry=typename Policy::Entry;using ListController=typename Policy::Controller;
    using ListViewportSource=typename Policy::ViewportSource;using ListViewportAnchor=typename Policy::ViewportAnchor;using ListReveal=typename Policy::Reveal;
    using ListError=typename Policy::Error;using ListErrorCode=typename Policy::ErrorCode;using ListEditEvent=typename Policy::EditEvent;using ListViewEvent=typename Policy::ViewEvent;
    using ListEditing=typename Policy::Editing;using ListDrag=typename Policy::Drag;using ListDragPoint=typename Policy::DragPoint;using ListDragProposal=typename Policy::DragProposal;
    using ListRowContext=typename Policy::RowContext;using ListRowSnapshot=typename Policy::RowSnapshot;
    struct RowInput final { ListItemDrawing drawing; };
    struct Measurement final {double width=0,height=0;std::weak_ptr<const ListSnapshot> snapshot;};
    struct Row final {
        std::string key;
        NativeIconCache icons;
        std::shared_ptr<const RowInput> input;
        std::shared_ptr<ListRowContext> context;
        ii::gui::Widget widget;
        std::optional<ii::gui::Widget> delegate,content,edit_host;
        double last_press=0,last_x=0,last_y=0;
        bool custom=false;
    };
    struct State final {
        ListViewInput authored;
        std::shared_ptr<const ListViewportSource> source;
        std::unordered_map<std::string,std::shared_ptr<Row>> rows;
        std::unordered_map<std::string,Measurement> measurements;
        ii::gui::Widget root,content;
        NativeListViewCallbacks callbacks;
        ListRowViewFactory row_view;
        ii::gui::ScrollViewport viewport;
        std::function<void(const ii::gui::ScrollViewport&)> viewport_handler;
        std::optional<ii::gui::ScrollViewport> pending;
        ListViewportAnchor anchor;
        std::string selected;
        std::optional<ii::core::Error> failure;
        std::shared_ptr<ListEditing> editing;
        std::optional<NativeInputField> field;
        std::optional<ListEditEvent> edit_event;
        std::weak_ptr<Row> edit_row;
        std::optional<ListDrag> drag;
        std::optional<ListDragProposal> drag_proposal;
        std::string drag_key;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t movement=0,revision=0;
        double committed_offset=0,projection_width=0;
        unsigned updates=0;bool live=false,retired=false,viewport_seen=false,rendering=false,measurement_dirty=false,committing=false,edit_rejected=false,dragging=false;
    };
    static void release(const std::shared_ptr<State>& state)noexcept{
        if(state->editing)(void)state->editing->cancel();state->editing.reset();state->field.reset();state->edit_event.reset();
        if(state->drag)state->drag->cancel();state->drag.reset();state->drag_proposal.reset();state->rows.clear();state->measurements.clear();
        state->source.reset();state->authored={};state->callbacks={};state->row_view={};state->viewport_handler={};state->pending.reset();state->failure.reset();
    }
    struct UpdateGuard final{std::shared_ptr<State> state;explicit UpdateGuard(std::shared_ptr<State> value):state(std::move(value)){++state->updates;}
        ~UpdateGuard(){if(--state->updates==0&&state->retired)release(state);}};
    explicit BasicNativeListView(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::core::Error convert(const ListError& error){
        const auto code=error.code==ListErrorCode::wrong_thread?ii::core::ErrorCode::wrong_executor:
            error.code==ListErrorCode::busy?ii::core::ErrorCode::busy:error.code==ListErrorCode::not_found?ii::core::ErrorCode::not_found:
            error.code==ListErrorCode::exhausted?ii::core::ErrorCode::out_of_range:ii::core::ErrorCode::invalid_argument;
        return {code,error.message};
    }
    [[nodiscard]] static bool valid(const ListViewInput& input)noexcept{
        const auto range=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
        const auto color=[&](StyleColor c){return range(c.red,0,1)&&range(c.green,0,1)&&range(c.blue,0,1)&&range(c.alpha,0,1);};
        const auto& p=input.palette;
        return input.controller&&(!(input.editable||input.reorderable)||input.editor)&&input.source_revision>=0&&input.icon_revision>=0&&input.view_revision>=0&&range(input.width,1,8192)&&range(input.height,1,8192)&&
            range(input.row_height,18,256)&&range(input.left_padding,0,8192)&&range(input.right_padding,0,8192)&&range(input.icon_size,0,256)&&
            range(input.spacing,0,8192)&&range(input.corner_radius,0,8192)&&input.overscan<=128&&color(p.text)&&color(p.description)&&
            color(p.disabled_text)&&color(p.idle)&&color(p.hover)&&color(p.pressed)&&color(p.selected)&&color(p.focus);
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ListViewportSource>> measured_projection(
            std::shared_ptr<const ListSnapshot> snapshot,const State* state,double width,double fallback){
        std::unordered_map<std::string,double> heights;
        if(state&&state->row_view&&width>0){
            for(const auto& entry:snapshot->entries){auto found=state->measurements.find(entry.key);
                if(found!=state->measurements.end()&&found->second.snapshot.lock()==snapshot&&
                   std::abs(found->second.width-width)<1e-6&&found->second.height>fallback)
                    heights.emplace(entry.key,found->second.height);}
        }
        auto source=ListViewportSource::create(std::move(snapshot),heights);if(!source)return std::unexpected(convert(source.error()));
        return std::make_shared<const ListViewportSource>(std::move(*source));
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ListViewportSource>> projection(const ListViewInput& input,const State* state=nullptr){
        if(!valid(input))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid flat list input");
        auto refreshed=input.controller->refresh();if(!refreshed)return std::unexpected(convert(refreshed.error()));
        auto snapshot=input.controller->snapshot();if(!snapshot)return std::unexpected(convert(snapshot.error()));
        return measured_projection(std::move(*snapshot),state,state&&state->viewport_seen?state->viewport.bounds.width:0,input.row_height);
    }
    [[nodiscard]] static ii::core::Result<bool> controller_changed(const std::shared_ptr<State>& state,
            const std::shared_ptr<ListController>& controller){
        auto refreshed=controller->refresh();if(!refreshed)return std::unexpected(convert(refreshed.error()));
        auto snapshot=controller->snapshot();if(!snapshot)return std::unexpected(convert(snapshot.error()));
        return *refreshed||*snapshot!=state->source->snapshot();
    }
    [[nodiscard]] static ii::gui::ScrollOptions scroll_options(const State& state){
        return {.width=state.authored.width,.height=state.authored.height,.enabled=true,.show_indicator=state.authored.show_indicator,
            .virtual_content_height=*state.source->content_extent(state.authored.row_height),
            .user_scroll_enabled=state.authored.enabled&&state.authored.interactive&&state.authored.scrollable};
    }
    [[nodiscard]] static ii::gui::ActionOptions action_options(const RowInput& input){
        auto options=list_item_action(input.drawing);
        // Selection commits the current editor on activation; do not retire
        // its keyed generation midway through a new row's pointer press.
        options.pointer_focusable=false;
        return options;
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint(
            const std::shared_ptr<State>& state,const std::shared_ptr<Row>& row,const ii::gui::WidgetPaintState& view){
        if(!row->input)return std::shared_ptr<const ii::gui::WidgetDrawing>(std::make_shared<ii::gui::WidgetDrawing>());
        auto base=list_item_paint(row->input->drawing,view,!row->custom);if(!base)return base;
        if(!state->dragging||!state->drag_proposal||state->drag_proposal->target!=row->key||view.size.width<=0||view.size.height<=0)return base;
        auto drawing=std::make_shared<ii::gui::WidgetDrawing>(**base);
        const auto color=ii::gui::srgb_to_linear(row->input->drawing.model.configuration().palette.focus);
        const auto thickness=float((std::min)(2.,view.size.height));
        drawing->panels.push_back({{0,state->drag_proposal->after?float(view.size.height)-thickness:0,float(view.size.width),thickness},color,color});
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
    }
    [[nodiscard]] static bool current(const std::shared_ptr<State>& state,const std::shared_ptr<Row>& row){
        const auto found=state->rows.find(row->key);return state->live&&found!=state->rows.end()&&found->second==row;
    }
    [[nodiscard]] static bool double_press(const Row& row,const ii::gui::WidgetInput& event)noexcept{
        const auto x=event.local_position?event.local_position->x:event.x;
        const auto y=event.local_position?event.local_position->y:event.y;
        const bool timed=event.native_click_count==0&&std::isfinite(event.timestamp)&&event.timestamp>row.last_press&&row.last_press>0&&
            event.timestamp-row.last_press<=.4&&std::hypot(x-row.last_x,y-row.last_y)<=4;
        return event.native_click_count>=2||timed;
    }
    [[nodiscard]] static ii::core::Result<void> measurement(const std::shared_ptr<State>& state,const std::shared_ptr<Row>& row,double width,double height){
        UpdateGuard guard(state);if(!current(state,row))return {};
        if(!std::isfinite(width)||width<=0||width>8192||!std::isfinite(height)||height<0||height>8192)
            return ii::core::fail(ii::core::ErrorCode::out_of_range,"Invalid flat list delegate measurement");
        const auto snapshot=state->source->snapshot();const auto found=state->measurements.find(row->key);
        if(found!=state->measurements.end()&&found->second.snapshot.lock()==snapshot&&
           std::abs(found->second.width-width)<1e-6&&std::abs(found->second.height-height)<1e-6)return {};
        if(state->measurements.size()>=Policy::max_items&&!state->measurements.contains(row->key))state->measurements.clear();
        state->measurements[row->key]={width,height,snapshot};state->measurement_dirty=true;
        return state->root.set_scroll_handler(state->viewport_handler);
    }
    static void activate(const std::shared_ptr<State>& state,const std::shared_ptr<Row>& row)noexcept;
    [[nodiscard]] static ii::core::Result<void> request_activate(const std::shared_ptr<State>& state,const std::shared_ptr<Row>& row);
    [[nodiscard]] static ii::core::Result<void> begin_edit(const std::shared_ptr<State>& state);
    [[nodiscard]] static ii::core::Result<void> commit_edit(const std::shared_ptr<State>& state);
    [[nodiscard]] static ii::core::Result<void> clear_edit(const std::shared_ptr<State>& state,bool focus);
    [[nodiscard]] static ii::core::Result<bool> row_input(const std::shared_ptr<State>& state,const std::shared_ptr<Row>& row,const ii::gui::WidgetInput& event);
    [[nodiscard]] static ii::core::Result<bool> pointer(const std::shared_ptr<State>& state,const ii::gui::WidgetInput& event);
    [[nodiscard]] static ii::core::Result<void> clear_drag(const std::shared_ptr<State>& state);
    [[nodiscard]] static ii::core::Result<void> drag_visuals(const std::shared_ptr<State>& state,std::string_view old_target);
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const RowInput>> prepare(const std::shared_ptr<State>& state,
            const std::shared_ptr<Row>& row,const ListEntry& item,double width,double height){
        auto result=std::make_shared<RowInput>();ListItemConfiguration c;
        c.label=Policy::display_text(item.label);c.description=Policy::display_text(item.description);
        c.width=width;c.height=height;c.left_padding=state->authored.left_padding;
        c.right_padding=state->authored.right_padding;c.icon_size=state->authored.icon_size;c.spacing=state->authored.spacing;
        c.corner_radius=state->authored.corner_radius;c.enabled=state->authored.enabled&&item.enabled;
        c.selectable=item.selectable;c.interactive=state->authored.interactive;c.selected=state->selected==item.key;c.palette=state->authored.palette;
        if(!state->row_view&&!item.icon_name.empty()&&c.icon_size>0){auto icon=row->icons.resolve(item.icon_name,state->authored.icon_source,c.icon_size);
            if(!icon)return std::unexpected(icon.error());result->drawing.icon=std::move(*icon);c.has_icon=bool(result->drawing.icon);}
        if(!result->drawing.model.set_configuration(std::move(c)))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid flat list row");
        return std::shared_ptr<const RowInput>(std::move(result));
    }
    [[nodiscard]] static ii::core::Result<void> frame(const std::shared_ptr<State>& state,const ii::gui::ScrollViewport& viewport){
        const auto revision=state->revision;
        if(state->row_view&&(state->measurement_dirty||std::abs(state->projection_width-viewport.bounds.width)>1e-6)){
            auto source=measured_projection(state->source->snapshot(),state.get(),viewport.bounds.width,state->authored.row_height);
            if(!source)return std::unexpected(source.error());state->source=std::move(*source);
            state->measurement_dirty=false;state->projection_width=viewport.bounds.width;
        }
        auto planned=state->source->frame({.width=double(viewport.bounds.width),.height=double(viewport.bounds.height),.offset=viewport.offset,
            .row_height=state->authored.row_height,.overscan=state->authored.overscan,.movement=state->movement,.anchor=state->anchor});
        if(!planned)return std::unexpected(convert(planned.error()));
        struct Change final {std::shared_ptr<Row> row;std::shared_ptr<const RowInput> input;std::optional<ListRowSnapshot> snapshot;};
        std::unordered_map<std::string,std::shared_ptr<Row>> next;std::vector<Change> changes;
        std::vector<ii::gui::KeyedContentItem> items;const auto snapshot=state->source->snapshot();
        next.reserve(planned->rows.size());changes.reserve(planned->rows.size());items.reserve(planned->rows.size());
        std::size_t text_bytes=0;std::weak_ptr<State> weak=state;
        for(const auto& cell:planned->rows){
            const auto& item=snapshot->entries[cell.index];auto found=state->rows.find(item.key);
            auto row=found==state->rows.end()?std::make_shared<Row>():found->second;if(row->key.empty())row->key=item.key;
            auto value=prepare(state,row,item,viewport.bounds.width,cell.height);if(!value)return std::unexpected(value.error());
            const auto& configuration=(*value)->drawing.model.configuration();
            const auto bytes=configuration.label.size()+configuration.description.size();
            if(bytes>4*1024*1024-text_bytes)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Visible list text exceeds its frame budget");
            std::optional<ListRowSnapshot> row_snapshot;
            if(state->row_view){row_snapshot=ListRowSnapshot{.entry=item,.source_index=cell.index,.selected=configuration.selected,
                .enabled=configuration.enabled,.height=cell.height};
                if(!row->context){
                    std::weak_ptr<Row> weak_row=row;
                    row_snapshot->trigger=Policy::make_action([weak,weak_row]()->ii::core::Result<void>{
                        auto owner=weak.lock();auto generation=weak_row.lock();
                        if(!owner||!generation)return ii::core::fail(ii::core::ErrorCode::closed,"Flat list row retired");
                        return request_activate(owner,generation);
                    });
                    row->context=Policy::make_context(*row_snapshot);
                }else row_snapshot->trigger=row->context->get().trigger;
            }
            text_bytes+=bytes;next.emplace(row->key,row);changes.push_back({row,*value,std::move(row_snapshot)});std::weak_ptr<Row> weak_row=row;
            items.push_back({row->key,[weak,weak_row,value=*value](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
                auto state=weak.lock();auto row=weak_row.lock();if(!state||!row||!state->live)return ii::core::fail(ii::core::ErrorCode::closed,"Flat list row retired");
                row->input=value;row->custom=bool(state->row_view);
                auto widget=builder.action_surface({},"row",action_options(*value),[weak,weak_row]{if(auto state=weak.lock())if(auto row=weak_row.lock())activate(state,row);});
                if(!widget)return std::unexpected(widget.error());row->widget=*widget;
                if(auto routed=widget->set_input_handler([weak,weak_row](const ii::gui::WidgetInput& event)->ii::core::Result<bool>{
                    auto owner=weak.lock();auto generation=weak_row.lock();if(!owner||!generation)return false;
                    try{return row_input(owner,generation,event);}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Flat list row input failed",std::current_exception());}
                });!routed)return routed;
                if(auto r=widget->set_paint_handler([weak,weak_row](const auto& paint_state)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
                    auto state=weak.lock();auto row=weak_row.lock();if(!state||!row)return ii::core::fail(ii::core::ErrorCode::closed,"Flat list row retired");return paint(state,row,paint_state);
                });!r)return r;
                if(state->row_view){
                    const auto& c=value->drawing.model.configuration();const ii::gui::Size size{c.width,c.height};
                    auto container=builder.absolute_stack(*widget,"delegate",size);if(!container)return std::unexpected(container.error());row->delegate=*container;
                    if(auto clipped=container->set_clip_content(true);!clipped)return clipped;
                    auto content=builder.content_host(*container,"delegate.content",{size.width,0});if(!content)return std::unexpected(content.error());row->content=*content;
                    if(auto positioned=content->set_frame({0,0,float(size.width),float(size.height)});!positioned)return positioned;
                    if(auto passthrough=content->set_content_input_passthrough(true);!passthrough)return passthrough;
                    auto context=row->context;auto factory=state->row_view;
                    if(auto retired=builder.on_retire([context]{Policy::close_context(*context);});!retired)return retired;
                    if(auto mounted=content->set_content([factory=std::move(factory),context=std::move(context)](ii::gui::WidgetBuilder& child){return factory(child,context);});!mounted)return mounted;
                    if(auto observed=content->set_measure_handler([weak,weak_row](const ii::gui::WidgetMeasurement& value){
                        auto owner=weak.lock();auto generation=weak_row.lock();if(!owner||!generation||!current(owner,generation))return;
                        if(auto result=measurement(owner,generation,value.available_width,value.natural.height);!result){
                            owner->failure=result.error();throw std::runtime_error(result.error().message);}
                    });!observed)return observed;
                }
                const auto& c=value->drawing.model.configuration();
                auto editor=builder.content_host(*widget,"editor.host",{c.width,c.height});if(!editor)return std::unexpected(editor.error());
                row->edit_host=*editor;if(auto hidden=editor->set_visible(false);!hidden)return hidden;
                return {};
            },ii::gui::Rect{0,float(cell.y),float(viewport.bounds.width),float(cell.height)}});
        }
        if(state->editing&&state->edit_event&&!state->committing){
            const auto& key=state->edit_event->entry.key;
            auto visible=std::ranges::find_if(planned->rows,[&](const auto& cell){
                return snapshot->entries[cell.index].key==key&&cell.y+cell.height>0&&cell.y<viewport.bounds.height&&
                       snapshot->entries[cell.index].enabled&&snapshot->entries[cell.index].editable;});
            if(visible==planned->rows.end()||state->selected!=key){
                if(auto cleared=clear_edit(state,false);!cleared)return cleared;
                if(state->revision!=revision)return {};
            }
        }
        if(auto r=state->content.set_keyed_content(std::move(items),{.viewport=ii::gui::Size{double(viewport.bounds.width),double(viewport.bounds.height)}});!r)return r;
        if(!state->live)return {};
        for(auto& change:changes){
            if(!change.row->widget.snapshot())return ii::core::fail(ii::core::ErrorCode::closed,"Flat list row did not publish");
            change.row->input=std::move(change.input);
            if(auto r=change.row->widget.set_action_options(action_options(*change.row->input));!r)return r;
            if(change.row->delegate){const auto& c=change.row->input->drawing.model.configuration();
                if(auto r=change.row->delegate->set_enabled(c.enabled);!r)return r;
                if(auto r=change.row->delegate->set_extent({c.width,c.height});!r)return r;
                if(auto r=change.row->content->set_extent({c.width,0});!r)return r;
                if(auto r=change.row->content->set_frame({0,0,float(c.width),float(c.height)});!r)return r;}
            if(change.row->edit_host){const auto& c=change.row->input->drawing.model.configuration();
                if(auto r=change.row->edit_host->set_extent({c.width,c.height});!r)return r;}
            if(auto r=change.row->widget.request_repaint();!r)return r;
        }
        state->rows=std::move(next);state->anchor=planned->anchor;state->committed_offset=planned->offset;
        for(auto& change:changes){
            if(!state->live)return ii::core::fail(ii::core::ErrorCode::closed,"Flat list retired during row publication");
            if(state->revision!=revision)return {};
            if(change.row->context&&change.snapshot&&current(state,change.row))Policy::publish_context(*change.row->context,std::move(*change.snapshot));
        }
        if(state->revision!=revision)return {};
        for(const auto& change:changes)if(change.row->content&&current(state,change.row)){
            auto natural=change.row->content->measure(viewport.bounds.width);if(!natural)return std::unexpected(natural.error());
            if(auto r=measurement(state,change.row,viewport.bounds.width,natural->height);!r)return r;
        }
        return state->root.set_virtual_scroll_content({double(viewport.bounds.width),planned->content_height},0,planned->offset);
    }
    [[nodiscard]] static ii::core::Result<void> viewport(const std::shared_ptr<State>& state,ii::gui::ScrollViewport value){
        if(state->rendering){state->pending=value;return {};}
        for(unsigned pass=0;pass<8;++pass){
            if(state->viewport_seen&&std::abs(value.offset-state->committed_offset)>1e-6){
                if(state->movement==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Flat list movement exhausted");++state->movement;
            }
            state->viewport=value;state->viewport_seen=true;state->pending.reset();state->rendering=true;
            ii::core::Result<void> result;try{result=frame(state,value);}catch(...){result=ii::core::fail(ii::core::ErrorCode::failed,"Flat list viewport failed",std::current_exception());}
            state->rendering=false;if(!result)return result;if(!state->pending)return {};value=*state->pending;state->pending.reset();
        }
        return ii::core::fail(ii::core::ErrorCode::busy,"Flat list viewport did not settle");
    }
    [[nodiscard]] static ii::core::Result<void> refresh(const std::shared_ptr<State>& state){
        if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Flat list revision exhausted");
        auto source=projection(state->authored,state.get());if(!source)return std::unexpected(source.error());
        if(state->drag&&(*source)->snapshot()!=state->source->snapshot()){
            if(auto cleared=clear_drag(state);!cleared)return cleared;
        }
        auto selected=state->authored.controller->selection();if(!selected)return std::unexpected(convert(selected.error()));
        auto old=state->source;auto previous=state->selected;const auto revision=++state->revision;
        state->source=std::move(*source);state->selected=std::move(*selected);
        if(state->viewport_seen){auto r=viewport(state,state->viewport);if(!r){
            if(state->revision==revision){state->source=std::move(old);state->selected=std::move(previous);(void)viewport(state,state->viewport);}return r;}}
        return {};
    }
    [[nodiscard]] static ii::core::Result<void> select(const std::shared_ptr<State>& state,std::string key){
        if(state->editing&&state->edit_event&&state->edit_event->entry.key!=key&&!state->committing){
            auto controller=state->authored.controller;
            if(!state->edit_rejected){
                auto result=commit_edit(state);
                if(!result){if(state->field)(void)state->field->root().focus();return result;}
                if(!state->live||state->authored.controller!=controller)return {};
            }
            if(state->editing){
                auto failure=state->failure.value_or(ii::core::Error{ii::core::ErrorCode::invalid_argument,"List edit was rejected"});
                if(state->field)(void)state->field->root().focus();return std::unexpected(std::move(failure));
            }
        }
        if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Flat list revision exhausted");
        auto selected=state->authored.controller->select(key);if(!selected)return std::unexpected(convert(selected.error()));
        if(!*selected)return {};
        const auto revision=++state->revision;const auto previous=std::exchange(state->selected,std::move(key));
        if(state->editing&&state->edit_event&&state->edit_event->entry.key!=state->selected&&!state->committing){
            if(auto cleared=clear_edit(state,false);!cleared)return cleared;
            if(!state->live||state->revision!=revision)return {};
        }
        std::exception_ptr failure;
        for(const auto& name:{previous,state->selected}){auto found=state->rows.find(name);if(found==state->rows.end()||!found->second->input)continue;
            auto row=found->second;auto input=std::make_shared<RowInput>(*row->input);auto configuration=input->drawing.model.configuration();
            const bool is_selected=name==state->selected;configuration.selected=is_selected;(void)input->drawing.model.set_configuration(std::move(configuration));row->input=std::move(input);
            if(auto r=row->widget.request_repaint();!r)return r;
            if(!state->live||state->revision!=revision)break;
            if(row->context){try{auto value=row->context->get();value.selected=is_selected;Policy::publish_context(*row->context,std::move(value));}
                catch(...){if(!failure)failure=std::current_exception();}
                if(!state->live||state->revision!=revision)break;}}
        if(failure)return ii::core::fail(ii::core::ErrorCode::failed,"Flat list selection observer failed",failure);
        return {};
    }
    [[nodiscard]] static ii::core::Result<bool> navigate(const std::shared_ptr<State>& state,const ii::gui::WidgetInput& event){
        using Key=ii::gui::WidgetKey;
        if(event.kind!=ii::gui::WidgetInputKind::key_down||!state->authored.enabled||!state->authored.interactive||
           state->editing||event.text_input_active||event.native_navigation_active)return false;
        if(event.key==Key::enter&&state->authored.editable){
            if(event.native_control_active&&state->row_view)for(const auto& [key,row]:state->rows)if(row->content){
                auto interaction=row->content->interaction();if(!interaction)return std::unexpected(interaction.error());
                if(interaction->focused)return false;
            }
            auto result=begin_edit(state);
            if(!result&&result.error().code!=ii::core::ErrorCode::invalid_argument&&result.error().code!=ii::core::ErrorCode::not_found&&
               result.error().code!=ii::core::ErrorCode::busy)
                return std::unexpected(result.error());
            return true;
        }
        if(!state->authored.keyboard_navigation)return false;
        if(event.key!=Key::up&&event.key!=Key::down&&event.key!=Key::home&&event.key!=Key::end&&event.key!=Key::page_up&&event.key!=Key::page_down)return false;
        auto controller=state->authored.controller;const auto revision=state->revision;
        auto refreshed=controller_changed(state,controller);if(!refreshed)return std::unexpected(refreshed.error());
        if(!state->live||state->authored.controller!=controller||state->revision!=revision)return true;
        if(*refreshed){if(auto r=refresh(state);!r)return std::unexpected(r.error());return true;}
        const auto snapshot=state->source->snapshot();if(snapshot->entries.empty())return true;
        auto index=std::ranges::find_if(snapshot->entries,[&](const auto& item){return item.key==state->selected;});
        const bool has_selection=index!=snapshot->entries.end();
        const bool backwards=event.key==Key::up||event.key==Key::page_up||event.key==Key::end;
        auto position=has_selection?std::size_t(index-snapshot->entries.begin()):(backwards?snapshot->entries.size()-1:0);
        if(event.key==Key::home)position=0;else if(event.key==Key::end)position=snapshot->entries.size()-1;
        else if(has_selection&&(event.key==Key::page_up||event.key==Key::page_down)){
            const auto top=*state->source->row_offset(position,state->authored.row_height);
            const auto total=*state->source->content_extent(state->authored.row_height);
            const auto target=event.key==Key::page_up?(std::max)(0.,top-state->viewport.bounds.height):
                (std::min)(std::nextafter(total,0.),top+state->viewport.bounds.height);
            auto next=state->source->row_at(target,state->authored.row_height).value_or(position);
            if(next==position){if(event.key==Key::page_up&&position)next=position-1;
                else if(event.key==Key::page_down&&position+1<snapshot->entries.size())next=position+1;}
            position=next;
        }else if(has_selection&&event.key==Key::up)position=position?position-1:0;
        else if(has_selection&&event.key==Key::down)position=(std::min)(snapshot->entries.size()-1,position+1);
        while(!snapshot->entries[position].enabled||!snapshot->entries[position].selectable){
            if(backwards){if(!position)return true;--position;}else{if(position+1==snapshot->entries.size())return true;++position;}
        }
        const auto& item=snapshot->entries[position];if(auto r=select(state,item.key);!r)return std::unexpected(r.error());
        if(!state->live||state->selected!=item.key||state->source->snapshot()!=snapshot)return true;
        const auto top=*state->source->row_offset(position,state->authored.row_height);
        const auto bottom=top+*state->source->row_extent(position,state->authored.row_height);
        auto offset=state->viewport.offset;if(top<offset)offset=top;else if(bottom>offset+state->viewport.bounds.height)offset=bottom-state->viewport.bounds.height;
        if(auto r=state->root.set_scroll_offset(offset);!r)return std::unexpected(r.error());
        auto row=state->rows.find(item.key);if(row!=state->rows.end())(void)row->second->widget.focus();
        if(state->live&&state->callbacks.activated){auto callback=state->callbacks.activated;callback(ListViewEvent{item,position});}
        return true;
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state,ListViewInput input){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Flat list requires its owner thread");
        UpdateGuard guard(state);if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Flat list retired");
        try{
            if(auto live=state->root.snapshot();!live)return std::unexpected(live.error());
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Flat list revision exhausted");
            const auto observed=state->revision;auto source=projection(input,state.get());if(!state->live||state->revision!=observed)return {};if(!source)return std::unexpected(source.error());
            auto selected=input.controller->selection();if(!state->live||state->revision!=observed)return {};if(!selected)return std::unexpected(convert(selected.error()));
            if(state->drag&&(!input.enabled||!input.interactive||!input.reorderable||input.editor!=state->authored.editor||
                             input.controller!=state->authored.controller||(*source)->snapshot()!=state->source->snapshot())){
                if(auto cleared=clear_drag(state);!cleared)return cleared;
            }
            if(state->editing&&(!input.enabled||!input.interactive||!input.editable||input.editor!=state->authored.editor||
                                input.controller!=state->authored.controller)){
                if(auto cleared=clear_edit(state,false);!cleared)return cleared;
            }
            auto old_input=state->authored;auto old_source=state->source;auto old_selected=state->selected;
            const auto revision=++state->revision;state->authored=std::move(input);state->source=std::move(*source);state->selected=std::move(*selected);
            const auto rollback=[&]{if(state->revision==revision){state->authored=old_input;state->source=old_source;state->selected=old_selected;
                if(state->viewport_seen)(void)viewport(state,state->viewport);(void)state->root.set_scroll_options(scroll_options(*state));(void)state->root.set_enabled(state->authored.enabled);}};
            if(state->viewport_seen)if(auto r=viewport(state,state->viewport);!r){rollback();return r;}
            if(state->revision!=revision)return {};
            if(auto r=state->root.set_scroll_options(scroll_options(*state));!r){rollback();return r;}
            if(state->revision!=revision)return {};
            if(auto r=state->root.set_enabled(state->authored.enabled);!r){rollback();return r;}
            if(state->revision!=revision)return {};
            if(state->field&&state->editing&&old_input.palette!=state->authored.palette){
                auto options=state->field->options();if(!options){rollback();return std::unexpected(options.error());}
                if(state->revision!=revision)return {};
                auto palette=options->palette;palette.background=state->authored.palette.idle;
                palette.text=state->authored.palette.text;palette.focus=state->authored.palette.focus;
                palette.selection=palette.focus;palette.selection.alpha*=.38;
                if(auto changed=state->field->set_palette(palette);!changed){rollback();return changed;}
                if(state->revision!=revision)return {};
            }
            state->failure.reset();return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Flat list update failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeListView> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,ListViewInput input,NativeListViewCallbacks callbacks={},ListRowViewFactory row_view={}){
        try{
            auto source=projection(input);if(!source)return std::unexpected(source.error());
            auto selected=input.controller->selection();if(!selected)return std::unexpected(convert(selected.error()));
            auto state=std::make_shared<State>();state->authored=std::move(input);state->source=std::move(*source);state->selected=std::move(*selected);state->callbacks=std::move(callbacks);state->row_view=std::move(row_view);
            std::weak_ptr<State> weak=state;auto root=builder.scroll_view(parent,id,scroll_options(*state));if(!root)return std::unexpected(root.error());state->root=*root;
            auto content=builder.content_host(*root,std::string(id)+".rows",{state->authored.width,state->authored.height});if(!content)return std::unexpected(content.error());state->content=*content;
            state->viewport_handler=[weak](const ii::gui::ScrollViewport& value){if(auto state=weak.lock();state&&state->live){
                UpdateGuard guard(state);auto result=viewport(state,value);if(!result){state->failure=result.error();
                    if(result.error().code==ii::core::ErrorCode::busy)(void)state->root.set_scroll_handler(state->viewport_handler);
                    throw std::runtime_error(result.error().message);}state->failure.reset();}};
            if(auto r=root->set_scroll_handler(state->viewport_handler);!r)return std::unexpected(r.error());
            if(auto r=root->set_input_handler([weak](const auto& event)->ii::core::Result<bool>{auto state=weak.lock();if(!state)return false;
                UpdateGuard guard(state);try{auto handled=pointer(state,event);if(!handled){state->failure=handled.error();return handled;}if(*handled)return true;
                    auto result=navigate(state,event);if(!result)state->failure=result.error();return result;}
                catch(...){state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Flat list input failed",std::current_exception()};return std::unexpected(*state->failure);}});!r)return std::unexpected(r.error());
            if(auto r=root->set_enabled(state->authored.enabled);!r)return std::unexpected(r.error());
            if(auto r=builder.retain(state);!r)return std::unexpected(r.error());
            if(auto r=builder.on_publish([weak]{if(auto state=weak.lock()){state->live=true;if(auto r=state->root.set_scroll_handler(state->viewport_handler);!r)state->failure=r.error();}});!r)return std::unexpected(r.error());
            if(auto r=builder.on_retire([weak]{if(auto state=weak.lock()){state->live=false;state->retired=true;
                if(state->revision!=(std::numeric_limits<std::uint64_t>::max)())++state->revision;
                if(state->updates==0)release(state);}});!r)return std::unexpected(r.error());
            return BasicNativeListView(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Flat list construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<ii::gui::Widget> row(std::string_view key)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Flat list requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());auto found=state_->rows.find(std::string(key));
        if(found==state_->rows.end())return ii::core::fail(ii::core::ErrorCode::not_found,"Flat list row is not mounted");return found->second->widget;
    }
    [[nodiscard]] ii::core::Result<std::vector<std::string>> mounted_keys()const{return state_->content.keyed_content_keys();}
    [[nodiscard]] ii::core::Result<void> set_options(ListViewInput input)const{return update(state_,std::move(input));}
    [[nodiscard]] ii::core::Result<void> begin_edit()const{
        try{return begin_edit(state_);}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Opening a list edit failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> commit_edit()const{
        try{return commit_edit(state_);}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Committing a list edit failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> cancel_edit()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Flat list requires its owner thread");
        if(!state_->editing)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"No list row is being edited");
        try{return clear_edit(state_,true);}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Canceling a list edit failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<NativeInputField> editor()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Flat list requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());
        if(!state_->field)return ii::core::fail(ii::core::ErrorCode::not_found,"No list row is being edited");return *state_->field;
    }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Flat list requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->failure;
    }
private:
    std::shared_ptr<State> state_;
};

template<class Policy>
inline ii::core::Result<void> BasicNativeListView<Policy>::clear_edit(const std::shared_ptr<State>& state,bool focus){
    UpdateGuard guard(state);
    auto editing=std::exchange(state->editing,{});auto row=state->edit_row.lock();
    state->edit_row.reset();state->field.reset();state->edit_event.reset();state->failure.reset();state->edit_rejected=false;
    if(editing)(void)editing->cancel();
    if(!state->live)return {};
    if(auto removed=state->root.set_escape_action({});!removed)return removed;
    if(row&&row->edit_host){
        if(auto hidden=row->edit_host->set_visible(false);!hidden&&hidden.error().code!=ii::core::ErrorCode::closed)return hidden;
        if(auto cleared=row->edit_host->clear_content();!cleared&&cleared.error().code!=ii::core::ErrorCode::closed)return cleared;
    }
    if(focus&&row&&current(state,row))return row->widget.focus();return {};
}

template<class Policy>
inline ii::core::Result<void> BasicNativeListView<Policy>::commit_edit(const std::shared_ptr<State>& state){
    if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Flat list requires its owner thread");
    UpdateGuard guard(state);
    if(!state->editing||!state->edit_event||!state->field)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"No list row is being edited");
    if(state->committing)return ii::core::fail(ii::core::ErrorCode::busy,"List edit is committing");
    auto editing=state->editing;auto row=state->edit_row.lock();auto editor=state->authored.editor;auto controller=state->authored.controller;
    auto field=state->field->snapshot();if(!field)return std::unexpected(field.error());
    if(!state->live||state->editing!=editing||state->authored.editor!=editor||state->authored.controller!=controller)
        return ii::core::fail(ii::core::ErrorCode::busy,"List changed during its edit");
    if(auto draft=editing->set_draft(field->text);!draft)return std::unexpected(convert(draft.error()));
    auto event=*state->edit_event;event.text=field->text;
    state->committing=true;struct Reset final{bool& flag;~Reset(){flag=false;}}reset{state->committing};
    auto result=editing->commit();
    if(!state->live||state->editing!=editing||state->authored.editor!=editor||state->authored.controller!=controller)return {};
    if(!row||!current(state,row)){(void)clear_edit(state,false);return {};}
    if(!result){state->edit_rejected=true;state->failure=convert(result.error());
        if(state->callbacks.rejected){auto callback=state->callbacks.rejected;callback(event,result.error());}
        return std::unexpected(convert(result.error()));}
    if(auto cleared=clear_edit(state,true);!cleared)return cleared;
    if(!state->live||state->authored.editor!=editor||state->authored.controller!=controller)return {};
    auto refreshed=refresh(state);if(!refreshed)state->failure=refreshed.error();
    else state->failure.reset();
    if(state->live&&state->authored.editor==editor&&state->authored.controller==controller&&state->callbacks.submitted){
        auto callback=state->callbacks.submitted;callback(event);
    }
    if(!refreshed)return refreshed;return {};
}

template<class Policy>
inline ii::core::Result<void> BasicNativeListView<Policy>::begin_edit(const std::shared_ptr<State>& state){
    if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Flat list requires its owner thread");
    UpdateGuard guard(state);
    if(auto current=state->root.snapshot();!current)return std::unexpected(current.error());
    if(!state->live||!state->authored.enabled||!state->authored.interactive||!state->authored.editable||!state->authored.editor)
        return ii::core::fail(ii::core::ErrorCode::invalid_argument,"List editing is unavailable");
    if(state->editing||state->committing||state->drag)return ii::core::fail(ii::core::ErrorCode::busy,"A list edit or drag is already active");
    auto controller=state->authored.controller;auto editor=state->authored.editor;const auto revision=state->revision;
    auto changed=controller_changed(state,controller);if(!changed)return std::unexpected(changed.error());
    if(!state->live||state->authored.controller!=controller||state->authored.editor!=editor||state->revision!=revision)
        return ii::core::fail(ii::core::ErrorCode::busy,"List changed before editing");
    auto selected=controller->selection();if(!selected)return std::unexpected(convert(selected.error()));
    if(*changed||*selected!=state->selected){if(auto updated=refresh(state);!updated)return updated;
        return ii::core::fail(ii::core::ErrorCode::busy,"List changed before editing");}
    if(selected->empty())return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Select a list row before editing");
    auto candidate=ListEditing::begin(state->source->snapshot(),editor,*selected);if(!candidate)return std::unexpected(convert(candidate.error()));
    auto value=candidate->state();if(!value)return std::unexpected(convert(value.error()));
    auto target=state->source->frame({.width=double(state->viewport.bounds.width),.height=double(state->viewport.bounds.height),
        .offset=state->viewport.offset,.row_height=state->authored.row_height,.overscan=state->authored.overscan,
        .movement=state->movement,.reveal_key=*selected,.reveal=ListReveal::contain});
    if(!target)return std::unexpected(convert(target.error()));
    if(auto scrolled=state->root.set_scroll_offset(target->offset);!scrolled)return scrolled;
    if(!state->live||state->revision!=revision||state->authored.controller!=controller||state->authored.editor!=editor)
        return ii::core::fail(ii::core::ErrorCode::busy,"List changed before editing");
    auto found=state->rows.find(*selected);if(found==state->rows.end()||!found->second->edit_host||!found->second->input)
        return ii::core::fail(ii::core::ErrorCode::not_found,"List edit row is not visible");
    auto row=found->second;const auto& c=row->input->drawing.model.configuration();
    InputFieldConfiguration field;field.text=value->draft;field.width=c.width;field.height=c.height;
    field.horizontal_padding=(std::min)(c.left_padding,c.width/2);field.corner_radius=c.corner_radius;
    field.accessibility_name="Rename "+Policy::display_text(value->original.label);
    field.palette.background=state->authored.palette.idle;field.palette.text=state->authored.palette.text;field.palette.focus=state->authored.palette.focus;
    field.palette.selection=field.palette.focus;field.palette.selection.alpha*=.38;
    auto staged=std::make_shared<std::optional<NativeInputField>>();std::weak_ptr<State> weak=state;
    auto mounted=row->edit_host->set_content([weak,field,staged](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
        auto input=NativeInputField::create(builder,{},"field",field,{
            .edited=[weak](std::string text){if(auto state=weak.lock();state&&state->live&&state->editing&&state->edit_event){UpdateGuard guard(state);try{
                auto editing=state->editing;auto result=editing->set_draft(text);if(!result){state->failure=convert(result.error());return;}
                if(*result)state->edit_rejected=false;
                if(*result&&state->callbacks.edited){auto event=*state->edit_event;event.text=std::move(text);auto callback=state->callbacks.edited;callback(event);}
            }catch(...){state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List edit callback failed",std::current_exception()};}}},
            .submitted=[weak]{if(auto state=weak.lock();state&&state->live&&state->editing){UpdateGuard guard(state);try{
                if(auto result=commit_edit(state);!result)state->failure=result.error();
            }catch(...){state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List edit submission failed",std::current_exception()};}}},
            .focus_changed=[weak](bool focused){if(!focused)if(auto state=weak.lock();state&&state->live&&state->editing&&!state->committing){UpdateGuard guard(state);try{
                if(auto result=commit_edit(state);!result)state->failure=result.error();
            }catch(...){state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List edit focus submission failed",std::current_exception()};}}}
        });
        if(!input)return std::unexpected(input.error());*staged=*input;return {};
    });
    if(!mounted)return mounted;
    if(!state->live||state->revision!=revision||state->authored.controller!=controller||state->authored.editor!=editor||!current(state,row)||!*staged){
        (void)row->edit_host->clear_content();return ii::core::fail(ii::core::ErrorCode::busy,"List changed while opening its editor");}
    state->editing=std::make_shared<ListEditing>(std::move(*candidate));state->field=**staged;state->edit_row=row;
    state->edit_event=ListEditEvent{value->original,value->source_index,value->draft};
    if(auto escape=state->root.set_escape_action([weak]{if(auto state=weak.lock();state&&state->live&&state->editing){UpdateGuard guard(state);try{
        if(auto result=clear_edit(state,true);!result)state->failure=result.error();
    }catch(...){state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List edit cancellation failed",std::current_exception()};}}});!escape){(void)clear_edit(state,false);return escape;}
    if(auto shown=row->edit_host->set_visible(true);!shown){(void)clear_edit(state,false);return shown;}
    if(auto focused=state->field->root().focus();!focused){(void)clear_edit(state,false);return focused;}
    return {};
}

template<class Policy>
inline ii::core::Result<void> BasicNativeListView<Policy>::drag_visuals(const std::shared_ptr<State>& state,std::string_view old_target){
    UpdateGuard guard(state);
    const std::string previous(old_target);const auto next=state->drag_proposal?state->drag_proposal->target:std::string{};
    for(const auto& key:{previous,next})if(!key.empty()){
        auto found=state->rows.find(key);if(found!=state->rows.end()&&current(state,found->second))
            if(auto result=found->second->widget.request_repaint();!result)return result;
    }
    return {};
}

template<class Policy>
inline ii::core::Result<void> BasicNativeListView<Policy>::clear_drag(const std::shared_ptr<State>& state){
    UpdateGuard guard(state);
    const auto target=state->drag_proposal?state->drag_proposal->target:std::string{};
    if(state->drag)state->drag->cancel();state->drag.reset();state->drag_proposal.reset();state->drag_key.clear();state->dragging=false;
    if(!state->live)return {};
    if(auto removed=state->root.set_escape_action({});!removed)return removed;
    return drag_visuals(state,target);
}

template<class Policy>
inline ii::core::Result<bool> BasicNativeListView<Policy>::pointer(const std::shared_ptr<State>& state,const ii::gui::WidgetInput& event){
    UpdateGuard guard(state);
    using ii::gui::WidgetInputKind;using ii::gui::WidgetKey;
    if(!state->live)return false;
    if(state->drag&&(Policy::pointer_canceled(event.kind)||
        (event.kind==WidgetInputKind::key_down&&event.key==WidgetKey::escape))){
        if(auto cleared=clear_drag(state);!cleared)return std::unexpected(cleared.error());return true;
    }
    if(event.kind==WidgetInputKind::pointer_down){
        if(!state->authored.enabled||!state->authored.interactive||!state->authored.reorderable||!state->authored.editor||
           state->editing||state->drag||!state->viewport_seen||!event.local_position||event.native_content_control_active||
           (event.buttons&&!(event.buttons&1)))return false;
        const auto x=event.local_position->x,y=event.local_position->y;
        if(x<0||y<0||x>=state->viewport.bounds.width||y>=state->viewport.bounds.height)return false;
        auto controller=state->authored.controller;auto editor=state->authored.editor;
        auto changed=controller_changed(state,controller);if(!changed)return std::unexpected(changed.error());
        if(!state->live||state->authored.controller!=controller||state->authored.editor!=editor||
           !state->authored.enabled||!state->authored.interactive||!state->authored.reorderable)return true;
        if(*changed){
            if(auto result=refresh(state);!result)return std::unexpected(result.error());return true;}
        const auto snapshot=state->source->snapshot();const auto index=state->source->row_at(state->viewport.offset+y,state->authored.row_height);
        if(!index)return false;const auto& entry=snapshot->entries[*index];
        if(!entry.enabled||!entry.selectable||!entry.draggable)return false;
        auto found=state->rows.find(entry.key);if(found==state->rows.end()||!current(state,found->second))return false;
        auto row=found->second;
        auto row_event=event;const auto row_top=*state->source->row_offset(*index,state->authored.row_height)-state->viewport.offset;
        row_event.local_position=ii::gui::WidgetPoint{x,y-row_top};
        if(state->authored.editable&&entry.editable&&double_press(*row,row_event))return false;
        auto rename=row_input(state,row,row_event);if(!rename)return std::unexpected(rename.error());if(*rename)return true;
        auto drag=ListDrag::begin(snapshot,entry.key,{x,y});if(!drag)return std::unexpected(convert(drag.error()));
        if(auto selected=select(state,entry.key);!selected)return std::unexpected(selected.error());
        if(!state->live||state->source->snapshot()!=snapshot||state->authored.controller!=controller||state->authored.editor!=editor||
           !state->authored.enabled||!state->authored.interactive||!state->authored.reorderable||state->editing||state->drag||!current(state,row))return true;
        state->drag=std::move(*drag);state->drag_key=entry.key;
        std::weak_ptr<State> weak=state;
        if(auto escape=state->root.set_escape_action([weak]{if(auto owner=weak.lock();owner&&owner->live&&owner->drag)try{
                if(auto result=clear_drag(owner);!result)owner->failure=result.error();
            }catch(...){owner->failure=ii::core::Error{ii::core::ErrorCode::failed,"List drag cancellation failed",std::current_exception()};}});!escape){
            (void)clear_drag(state);return std::unexpected(escape.error());}
        if(auto focused=row->widget.focus();!focused){(void)clear_drag(state);return std::unexpected(focused.error());}
        return true;
    }
    if(!state->drag||(event.kind!=WidgetInputKind::pointer_move&&event.kind!=WidgetInputKind::pointer_up&&
                     event.kind!=WidgetInputKind::pointer_leave))return false;
    if(event.kind==WidgetInputKind::pointer_leave)return true;
    if(!event.local_position){(void)clear_drag(state);return true;}
    auto controller=state->authored.controller;auto changed=controller_changed(state,controller);
    if(!changed){(void)clear_drag(state);return std::unexpected(changed.error());}
    if(!state->live||!state->drag||state->authored.controller!=controller)return true;
    if(*changed){
        if(auto cleared=clear_drag(state);!cleared)return std::unexpected(cleared.error());
        if(auto result=refresh(state);!result)return std::unexpected(result.error());return true;}
    const ListDragPoint position{event.local_position->x,event.local_position->y};
    const auto calculate=[&]{return state->drag->update(position,{state->source,double(state->viewport.bounds.width),
        double(state->viewport.bounds.height),state->viewport.offset,state->authored.row_height});};
    auto planned=calculate();if(!planned){(void)clear_drag(state);return std::unexpected(convert(planned.error()));}
    if(event.kind==WidgetInputKind::pointer_move&&planned->dragging&&planned->scroll_direction&&state->authored.scrollable){
        const auto height=*state->source->content_extent(state->authored.row_height);
        const auto logical=std::clamp(state->viewport.offset+position.y,0.,std::nextafter(height,0.));
        const auto row=state->source->row_at(logical,state->authored.row_height);
        const auto step=row?*state->source->row_extent(*row,state->authored.row_height):state->authored.row_height;
        if(auto scrolled=state->root.set_scroll_offset((std::max)(0.,state->viewport.offset+planned->scroll_direction*step));!scrolled){
            (void)clear_drag(state);return std::unexpected(scrolled.error());}
        if(!state->live||!state->drag)return true;
        planned=calculate();if(!planned){(void)clear_drag(state);return std::unexpected(convert(planned.error()));}
    }
    const auto old_target=state->drag_proposal?state->drag_proposal->target:std::string{};
    state->dragging=planned->dragging;state->drag_proposal=planned->proposal;
    if(auto painted=drag_visuals(state,old_target);!painted){(void)clear_drag(state);return std::unexpected(painted.error());}
    if(event.kind==WidgetInputKind::pointer_move)return true;
    const auto key=state->drag_key;const auto dragging=state->dragging;
    auto proposal=state->drag->finish();if(!proposal){(void)clear_drag(state);return std::unexpected(convert(proposal.error()));}
    if(auto cleared=clear_drag(state);!cleared)return std::unexpected(cleared.error());
    if(!state->live)return true;
    if(!dragging){auto row=state->rows.find(key);if(row!=state->rows.end())activate(state,row->second);return true;}
    if(!*proposal)return true;
    auto editor=state->authored.editor;controller=state->authored.controller;
    if(!editor||!state->authored.reorderable||!state->authored.enabled||!state->authored.interactive)return true;
    auto committed=editor->apply((*proposal)->request);
    if(!committed){if(state->live)if(auto result=refresh(state);!result)state->failure=result.error();return std::unexpected(convert(committed.error()));}
    if(!state->live||state->authored.editor!=editor||state->authored.controller!=controller)return true;
    auto refreshed=refresh(state);
    if(state->live&&state->authored.editor==editor&&state->authored.controller==controller&&
       *committed!=(*proposal)->request.revision&&state->callbacks.moved){auto callback=state->callbacks.moved;callback(**proposal);}
    if(!refreshed)return std::unexpected(refreshed.error());return true;
}

template<class Policy>
inline ii::core::Result<bool> BasicNativeListView<Policy>::row_input(const std::shared_ptr<State>& state,const std::shared_ptr<Row>& row,const ii::gui::WidgetInput& event){
    UpdateGuard guard(state);
    if(event.kind!=ii::gui::WidgetInputKind::pointer_down||!current(state,row)||!state->authored.enabled||
       !state->authored.interactive||!state->authored.editable||!state->authored.editor||state->editing||
       event.native_content_control_active||(event.buttons&&!(event.buttons&1)))return false;
    const bool doubled=double_press(*row,event);
    row->last_press=std::isfinite(event.timestamp)&&event.timestamp>0?event.timestamp:0;
    row->last_x=event.local_position?event.local_position->x:event.x;row->last_y=event.local_position?event.local_position->y:event.y;
    if(!doubled)return false;row->last_press=0;
    auto controller=state->authored.controller;const auto revision=state->revision;
    auto changed=controller_changed(state,controller);if(!changed)return std::unexpected(changed.error());
    if(!state->live||state->authored.controller!=controller||state->revision!=revision||!current(state,row))return true;
    if(*changed){if(auto updated=refresh(state);!updated)return std::unexpected(updated.error());return true;}
    const auto snapshot=state->source->snapshot();auto found=std::ranges::find_if(snapshot->entries,[&](const auto& entry){return entry.key==row->key;});
    if(found==snapshot->entries.end()||!found->enabled||!found->selectable||!found->editable)return false;
    if(state->selected!=row->key){if(auto selected=select(state,row->key);!selected)return std::unexpected(selected.error());}
    auto result=begin_edit(state);
    if(!result){if(result.error().code==ii::core::ErrorCode::busy)return true;
        if(result.error().code==ii::core::ErrorCode::invalid_argument||result.error().code==ii::core::ErrorCode::not_found)return false;
        return std::unexpected(result.error());}
    return true;
}

template<class Policy>
inline ii::core::Result<void> BasicNativeListView<Policy>::request_activate(const std::shared_ptr<State>& state,const std::shared_ptr<Row>& row){
    if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Flat list requires its owner thread");
    UpdateGuard guard(state);
    try{
        if(!current(state,row))return ii::core::fail(ii::core::ErrorCode::closed,"Flat list row retired");
        if(!state->authored.enabled||!state->authored.interactive)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Flat list is not interactive");
        auto controller=state->authored.controller;const auto revision=state->revision;
        auto refreshed=controller_changed(state,controller);if(!refreshed)return std::unexpected(refreshed.error());
        if(!state->live||state->authored.controller!=controller||state->revision!=revision||!current(state,row))return {};
        if(*refreshed)return refresh(state);
        const auto snapshot=state->source->snapshot();auto found=std::ranges::find_if(snapshot->entries,[&](const auto& item){return item.key==row->key;});
        if(found==snapshot->entries.end()||!found->enabled||!found->selectable)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Flat list row cannot be activated");
        if(auto r=select(state,found->key);!r)return r;
        if(current(state,row)&&state->selected==row->key&&state->source->snapshot()==snapshot&&state->callbacks.activated){auto callback=state->callbacks.activated;callback(ListViewEvent{*found,std::size_t(found-snapshot->entries.begin())});}
        return {};
    }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Flat list activation failed",std::current_exception());}
}
template<class Policy>
inline void BasicNativeListView<Policy>::activate(const std::shared_ptr<State>& state,const std::shared_ptr<Row>& row)noexcept{
    UpdateGuard guard(state);
    try{
        if(current(state,row)&&state->authored.enabled&&state->authored.interactive){
            if(auto focused=row->widget.focus();!focused){state->failure=focused.error();return;}
        }
        if(auto result=request_activate(state,row);!result)state->failure=result.error();
    }catch(...){state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Flat list activation failed",std::current_exception()};}
}

} // namespace ii::gui
