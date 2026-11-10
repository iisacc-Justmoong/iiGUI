#pragma once
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace ii::gui {
enum class SegmentedShape { round_rect, cylinder };
enum class SegmentedOrientation { horizontal, vertical };
struct SegmentedControlConfiguration final {
    double horizontal_padding=0,vertical_padding=0,spacing=2,border_width=0,corner_radius=default_widget_corner_radius;
    SegmentedShape shape=SegmentedShape::round_rect;
    SegmentedOrientation orientation=SegmentedOrientation::horizontal;
    bool enabled=true,force_borderless_tone=true;
    WidgetColor background{},border{};
    friend bool operator==(const SegmentedControlConfiguration&,const SegmentedControlConfiguration&)=default;
};
struct SegmentedControlGeometry final {double width=0,height=0,radius=0,border_width=0;};
struct DefaultSegmentedControlModelPolicy final {
    using Configuration=SegmentedControlConfiguration;using Geometry=SegmentedControlGeometry;
    using Shape=SegmentedShape;using Orientation=SegmentedOrientation;using Color=WidgetColor;
};
// A presentation container: the children own their actions and selection.
template<class Policy>
class BasicSegmentedControl final {
    using Configuration=typename Policy::Configuration;using Geometry=typename Policy::Geometry;
    using Shape=typename Policy::Shape;using Orientation=typename Policy::Orientation;using Color=typename Policy::Color;
    Configuration configuration_;
public:
    [[nodiscard]] static bool validate(const Configuration& v)noexcept{
        const auto bound=[](double n,double hi){return std::isfinite(n)&&n>=0&&n<=hi;};
        const auto color=[&](Color c){return bound(c.red,1)&&bound(c.green,1)&&bound(c.blue,1)&&bound(c.alpha,1);};
        return bound(v.horizontal_padding,8192)&&bound(v.vertical_padding,8192)&&bound(v.spacing,32767)&&
            bound(v.border_width,8192)&&bound(v.corner_radius,8192)&&
            (v.shape==Shape::round_rect||v.shape==Shape::cylinder)&&
            (v.orientation==Orientation::horizontal||v.orientation==Orientation::vertical)&&color(v.background)&&color(v.border);
    }
    [[nodiscard]] bool set_configuration(Configuration value){
        if(!validate(value))return false;configuration_=std::move(value);return true;
    }
    [[nodiscard]] const Configuration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] std::optional<Geometry> geometry(double width,double height)const noexcept{
        if(!std::isfinite(width)||!std::isfinite(height)||width<0||height<0||width>8192||height>8192)return std::nullopt;
        const auto max_radius=(std::min)(width,height)/2;
        const auto radius=configuration_.shape==Shape::cylinder?max_radius:(std::min)(configuration_.corner_radius,max_radius);
        return Geometry{width,height,radius,(std::min)(configuration_.border_width,max_radius)};
    }
};
using SegmentedControlModel=BasicSegmentedControl<DefaultSegmentedControlModelPolicy>;
} // namespace ii::gui
