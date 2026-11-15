#pragma once
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
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
template<class Policy>class BasicNativeTableRow final {
    using TableRowInput=typename Policy::Input;using TableRowView=typename Policy::Model;
    using NativeTableCellItem=typename Policy::Cell;using TableCellItemInput=typename Policy::CellInput;
    using NativeTableRowCallbacks=typename Policy::Callbacks;
    struct Cell final {std::string key;std::optional<NativeTableCellItem> view;};
    struct Prepared final {TableRowInput input;TableRowView model;std::vector<std::shared_ptr<Cell>> cells;std::unordered_map<std::string,std::size_t> indices;};
    using Snapshot=std::shared_ptr<const Prepared>;
    struct Slot final {std::string key;double x=0,width=0;friend bool operator==(const Slot&,const Slot&)=default;};
    struct State final {
        Snapshot prepared;
        std::unordered_map<std::string,std::shared_ptr<Cell>> cells;
        std::vector<Slot> mounted_slots;
        ii::gui::Widget root,content;
        NativeTableRowCallbacks callbacks;
        std::function<void(ii::gui::Rect)> layout_handler;
        std::optional<TableRowInput> pending;
        ii::gui::Size extent;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        unsigned callbacks_active=0;
        bool live=false,mounted=false;
    };
    explicit BasicNativeTableRow(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const std::shared_ptr<State>& state,TableRowInput input){
        try{
            if(!std::isfinite(input.offset)||input.offset<0||input.offset>1e15)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid table row offset");
            auto next=std::make_shared<Prepared>();next->input=std::move(input);
            if(!next->model.set_configuration(next->input.configuration))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid table row configuration");
            next->cells.reserve(next->input.configuration.cells.size());next->indices.reserve(next->input.configuration.cells.size());
            for(const auto& value:next->input.configuration.cells){auto found=state->cells.find(value.key);
                auto cell=found==state->cells.end()?std::make_shared<Cell>():found->second;if(cell->key.empty())cell->key=value.key;
                next->indices.emplace(value.key,next->cells.size());next->cells.push_back(std::move(cell));}
            return Snapshot(std::move(next));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table row preparation failed",std::current_exception());}
    }
    [[nodiscard]] static TableCellItemInput cell_input(const Snapshot& prepared,std::size_t index){
        const auto& c=prepared->input.configuration;const auto& cell=c.cells[index];TableCellItemInput input;auto& value=input.configuration;
        value.text=cell.value.text;value.value_type=cell.value.value_type;value.width=c.cell_width;value.height=c.cell_height;value.padding=c.content_spacing;
        value.font_size=c.font_size;value.palette=c.palette;value.enabled=c.enabled&&cell.value.enabled;value.inputable=c.inputable&&cell.value.editable;
        value.selected=cell.selected;value.current=cell.current;value.show_divider=c.show_divider;input.validator=cell.validator;return input;
    }
    [[nodiscard]] static std::optional<std::size_t> index(const std::shared_ptr<State>& state,std::string_view key){
        if(!state->prepared)return std::nullopt;
        const auto found=state->prepared->indices.find(std::string(key));
        return found==state->prepared->indices.end()?std::nullopt:std::optional<std::size_t>{found->second};
    }
    [[nodiscard]] static bool current(const std::shared_ptr<State>& state,const std::shared_ptr<Cell>& cell){
        const auto found=state->cells.find(cell->key);return state->live&&found!=state->cells.end()&&found->second==cell;
    }
    struct CallbackGuard final {State& state;explicit CallbackGuard(State& value):state(value){++state.callbacks_active;}~CallbackGuard(){--state.callbacks_active;}};
    static void edited(const std::shared_ptr<State>& state,const std::shared_ptr<Cell>& cell,std::string text)noexcept{
        try{if(!current(state,cell))return;auto position=index(state,cell->key);if(!position)return;
            CallbackGuard guard(*state);if(state->callbacks.edited){auto callback=state->callbacks.edited;callback(*position,std::move(text));}
        }catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Table row edit callback failed",std::current_exception()};}catch(...) {}}
    }
    static void rejected(const std::shared_ptr<State>& state,const std::shared_ptr<Cell>& cell,std::string text,std::string type)noexcept{
        try{if(!current(state,cell))return;auto position=index(state,cell->key);if(!position)return;
            CallbackGuard guard(*state);if(state->callbacks.rejected){auto callback=state->callbacks.rejected;callback(*position,std::move(text),std::move(type));}
        }catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Table row rejection callback failed",std::current_exception()};}catch(...) {}}
    }
    static void submitted(const std::shared_ptr<State>& state,const std::shared_ptr<Cell>& cell,std::string text)noexcept{
        try{
            if(!current(state,cell))return;auto position=index(state,cell->key);if(!position)return;
            if(state->prepared->input.configuration.cells[*position].value.text!=text){
                if(state->revision==(std::numeric_limits<std::uint64_t>::max)())throw std::out_of_range("Table row revision exhausted");
                auto next=state->prepared->input;next.configuration.cells[*position].value.text=text;
                auto prepared=prepare(state,std::move(next));if(!prepared){state->failure=prepared.error();return;}
                state->prepared=std::move(*prepared);++state->revision;
            }
            CallbackGuard guard(*state);if(state->callbacks.submitted){auto callback=state->callbacks.submitted;callback(*position,std::move(text));}
        }catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Table row submission callback failed",std::current_exception()};}catch(...) {}}
    }
    [[nodiscard]] static ii::core::Result<bool> render(const std::shared_ptr<State>& state,std::uint64_t revision){
        if(state->extent.width<1||state->extent.height<1)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Table row viewport is too small");
        auto frame=state->prepared->model.layout(state->extent.width,state->prepared->input.offset);
        if(!frame)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid table row viewport");
        std::vector<Slot> slots;std::vector<TableCellItemInput> inputs;std::unordered_map<std::string,std::shared_ptr<Cell>> next;
        slots.reserve(frame->cells.size());inputs.reserve(frame->cells.size());next.reserve(frame->cells.size());
        for(const auto& item:frame->cells){const auto& cell=state->prepared->cells[item.index];slots.push_back({cell->key,item.x,item.width});
            inputs.push_back(cell_input(state->prepared,item.index));next.emplace(cell->key,cell);}
        if(auto placed=state->content.set_frame({0,0,float(state->extent.width),float(state->extent.height)});!placed)return std::unexpected(placed.error());
        if(state->revision!=revision)return false;
        if(!state->mounted||slots!=state->mounted_slots){
            std::vector<ii::gui::KeyedContentItem> items;items.reserve(slots.size());std::weak_ptr<State> weak=state;
            for(std::size_t i=0;i<slots.size();++i){auto cell=next.at(slots[i].key);std::weak_ptr<Cell> weak_cell=cell;
                items.push_back({slots[i].key,[weak,weak_cell,value=inputs[i]](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
                    auto state=weak.lock();auto cell=weak_cell.lock();if(!state||!cell||!state->live)return ii::core::fail(ii::core::ErrorCode::closed,"Table row cell retired");
                    auto view=NativeTableCellItem::create(builder,{},"cell",value,{
                        .edited=[weak,weak_cell](std::string text){if(auto state=weak.lock())if(auto cell=weak_cell.lock())edited(state,cell,std::move(text));},
                        .submitted=[weak,weak_cell](std::string text){if(auto state=weak.lock())if(auto cell=weak_cell.lock())submitted(state,cell,std::move(text));},
                        .rejected=[weak,weak_cell](std::string text,std::string type){if(auto state=weak.lock())if(auto cell=weak_cell.lock())rejected(state,cell,std::move(text),std::move(type));}
                    });
                    if(!view)return std::unexpected(view.error());cell->view=*view;return {};
                },ii::gui::Rect{float(slots[i].x),0,float(slots[i].width),float(state->extent.height)}});
            }
            if(auto mounted=state->content.set_keyed_content(std::move(items),{.viewport=state->extent});!mounted)return std::unexpected(mounted.error());
            if(state->revision!=revision)return false;
        }
        for(std::size_t i=0;i<slots.size();++i){auto cell=next.at(slots[i].key);
            if(!cell->view)return ii::core::fail(ii::core::ErrorCode::failed,"Table row cell did not publish");
            if(auto changed=cell->view->set_options(inputs[i]);!changed)return std::unexpected(changed.error());if(state->revision!=revision)return false;}
        state->cells=std::move(next);state->mounted_slots=std::move(slots);state->mounted=true;return true;
    }
    [[nodiscard]] static bool remount_needed(const std::shared_ptr<State>& state,const Snapshot& candidate){
        auto frame=candidate->model.layout(candidate->input.configuration.width,candidate->input.offset);if(!frame)return true;
        if(!state->mounted||frame->cells.size()!=state->mounted_slots.size()||candidate->input.configuration.cell_height!=state->extent.height)return true;
        for(std::size_t i=0;i<frame->cells.size();++i){const auto& slot=frame->cells[i];const auto& old=state->mounted_slots[i];
            if(candidate->cells[slot.index]->key!=old.key||slot.x!=old.x||slot.width!=old.width)return true;}
        return false;
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state,TableRowInput input){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Table row requires its owner thread");
        try{
            if(auto live=state->root.snapshot();!live)return std::unexpected(live.error());if(state->prepared->input==input&&!state->pending)return {};
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Table row revision exhausted");
            const auto observed=state->revision;auto next=prepare(state,std::move(input));if(!next)return std::unexpected(next.error());
            if(!state->live||state->revision!=observed)return {};
            if(state->callbacks_active&&remount_needed(state,*next)){
                state->pending=(*next)->input;if(auto armed=state->root.set_layout_handler(state->layout_handler);!armed)return armed;return {};
            }
            auto previous=state->prepared;const auto old_extent=state->extent;const auto revision=++state->revision;state->prepared=std::move(*next);
            state->extent={state->prepared->input.configuration.width,state->prepared->input.configuration.cell_height};
            const auto rollback=[&]{if(state->revision==revision){state->prepared=previous;state->extent=old_extent;
                (void)state->root.set_extent(old_extent);(void)render(state,revision);(void)state->root.set_visible(previous->input.configuration.visible);}};
            if(auto sized=state->root.set_extent(state->extent);!sized){rollback();return sized;}
            if(state->revision!=revision)return {};
            auto drawn=render(state,revision);if(!drawn){rollback();return std::unexpected(drawn.error());}if(!*drawn||state->revision!=revision)return {};
            if(auto visible=state->root.set_visible(state->prepared->input.configuration.visible);!visible){rollback();return visible;}
            state->pending.reset();state->failure.reset();return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table row update failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeTableRow> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,std::string_view id,
            TableRowInput input={},NativeTableRowCallbacks callbacks={}){
        try{
            auto state=std::make_shared<State>();auto prepared=prepare(state,std::move(input));if(!prepared)return std::unexpected(prepared.error());
            state->prepared=std::move(*prepared);state->callbacks=std::move(callbacks);
            state->extent={state->prepared->input.configuration.width,state->prepared->input.configuration.cell_height};std::weak_ptr<State> weak=state;
            auto root=builder.absolute_stack(parent,id,state->extent);if(!root)return std::unexpected(root.error());state->root=*root;
            auto content=builder.content_host(*root,std::string(id)+".cells",state->extent);if(!content)return std::unexpected(content.error());state->content=*content;
            if(auto clipped=root->set_clip_content(true);!clipped)return std::unexpected(clipped.error());
            state->layout_handler=[weak](ii::gui::Rect bounds){if(auto state=weak.lock();state&&state->live){
                state->extent={bounds.width,bounds.height};
                if(state->pending){
                    if(state->callbacks_active){(void)state->root.set_layout_handler(state->layout_handler);throw std::runtime_error("Table row update is deferred");}
                    auto pending=*state->pending;state->pending.reset();auto result=update(state,pending);
                    if(!result){state->failure=result.error();if(result.error().code==ii::core::ErrorCode::busy){state->pending=std::move(pending);(void)state->root.set_layout_handler(state->layout_handler);throw std::runtime_error(result.error().message);}return;}
                }
                if(!state->live)return;
                auto result=render(state,state->revision);if(!result){state->failure=result.error();if(result.error().code==ii::core::ErrorCode::busy){
                    (void)state->root.set_layout_handler(state->layout_handler);throw std::runtime_error(result.error().message);}}
                else if(*result)state->failure.reset();
            }};
            if(auto handler=root->set_layout_handler(state->layout_handler);!handler)return std::unexpected(handler.error());
            if(auto visible=root->set_visible(state->prepared->input.configuration.visible);!visible)return std::unexpected(visible.error());
            if(auto retained=builder.retain(state);!retained)return std::unexpected(retained.error());
            if(auto published=builder.on_publish([weak]{if(auto state=weak.lock()){
                state->live=true;
                // A staged generation can receive its first layout before it is
                // published. Request another delivery so its cells are mounted.
                if(auto handler=state->root.set_layout_handler(state->layout_handler);!handler)state->failure=handler.error();
            }});!published)return std::unexpected(published.error());
            if(auto retired=builder.on_retire([weak]{if(auto state=weak.lock()){state->live=false;
                if(state->revision!=(std::numeric_limits<std::uint64_t>::max)())++state->revision;
                state->pending.reset();state->cells.clear();state->mounted_slots.clear();state->prepared.reset();state->callbacks={};state->layout_handler={};}});!retired)return std::unexpected(retired.error());
            return BasicNativeTableRow(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table row construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<TableRowInput> options()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Table row requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->prepared->input;
    }
    [[nodiscard]] ii::core::Result<void> set_options(TableRowInput value)const{return update(state_,std::move(value));}
    [[nodiscard]] ii::core::Result<NativeTableCellItem> cell(std::string_view key)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Table row requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());if(!state_->mounted){auto rendered=render(state_,state_->revision);if(!rendered)return std::unexpected(rendered.error());}
        const auto found=state_->cells.find(std::string(key));if(found==state_->cells.end()||!found->second->view)return ii::core::fail(ii::core::ErrorCode::not_found,"Table row cell is not visible");return *found->second->view;
    }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Table row requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->failure;
    }
private:
    std::shared_ptr<State> state_;
};

} // namespace ii::gui
