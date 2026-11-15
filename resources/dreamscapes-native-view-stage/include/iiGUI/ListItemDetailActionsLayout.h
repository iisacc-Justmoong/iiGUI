#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
namespace ii::gui {
template<class Policy>
class BasicListItemDetailActions final {
    using ListItemDetailActionsConfiguration=typename Policy::ListItemDetailActionsConfiguration;
    using ListItemDetailActionsGeometry=typename Policy::ListItemDetailActionsGeometry;
    using ListItemSize=typename Policy::ListItemSize;
    using ListItemRect=typename Policy::ListItemRect;
    [[nodiscard]] static bool bound(double value,double maximum=8192)noexcept{
        return std::isfinite(value)&&value>=0&&value<=maximum;
    }
    [[nodiscard]] static double header_height(const ListItemDetailActionsConfiguration& c)noexcept{
        return (std::max)({c.show_leading_icon?c.leading_height.value_or(c.icon_size):0.,
                         c.show_bookmark?c.icon_size:0.,c.show_description?39.:13.});
    }
    [[nodiscard]] static double footer_height(const ListItemDetailActionsConfiguration& c)noexcept{
        return std::ranges::any_of(c.show_actions,[](bool value){return value;})?22.:11.;
    }
public:
    [[nodiscard]] static double maximum_leading_height(const ListItemDetailActionsConfiguration& c)noexcept{
        const auto rest=2*c.vertical_padding+c.section_spacing+footer_height(c)+(c.show_metadata?11+c.section_spacing:0);
        return (std::max)(0.,8192.-rest);
    }
    [[nodiscard]] static bool validate(const ListItemDetailActionsConfiguration& c)noexcept{
        return bound(c.width)&&bound(c.height)&&bound(c.horizontal_padding)&&bound(c.vertical_padding)&&
            bound(c.content_spacing)&&bound(c.section_spacing)&&bound(c.icon_size,256)&&bound(c.action_width)&&c.action_width>0&&
            (!c.leading_width||bound(*c.leading_width))&&(!c.leading_height||bound(*c.leading_height));
    }
    [[nodiscard]] static std::optional<ListItemSize> intrinsic_size(const ListItemDetailActionsConfiguration& c)noexcept{
        if(!validate(c))return std::nullopt;
        const auto natural=2*c.vertical_padding+header_height(c)+c.section_spacing+footer_height(c)+
            (c.show_metadata?11+c.section_spacing:0);
        if(natural>8192)return std::nullopt;
        return ListItemSize{c.width>0?c.width:400,c.height>0?c.height:natural};
    }
    [[nodiscard]] static std::optional<ListItemDetailActionsGeometry> geometry(
            const ListItemDetailActionsConfiguration& c,double width,double height)noexcept{
        if(!intrinsic_size(c)||!bound(width)||!bound(height))return std::nullopt;
        const auto clip=[=](ListItemRect r){
            const auto x=std::clamp(r.x,0.,width),y=std::clamp(r.y,0.,height);
            return ListItemRect{x,y,(std::max)(0.,(std::min)(width,r.x+r.width)-x),(std::max)(0.,(std::min)(height,r.y+r.height)-y)};
        };
        ListItemDetailActionsGeometry g;
        const auto left=(std::min)(c.horizontal_padding,width/2),right=width-left;
        const auto inner=(std::max)(0.,right-left),header=header_height(c),top=c.vertical_padding;
        auto text_left=left,text_right=right;
        if(c.show_leading_icon){const auto w=(std::min)(c.leading_width.value_or(c.icon_size),inner),h=c.leading_height.value_or(c.icon_size);
            g.leading=clip({left,top+(header-h)/2,w,h});
            text_left=(std::min)(right,left+w+(w>0?(std::min)(c.content_spacing,(std::max)(0.,inner-w)):0.));}
        if(c.show_bookmark){const auto size=(std::min)(c.icon_size,(std::max)(0.,right-text_left));
            g.bookmark=clip({right-size,top+(header-size)/2,size,size});
            text_right=(std::max)(text_left,right-size-(std::min)(c.content_spacing,(std::max)(0.,right-size-text_left)));}
        const auto text_width=(std::max)(0.,text_right-text_left);
        g.label=clip({text_left,top,text_width,13});
        if(c.show_description)g.description=clip({text_left,top+17,text_width,22});
        auto y=top+header+c.section_spacing;
        if(c.show_metadata){
            const auto gap=(std::min)(c.content_spacing,inner/2),cell=(std::max)(0.,(inner-2*gap)/3);
            for(std::size_t i=0;i<3;++i)g.metadata[i]=clip({left+double(i)*(cell+gap),y,cell,11});
            y+=11+c.section_spacing;
        }
        const auto count=std::ranges::count(c.show_actions,true);
        const auto footer=footer_height(c);
        const auto desired=double(count)*c.action_width+double(count?count-1:0)*c.content_spacing;
        const auto action_width=(std::min)(inner,desired),status_gap=action_width>0?(std::min)(c.content_spacing,inner-action_width):0.;
        g.status=clip({left,y+(footer-11)/2,(std::max)(0.,inner-action_width-status_gap),11});
        if(action_width>0)g.actions=clip({right-action_width,y,action_width,footer});
        return g;
    }
};

} // namespace ii::gui
