#pragma once
#include <iiGUI/PopoverModel.h>
#include <iiGUI/MaterialModel.h>
#include <iiGUI/MaterialGradient.h>
#include <iiGUI/NativeWidgets.h>
#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>
#if !defined(IIGUI_POPOVER_UNDERLAY_BOUNDS_API) || IIGUI_POPOVER_UNDERLAY_BOUNDS_API < 1
#error "iiGUI Popover requires bounds-aware underlays"
#endif
#if !defined(IIGUI_POPOVER_COMBINED_MATERIAL_MOTION_API) || IIGUI_POPOVER_COMBINED_MATERIAL_MOTION_API < 1
#error "iiGUI Popover requires combined material motion"
#endif
#if !defined(IIGUI_POPOVER_UNTINTED_UNDERLAY_API) || IIGUI_POPOVER_UNTINTED_UNDERLAY_API < 1
#error "iiGUI Popover requires colored underlays"
#endif

namespace ii::gui {
// The framework owns popover motion, underlay, callbacks and resource lifetime.
template<class Policy>
class BasicNativePopover final {
    using Popover=typename Policy::Model;using PopoverConfiguration=typename Policy::Options;using PopoverPlacement=typename Policy::Placement;
    using MaterialModel=typename Policy::MaterialModel;using MaterialConfiguration=typename Policy::MaterialOptions;
    using MaterialKind=typename Policy::MaterialKind;using NativeMaterialGradient=typename Policy::Gradient;
    using NativePopoverCallbacks=typename Policy::Callbacks;
    struct State final {
        Popover model;
        MaterialModel material;
        std::shared_ptr<NativeMaterialGradient> gradient=std::make_shared<NativeMaterialGradient>();
        ii::gui::PopoverUnderlayBoundsProvider underlay;
        ii::gui::Widget root;
        NativePopoverCallbacks callbacks;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        bool live=false,frames_supported=true,retired=false;
    };
    std::shared_ptr<State> state_;
    explicit BasicNativePopover(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::core::Result<void> check(const std::shared_ptr<State>& state){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Popover requires its owner thread");
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Popover has retired");
        if(auto current=state->root.snapshot();!current)return std::unexpected(current.error());return {};
    }
    [[nodiscard]] static MaterialConfiguration material_options(const PopoverConfiguration& value){
        const auto tint=value.surface_color.value_or(Policy::default_surface());
        MaterialConfiguration input;input.kind=MaterialKind::panel;input.color=tint;
        input.primary_color=value.primary_color;
        input.tint_opacity=value.surface_opacity;input.radius=value.corner_radius;input.blur_enabled=value.blur_enabled;
        return input;
    }
    [[nodiscard]] static ii::gui::PopoverOptions host_options(const PopoverConfiguration& value,bool frames_supported){
        ii::gui::PopoverOptions result;
        result.width=value.width;result.height=value.height;result.padding=value.padding;result.margins=value.margins;
        result.corner_radius=value.corner_radius;result.surface_opacity=value.surface_opacity;
        const auto input=material_options(value);const auto tint=*input.color;
        result.surface_color=ii::gui::WidgetColor{tint.red,tint.green,tint.blue,tint.alpha};
        const auto material=MaterialModel::resolve(input);
        result.opaque_base=!value.blur_enabled;
        if(value.blur_enabled)result.blur_radius=material.blur_radius;
        result.border_width=material.border_width;
        result.border_color={material.border.red,material.border.green,material.border.blue,material.border.alpha};
        result.inner_highlight_opacity=material.inner_highlight_opacity;
        result.inner_highlight_offset_y=material.inner_highlight_offset_y;
        result.shadow=ii::gui::PopoverShadow{.offset_x=0,.offset_y=material.shadow_offset_y,
            .blur_radius=material.shadow_radius,.color={0,0,0,material.shadow_opacity}};
        result.modal=value.modal;result.dim=value.dim;result.focus_on_open=value.focus_on_open;result.clip_content=value.clip_content;
        result.dismiss_on_escape=value.dismiss_on_escape;result.dismiss_on_outside_press=value.dismiss_on_outside_press;
        result.dismiss_on_context_request=value.dismiss_on_context_request;result.accessibility_name=value.accessibility_name;
        result.placement=value.placement==PopoverPlacement::centered?ii::gui::PopoverPlacement::centered:ii::gui::PopoverPlacement::anchored;
        ii::gui::PopoverTransition transition;
        transition.enter_duration=std::chrono::milliseconds{420};
        transition.enter_easing=ii::animation::Easing::out_back;
        transition.overshoot=1.45;transition.collapsed_scale=.92;
        transition.enabled=value.motion_enabled&&frames_supported;
        transition.opacity=ii::gui::PopoverOpacityTransition{
            .enter_duration=std::chrono::milliseconds{160},.exit_duration=std::chrono::milliseconds{150}};
        transition.hold_scale_on_exit=true;
        result.transition=std::move(transition);
        return result;
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativePopover> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,PopoverConfiguration value={},NativePopoverCallbacks callbacks={}){
        if(!Popover::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Popover configuration");
        try{
            auto state=std::make_shared<State>();(void)state->model.set_configuration(value);state->callbacks=std::move(callbacks);
            if(!state->material.set_configuration(material_options(value),{}))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Popover material");
            std::weak_ptr<State> weak=state;
            const auto event=[weak](auto member,bool open){return [weak,member,open]{if(auto value=weak.lock();value&&value->live){
                auto current=value->root.snapshot();if(!current||current->opened!=open||!value->live)return;auto callback=value->callbacks.*member;if(callback)callback();}};};
            const auto events=[&]{return ii::gui::PopoverEvents{.opened=event(&NativePopoverCallbacks::opened,true),
                .closed=event(&NativePopoverCallbacks::closed,false),.canceled=event(&NativePopoverCallbacks::canceled,false)};};
            // Probe while this popup has no activation factory: at this point
            // unsupported can only mean that the host has no frame source.
            // This also records capability for an initially disabled popup.
            auto probe=host_options(value,true);probe.transition->enabled=true;
            auto popup=builder.popover(parent,id,std::move(probe),events());
            if(!popup&&popup.error().code==ii::core::ErrorCode::unsupported){
                state->frames_supported=false;
                popup=builder.popover(parent,id,host_options(value,false),events());
            }else if(!popup&&!value.motion_enabled){
                // An explicitly still popup does not depend on a working
                // frame source. A later attempt to enable motion retries it
                // and reports any error without replacing the last settings.
                popup=builder.popover(parent,id,host_options(value,false),events());
            }else if(popup&&!value.motion_enabled){
                if(auto disabled=popup->set_popover_options(host_options(value,true));!disabled)
                    return std::unexpected(disabled.error());
            }
            if(!popup)return std::unexpected(popup.error());state->root=*popup;
            if(auto follows=popup->set_popover_material_follows_content(true);!follows)
                return std::unexpected(follows.error());
            state->underlay=[weak](ii::gui::Size size,double scale)
                    ->ii::core::Result<ii::gui::PopoverUnderlayFrame>{
                auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Popover material has retired");
                auto gradient=state->gradient;if(!gradient)return ii::core::fail(ii::core::ErrorCode::closed,"Popover material has retired");
                const auto [padding_x,padding_y]=NativeMaterialGradient::blur_padding(size.width,size.height,state->material.resolved().blur_radius);
                auto image=gradient->render_colored(state->material,size.width,size.height,scale,padding_x,padding_y);
                if(!image)return std::unexpected(image.error());
                return ii::gui::PopoverUnderlayFrame{std::move(*image),
                    {float(-padding_x),float(-padding_y),float(size.width+2*padding_x),float(size.height+2*padding_y)}};
            };
            if(auto underlay=popup->set_popover_untinted_underlay_with_bounds(state->underlay);!underlay)
                return std::unexpected(underlay.error());
            if(auto retained=builder.retain(state);!retained)return std::unexpected(retained.error());
            if(auto published=builder.on_publish([weak]{if(auto value=weak.lock())value->live=true;});!published)return std::unexpected(published.error());
            if(auto retired=builder.on_retire([weak]{if(auto value=weak.lock()){
                value->live=false;value->retired=true;if(value->revision!=(std::numeric_limits<std::uint64_t>::max)())++value->revision;
                value->callbacks={};value->underlay={};value->gradient.reset();
            }});!retired)return std::unexpected(retired.error());
            return BasicNativePopover(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Popover creation failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<PopoverConfiguration> options()const{
        if(auto valid=check(state_);!valid)return std::unexpected(valid.error());
        try{return state_->model.configuration();}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Popover options failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_options(PopoverConfiguration value)const{
        auto state=state_;
        if(auto valid=check(state);!valid)return valid;
        if(!Popover::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Popover configuration");
        try{
            if(value==state->model.configuration())return {};
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Popover revision exhausted");
            MaterialModel material;if(!material.set_configuration(material_options(value),{}))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Popover material");
            auto provider=state->underlay;auto host=host_options(value,state->frames_supported);auto previous=state->model.configuration();
            auto previous_material=state->material;const auto revision=++state->revision;(void)state->model.set_configuration(value);
            if(auto updated=state->root.set_popover_options(std::move(host));!updated){if(state->revision==revision)(void)state->model.set_configuration(std::move(previous));return updated;}
            if(state->revision==revision){
                state->material=std::move(material);
                if(auto updated=state->root.set_popover_untinted_underlay_with_bounds(std::move(provider));!updated){
                    if(state->revision==revision){
                        state->material=std::move(previous_material);(void)state->model.set_configuration(previous);
                        (void)state->root.set_popover_options(host_options(previous,state->frames_supported));
                    }
                    return updated;
                }
            }
            if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Popover retired during update");
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Popover update failed",std::current_exception());}
    }
};
} // namespace ii::gui
