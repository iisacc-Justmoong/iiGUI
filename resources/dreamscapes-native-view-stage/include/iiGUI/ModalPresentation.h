#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <string>
#include <utility>
namespace ii::gui {
// Builds native modal text and action inputs from app-provided values.
struct ModalPresentation final {
    template<class Encoded>[[nodiscard]] static WidgetColor color(const Encoded& value)noexcept{
        return {value.red,value.green,value.blue,value.alpha};
    }
    template<class Encoded>[[nodiscard]] static LabelOptions title(std::string text,const Encoded& color_value){
        return {.text=std::move(text),.font_size=22,.selectable=false,.role=TextRole::title2,
                .color=color(color_value),.line_height=22,.font_weight=700};
    }
    template<class Encoded>[[nodiscard]] static LabelOptions description(std::string text,const Encoded& color_value){
        return {.text=std::move(text),.font_size=13,.selectable=false,.role=TextRole::body,
                .color=color(color_value),.line_height=18,.font_weight=500};
    }
    template<class ButtonInput,class AlertButtonConfiguration,class AlertButtonModel,class Action,class Tone>
    [[nodiscard]] static core::Result<ButtonInput> action(unsigned index,const Action& action,Tone primary,Tone normal){
        if(index>=3)return core::fail(core::ErrorCode::out_of_range,"Modal action index is out of range");
        AlertButtonConfiguration button;button.text=std::string(action.text);button.enabled=action.enabled;
        button.tone=index==0?primary:normal;button.height=20;
        auto appearance=AlertButtonModel::resolve(button);
        if(!appearance)return core::fail(core::ErrorCode::invalid_argument,"Invalid modal action");
        ButtonInput result;result.configuration=std::move(appearance->button);return result;
    }
};
} // namespace ii::gui
