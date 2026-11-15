#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
namespace ii::gui {
// The application injects the model, presentation, scheduler and target.
// The builder supplies widget services and the Core event loop.
template<class Policy>
class BasicNativeTooltip final {
    using NativeTooltipPresentation=typename Policy::Presentation;using TooltipController=typename Policy::Controller;
    using NativeTooltipScheduler=typename Policy::Scheduler;using NativeTooltipTarget=typename Policy::Target;
    using Tooltip=typename Policy::Model;using TooltipConfiguration=typename Policy::Configuration;
    using TooltipPresentationStyle=typename Policy::Style;using TooltipTargetOptions=typename Policy::Options;
    using TooltipCallbacks=typename Policy::Callbacks;using TooltipInput=typename Policy::Input;
    using TooltipDiagnostic=typename Policy::Diagnostic;using TooltipSnapshot=typename Policy::Snapshot;
    std::shared_ptr<NativeTooltipPresentation> presentation_;
    TooltipController controller_;
    NativeTooltipScheduler scheduler_;
    NativeTooltipTarget target_;
    BasicNativeTooltip(std::shared_ptr<NativeTooltipPresentation> presentation,TooltipController controller,
            NativeTooltipScheduler scheduler,NativeTooltipTarget target)
        :presentation_(std::move(presentation)),controller_(std::move(controller)),
         scheduler_(std::move(scheduler)),target_(std::move(target)){}
public:
    [[nodiscard]] static ii::core::Result<BasicNativeTooltip> create(ii::gui::WidgetBuilder& builder,
            ii::gui::Widget parent,std::string_view id,TooltipConfiguration configuration={},
            TooltipPresentationStyle style={},TooltipTargetOptions options={},TooltipCallbacks callbacks={}){
        if(!Tooltip::validate(configuration)||configuration.content_padding!=style.content_padding)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Tooltip content padding and configuration must agree");
        auto loop=builder.event_loop();if(!loop)return std::unexpected(loop.error());
        auto scheduler=NativeTooltipScheduler::create(loop->get().sender());if(!scheduler)return std::unexpected(scheduler.error());
        auto presentation=NativeTooltipPresentation::create(builder,std::move(parent),id,std::move(style),scheduler->weak_schedule());
        if(!presentation)return std::unexpected(presentation.error());
        auto controller=TooltipController::create(*presentation,std::move(configuration),std::move(callbacks));
        if(!controller)return std::unexpected(controller.error());
        auto target=NativeTooltipTarget::create(builder,*presentation,*controller,*scheduler,std::move(options));
        if(!target)return std::unexpected(target.error());
        if(auto wake=scheduler->set_wake(target->weak_tick());!wake)return std::unexpected(wake.error());
        return BasicNativeTooltip(std::move(*presentation),std::move(*controller),std::move(*scheduler),std::move(*target));
    }
    [[nodiscard]] ii::gui::Widget root()const{return presentation_->root();}
    [[nodiscard]] ii::gui::Widget content_root()const{return presentation_->content_root();}
    [[nodiscard]] ii::core::Result<void> set_target(ii::gui::Widget target)const{return target_.set_target(std::move(target));}
    [[nodiscard]] ii::core::Result<TooltipInput> options()const{return target_.options();}
    [[nodiscard]] ii::core::Result<std::shared_ptr<const ii::core::Observable<TooltipDiagnostic>>> diagnostic_source()const{return target_.diagnostic_source();}
    [[nodiscard]] ii::core::Result<void> set_options(TooltipInput value)const{return target_.set_options(std::move(value));}
    [[nodiscard]] ii::core::Result<void> reconcile()const{return target_.reconcile();}
    [[nodiscard]] ii::core::Result<void> refresh()const{return target_.refresh();}
    [[nodiscard]] ii::core::Result<void> open()const{return target_.open();}
    [[nodiscard]] ii::core::Result<void> close()const{return target_.close();}
    [[nodiscard]] ii::core::Result<TooltipSnapshot> snapshot()const {
        auto observing=target_.status();if(!observing)return std::unexpected(observing.error());
        return controller_.snapshot();
    }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> last_failure()const {
        auto observed=target_.last_failure();if(!observed||*observed)return observed;
        return scheduler_.last_failure();
    }
};
} // namespace ii::gui
