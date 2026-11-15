#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
namespace ii::gui {
template<class Policy>
class BasicListItemDetailControls final {
    using ListItemDetailControlsConfiguration=typename Policy::ListItemDetailControlsConfiguration;
    using ListItemDetailControlsGeometry=typename Policy::ListItemDetailControlsGeometry;
    using ListItemSize=typename Policy::ListItemSize;
    using ListItemRect=typename Policy::ListItemRect;
    using ListItemDetailControlVariant=typename Policy::ListItemDetailControlVariant;
    [[nodiscard]] static bool bound(double value,double maximum=8192)noexcept{
        return std::isfinite(value)&&value>=0&&value<=maximum;
    }
    [[nodiscard]] static bool settings(const ListItemDetailControlsConfiguration& c)noexcept{
        return c.variant==ListItemDetailControlVariant::settings;
    }
    [[nodiscard]] static double header_height(const ListItemDetailControlsConfiguration& c)noexcept{
        auto h=c.show_description?28.:13.;
        if(settings(c)&&c.show_leading_icon)h=(std::max)(h,c.leading_height.value_or(c.icon_size));
        if(settings(c)&&c.show_toggle)h=(std::max)(h,22.);
        if(!settings(c)&&c.show_selector)h=(std::max)(h,20.);
        return h;
    }
    [[nodiscard]] static double first_height(const ListItemDetailControlsConfiguration& c)noexcept{
        return settings(c)?(c.show_selector?20.:11.):(c.show_unit_selector?20.:18.);
    }
    [[nodiscard]] static double scope_height(const ListItemDetailControlsConfiguration& c)noexcept{
        return c.show_segments?(std::max)(11.,c.segment_height):11.;
    }
    [[nodiscard]] static double footer_height(const ListItemDetailControlsConfiguration& c)noexcept{
        return c.show_primary_action||(settings(c)&&c.show_secondary_action)||(!settings(c)&&c.show_toggle)?22.:11.;
    }
public:
    [[nodiscard]] static double maximum_leading_height(const ListItemDetailControlsConfiguration& c)noexcept{
        const auto rest=2*c.vertical_padding+first_height(c)+footer_height(c)+2*c.section_spacing+
            (settings(c)?scope_height(c)+c.section_spacing:0);
        return (std::max)(0.,8192.-rest);
    }
    [[nodiscard]] static bool validate(const ListItemDetailControlsConfiguration& c)noexcept{
        return (c.variant==ListItemDetailControlVariant::quantity||c.variant==ListItemDetailControlVariant::settings)&&
            bound(c.width)&&bound(c.height)&&bound(c.horizontal_padding)&&bound(c.vertical_padding)&&
            bound(c.content_spacing)&&bound(c.section_spacing)&&bound(c.icon_size,256)&&bound(c.selector_width)&&
            bound(c.unit_selector_width)&&bound(c.quantity_width)&&bound(c.quantity_spacing)&&bound(c.action_width)&&
            c.action_width>0&&bound(c.segment_width)&&bound(c.segment_height)&&
            (!c.leading_width||bound(*c.leading_width))&&(!c.leading_height||bound(*c.leading_height));
    }
    [[nodiscard]] static std::optional<ListItemSize> intrinsic_size(const ListItemDetailControlsConfiguration& c)noexcept{
        if(!validate(c))return std::nullopt;
        const auto natural=2*c.vertical_padding+header_height(c)+first_height(c)+footer_height(c)+2*c.section_spacing+
            (settings(c)?scope_height(c)+c.section_spacing:0);
        if(natural>8192)return std::nullopt;
        return ListItemSize{c.width>0?c.width:400,c.height>0?c.height:natural};
    }
    [[nodiscard]] static std::optional<ListItemDetailControlsGeometry> geometry(
            const ListItemDetailControlsConfiguration& c,double width,double height)noexcept{
        if(!intrinsic_size(c)||!bound(width)||!bound(height))return std::nullopt;
        const auto clip=[=](ListItemRect r){
            const auto x=std::clamp(r.x,0.,width),y=std::clamp(r.y,0.,height);
            return ListItemRect{x,y,(std::max)(0.,(std::min)(width,r.x+r.width)-x),(std::max)(0.,(std::min)(height,r.y+r.height)-y)};
        };
        ListItemDetailControlsGeometry g;const auto left=(std::min)(c.horizontal_padding,width/2),right=width-left;
        const auto inner=(std::max)(0.,right-left),top=c.vertical_padding,header=header_height(c);
        auto header_left=left,header_right=right;
        if(settings(c)&&c.show_leading_icon){const auto w=(std::min)(c.leading_width.value_or(c.icon_size),inner),h=c.leading_height.value_or(c.icon_size);
            g.leading=clip({left,top+(header-h)/2,w,h});
            header_left=(std::min)(right,left+w+(w>0?(std::min)(c.content_spacing,(std::max)(0.,inner-w)):0.));}
        const auto consume_right=[&](double& cursor,double available_left,double requested,double y,double h,ListItemRect& out){
            const auto w=(std::min)(requested,(std::max)(0.,cursor-available_left));out=clip({cursor-w,y,w,h});
            cursor-=w;if(w>0)cursor-=(std::min)(c.content_spacing,(std::max)(0.,cursor-available_left));
        };
        if(settings(c)&&c.show_toggle)consume_right(header_right,header_left,38,top+(header-22)/2,22,g.toggle);
        if(!settings(c)&&c.show_selector)consume_right(header_right,header_left,c.selector_width,top+(header-20)/2,20,g.header_selector);
        const auto text_width=(std::max)(0.,header_right-header_left);
        g.label=clip({header_left,top,text_width,13});if(c.show_description)g.description=clip({header_left,top+17,text_width,11});
        auto y=top+header+c.section_spacing;const auto first=first_height(c);auto cursor=right;
        if(settings(c)){
            if(c.show_selector)consume_right(cursor,left,c.selector_width,y+(first-20)/2,20,g.selector);
            g.mode_caption=clip({left,y+(first-11)/2,(std::max)(0.,cursor-left),11});
            y+=first+c.section_spacing;const auto scope=scope_height(c);cursor=right;
            if(c.show_segments)consume_right(cursor,left,c.segment_width,y+(scope-c.segment_height)/2,c.segment_height,g.segments);
            g.scope_caption=clip({left,y+(scope-11)/2,(std::max)(0.,cursor-left),11});y+=scope+c.section_spacing;
        }else{
            if(c.show_unit_selector)consume_right(cursor,left,c.unit_selector_width,y+(first-20)/2,20,g.unit_selector);
            if(c.show_stepper){const auto size=(std::min)(18.,(std::max)(0.,cursor-left));
                g.quantity_stepper=clip({cursor-size,y+(first-18)/2,size,18});cursor-=size;
                if(size>0)cursor-=(std::min)(c.quantity_spacing,(std::max)(0.,cursor-left));}
            const auto qwidth=(std::min)(c.quantity_width,(std::max)(0.,cursor-left));
            g.quantity_label=clip({cursor-qwidth,y+(first-16)/2,qwidth,16});cursor-=qwidth;
            if(qwidth>0)cursor-=(std::min)(c.content_spacing,(std::max)(0.,cursor-left));
            g.quantity_caption=clip({left,y+(first-11)/2,(std::max)(0.,cursor-left),11});y+=first+c.section_spacing;
        }
        const auto footer=footer_height(c);auto status_left=left;cursor=right;
        if(!settings(c)&&c.show_toggle){const auto w=(std::min)(38.,inner);g.toggle=clip({left,y+(footer-22)/2,w,22});
            status_left=(std::min)(right,left+w+(std::min)(c.content_spacing,(std::max)(0.,inner-w)));}
        const auto count=unsigned(c.show_primary_action)+unsigned(settings(c)&&c.show_secondary_action);
        const auto action_width=(std::min)((std::max)(0.,cursor-status_left),double(count)*c.action_width+double(count?count-1:0)*c.content_spacing);
        if(action_width>0)consume_right(cursor,status_left,action_width,y+(footer-22)/2,22,g.actions);
        g.status=clip({status_left,y+(footer-11)/2,(std::max)(0.,cursor-status_left),11});return g;
    }
};

} // namespace ii::gui
