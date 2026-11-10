#pragma once
#include <iiGUI/NativeInputFieldView.h>
#include <iiGUI/InputClearDrawing.h>
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
namespace ii::gui {
template<class Policy>
class BasicNativeSearchField final {
    using SearchFieldConfiguration=typename Policy::Configuration;using InputFieldConfiguration=typename Policy::InputConfiguration;
    using InputFieldSurface=typename Policy::Surface;using StyleColor=typename Policy::Color;using Metrics=typename Policy::Metrics;
    using NativeInputField=typename Policy::Field;using NativeInputFieldCallbacks=typename Policy::FieldCallbacks;
    using NativeInputFieldSnapshot=typename Policy::Snapshot;using NativeSearchFieldCallbacks=typename Policy::Callbacks;
    using NativeSearchFieldIds=typename Policy::Ids;using TextFieldMaterial=typename Policy::Material;
    struct State final {
        SearchFieldConfiguration options;
        NativeSearchFieldCallbacks callbacks;
        Metrics metrics;
        std::optional<TextFieldMaterial> material{std::in_place};
        ii::gui::Widget root,icon,clear;
        std::optional<NativeInputField> field;
        ii::gui::Size extent;
        bool has_text=false,ancestor_enabled=true;
        std::optional<ii::core::Error> layout_failure;
        std::uint64_t revision=0;
        const std::thread::id owner=std::this_thread::get_id();
        State(SearchFieldConfiguration value,NativeSearchFieldCallbacks events,Metrics style)
            :options(std::move(value)),callbacks(std::move(events)),metrics(std::move(style)),
             extent{options.width,metrics.height},has_text(!options.text.empty()){}
    };
    std::shared_ptr<State> state_;
    explicit BasicNativeSearchField(std::shared_ptr<State> state):state_(std::move(state)){}

    static void good(ii::core::Result<void> result){
        if(!result)throw std::runtime_error(result.error().message);
    }
    [[nodiscard]] static InputFieldConfiguration input_options(const SearchFieldConfiguration& options,const Metrics& m,bool ancestor_enabled=true){
        InputFieldConfiguration result;
        result.text=options.text;result.placeholder=options.placeholder;result.accessibility_name=options.accessibility_name;
        result.width=options.width;result.height=m.height;result.horizontal_padding=m.padding+m.icon+m.gap;
        result.vertical_padding=m.vertical_padding;result.corner_radius=options.surface==InputFieldSurface::inline_?0:
            options.surface==InputFieldSurface::cylinder?Policy::cylinder_radius:m.radius;
        result.font_family=m.font;result.font_size=m.font_size;result.line_height=m.line_height;
        result.enabled=options.enabled;result.read_only=options.read_only;
        // NativeInputField dims disabled paint by one half. Preserve the
        // independent Figma disabled roles after that host-wide modulation.
        const auto disabled=[](StyleColor color){color.alpha=(std::min)(1.,color.alpha*2);return color;};
        const bool effective_enabled=options.enabled&&ancestor_enabled;
        // The parent paints the complete Figma material underneath the text.
        result.palette.background={0,0,0,0};
        result.palette.hover=result.palette.background;
        result.palette.text=effective_enabled?m.text:disabled(m.disabled_text);
        result.palette.placeholder=result.palette.text;result.palette.caret=m.description;
        result.palette.selection=m.accent;result.palette.focus=m.accent;
        return result;
    }
    [[nodiscard]] static ii::gui::ActionOptions action_options(const State& state){
        return {.accessibility_name=state.options.clear_accessibility_name,
                .enabled=state.options.enabled&&!state.options.read_only,.focus_ring=false,
                .width=state.metrics.icon,.height=state.metrics.icon,.pointer_focusable=false};
    }
    [[nodiscard]] static ii::core::Result<void> frames(const std::shared_ptr<State>& state){
        const auto w=(std::max)(0.,state->extent.width),h=(std::max)(0.,state->extent.height);
        const auto size=(std::min)({state->metrics.icon,w,h}),padding=(std::min)(state->metrics.padding,(std::max)(0.,(w-size)/2));
        const auto y=(std::max)(0.,(h-size)/2),left=padding,right=(std::max)(0.,w-padding-size);
        const bool room=w>=state->metrics.icon+2*state->metrics.padding&&h>=state->metrics.icon;
        const bool enabled=state->options.enabled&&state->ancestor_enabled;
        const bool show_clear=room&&enabled&&!state->options.read_only&&
            state->options.clear_button_visible&&state->has_text;
        const bool show_icon=room&&(!show_clear||right>=left+size+state->metrics.gap);
        if(auto r=state->field->root().set_frame({0,0,float(w),float(h)});!r)return r;
        if(auto r=state->icon.set_frame({float(left),float(y),float(size),float(size)});!r)return r;
        if(auto r=state->clear.set_frame({float(right),float(y),float(size),float(size)});!r)return r;
        if(auto r=state->icon.set_opacity(enabled?1.:state->metrics.disabled_text.alpha);!r)return r;
        if(auto r=state->icon.set_visible(show_icon);!r)return r;
        return state->clear.set_visible(show_clear);
    }
    [[nodiscard]] static ii::core::Result<void> displayed(const std::shared_ptr<State>& state,std::string text){
        state->has_text=!text.empty();
        return frames(state);
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
    paint_clear(const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& visual){
#if defined(__APPLE__)
        constexpr bool system_symbol=true;
#else
        constexpr bool system_symbol=false;
#endif
        return ii::gui::input_field_clear_drawing(visual,state->metrics.description,state->metrics.background,
            state->metrics.font_size,state->metrics.clear_line_height,state->metrics.font,system_symbol);
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeSearchField> create(ii::gui::WidgetBuilder& builder,
            ii::gui::Widget parent,std::string_view id,SearchFieldConfiguration options={},
            NativeSearchFieldCallbacks callbacks={},NativeSearchFieldIds ids={}){
        if(!Policy::valid_configuration(options))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid search field configuration");
        try{
            auto state=std::make_shared<State>(std::move(options),std::move(callbacks),Policy::metrics());
            const auto prefix=std::string(id);auto root=builder.absolute_stack(parent,id,{state->options.width,state->metrics.height});
            if(!root)return std::unexpected(root.error());state->root=*root;
            if(auto r=root->set_clip_content(true);!r)return std::unexpected(r.error());
            std::weak_ptr<State> weak=state;
            if(auto r=root->set_paint_handler([weak](const ii::gui::WidgetPaintState& paint){
                auto state=weak.lock();if(!state||!state->material)return ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
                    ii::core::fail(ii::core::ErrorCode::closed,"Search field has retired")};
                const bool inline_surface=state->options.surface==InputFieldSurface::inline_;
                const auto radius=inline_surface?0:state->options.surface==InputFieldSurface::cylinder?
                    Policy::cylinder_radius:state->metrics.radius;
                const auto variant=inline_surface?Policy::MaterialVariant::inline_:Policy::MaterialVariant::standard;
                return state->material->paint(paint,state->options.enabled,radius,variant);
            });!r)return std::unexpected(r.error());
            NativeInputFieldCallbacks input_events;
            input_events.edited=[weak](std::string value){if(auto state=weak.lock())if(auto callback=state->callbacks.edited)callback(std::move(value));};
            input_events.submitted=[weak]{if(auto state=weak.lock())if(auto callback=state->callbacks.submitted)callback();};
            input_events.displayed_changed=[weak](std::string value){if(auto state=weak.lock())good(displayed(state,std::move(value)));};
            auto field=NativeInputField::create(builder,*root,ids.editor.empty()?prefix+".editor":ids.editor,
                input_options(state->options,state->metrics),std::move(input_events));
            if(!field)return std::unexpected(field.error());state->field=std::move(*field);
            typename Policy::IconCache icons;auto image=icons.resolve(Policy::icon_name,{},state->metrics.icon);if(!image)return std::unexpected(image.error());
            auto icon=builder.image(*root,ids.search_icon.empty()?prefix+".search":ids.search_icon,
                {.image=*image,.width=state->metrics.icon,.height=state->metrics.icon});
            if(!icon)return std::unexpected(icon.error());state->icon=*icon;
            auto clear=builder.action_surface(*root,ids.clear.empty()?prefix+".clear":ids.clear,action_options(*state),[weak]{
                if(auto state=weak.lock()){
                    good(state->field->clear_as_user());
                    if(state->root.snapshot())good(state->field->root().focus());
                }
            });
            if(!clear)return std::unexpected(clear.error());state->clear=*clear;
            if(auto r=clear->set_paint_handler([weak](const ii::gui::WidgetPaintState& paint)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
                auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Search field has retired");
                return paint_clear(state,paint);
            });!r)return std::unexpected(r.error());
            if(auto r=root->set_layout_handler([weak](ii::gui::Rect bounds){if(auto state=weak.lock()){
                state->extent={bounds.width,bounds.height};auto result=frames(state);
                if(!result)state->layout_failure=result.error();else state->layout_failure.reset();
            }});!r)return std::unexpected(r.error());
            if(auto r=root->set_interaction_handler([weak](const ii::gui::WidgetInteraction& interaction){
                if(auto state=weak.lock();state&&interaction.alive&&state->ancestor_enabled!=interaction.enabled){
                    state->ancestor_enabled=interaction.enabled;
                    good(state->field->set_palette(input_options(state->options,state->metrics,state->ancestor_enabled).palette));
                    good(frames(state));
                }
            });!r)return std::unexpected(r.error());
            if(auto r=frames(state);!r)return std::unexpected(r.error());
            if(auto r=builder.retain(state);!r)return std::unexpected(r.error());
            if(auto r=builder.on_retire([weak]{if(auto state=weak.lock()){state->callbacks={};state->material.reset();
                if(state->revision!=(std::numeric_limits<std::uint64_t>::max)())++state->revision;}});!r)return std::unexpected(r.error());
            return BasicNativeSearchField(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI search field creation failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::gui::Widget editor()const{return state_->field->root();}
    [[nodiscard]] ii::core::Result<NativeInputFieldSnapshot> snapshot()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Search field requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());
        if(state_->layout_failure)return std::unexpected(*state_->layout_failure);
        return state_->field->snapshot();
    }
    [[nodiscard]] ii::core::Result<SearchFieldConfiguration> options()const{
        auto current=snapshot();if(!current)return std::unexpected(current.error());
        try{auto result=state_->options;result.text=std::move(current->text);return result;}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Search options failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_text(std::string value)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Search field requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());
        if(!Policy::valid_text(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid search text");
        try{
            if(state_->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Search revision exhausted");
            auto previous=state_->options.text;const auto revision=++state_->revision;state_->options.text=value;
            auto result=state_->field->set_text(std::move(value));
            if(!result&&state_->revision==revision)state_->options.text=std::move(previous);
            return result;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Search text update failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_options(SearchFieldConfiguration options)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Search field requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());
        if(!Policy::valid_configuration(options))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid search field configuration");
        try{
            if(options==state_->options)return {};
            if(state_->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Search revision exhausted");
            auto previous=state_->options;const auto revision=++state_->revision;state_->options=std::move(options);
            auto result=state_->field->set_options(input_options(state_->options,state_->metrics,state_->ancestor_enabled));
            if(!result){if(state_->revision==revision)state_->options=std::move(previous);return result;}
            if(state_->revision!=revision)return {};
            if(auto action=state_->clear.set_action_options(action_options(*state_));!action)return action;
            if(state_->revision!=revision)return {};
            if(auto extent=state_->root.set_extent({state_->options.width,state_->metrics.height});!extent)return extent;
            if(state_->revision!=revision)return {};
            if(auto repainted=state_->root.request_repaint();!repainted)return repainted;
            return frames(state_);
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Search options update failed",std::current_exception());}
    }
};
} // namespace ii::gui
