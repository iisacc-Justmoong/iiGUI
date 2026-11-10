#pragma once
#include <iiGUI/Appearance.h>
#include <algorithm>
#include <cmath>
#include <optional>

namespace ii::gui {
enum class DividerAxis { horizontal, vertical };
struct DividerConfiguration final {
    DividerAxis axis = DividerAxis::horizontal;
    double length = 220, thickness = 1, cross_padding = 1, line_padding = 0;
    WidgetColor color{0, 0, 0, 1};
    friend bool operator==(const DividerConfiguration&,const DividerConfiguration&) = default;
};
struct DividerSize final { double width=0,height=0; };
struct DividerRect final { double x=0,y=0,width=0,height=0; };

// The model owns a non-interactive line and its natural size. Hosts only
// allocate an extent and submit the resulting rectangle to a renderer.
class Divider final {
public:
    [[nodiscard]] const DividerConfiguration& configuration() const noexcept { return configuration_; }
    [[nodiscard]] static bool validate(const DividerConfiguration& value) noexcept {
        const auto bound=[](double n){return std::isfinite(n)&&n>=0&&n<=8192;};
        const auto channel=[](double n){return std::isfinite(n)&&n>=0&&n<=1;};
        if(value.axis!=DividerAxis::horizontal&&value.axis!=DividerAxis::vertical)return false;
        return bound(value.length)&&bound(value.thickness)&&bound(value.cross_padding)&&bound(value.line_padding)&&
            value.thickness+2*value.cross_padding<=8192&&channel(value.color.red)&&channel(value.color.green)&&
            channel(value.color.blue)&&channel(value.color.alpha);
    }
    bool set_configuration(DividerConfiguration value) noexcept {
        if(!validate(value))return false;
        configuration_=value;return true;
    }
    [[nodiscard]] DividerSize extent() const noexcept {
        const auto cross=configuration_.thickness+2*configuration_.cross_padding;
        return configuration_.axis==DividerAxis::horizontal?DividerSize{configuration_.length,cross}:
                                                            DividerSize{cross,configuration_.length};
    }
    [[nodiscard]] std::optional<DividerRect> geometry(double width,double height) const noexcept {
        const auto bound=[](double n){return std::isfinite(n)&&n>=0&&n<=8192;};
        if(!bound(width)||!bound(height))return std::nullopt;
        const bool horizontal=configuration_.axis==DividerAxis::horizontal;
        const auto primary=horizontal?width:height,cross=horizontal?height:width;
        const auto inset=(std::min)(configuration_.line_padding,primary/2);
        const auto thickness=(std::min)(configuration_.thickness,cross);
        const auto cross_offset=(cross-thickness)/2;
        const auto line_length=(std::max)(0.0,primary-2*inset);
        return horizontal?DividerRect{inset,cross_offset,line_length,thickness}:
                          DividerRect{cross_offset,inset,thickness,line_length};
    }
private:
    DividerConfiguration configuration_;
};
} // namespace ii::gui
