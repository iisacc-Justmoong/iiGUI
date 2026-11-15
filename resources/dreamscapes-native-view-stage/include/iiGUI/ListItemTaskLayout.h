#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
namespace ii::gui {
template<class Policy>
class BasicListItemTask final {
    using ListItemTaskConfiguration=typename Policy::ListItemTaskConfiguration;
    using ListItemTaskGeometry=typename Policy::ListItemTaskGeometry;
    using ListItemSize=typename Policy::ListItemSize;
    using ListItemRect=typename Policy::ListItemRect;
    [[nodiscard]] static bool bound(double value,double maximum=8192)noexcept{
        return std::isfinite(value)&&value>=0&&value<=maximum;
    }
    [[nodiscard]] static double header_height(const ListItemTaskConfiguration& c)noexcept{
        return (std::max)({13.,c.show_selection?c.selection_size:0.,c.show_selector?20.:0.});
    }
    [[nodiscard]] static double footer_height(const ListItemTaskConfiguration& c)noexcept{
        return (std::max)({16.,c.stepper_size,c.show_primary_action||c.show_secondary_action?22.:0.});
    }
public:
    [[nodiscard]] static bool validate(const ListItemTaskConfiguration& c)noexcept{
        return bound(c.width)&&bound(c.height)&&bound(c.horizontal_padding)&&bound(c.vertical_padding)&&
            bound(c.content_spacing)&&bound(c.section_spacing)&&bound(c.selection_size,256)&&bound(c.selector_width)&&
            bound(c.quantity_width)&&bound(c.quantity_spacing)&&bound(c.stepper_size,256)&&bound(c.action_width)&&c.action_width>0;
    }
    [[nodiscard]] static std::optional<ListItemSize> intrinsic_size(const ListItemTaskConfiguration& c)noexcept{
        if(!validate(c))return std::nullopt;
        const auto natural=2*c.vertical_padding+header_height(c)+c.section_spacing+footer_height(c)+
            (c.show_description?22+c.section_spacing:0)+(c.show_metadata?11+c.section_spacing:0);
        if(natural>8192)return std::nullopt;
        return ListItemSize{c.width>0?c.width:400,c.height>0?c.height:natural};
    }
    [[nodiscard]] static std::optional<ListItemTaskGeometry> geometry(
            const ListItemTaskConfiguration& c,double width,double height)noexcept{
        if(!intrinsic_size(c)||!bound(width)||!bound(height))return std::nullopt;
        const auto clip=[=](ListItemRect r){
            const auto x=std::clamp(r.x,0.,width),y=std::clamp(r.y,0.,height);
            return ListItemRect{x,y,(std::max)(0.,(std::min)(width,r.x+r.width)-x),(std::max)(0.,(std::min)(height,r.y+r.height)-y)};
        };
        ListItemTaskGeometry g;const auto left=(std::min)(c.horizontal_padding,width/2),right=width-left;
        const auto inner=(std::max)(0.,right-left),top=c.vertical_padding,header=header_height(c);
        auto header_left=left,header_right=right;
        if(c.show_selection){const auto size=(std::min)(c.selection_size,inner);
            g.selection=clip({left,top+(header-size)/2,size,size});
            header_left=(std::min)(right,left+size+(std::min)(c.content_spacing,(std::max)(0.,inner-size)));}
        const auto consume_right=[&](double& cursor,double available_left,double requested,double y,double h,ListItemRect& out,double gap){
            const auto w=(std::min)(requested,(std::max)(0.,cursor-available_left));out=clip({cursor-w,y,w,h});
            cursor-=w;if(w>0)cursor-=(std::min)(gap,(std::max)(0.,cursor-available_left));
        };
        if(c.show_selector)consume_right(header_right,header_left,c.selector_width,top+(header-20)/2,20,g.selector,c.content_spacing);
        g.label=clip({header_left,top+(header-13)/2,(std::max)(0.,header_right-header_left),13});
        auto y=top+header+c.section_spacing;
        if(c.show_description){g.description=clip({left,y,inner,22});y+=22+c.section_spacing;}
        if(c.show_metadata){const auto gap=(std::min)(c.content_spacing,inner/2),cell=(std::max)(0.,(inner-2*gap)/3);
            for(std::size_t i=0;i<3;++i)g.metadata[i]=clip({left+double(i)*(cell+gap),y,cell,11});y+=11+c.section_spacing;}
        const auto footer=footer_height(c);auto cursor=right;
        const auto count=unsigned(c.show_primary_action)+unsigned(c.show_secondary_action);
        const auto action_width=double(count)*c.action_width+double(count?count-1:0)*c.content_spacing;
        if(action_width>0)consume_right(cursor,left,action_width,y+(footer-22)/2,22,g.actions,c.content_spacing);
        if(c.show_stepper)consume_right(cursor,left,c.stepper_size,y+(footer-c.stepper_size)/2,c.stepper_size,g.quantity_stepper,c.quantity_spacing);
        consume_right(cursor,left,c.quantity_width,y+(footer-16)/2,16,g.quantity_label,c.content_spacing);
        g.status=clip({left,y+(footer-11)/2,(std::max)(0.,cursor-left),11});return g;
    }
};

} // namespace ii::gui
