#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/Text.h>
#include <algorithm>
#include <cmath>
#include <memory>

namespace ii::gui {
// Stateless ink construction shared by standalone mobile tabs and tab bars.
template<class Policy> struct BasicMobileTabPainter final {
    using Item=typename Policy::Item;
    using StyleColor=typename Policy::Color;
    using Tokens=typename Policy::Tokens;
    using Colors=typename Policy::Colors;
    [[nodiscard]] static Color linear(StyleColor c)noexcept{
        const auto convert=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
        return {float(convert(c.red)),float(convert(c.green)),float(convert(c.blue)),float(c.alpha)};
    }
    [[nodiscard]] static std::shared_ptr<const WidgetDrawing> item(
            const Item& item,bool selected,bool enabled,bool android,bool show_label,
            StyleColor focus,std::shared_ptr<const RasterImage> image,const WidgetPaintState& input){
        auto result=std::make_shared<WidgetDrawing>();
        const auto radius=(std::min)({android?Tokens::android_selected_radius:Tokens::ios_selected_radius,input.size.width/2,input.size.height/2});
        if(input.hovered||input.pressed){
            const auto surface=linear(Colors::panel_surface_input_hover);
            result->panels.push_back({{0,0,float(input.size.width),float(input.size.height)},surface,surface,float(radius)});
        }
        if(input.focused&&item.enabled&&enabled&&input.size.width>0&&input.size.height>0){
            const auto ink=linear(focus);
            result->panels.push_back({{0,0,float(input.size.width),float(input.size.height)},ink,ink,float(radius),
                float((std::min)({Tokens::border_width,input.size.width/2,input.size.height/2}))});
        }
        const auto icon=(std::min)({Tokens::icon_size,input.size.width,input.size.height});
        const bool label=show_label&&!item.text.empty();
        const auto top=label?(std::max)(0.,(input.size.height-icon-Tokens::label_line_height-Tokens::icon_label_gap)/2):(input.size.height-icon)/2;
        if(image&&icon>0)result->images.push_back({.bounds={float((input.size.width-icon)/2),float(top),float(icon),float(icon)},
            .image=std::move(image),.opacity=item.enabled&&enabled?1.f:float(Colors::text_disabled_color.alpha)});
        if(label){const auto ink=linear(!item.enabled||!enabled?Colors::text_disabled_color:
                                         selected?Colors::text_title_header_color:Colors::text_body_color);
            result->texts.push_back({.bounds={0,float(top+icon+Tokens::icon_label_gap),float(input.size.width),float(Tokens::label_line_height)},
                .text=item.text,.color=ink,.font_size=float(Tokens::label_size),.alignment=TextAlignment::center,
                .font_family=input.font_family,.line_height=float(Tokens::label_line_height),.elide_right=true});}
        return result;
    }
    [[nodiscard]] static std::shared_ptr<const WidgetDrawing> badge(const Item& item,const WidgetPaintState& input){
        auto result=std::make_shared<WidgetDrawing>();
        if(input.size.width<=0||input.size.height<=0)return result;
        const auto ink=linear(Colors::accent_red_default);
        result->panels.push_back({{0,0,float(input.size.width),float(input.size.height)},ink,ink,float(input.size.height/2)});
        if(!item.badge_dot&&!item.badge.empty())result->texts.push_back({.bounds={0,0,float(input.size.width),float(input.size.height)},
            .text=item.badge,.color=linear(Colors::text_title_header_color),.font_size=float(Tokens::label_size),.font_weight=600,
            .alignment=TextAlignment::center,.font_family=input.font_family,.line_height=float(Tokens::label_line_height),.elide_right=true});
        return result;
    }
    [[nodiscard]] static core::Result<std::shared_ptr<const WidgetDrawing>> standalone(
            const Item& value,bool selected,bool enabled,bool android,bool show_label,double selection_opacity,
            StyleColor focus,std::shared_ptr<const RasterImage> image,const WidgetPaintState& input){
        auto base=item(value,selected,enabled,android,show_label,focus,std::move(image),input);
        auto drawing=std::make_shared<WidgetDrawing>(*base);
        selection_opacity=std::clamp(selection_opacity,0.,1.);
        if(selection_opacity>0&&input.size.width>0&&input.size.height>0){
            auto ink=linear(Colors::panel_surface_input);ink.alpha*=float(selection_opacity);
            const auto radius=(std::min)({android?Tokens::android_selected_radius:Tokens::ios_selected_radius,input.size.width/2,input.size.height/2});
            drawing->panels.insert(drawing->panels.begin(),{{0,0,float(input.size.width),float(input.size.height)},ink,ink,float(radius)});
        }
        if(value.badge_dot||!value.badge.empty()){
            const auto height=value.badge_dot?Tokens::badge_dot:Tokens::badge_height;
            auto width=height;
            if(!value.badge_dot){
                auto measured=measure_text_layout({.text=value.badge,.font_size=float(Tokens::label_size),.font_weight=600,
                    .font_family=input.font_family,.line_height=float(Tokens::label_line_height),.elide_right=true});
                if(!measured)return std::unexpected(measured.error());
                width=(std::max)(height,measured->width+2*Tokens::badge_padding);
            }
            width=(std::min)(width,input.size.width);
            const auto icon=(std::min)({Tokens::icon_size,input.size.width,input.size.height});
            const bool label=show_label&&!value.text.empty();
            const auto top=label?(std::max)(0.,(input.size.height-icon-Tokens::label_line_height-Tokens::icon_label_gap)/2):(input.size.height-icon)/2;
            const auto x=std::clamp((input.size.width+icon)/2-Tokens::badge_offset,0.,(std::max)(0.,input.size.width-width));
            const auto y=(std::max)(0.,top-Tokens::badge_offset);
            WidgetPaintState badge_state=input;badge_state.size={width,(std::min)(height,input.size.height)};
            auto small=badge(value,badge_state);
            for(auto panel:small->panels){panel.bounds.x+=float(x);panel.bounds.y+=float(y);drawing->panels.push_back(std::move(panel));}
            for(auto text:small->texts){text.bounds.x+=float(x);text.bounds.y+=float(y);drawing->texts.push_back(std::move(text));}
        }
        return std::shared_ptr<const WidgetDrawing>(std::move(drawing));
    }
};
} // namespace ii::gui
