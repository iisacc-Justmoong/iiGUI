#pragma once
#include <iiGUI/ImageModel.h>
#include <cmath>
#include <string>
#include <utility>

namespace ii::gui {
enum class AvatarSize { small, medium, large };
struct AvatarConfiguration final {
    AvatarSize size=AvatarSize::large;
    double width=0,height=0;
    WidgetColor background{0,0,0,0};
    bool enabled=true;
    std::string accessibility_name="User avatar";
    friend bool operator==(const AvatarConfiguration&,const AvatarConfiguration&)=default;
};

// The host supplies pixels; the iiGUI image model supplies the crop and mask.
class Avatar final {
    AvatarConfiguration configuration_;
public:
    [[nodiscard]] static bool validate(const AvatarConfiguration& value)noexcept {
        const auto metric=[](double n){return std::isfinite(n)&&n>=0&&n<=8192;};
        const auto channel=[](double n){return std::isfinite(n)&&n>=0&&n<=1;};
        return (value.size==AvatarSize::small||value.size==AvatarSize::medium||value.size==AvatarSize::large)&&
            metric(value.width)&&metric(value.height)&&channel(value.background.red)&&channel(value.background.green)&&
            channel(value.background.blue)&&channel(value.background.alpha)&&valid_image_accessibility_text(value.accessibility_name);
    }
    bool set_configuration(AvatarConfiguration value){
        if(!validate(value))return false;configuration_=std::move(value);return true;
    }
    [[nodiscard]] const AvatarConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] ImageConfiguration image_configuration()const {
        const auto natural=configuration_.size==AvatarSize::small?24.:configuration_.size==AvatarSize::medium?40.:64.;
        return {.width=configuration_.width?configuration_.width:natural,
            .height=configuration_.height?configuration_.height:natural,.padding=0,.corner_radius=0,.border_width=0,
            .background=configuration_.background,.border=std::nullopt,.fit=ImageFit::cover,.preserve_aspect=false,
            .circular=true,.allow_empty=true,.accessibility_name=configuration_.accessibility_name};
    }
};
} // namespace ii::gui
