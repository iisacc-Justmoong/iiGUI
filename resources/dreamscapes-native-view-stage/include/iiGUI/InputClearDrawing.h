#pragma once
#include <iiGUI/SrgbColor.h>

#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <cmath>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace ii::gui {
template<class EncodedColor>
[[nodiscard]] inline ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> input_field_clear_drawing(
        const ii::gui::WidgetPaintState& visual,EncodedColor description,EncodedColor background,
        double font_size,double line_height,std::string_view font_family,bool system_symbol){
    try{
        const auto width=float(visual.size.width),height=float(visual.size.height);
        if(!std::isfinite(width)||!std::isfinite(height)||width<0||height<0||width>8192||height>8192||
           !std::isfinite(font_size)||font_size<=0||!std::isfinite(line_height)||line_height<=0)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid input clear drawing geometry");
        auto drawing=std::make_shared<ii::gui::WidgetDrawing>();
        if(width>0&&height>0){
            const auto line=float(line_height);auto foreground=description;
            if(!system_symbol){
                const auto bubble=ii::gui::srgb_to_linear(description);
                drawing->panels.push_back({{0,0,width,height},bubble,bubble,(std::min)(width,height)/2});
                foreground=background;foreground.alpha=1;
            }
            ii::gui::TextRun run{{0,(height-line)/2,width,line},system_symbol?"\xF4\x80\x81\xA1":"×",
                ii::gui::srgb_to_linear(foreground),float(font_size),400,ii::gui::TextAlignment::center,
                system_symbol?"SF Pro":std::string(font_family)};
            run.line_height=line;run.clip=ii::gui::Rect{0,0,width,height};drawing->texts.push_back(std::move(run));
        }
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
    }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Input clear drawing failed",std::current_exception());}
}
} // namespace ii::gui
