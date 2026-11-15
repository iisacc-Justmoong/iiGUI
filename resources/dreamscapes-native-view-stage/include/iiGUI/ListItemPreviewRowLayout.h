#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
namespace ii::gui {
template<class Policy>
class BasicListItemPreviewRow final {
    using ListItemPreviewRowConfiguration=typename Policy::ListItemPreviewRowConfiguration;
    using ListItemPreviewRowGeometry=typename Policy::ListItemPreviewRowGeometry;
    using ListItemSize=typename Policy::ListItemSize;
    using ListItemRect=typename Policy::ListItemRect;
    using ListItemPreviewVariant=typename Policy::ListItemPreviewVariant;
    [[nodiscard]] static bool bound(double value,double maximum=8192)noexcept{
        return std::isfinite(value)&&value>=0&&value<=maximum;
    }
    [[nodiscard]] static bool media(const ListItemPreviewRowConfiguration& c)noexcept{return c.variant==ListItemPreviewVariant::media;}
    [[nodiscard]] static double content_height(const ListItemPreviewRowConfiguration& c)noexcept{
        return c.show_description?(media(c)?28.:39.):13.;
    }
    [[nodiscard]] static double header_height(const ListItemPreviewRowConfiguration& c)noexcept{
        return (std::max)({content_height(c),c.show_preview?c.preview_height.value_or(c.preview_size):0.,media(c)?26.:(c.show_more?22.:0.)});
    }
    [[nodiscard]] static double footer_height(const ListItemPreviewRowConfiguration& c)noexcept{
        return (std::max)({11.,media(c)&&c.show_segments?c.segment_height:0.,c.show_primary_action||c.show_secondary_action?22.:0.});
    }
public:
    [[nodiscard]] static double maximum_preview_height(const ListItemPreviewRowConfiguration& c)noexcept{
        const auto rest=2*c.vertical_padding+c.section_spacing+footer_height(c)+(!media(c)&&c.show_metadata?11+c.section_spacing:0);
        return (std::max)(0.,8192.-rest);
    }
    [[nodiscard]] static bool validate(const ListItemPreviewRowConfiguration& c)noexcept{
        return (c.variant==ListItemPreviewVariant::resource||c.variant==ListItemPreviewVariant::media)&&
            bound(c.width)&&bound(c.height)&&bound(c.horizontal_padding)&&bound(c.vertical_padding)&&bound(c.content_spacing)&&
            bound(c.section_spacing)&&bound(c.preview_size,8192)&&bound(c.action_width)&&c.action_width>0&&
            bound(c.segment_width)&&bound(c.segment_height)&&(!c.preview_width||bound(*c.preview_width))&&
            (!c.preview_height||bound(*c.preview_height));
    }
    [[nodiscard]] static std::optional<ListItemSize> intrinsic_size(const ListItemPreviewRowConfiguration& c)noexcept{
        if(!validate(c))return std::nullopt;
        const auto natural=2*c.vertical_padding+header_height(c)+c.section_spacing+footer_height(c)+
            (!media(c)&&c.show_metadata?11+c.section_spacing:0);
        if(natural>8192)return std::nullopt;
        return ListItemSize{c.width>0?c.width:400,c.height>0?c.height:natural};
    }
    [[nodiscard]] static std::optional<ListItemPreviewRowGeometry> geometry(
            const ListItemPreviewRowConfiguration& c,double width,double height)noexcept{
        if(!intrinsic_size(c)||!bound(width)||!bound(height))return std::nullopt;
        const auto clip=[=](ListItemRect r){
            const auto x=std::clamp(r.x,0.,width),y=std::clamp(r.y,0.,height);
            return ListItemRect{x,y,(std::max)(0.,(std::min)(width,r.x+r.width)-x),(std::max)(0.,(std::min)(height,r.y+r.height)-y)};
        };
        ListItemPreviewRowGeometry g;const auto left=(std::min)(c.horizontal_padding,width/2),right=width-left;
        const auto inner=(std::max)(0.,right-left),top=c.vertical_padding,header=header_height(c);
        auto text_left=left,text_right=right;
        if(c.show_preview){const auto w=(std::min)(c.preview_width.value_or(c.preview_size),inner),h=c.preview_height.value_or(c.preview_size);
            g.preview=clip({left,top+(header-h)/2,w,h});
            text_left=(std::min)(right,left+w+(w>0?(std::min)(c.content_spacing,(std::max)(0.,inner-w)):0.));}
        const auto consume_right=[&](double& cursor,double available_left,double requested,double y,double h,ListItemRect& out){
            const auto w=(std::min)(requested,(std::max)(0.,cursor-available_left));out=clip({cursor-w,y,w,h});
            cursor-=w;if(w>0)cursor-=(std::min)(c.content_spacing,(std::max)(0.,cursor-available_left));
        };
        if(media(c)){
            ListItemRect info;consume_right(text_right,text_left,c.action_width,top+(header-26)/2,26,info);
            g.duration=clip({info.x,info.y,info.width,(std::min)(11.,info.height)});
            g.media_detail=clip({info.x,info.y+15,info.width,(std::max)(0.,(std::min)(11.,info.height-15))});
        }else if(c.show_more)consume_right(text_right,text_left,c.action_width,top+(header-22)/2,22,g.more);
        const auto text_y=top+(header-content_height(c))/2,text_width=(std::max)(0.,text_right-text_left);
        g.label=clip({text_left,text_y,text_width,13});if(c.show_description)
            g.description=clip({text_left,text_y+17,text_width,media(c)?11.:22.});
        auto y=top+header+c.section_spacing;
        if(!media(c)&&c.show_metadata){const auto gap=(std::min)(c.content_spacing,inner/2),cell=(std::max)(0.,(inner-2*gap)/3);
            for(std::size_t i=0;i<3;++i)g.metadata[i]=clip({left+double(i)*(cell+gap),y,cell,11});y+=11+c.section_spacing;}
        const auto footer=footer_height(c);auto cursor=right,status_left=left;
        if(media(c)&&c.show_segments){const auto w=(std::min)(c.segment_width,inner);g.segments=clip({left,y+(footer-c.segment_height)/2,w,c.segment_height});
            status_left=(std::min)(right,left+w+(std::min)(c.content_spacing,(std::max)(0.,inner-w)));}
        const auto count=unsigned(c.show_primary_action)+unsigned(c.show_secondary_action);
        const auto action_width=double(count)*c.action_width+double(count?count-1:0)*c.content_spacing;
        if(action_width>0)consume_right(cursor,status_left,action_width,y+(footer-22)/2,22,g.actions);
        g.status=clip({status_left,y+(footer-11)/2,(std::max)(0.,cursor-status_left),11});return g;
    }
};

} // namespace ii::gui
