#pragma once

#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <cmath>
#include <exception>
#include <memory>

#define IIGUI_PROGRESS_DRAWING_API 1

namespace ii::gui {

// Logical geometry and encoded sRGB are supplied by the caller's style policy.
// The result is an ordered, renderer-independent iiGUI drawing.
struct ProgressDrawing final {
    double width=0,height=0,fill_x=0,fill_width=0;
    double track_radius=0,fill_radius=0,stroke_width=0;
    WidgetColor track,fill,outline;
};

[[nodiscard]] inline core::Result<std::shared_ptr<const WidgetDrawing>> progress_drawing(const ProgressDrawing& value) {
    const auto valid_extent=[](double n){return std::isfinite(n)&&n>=0&&n<=32767;};
    const auto valid_color=[](WidgetColor c){
        const auto channel=[](double n){return std::isfinite(n)&&n>=0&&n<=1;};
        return channel(c.red)&&channel(c.green)&&channel(c.blue)&&channel(c.alpha);
    };
    if(!valid_extent(value.width)||!valid_extent(value.height)||!valid_extent(value.fill_x)||
       !valid_extent(value.fill_width)||!valid_extent(value.track_radius)||!valid_extent(value.fill_radius)||
       !valid_extent(value.stroke_width)||!valid_color(value.track)||!valid_color(value.fill)||!valid_color(value.outline))
        return core::fail(core::ErrorCode::invalid_argument,"Invalid progress drawing");
    if(value.width>0&&value.height>0){
        const auto width=float(value.width),height=float(value.height),fill_width=float(value.fill_width);
        const auto radius=float(value.track_radius),fill_radius=float(value.fill_radius),stroke=float(value.stroke_width);
        if(width<=0||height<=0||width>8192||height>8192||fill_width>8192||
           radius>(std::min)(width,height)/2||stroke>(std::min)(width,height)/2||
           (value.stroke_width>0&&stroke<=0)||
           (value.fill_width>0&&(fill_width<=0||fill_radius>(std::min)(fill_width,height)/2)))
            return core::fail(core::ErrorCode::invalid_argument,"Progress drawing exceeds panel bounds");
    }
    try{
        auto drawing=std::make_shared<WidgetDrawing>();
        if(value.width>0&&value.height>0){
            const auto linear=[](WidgetColor color){
                const auto channel=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
                return Color{float(channel(color.red)),float(channel(color.green)),float(channel(color.blue)),float(color.alpha)};
            };
            const Rect bounds{0,0,float(value.width),float(value.height)};
            const auto track=linear(value.track);
            drawing->panels.push_back({bounds,track,track,float(value.track_radius)});
            if(value.fill_width>0){
                const auto fill=linear(value.fill);
                drawing->panels.push_back({{float(value.fill_x),0,float(value.fill_width),float(value.height)},
                                           fill,fill,float(value.fill_radius)});
            }
            if(value.stroke_width>0){
                const auto outline=linear(value.outline);
                drawing->panels.push_back({bounds,outline,outline,float(value.track_radius),float(value.stroke_width)});
            }
        }
        return std::shared_ptr<const WidgetDrawing>(std::move(drawing));
    }catch(...){return core::fail(core::ErrorCode::failed,"Progress drawing allocation failed",std::current_exception());}
}

} // namespace ii::gui
