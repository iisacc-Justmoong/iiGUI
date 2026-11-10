#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
namespace ii::gui {
template<class Policy>
class BasicListItemForm final {
    using ListItemFormConfiguration=typename Policy::ListItemFormConfiguration;
    using ListItemFormGeometry=typename Policy::ListItemFormGeometry;
    using ListItemSize=typename Policy::ListItemSize;
    using ListItemRect=typename Policy::ListItemRect;
    [[nodiscard]] static bool bound(double value,double maximum=8192)noexcept{
        return std::isfinite(value)&&value>=0&&value<=maximum;
    }
    [[nodiscard]] static double header_height(const ListItemFormConfiguration& c)noexcept{
        return (std::max)(c.show_description?28.:13.,c.show_selector?20.:0.);
    }
    [[nodiscard]] static double field_height(const ListItemFormConfiguration& c,bool shown)noexcept{
        return (std::max)(11.,shown?c.input_height:0.);
    }
    [[nodiscard]] static double footer_height(const ListItemFormConfiguration& c)noexcept{
        return c.show_toggle||c.show_primary_action||c.show_secondary_action?22.:11.;
    }
public:
    [[nodiscard]] static bool validate(const ListItemFormConfiguration& c)noexcept{
        return bound(c.width)&&bound(c.height)&&bound(c.horizontal_padding)&&bound(c.vertical_padding)&&
            bound(c.content_spacing)&&bound(c.section_spacing)&&bound(c.selector_width)&&bound(c.input_width)&&
            bound(c.input_height)&&bound(c.action_width)&&c.action_width>0;
    }
    [[nodiscard]] static std::optional<ListItemSize> intrinsic_size(const ListItemFormConfiguration& c)noexcept{
        if(!validate(c))return std::nullopt;
        const auto natural=2*c.vertical_padding+header_height(c)+field_height(c,c.show_input1)+field_height(c,c.show_input2)+
            footer_height(c)+3*c.section_spacing;
        if(natural>8192)return std::nullopt;
        return ListItemSize{c.width>0?c.width:400,c.height>0?c.height:natural};
    }
    [[nodiscard]] static std::optional<ListItemFormGeometry> geometry(
            const ListItemFormConfiguration& c,double width,double height)noexcept{
        if(!intrinsic_size(c)||!bound(width)||!bound(height))return std::nullopt;
        const auto clip=[=](ListItemRect r){
            const auto x=std::clamp(r.x,0.,width),y=std::clamp(r.y,0.,height);
            return ListItemRect{x,y,(std::max)(0.,(std::min)(width,r.x+r.width)-x),(std::max)(0.,(std::min)(height,r.y+r.height)-y)};
        };
        ListItemFormGeometry g;const auto left=(std::min)(c.horizontal_padding,width/2),right=width-left;
        const auto inner=(std::max)(0.,right-left),top=c.vertical_padding,header=header_height(c);
        const auto consume_right=[&](double& cursor,double available_left,double requested,double y,double h,ListItemRect& out){
            const auto w=(std::min)(requested,(std::max)(0.,cursor-available_left));out=clip({cursor-w,y,w,h});
            cursor-=w;if(w>0)cursor-=(std::min)(c.content_spacing,(std::max)(0.,cursor-available_left));
        };
        auto cursor=right;
        if(c.show_selector)consume_right(cursor,left,c.selector_width,top+(header-20)/2,20,g.selector);
        const auto text_y=top+(header-(c.show_description?28.:13.))/2,text_width=(std::max)(0.,cursor-left);
        g.label=clip({left,text_y,text_width,13});if(c.show_description)g.description=clip({left,text_y+17,text_width,11});
        auto y=top+header+c.section_spacing;
        const auto field=[&](bool shown,ListItemRect& label,ListItemRect& input){
            const auto h=field_height(c,shown);cursor=right;
            if(shown)consume_right(cursor,left,c.input_width,y+(h-c.input_height)/2,c.input_height,input);
            label=clip({left,y+(h-11)/2,(std::max)(0.,cursor-left),11});y+=h+c.section_spacing;
        };
        field(c.show_input1,g.field_label1,g.input1);field(c.show_input2,g.field_label2,g.input2);
        const auto footer=footer_height(c);auto status_left=left;cursor=right;
        if(c.show_toggle){const auto w=(std::min)(38.,inner);g.toggle=clip({left,y+(footer-22)/2,w,22});
            status_left=(std::min)(right,left+w+(std::min)(c.content_spacing,(std::max)(0.,inner-w)));}
        const auto count=unsigned(c.show_primary_action)+unsigned(c.show_secondary_action);
        const auto action_width=double(count)*c.action_width+double(count?count-1:0)*c.content_spacing;
        if(action_width>0)consume_right(cursor,status_left,action_width,y+(footer-22)/2,22,g.actions);
        g.status=clip({status_left,y+(footer-11)/2,(std::max)(0.,cursor-status_left),11});return g;
    }
};

} // namespace ii::gui
