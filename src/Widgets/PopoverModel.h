#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
namespace ii::gui {
struct PopoverSize final {double width=0,height=0;friend bool operator==(const PopoverSize&,const PopoverSize&)=default;};
struct PopoverConfiguration final {
    double width=0,height=0,padding=12,margins=8,corner_radius=12,surface_opacity=.25;
    std::optional<WidgetColor> surface_color,primary_color;
    bool modal=false,dim=false,focus_on_open=true,clip_content=false,dismiss_on_escape=true,dismiss_on_outside_press=true;
    std::optional<bool> dismiss_on_context_request;std::string accessibility_name="Popover";
    PopoverPlacement placement=PopoverPlacement::anchored;bool blur_enabled=true,motion_enabled=true;
    friend bool operator==(const PopoverConfiguration&,const PopoverConfiguration&)=default;
};
struct DefaultPopoverModelPolicy final {
    using Configuration=PopoverConfiguration;using Size=PopoverSize;using Placement=PopoverPlacement;using Color=WidgetColor;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return value.size()<=65536&&value.find('\0')==std::string_view::npos&&ii::core::valid_utf8(value);}
};
// The framework owns validation and natural-size calculation.
template<class Policy>
class BasicPopover final {
    using PopoverConfiguration=typename Policy::Configuration;using PopoverSize=typename Policy::Size;
    using PopoverPlacement=typename Policy::Placement;using StyleColor=typename Policy::Color;
    PopoverConfiguration configuration_;
    [[nodiscard]] static bool scalar(double value,double limit=8192)noexcept{return std::isfinite(value)&&value>=0&&value<=limit;}
public:
    [[nodiscard]] static bool validate(const PopoverConfiguration& value)noexcept{
        const auto color=[](StyleColor c){return scalar(c.red,1)&&scalar(c.green,1)&&scalar(c.blue,1)&&scalar(c.alpha,1);};
        return scalar(value.width)&&scalar(value.height)&&scalar(value.padding)&&scalar(value.margins)&&scalar(value.corner_radius)&&
            scalar(value.surface_opacity,1)&&(!value.surface_color||color(*value.surface_color))&&
            (!value.primary_color||color(*value.primary_color))&&Policy::valid_text(value.accessibility_name)&&
            (value.placement==PopoverPlacement::anchored||value.placement==PopoverPlacement::centered);
    }
    [[nodiscard]] const PopoverConfiguration& configuration()const noexcept{return configuration_;}
    bool set_configuration(PopoverConfiguration value){if(!validate(value))return false;configuration_=std::move(value);return true;}
    [[nodiscard]] std::optional<PopoverSize> natural_size(PopoverSize content)const noexcept{
        if(!scalar(content.width)||!scalar(content.height))return std::nullopt;
        const auto width=configuration_.width?configuration_.width:content.width+2*configuration_.padding;
        const auto height=configuration_.height?configuration_.height:content.height+2*configuration_.padding;
        if(!scalar(width)||!scalar(height))return std::nullopt;return PopoverSize{width,height};
    }
};
using PopoverModel=BasicPopover<DefaultPopoverModelPolicy>;
} // namespace ii::gui
