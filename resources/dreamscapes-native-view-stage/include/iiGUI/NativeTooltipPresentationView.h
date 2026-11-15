#pragma once
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
namespace ii::gui {
template<class Policy>
class BasicNativeTooltipPresentation final:public Policy::Port {
    using TooltipPresentationPort=typename Policy::Port;using TooltipPresentationStyle=typename Policy::Style;
    using MaterialModel=typename Policy::MaterialModel;using NativeMaterial=typename Policy::Material;
    using TooltipOutline=typename Policy::Outline;using TooltipLayout=typename Policy::Layout;
public:
    [[nodiscard]] static bool valid_style(const TooltipPresentationStyle& value)noexcept{return validate(value);}
    using Scheduler=std::function<ii::core::Result<void>(std::optional<std::int64_t>)>;
private:
    
    ii::gui::Widget popup_,canvas_,viewport_,content_;
    std::optional<NativeMaterial> material_;
    TooltipPresentationStyle style_;
    Scheduler scheduler_;
    std::optional<TooltipLayout> current_;
    std::optional<ii::gui::PopoverOptions> popup_options_;
    std::chrono::milliseconds motion_duration_{420};
    std::optional<double> reconcile_offset_;
    const std::thread::id owner_=std::this_thread::get_id();
    bool live_=false,retired_=false,busy_=false,uncertain_=false,frames_supported_=true;
    void release()noexcept{scheduler_={};style_.motion.reset();material_.reset();current_.reset();popup_options_.reset();reconcile_offset_.reset();}
    struct Guard final {BasicNativeTooltipPresentation& owner;explicit Guard(BasicNativeTooltipPresentation& value):owner(value){owner.busy_=true;}
        ~Guard(){owner.busy_=false;if(owner.retired_)owner.release();}};
    BasicNativeTooltipPresentation(ii::gui::Widget popup,ii::gui::Widget canvas,ii::gui::Widget viewport,ii::gui::Widget content,
            NativeMaterial material,TooltipPresentationStyle style,Scheduler scheduler)
        :popup_(std::move(popup)),canvas_(std::move(canvas)),viewport_(std::move(viewport)),content_(std::move(content)),
         material_(std::move(material)),style_(std::move(style)),scheduler_(std::move(scheduler)){}
    [[nodiscard]] ii::core::Result<void> ready(bool empty=false)const {
        if(owner_!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Tooltip presentation requires its owner thread");
        if(retired_)return ii::core::fail(ii::core::ErrorCode::closed,"Tooltip presentation has retired");
        if(busy_)return ii::core::fail(ii::core::ErrorCode::busy,"Tooltip presentation is updating");
        if(!live_&&!empty)return ii::core::fail(ii::core::ErrorCode::busy,"Tooltip presentation is not published");
        return {};
    }
    [[nodiscard]] static bool validate(const TooltipPresentationStyle& value)noexcept{
        return std::isfinite(value.content_padding)&&value.content_padding>=0&&value.content_padding<=8192&&
            Policy::valid_text(value.accessibility_name)&&!value.material.width&&!value.material.height&&
            value.material.outline_path.empty()&&MaterialModel::validate(value.material);
    }
    [[nodiscard]] static ii::gui::PopoverTransition transition(const TooltipPresentationStyle& style,
            std::chrono::milliseconds duration,bool frames_supported){
        ii::gui::PopoverTransition value;
        value.enter_duration=duration;value.exit_duration=(std::min)(duration,std::chrono::milliseconds{150});
        value.enter_easing=ii::animation::Easing::out_back;value.exit_easing=ii::animation::Easing::linear;
        value.overshoot=1.45;value.collapsed_scale=.94;value.enabled=style.motion_enabled&&frames_supported;
        value.preferences=style.motion;value.hold_scale_on_exit=true;
        value.opacity=ii::gui::PopoverOpacityTransition{.enter_duration=(std::min)(duration,std::chrono::milliseconds{160}),
            .exit_duration=(std::min)(duration,std::chrono::milliseconds{150})};
        return value;
    }
    [[nodiscard]] ii::core::Result<void> set_popup_options(ii::gui::PopoverOptions options){
        auto configured=popup_.set_popover_options(options);
        if(!configured&&configured.error().code==ii::core::ErrorCode::unsupported&&options.transition&&options.transition->enabled){
            // This retained tooltip has no dynamic popup factory. An
            // unsupported animated setup therefore means no frame source.
            options.transition->enabled=false;
            configured=popup_.set_popover_options(options);
            if(configured)frames_supported_=false;
        }
        if(configured)popup_options_=std::move(options);
        return configured;
    }
public:
    [[nodiscard]] ii::core::Result<std::optional<double>> scroll_checkpoint()const{
        if(!live_)return std::optional<double>{};
        auto snapshot=viewport_.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
        return std::optional{snapshot->scroll_offset};
    }
    [[nodiscard]] ii::core::Result<void> restore_scroll(std::optional<double> offset){
        if(offset)return viewport_.set_scroll_offset(*offset);
        return {};
    }
private:
    [[nodiscard]] ii::core::Result<void> apply(const std::optional<TooltipLayout>& layout){
        try {
        if(!layout)return popup_.close_popup();
        auto outline=TooltipOutline::build(*layout);if(!outline)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid tooltip outline");
        const auto& frame=layout->frame;const auto& body=layout->body;
        if(frame.width<1||frame.height<1||std::abs(frame.x)>32767||std::abs(frame.y)>32767)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Tooltip frame cannot be presented by this host");
        auto material=style_.material;material.width=frame.width;material.height=frame.height;material.outline_path=std::move(*outline);
        if(!MaterialModel::validate(material))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid tooltip material");
        const auto local_x=(std::max)(0.,body.x-frame.x),local_y=(std::max)(0.,body.y-frame.y);
        const auto pad_x=(std::min)(style_.content_padding,body.width/2),pad_y=(std::min)(style_.content_padding,body.height/2);
        ii::gui::PopoverOptions popup;
        popup.width=frame.width;popup.height=frame.height;popup.padding=0;popup.margins=0;popup.surface_opacity=0;
        popup.focus_on_open=false;popup.dismiss_on_escape=false;popup.dismiss_on_outside_press=false;popup.opaque_base=false;
        popup.accessibility_name=style_.accessibility_name;popup.preserve_input=true;
        popup.transition=transition(style_,motion_duration_,frames_supported_);
        popup.bounds=ii::gui::Rect{float(frame.x),float(frame.y),float(frame.width),float(frame.height)};
        popup.input_bounds=ii::gui::Rect{float(local_x),float(local_y),float(body.width),float(body.height)};
        if(auto changed=material_->set_options(std::move(material));!changed)return changed;
        if(auto resized=canvas_.set_extent({frame.width,frame.height});!resized)return resized;
        if(auto framed=viewport_.set_frame({float(local_x+pad_x),float(local_y+pad_y),
                float((std::max)(0.,body.width-2*pad_x)),float((std::max)(0.,body.height-2*pad_y))});!framed)return framed;
        if(auto configured=viewport_.set_scroll_options({.enabled=style_.scroll_content});!configured)return configured;
        if(auto configured=set_popup_options(std::move(popup));!configured)return configured;
        return popup_.open_at(0,0);
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip host update failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<std::shared_ptr<BasicNativeTooltipPresentation>> create(ii::gui::WidgetBuilder& builder,
            ii::gui::Widget parent,std::string_view id,TooltipPresentationStyle style,Scheduler scheduler){
        if(!validate(style)||!scheduler)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid tooltip style or scheduler");
        try {
            ii::gui::PopoverOptions popup_options;popup_options.width=popup_options.height=1;popup_options.padding=0;popup_options.margins=0;
            popup_options.surface_opacity=0;popup_options.focus_on_open=false;popup_options.dismiss_on_escape=false;
            popup_options.dismiss_on_outside_press=false;popup_options.opaque_base=false;popup_options.preserve_input=true;
            popup_options.accessibility_name=style.accessibility_name;popup_options.bounds=ii::gui::Rect{0,0,1,1};
            popup_options.input_bounds=ii::gui::Rect{0,0,1,1};
            popup_options.transition=transition(style,std::chrono::milliseconds{420},false);
            const std::string prefix(id);auto popup=builder.popover(parent,id,std::move(popup_options));
            if(!popup)return std::unexpected(popup.error());
            auto initial=style.material;initial.width=1;initial.height=1;initial.outline_path.clear();
            auto material=NativeMaterial::create(builder,*popup,prefix+"/material",initial);
            if(!material)return std::unexpected(material.error());
            auto canvas=builder.absolute_stack(material->root(),prefix+"/canvas",{1,1});if(!canvas)return std::unexpected(canvas.error());
            ii::gui::StackOptions stack{};stack.spacing=0;stack.padding=0;stack.alignment=ii::gui::StackAlignment::leading;
            auto viewport=builder.scroll_view(*canvas,prefix+"/viewport",{.enabled=style.scroll_content});
            if(!viewport)return std::unexpected(viewport.error());
            auto content=builder.vstack(*viewport,prefix+"/content",stack);
            if(!content)return std::unexpected(content.error());
            auto port=std::shared_ptr<BasicNativeTooltipPresentation>(new BasicNativeTooltipPresentation(*popup,*canvas,*viewport,*content,
                std::move(*material),std::move(style),std::move(scheduler)));
            if(auto retained=builder.retain(port);!retained)return std::unexpected(retained.error());
            std::weak_ptr<BasicNativeTooltipPresentation> weak=port;
            if(auto registered=builder.on_publish([weak]{if(auto port=weak.lock();port&&!port->retired_)port->live_=true;});!registered)
                return std::unexpected(registered.error());
            if(auto registered=builder.on_retire([weak]{if(auto port=weak.lock()){
                port->retired_=true;port->live_=false;
                try{if(port->scheduler_&&!port->scheduler_(std::nullopt))port->uncertain_=true;}catch(...){port->uncertain_=true;}
                if(!port->busy_)port->release();
            }});!registered)return std::unexpected(registered.error());
            return port;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip presentation construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return popup_;}
    [[nodiscard]] ii::gui::Widget content_root()const{return content_;}
    [[nodiscard]] bool needs_reconciliation()const noexcept override{return uncertain_;}
    [[nodiscard]] ii::core::Result<TooltipPresentationStyle> style()const {
        if(auto valid=ready(true);!valid)return std::unexpected(valid.error());
        try{return style_;}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip style read failed",std::current_exception());}
    }
public:
    [[nodiscard]] ii::core::Result<void> set_style_with_duration(TooltipPresentationStyle value,std::chrono::milliseconds duration){
        // Before publication no popup is shown; retain the candidate style so
        // reactive inputs can settle without exposing a staged generation.
        if(auto permitted=ready(true);!permitted)return permitted;
        if(!validate(value)||duration<std::chrono::milliseconds::zero()||duration>std::chrono::hours(24))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid tooltip style or animation duration");
        if(uncertain_)return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip presentation needs reconciliation");
        try {
            Guard guard(*this);
            auto offset=scroll_checkpoint();if(!offset)return std::unexpected(offset.error());
            auto previous=style_;const auto previous_duration=motion_duration_;style_=std::move(value);motion_duration_=duration;
            const auto restore=[&]()->ii::core::Result<void>{
                style_=previous;motion_duration_=previous_duration;
                if(current_)return apply(current_);
                if(popup_options_){auto options=*popup_options_;options.transition=transition(style_,motion_duration_,frames_supported_);return set_popup_options(std::move(options));}
                return {};
            };
            if(current_){
                auto changed=apply(current_);
                if(retired_){style_=std::move(previous);motion_duration_=previous_duration;uncertain_=true;return ii::core::fail(ii::core::ErrorCode::closed,"Tooltip retired during style update");}
                if(!changed){auto restored=restore();
                    if(restored)restored=restore_scroll(*offset);
                    if(!restored){uncertain_=true;reconcile_offset_=*offset;}return changed;}
            }else if(live_){
                if(popup_options_){auto options=*popup_options_;options.transition=transition(style_,motion_duration_,frames_supported_);
                    if(auto changed=set_popup_options(std::move(options));!changed){auto restored=restore();if(restored)restored=restore_scroll(*offset);
                        if(!restored){uncertain_=true;reconcile_offset_=*offset;}return changed;}
                    if(retired_){style_=std::move(previous);motion_duration_=previous_duration;uncertain_=true;return ii::core::fail(ii::core::ErrorCode::closed,"Tooltip retired during motion update");}}
                if(previous.scroll_content&&!style_.scroll_content){
                    auto cleared=viewport_.set_scroll_offset(0);
                    if(retired_){style_=std::move(previous);motion_duration_=previous_duration;uncertain_=true;return ii::core::fail(ii::core::ErrorCode::closed,"Tooltip retired during scroll update");}
                    if(!cleared){auto restored=restore();if(restored)restored=restore_scroll(*offset);
                        if(!restored){uncertain_=true;reconcile_offset_=*offset;}return cleared;}
                }
            }
            uncertain_=false;return {};
        }catch(...){uncertain_=true;return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip style update failed",std::current_exception());}
    }
public:
    [[nodiscard]] ii::core::Result<void> set_style(TooltipPresentationStyle value){return set_style_with_duration(std::move(value),motion_duration_);}
    [[nodiscard]] ii::core::Result<void> set_deadline(std::optional<std::int64_t> deadline)override{
        if(auto valid=ready(!deadline);!valid)return valid;
        try {
            Guard guard(*this);auto scheduled=scheduler_(deadline);
            if(retired_){uncertain_=true;return ii::core::fail(ii::core::ErrorCode::closed,"Tooltip retired during scheduling");}
            return scheduled;
        }
        catch(...){uncertain_=true;return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip scheduler failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> present(std::optional<TooltipLayout> layout)override{
        if(auto valid=ready(!layout);!valid)return valid;
        if(!live_&&!layout)return {};
        try {
            Guard guard(*this);
            auto offset=scroll_checkpoint();if(!offset)return std::unexpected(offset.error());
            const auto accepted_offset=reconcile_offset_.has_value()?reconcile_offset_:*offset;
            auto result=apply(layout);
            if(result&&reconcile_offset_)result=restore_scroll(reconcile_offset_);
            if(retired_){uncertain_=true;return ii::core::fail(ii::core::ErrorCode::closed,"Tooltip retired during presentation");}
            if(!result){auto restored=apply(current_);if(restored)restored=restore_scroll(accepted_offset);
                if(!restored){uncertain_=true;reconcile_offset_=accepted_offset;}return result;}
            current_=std::move(layout);uncertain_=false;reconcile_offset_.reset();return {};
        }catch(...){uncertain_=true;return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip presentation failed",std::current_exception());}
    }
};
} // namespace ii::gui
