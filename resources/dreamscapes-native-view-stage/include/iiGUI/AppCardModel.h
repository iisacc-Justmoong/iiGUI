#pragma once
#include <iiGUI/Appearance.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace ii::gui {
enum class CardShape { round_rect,cylinder };
struct AppCardOptions final {
    std::string title,subtitle;
    bool enabled=true;
    double width=0,height=0,padding=18,spacing=10,corner_radius=12;
    double minimum_width=280,title_size=17,subtitle_size=12,heading_spacing=4,separator_height=1;
    CardShape shape=CardShape::round_rect;
    WidgetColor background{0,0,0,0};
    WidgetColor title_color{1,1,1,.9},subtitle_color{1,1,1,.8},divider_color{1,1,1,.12};
    friend bool operator==(const AppCardOptions&,const AppCardOptions&)=default;
};
struct AppCardSize final {double width=0,height=0;friend bool operator==(const AppCardSize&,const AppCardSize&)=default;};
struct AppCardRect final {double x=0,y=0,width=0,height=0;};
struct AppCardMeasurements final {double title_width=0,subtitle_width=0,content_width=0,content_height=0;};
struct AppCardGeometry final {
    double radius=0;
    bool header=false;
    AppCardRect title,subtitle,content;
    std::optional<AppCardRect> separator;
};

// This model owns the card's style and geometry. A host supplies text and
// content measurements and is free to render or embed the body independently.
class AppCardModel final {
    AppCardOptions configuration_;
    [[nodiscard]] static bool scalar(double value,double limit=8192)noexcept{return std::isfinite(value)&&value>=0&&value<=limit;}
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept {
        return value.size()<=65536&&value.find('\0')==std::string_view::npos&&ii::core::valid_utf8(value);
    }
    [[nodiscard]] static bool color(WidgetColor value)noexcept {
        return scalar(value.red,1)&&scalar(value.green,1)&&scalar(value.blue,1)&&scalar(value.alpha,1);
    }
    [[nodiscard]] double heading_height()const noexcept {
        const auto title=!configuration_.title.empty(),subtitle=!configuration_.subtitle.empty();
        return (title?configuration_.title_size:0.)+(subtitle?configuration_.subtitle_size:0.)+
            (title&&subtitle?configuration_.heading_spacing:0.);
    }
public:
    [[nodiscard]] const AppCardOptions& configuration()const noexcept{return configuration_;}
    [[nodiscard]] static bool validate(const AppCardOptions& value)noexcept {
        return valid_text(value.title)&&valid_text(value.subtitle)&&
            scalar(value.width)&&scalar(value.height)&&scalar(value.padding)&&scalar(value.spacing)&&scalar(value.corner_radius)&&
            scalar(value.minimum_width)&&scalar(value.heading_spacing)&&scalar(value.separator_height)&&
            value.title_size>=1&&scalar(value.title_size,256)&&value.subtitle_size>=1&&scalar(value.subtitle_size,256)&&
            (value.shape==CardShape::round_rect||value.shape==CardShape::cylinder)&&
            color(value.background)&&color(value.title_color)&&color(value.subtitle_color)&&color(value.divider_color);
    }
    bool set_configuration(AppCardOptions value){if(!validate(value))return false;configuration_=std::move(value);return true;}
    [[nodiscard]] std::optional<AppCardSize> natural_size(const AppCardMeasurements& measurements)const noexcept {
        constexpr double measurement_limit=1000000000.;
        if(!scalar(measurements.title_width,measurement_limit)||!scalar(measurements.subtitle_width,measurement_limit)||
           !scalar(measurements.content_width,measurement_limit)||!scalar(measurements.content_height,measurement_limit))return std::nullopt;
        const bool header=heading_height()>0;
        const double content_width=(std::max)({configuration_.title.empty()?0.:measurements.title_width,
            configuration_.subtitle.empty()?0.:measurements.subtitle_width,measurements.content_width});
        const auto width=(std::min)(8192.,(std::max)(configuration_.minimum_width,content_width+2*configuration_.padding));
        const auto height=(std::min)(8192.,2*configuration_.padding+(std::max)(1.,measurements.content_height)+
            (header?heading_height()+2*configuration_.spacing+configuration_.separator_height:0));
        return AppCardSize{configuration_.width?configuration_.width:width,configuration_.height?configuration_.height:height};
    }
    [[nodiscard]] std::optional<AppCardGeometry> geometry(double width,double height)const noexcept {
        if(!scalar(width)||!scalar(height))return std::nullopt;
        AppCardGeometry result;result.header=heading_height()>0;
        result.radius=(std::min)({configuration_.shape==CardShape::cylinder?(std::min)(width,height)/2:configuration_.corner_radius,width/2,height/2});
        const auto x=(std::min)(configuration_.padding,width/2),top=(std::min)(configuration_.padding,height/2);
        const auto inner_width=(std::max)(0.,width-2*x),bottom=(std::max)(top,height-top);
        const auto rectangle=[&](double y,double h){const auto start=std::clamp(y,0.,height);return AppCardRect{x,start,inner_width,(std::max)(0.,(std::min)(h,height-start))};};
        double cursor=top;
        if(!configuration_.title.empty()){result.title=rectangle(cursor,configuration_.title_size);cursor+=configuration_.title_size;}
        if(!configuration_.subtitle.empty()){
            if(!configuration_.title.empty())cursor+=configuration_.heading_spacing;result.subtitle=rectangle(cursor,configuration_.subtitle_size);cursor+=configuration_.subtitle_size;
        }
        if(result.header){cursor+=configuration_.spacing;result.separator=rectangle(cursor,configuration_.separator_height);cursor+=configuration_.separator_height+configuration_.spacing;}
        const auto body=std::clamp(cursor,0.,height);result.content={x,body,inner_width,(std::max)(0.,bottom-body)};
        return result;
    }
};
} // namespace ii::gui
