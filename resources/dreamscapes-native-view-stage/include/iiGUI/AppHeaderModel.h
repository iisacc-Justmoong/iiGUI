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
struct AppHeaderMetrics final {
    double horizontal_padding=0,vertical_padding=0,row_spacing=0,action_spacing=0,menu_padding=0,menu_height=0;
    friend bool operator==(const AppHeaderMetrics&,const AppHeaderMetrics&)=default;
};
struct AppHeaderOptions final {
    std::string title,subtitle;
    bool enabled=true,menu_visible=false,compact=false;
    double width=0,height=0;
    std::optional<double> content_horizontal_padding,content_vertical_padding,row_spacing,action_spacing,menu_button_padding;
    AppHeaderMetrics normal_metrics{16,16,12,8,10,32},compact_metrics{10,10,8,6,8,28};
    double title_size=22,subtitle_size=12,title_spacing=4,menu_icon_width=12;
    double minimum_height=56,implicit_vertical_padding=16,menu_corner_radius=8,focus_ring_width=1.5,menu_line_thickness=1.5,menu_line_spacing=4;
    unsigned title_weight=700,subtitle_weight=600;
    WidgetColor background{0,0,0,0},title_color{1,1,1,.9},subtitle_color{1,1,1,.8};
    WidgetColor menu_background{0,0,0,0},menu_pressed_background{0,0,0,0},menu_focus{1,1,1,1};
    friend bool operator==(const AppHeaderOptions&,const AppHeaderOptions&)=default;
};
struct AppHeaderMeasurements final {double title_width=0,subtitle_width=0,actions_width=0,actions_height=0;};
struct AppHeaderSize final {double width=0,height=0;};

class AppHeaderModel final {
    AppHeaderOptions configuration_;
    [[nodiscard]] static bool scalar(double value,double maximum=8192)noexcept{return std::isfinite(value)&&value>=0&&value<=maximum;}
    [[nodiscard]] static bool optional_scalar(const std::optional<double>& value)noexcept{return !value||scalar(*value);}
    [[nodiscard]] static bool color(WidgetColor value)noexcept {
        return scalar(value.red,1)&&scalar(value.green,1)&&scalar(value.blue,1)&&scalar(value.alpha,1);
    }
    [[nodiscard]] static bool valid_text(std::string_view text)noexcept {
        return text.size()<=65536&&text.find('\0')==std::string_view::npos&&ii::core::valid_utf8(text);
    }
    [[nodiscard]] static bool metrics(const AppHeaderMetrics& value)noexcept {
        return scalar(value.horizontal_padding)&&scalar(value.vertical_padding)&&scalar(value.row_spacing)&&
            scalar(value.action_spacing)&&scalar(value.menu_padding,4090)&&scalar(value.menu_height);
    }
    [[nodiscard]] static bool weight(unsigned value)noexcept{return value==400||value==500||value==600||value==700;}
public:
    [[nodiscard]] const AppHeaderOptions& configuration()const noexcept{return configuration_;}
    [[nodiscard]] static bool validate(const AppHeaderOptions& value)noexcept {
        return valid_text(value.title)&&valid_text(value.subtitle)&&scalar(value.width)&&scalar(value.height)&&
            optional_scalar(value.content_horizontal_padding)&&optional_scalar(value.content_vertical_padding)&&optional_scalar(value.row_spacing)&&
            optional_scalar(value.action_spacing)&&(!value.menu_button_padding||scalar(*value.menu_button_padding,4090))&&
            metrics(value.normal_metrics)&&metrics(value.compact_metrics)&&
            value.title_size>=1&&scalar(value.title_size,256)&&value.subtitle_size>=1&&scalar(value.subtitle_size,256)&&
            scalar(value.title_spacing)&&scalar(value.menu_icon_width,256)&&
            2*value.normal_metrics.menu_padding+value.menu_icon_width<=8192&&
            2*value.compact_metrics.menu_padding+value.menu_icon_width<=8192&&
            (!value.menu_button_padding||2*(*value.menu_button_padding)+value.menu_icon_width<=8192)&&
            scalar(value.minimum_height)&&scalar(value.implicit_vertical_padding)&&
            scalar(value.menu_corner_radius)&&scalar(value.focus_ring_width)&&scalar(value.menu_line_thickness)&&scalar(value.menu_line_spacing)&&
            weight(value.title_weight)&&weight(value.subtitle_weight)&&color(value.background)&&color(value.title_color)&&
            color(value.subtitle_color)&&color(value.menu_background)&&color(value.menu_pressed_background)&&color(value.menu_focus);
    }
    bool set_configuration(AppHeaderOptions value){if(!validate(value))return false;configuration_=std::move(value);return true;}
    [[nodiscard]] AppHeaderMetrics resolved()const noexcept {
        auto result=configuration_.compact?configuration_.compact_metrics:configuration_.normal_metrics;
        if(configuration_.content_horizontal_padding)result.horizontal_padding=*configuration_.content_horizontal_padding;
        if(configuration_.content_vertical_padding)result.vertical_padding=*configuration_.content_vertical_padding;
        if(configuration_.row_spacing)result.row_spacing=*configuration_.row_spacing;
        if(configuration_.action_spacing)result.action_spacing=*configuration_.action_spacing;
        if(configuration_.menu_button_padding)result.menu_padding=*configuration_.menu_button_padding;
        return result;
    }
    [[nodiscard]] std::optional<AppHeaderSize> natural_size(const AppHeaderMeasurements& value)const noexcept {
        constexpr auto limit=1000000000.;
        if(!scalar(value.title_width,limit)||!scalar(value.subtitle_width,limit)||!scalar(value.actions_width,limit)||!scalar(value.actions_height,limit))return std::nullopt;
        const auto metrics=resolved();const bool title=!configuration_.title.empty(),subtitle=!configuration_.subtitle.empty();
        const auto title_width=(std::max)(title?value.title_width:0.,subtitle?value.subtitle_width:0.);
        const auto title_height=(title?configuration_.title_size:0.)+(subtitle?configuration_.subtitle_size:0.)+(title&&subtitle?configuration_.title_spacing:0.);
        const auto menu_width=configuration_.menu_visible?2*metrics.menu_padding+configuration_.menu_icon_width:0.;
        const auto menu_gap=configuration_.menu_visible?metrics.row_spacing:0.;
        const auto action_gap=value.actions_width>0?metrics.row_spacing:0.;
        const auto width=(std::min)(8192.,2*metrics.horizontal_padding+menu_width+menu_gap+title_width+action_gap+value.actions_width);
        const auto height=(std::min)(8192.,(std::max)(configuration_.minimum_height,2*configuration_.implicit_vertical_padding+
            (std::max)({title_height,configuration_.menu_visible?metrics.menu_height:0.,value.actions_height})));
        return AppHeaderSize{configuration_.width?configuration_.width:width,configuration_.height?configuration_.height:height};
    }
};
} // namespace ii::gui
