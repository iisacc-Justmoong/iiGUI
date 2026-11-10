#pragma once
#include <iiGUI/NativeInputFieldView.h>
#include <iiGUI/SrgbColor.h>
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
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
template<class Policy>
class BasicNativeTableCellItem final {
    using TableCellItemInput=typename Policy::Input;using TableCellItem=typename Policy::Model;
    using TableCellItemRect=typename Policy::Rect;using NativeInputField=typename Policy::Field;
    using InputFieldConfiguration=typename Policy::InputConfiguration;using TableCellValueValidator=typename Policy::Validator;
    using NativeTableCellItemCallbacks=typename Policy::Callbacks;
    struct Prepared final {TableCellItemInput input;TableCellItem model;};
    using Snapshot=std::shared_ptr<const Prepared>;
    struct State final {
        Snapshot prepared;
        ii::gui::Widget root,surface,edit_host;
        std::optional<NativeInputField> field;
        NativeTableCellItemCallbacks callbacks;
        std::string original,draft,original_type;
        std::shared_ptr<const TableCellValueValidator> original_validator;
        ii::gui::Size extent;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        bool live=false,editing=false,committing=false;
    };
    explicit BasicNativeTableCellItem(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::core::Result<Snapshot> prepare(TableCellItemInput input){
        try{
            auto result=std::make_shared<Prepared>();result->input=std::move(input);
            auto c=result->input.configuration;c.text=Policy::display_text(c.text);c.accessibility_name=Policy::display_text(c.accessibility_name);
            if(!TableCellItem::validate(result->input.configuration)||!result->model.set_configuration(std::move(c)))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid table cell configuration");
            return Snapshot(std::move(result));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table cell preparation failed",std::current_exception());}
    }
    [[nodiscard]] static ii::gui::ActionOptions action(const Snapshot& value){
        const auto& c=value->model.configuration();
        return {.accessibility_name=c.accessibility_name.empty()?c.text:c.accessibility_name,.enabled=c.enabled,.focus_ring=false,
                .width=c.width,.height=c.height,.interaction_enabled=c.inputable};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint(const Snapshot& value,const ii::gui::WidgetPaintState& view){
        try{
            auto g=value->model.geometry(view.size.width,view.size.height);
            if(!g)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid table cell extent");
            auto result=std::make_shared<ii::gui::WidgetDrawing>();const auto& c=value->model.configuration();const auto visual=value->model.visual();
            const auto rect=[](TableCellItemRect r){return ii::gui::Rect{float(r.x),float(r.y),float(r.width),float(r.height)};};
            const auto background=ii::gui::srgb_to_linear(c.palette.background);
            if(g->surface.width>0&&g->surface.height>0)result->panels.push_back({rect(g->surface),background,background});
            if(visual.selected&&g->surface.width>0&&g->surface.height>0){const auto selection=ii::gui::srgb_to_linear(visual.selection);result->panels.push_back({rect(g->surface),selection,selection});}
            if(g->divider.width>0&&g->divider.height>0){const auto divider=ii::gui::srgb_to_linear(visual.divider);result->panels.push_back({rect(g->divider),divider,divider});}
            if(visual.current&&g->surface.width>=2&&g->surface.height>=2&&c.current_border_width>0){const auto border=ii::gui::srgb_to_linear(visual.border);
                result->panels.push_back({rect(g->surface),border,border,0,float((std::min)({c.current_border_width,g->surface.width/2,g->surface.height/2}))});}
            if(!c.text.empty()&&g->text.width>0&&g->text.height>0){auto ink=visual.text;if(!view.enabled)ink.alpha*=.35;
                result->texts.push_back({.bounds=rect(g->text),.text=c.text,.color=ii::gui::srgb_to_linear(ink),.font_size=float(c.font_size),.font_weight=500,
                    .alignment=ii::gui::TextAlignment::left,.font_family=view.font_family,.line_height=float(c.font_size+3),.elide_right=true,
                    .clip=c.clip_content?std::optional<ii::gui::Rect>{rect(g->surface)}:std::nullopt});}
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(result));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table cell paint failed",std::current_exception());}
    }
    [[nodiscard]] static InputFieldConfiguration field_options(const State& state){
        const auto& c=state.prepared->input.configuration;InputFieldConfiguration field;
        field.text=state.draft;field.accessibility_name=c.accessibility_name.empty()?"Edit table cell":c.accessibility_name;
        field.width=(std::max)(1.,state.extent.width);field.height=(std::max)(1.,state.extent.height);field.horizontal_padding=c.padding;
        field.corner_radius=0;field.font_size=c.font_size;field.enabled=c.enabled;
        field.palette.background=c.palette.background;field.palette.hover=c.palette.background;field.palette.text=c.palette.cell_text;
        field.palette.selection=c.palette.selection;field.palette.focus=c.palette.current;return field;
    }
    [[nodiscard]] static ii::core::Result<void> frames(const std::shared_ptr<State>& state){
        const ii::gui::Rect bounds{0,0,float(state->extent.width),float(state->extent.height)};
        if(auto result=state->surface.set_frame(bounds);!result)return result;
        return state->edit_host.set_frame(bounds);
    }
    [[nodiscard]] static ii::core::Result<bool> apply(const std::shared_ptr<State>& state,std::uint64_t revision){
        const auto step=[&](auto&& operation)->ii::core::Result<bool>{if(state->revision!=revision)return false;auto result=operation();
            if(!result)return std::unexpected(result.error());return state->revision==revision;};
        const auto& c=state->prepared->input.configuration;
        auto result=step([&]{return state->root.set_extent({c.width,c.height});});if(!result||!*result)return result;
        result=step([&]{return state->root.set_visible(c.visible);});if(!result||!*result)return result;
        result=step([&]{return state->surface.set_action_options(action(state->prepared));});if(!result||!*result)return result;
        result=step([&]{return frames(state);});if(!result||!*result)return result;
        return step([&]{return state->surface.request_repaint();});
    }
    [[nodiscard]] static ii::core::Result<void> clear_edit(const std::shared_ptr<State>& state){
        if(!state->editing)return {};
        state->editing=false;state->field.reset();state->original.clear();state->draft.clear();state->original_type.clear();state->original_validator.reset();
        if(auto hidden=state->edit_host.set_visible(false);!hidden)return hidden;
        return state->edit_host.clear_content();
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state,TableCellItemInput input){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Table cell requires its owner thread");
        try{
            if(auto live=state->root.snapshot();!live)return std::unexpected(live.error());if(state->prepared->input==input)return {};
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Table cell revision exhausted");
            const auto observed=state->revision;auto next=prepare(std::move(input));if(!next)return std::unexpected(next.error());
            if(!state->live||state->revision!=observed)return {};
            auto previous=state->prepared;const auto old_extent=state->extent;const auto revision=++state->revision;state->prepared=std::move(*next);
            const auto& c=state->prepared->input.configuration;state->extent={c.width,c.height};auto changed=apply(state,revision);
            const auto rollback=[&]{if(state->revision==revision){state->prepared=previous;state->extent=old_extent;(void)apply(state,revision);}};
            if(!changed){rollback();return std::unexpected(changed.error());}
            if(!*changed)return {};
            if(state->editing&&(!c.enabled||!c.inputable||!c.visible)){if(auto cleared=clear_edit(state);!cleared)return cleared;}
            else if(state->editing){
                if(state->field&&c.palette!=previous->input.configuration.palette){
                    if(auto styled=state->field->set_palette(field_options(*state).palette);!styled){rollback();return styled;}
                    if(state->revision!=revision)return {};
                }
                if(c.text==state->draft&&c.value_type==state->original_type&&state->prepared->input.validator==state->original_validator)
                    state->original=state->draft;
            }
            state->failure.reset();return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table cell update failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> reject(const std::shared_ptr<State>& state,std::string message){
        const auto value=state->draft,type=state->prepared->input.configuration.value_type;
        if(state->live&&state->callbacks.rejected){auto callback=state->callbacks.rejected;callback(value,type);}
        return ii::core::fail(ii::core::ErrorCode::invalid_argument,std::move(message));
    }
    [[nodiscard]] static ii::core::Result<void> commit(const std::shared_ptr<State>& state){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Table cell requires its owner thread");
        if(!state->live||!state->editing||!state->field)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"No table cell edit is active");
        if(state->committing)return ii::core::fail(ii::core::ErrorCode::busy,"Table cell edit is committing");
        state->committing=true;struct Reset{bool& value;~Reset(){value=false;}}reset{state->committing};
        auto field=state->field->snapshot();if(!field)return std::unexpected(field.error());
        if(field->marked)return ii::core::fail(ii::core::ErrorCode::busy,"Finish text composition before submitting the cell");
        state->draft=field->text;
        const auto current=state->prepared;const auto revision=state->revision;const auto& c=current->input.configuration;
        if((c.text!=state->original&&c.text!=state->draft)||c.value_type!=state->original_type||current->input.validator!=state->original_validator)
            return reject(state,"Table cell changed during editing");
        auto model=TableCellItem{};auto full=current->input.configuration;(void)model.set_configuration(std::move(full));
        auto accepted=model.accept(state->draft,current->input.validator);
        if(!state->live||!state->editing||state->revision!=revision||state->prepared!=current)
            return ii::core::fail(ii::core::ErrorCode::busy,"Table cell changed during validation");
        if(!accepted)return reject(state,accepted.error().message);
        const auto text=state->draft;auto next=current->input;next.configuration.text=text;
        if(auto changed=update(state,std::move(next));!changed)return changed;
        if(!state->live||!state->editing)return {};
        const auto completed=state->revision;if(auto cleared=clear_edit(state);!cleared)return cleared;
        if(state->live&&state->revision==completed&&state->callbacks.submitted){auto callback=state->callbacks.submitted;callback(text);}
        state->failure.reset();return {};
    }
    [[nodiscard]] static ii::core::Result<void> begin(const std::shared_ptr<State>& state){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Table cell requires its owner thread");
        if(auto live=state->root.snapshot();!live)return std::unexpected(live.error());
        if(!state->live||!state->prepared->input.configuration.enabled||!state->prepared->input.configuration.inputable||!state->prepared->input.configuration.visible)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Table cell editing is unavailable");
        if(state->editing||state->committing)return ii::core::fail(ii::core::ErrorCode::busy,"Table cell edit is already active");
        const auto current=state->prepared;const auto revision=state->revision;
        state->original=current->input.configuration.text;state->draft=state->original;state->original_type=current->input.configuration.value_type;
        state->original_validator=current->input.validator;const auto options=field_options(*state);
        auto staged=std::make_shared<std::optional<NativeInputField>>();std::weak_ptr<State> weak=state;
        auto mounted=state->edit_host.set_content([weak,options,staged](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
            auto field=NativeInputField::create(builder,{},"field",options,{
                .edited=[weak](std::string text){if(auto state=weak.lock();state&&state->live&&state->editing)try{
                    state->draft=text;state->failure.reset();if(state->callbacks.edited){auto callback=state->callbacks.edited;callback(std::move(text));}
                }catch(...){state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Table cell edit callback failed",std::current_exception()};}},
                .submitted=[weak]{if(auto state=weak.lock();state&&state->live&&state->editing)try{auto result=commit(state);if(!result&&state->live)state->failure=result.error();}
                    catch(...){state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Table cell submission failed",std::current_exception()};}},
                .focus_changed=[weak](bool focused){if(!focused)if(auto state=weak.lock();state&&state->live&&state->editing&&!state->committing)try{
                    if(auto result=clear_edit(state);!result)state->failure=result.error();
                }catch(...){state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Table cell cancellation failed",std::current_exception()};}}
            });
            if(!field)return std::unexpected(field.error());*staged=*field;return {};
        });
        if(!mounted)return mounted;
        if(!state->live||state->revision!=revision||state->prepared!=current||!*staged){(void)state->edit_host.clear_content();return ii::core::fail(ii::core::ErrorCode::busy,"Table cell changed while opening its editor");}
        state->field=**staged;state->editing=true;
        if(auto escape=state->field->root().set_escape_action([weak]{if(auto state=weak.lock();state&&state->live&&state->editing&&!state->committing)try{
            if(auto result=clear_edit(state);!result)state->failure=result.error();
        }catch(...){state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Table cell cancellation failed",std::current_exception()};}});!escape){
            (void)clear_edit(state);return escape;
        }
        if(auto shown=state->edit_host.set_visible(true);!shown){(void)clear_edit(state);return shown;}
        if(auto focused=state->field->root().focus();!focused){(void)clear_edit(state);return focused;}
        state->failure.reset();return {};
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeTableCellItem> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,std::string_view id,
            TableCellItemInput input={},NativeTableCellItemCallbacks callbacks={}){
        try{
            auto state=std::make_shared<State>();auto prepared=prepare(std::move(input));if(!prepared)return std::unexpected(prepared.error());
            state->prepared=std::move(*prepared);state->callbacks=std::move(callbacks);const auto& c=state->prepared->input.configuration;
            state->extent={c.width,c.height};std::weak_ptr<State> weak=state;const auto name=[&](std::string_view suffix){return std::string(id)+"."+std::string(suffix);};
            auto root=builder.absolute_stack(parent,id,state->extent);if(!root)return std::unexpected(root.error());state->root=*root;
            auto surface=builder.action_surface(*root,name("surface"),action(state->prepared),[weak]{if(auto state=weak.lock();state&&state->live)try{
                if(auto result=begin(state);!result)state->failure=result.error();
            }catch(...){state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Table cell activation failed",std::current_exception()};}});
            if(!surface)return std::unexpected(surface.error());state->surface=*surface;
            if(auto installed=surface->set_paint_handler([weak](const auto& view)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
                auto state=weak.lock();if(!state||!state->prepared)return ii::core::fail(ii::core::ErrorCode::closed,"Table cell retired");return paint(state->prepared,view);
            });!installed)return std::unexpected(installed.error());
            auto edit_host=builder.content_host(*root,name("editor"),state->extent);if(!edit_host)return std::unexpected(edit_host.error());state->edit_host=*edit_host;
            if(auto escape=edit_host->set_escape_action([weak]{if(auto state=weak.lock();state&&state->live&&state->editing&&!state->committing)try{
                if(auto result=clear_edit(state);!result)state->failure=result.error();
            }catch(...){state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Table cell cancellation failed",std::current_exception()};}});!escape)
                return std::unexpected(escape.error());
            if(auto hidden=edit_host->set_visible(false);!hidden)return std::unexpected(hidden.error());
            if(auto handler=root->set_layout_handler([weak](ii::gui::Rect bounds){if(auto state=weak.lock()){
                if(state->extent.width==bounds.width&&state->extent.height==bounds.height)return;state->extent={bounds.width,bounds.height};
                if(auto result=frames(state);!result)state->failure=result.error();
            }});!handler)return std::unexpected(handler.error());
            auto applied=apply(state,state->revision);if(!applied)return std::unexpected(applied.error());
            if(auto retained=builder.retain(state);!retained)return std::unexpected(retained.error());
            if(auto published=builder.on_publish([weak]{if(auto state=weak.lock())state->live=true;});!published)return std::unexpected(published.error());
            if(auto retired=builder.on_retire([weak]{if(auto state=weak.lock()){state->live=false;state->editing=false;state->field.reset();
                if(state->revision!=(std::numeric_limits<std::uint64_t>::max)())++state->revision;
                state->callbacks={};state->prepared.reset();state->original_validator.reset();state->original.clear();state->draft.clear();state->original_type.clear();}});!retired)return std::unexpected(retired.error());
            return BasicNativeTableCellItem(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table cell construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::gui::Widget surface()const{return state_->surface;}
    [[nodiscard]] ii::core::Result<TableCellItemInput> options()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Table cell requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->prepared->input;
    }
    [[nodiscard]] ii::core::Result<void> set_options(TableCellItemInput value)const{return update(state_,std::move(value));}
    [[nodiscard]] ii::core::Result<void> begin_edit()const{return begin(state_);}
    [[nodiscard]] ii::core::Result<void> commit_edit()const{try{return commit(state_);}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table cell submission failed",std::current_exception());}}
    [[nodiscard]] ii::core::Result<void> cancel_edit()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Table cell requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());if(state_->committing)return ii::core::fail(ii::core::ErrorCode::busy,"Table cell edit is committing");return clear_edit(state_);
    }
    [[nodiscard]] ii::core::Result<NativeInputField> editor()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Table cell requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());if(!state_->editing||!state_->field)return ii::core::fail(ii::core::ErrorCode::not_found,"No table cell editor is active");return *state_->field;
    }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Table cell requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->failure;
    }
private:
    std::shared_ptr<State> state_;
};

} // namespace ii::gui
