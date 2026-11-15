#pragma once
#include <iiGUI/NativeImageView.h>
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <memory>
#include <string>
#include <utility>

namespace ii::gui {
// Geometry and drawing inputs for an alert; applications supply colors and assets.
struct AlertPresentation final {
    using ImageInput=ii::gui::ImageInput;
    template<class Encoded>[[nodiscard]] static WidgetColor color(const Encoded& value)noexcept{
        return {value.red,value.green,value.blue,value.alpha};
    }
    template<class Background,class Border>[[nodiscard]] static ImageInput icon(
            std::shared_ptr<const RasterImage> image,double extent,unsigned actions,const Background& background,const Border& border){
        ImageInput result;result.image=std::move(image);auto& value=result.configuration;
        value.width=value.height=extent;value.padding=extent*(actions==3?15./86:11./86);
        value.corner_radius=extent*28./86;value.border_width=1;value.background=color(background);value.border=color(border);
        value.fit=ImageFit::contain;value.allow_empty=true;return result;
    }
    template<class Encoded>[[nodiscard]] static LabelOptions title(std::string text,const Encoded& color_value){
        return {.text=std::move(text),.font_size=26,.selectable=false,.role=TextRole::title,.color=color(color_value),
                .alignment=TextAlignment::center,.line_height=26,.font_weight=700};
    }
    template<class Encoded>[[nodiscard]] static LabelOptions description(std::string text,const Encoded& color_value){
        return {.text=std::move(text),.font_size=13,.selectable=false,.role=TextRole::body,.color=color(color_value),
                .alignment=TextAlignment::center,.line_height=13,.font_weight=500};
    }
    template<class ButtonConfiguration,class ButtonModel,class Model>
    [[nodiscard]] static core::Result<ButtonConfiguration> action(const Model& model,unsigned index){
        if(index>=3)return core::fail(core::ErrorCode::out_of_range,"Alert action index is out of range");
        ButtonConfiguration button;const auto item=model.action(index);const auto& c=model.configuration();
        button.text=std::string(item.text);button.enabled=item.enabled;button.tone=item.tone;button.dialog_style=true;
        button.primary_color=c.primary_color;
        button.height=index==2&&model.action_count()==3?c.metrics.tertiary_action_height:c.metrics.action_height;
        if(item.destructive)button.text_color=button.palette.danger;
        if(!ButtonModel::resolve(button))return core::fail(core::ErrorCode::invalid_argument,"Invalid alert action");
        return button;
    }
};
} // namespace ii::gui
