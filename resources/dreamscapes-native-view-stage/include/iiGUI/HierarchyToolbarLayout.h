#pragma once
#include <iiGUI/ToolbarButtonModel.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace ii::gui {
struct HierarchyToolbarLayoutItem final {
    std::string icon_name, text;
    bool visible=true;
};
struct HierarchyToolbarLayoutSnapshot final { std::vector<HierarchyToolbarLayoutItem> items; };
struct HierarchyToolbarLayoutAppearance final {
    double minimum_width=200,width=0,height=0,horizontal_padding=8,vertical_padding=2;
    double spacing=0,slot_size=22,icon_size=18,corner_radius=8,font_size=12,background_opacity=0;
    double slot_padding=4,icon_text_spacing=4;
    bool enabled=true,distribute_spacing=false;
    WidgetColor background{};
    ToolbarButtonPalette palette;
};
struct HierarchyToolbarLayoutRect final { double x=0,y=0,width=0,height=0; };
struct HierarchyToolbarLayoutGeometry final {
    double width=0,height=0;
    std::vector<HierarchyToolbarLayoutRect> slots;
};
struct DefaultHierarchyToolbarLayoutPolicy final {
    using Appearance=HierarchyToolbarLayoutAppearance;using Snapshot=HierarchyToolbarLayoutSnapshot;
    using Geometry=HierarchyToolbarLayoutGeometry;using Color=WidgetColor;
    using ButtonConfiguration=ToolbarButtonConfiguration;using ButtonModel=ToolbarButtonModel;
    [[nodiscard]] static double slot_padding(const Appearance& value)noexcept{return value.slot_padding;}
    [[nodiscard]] static double icon_text_spacing(const Appearance& value)noexcept{return value.icon_text_spacing;}
};
// Policies provide application-owned data and metrics. The framework computes
// validation and geometry without observing or polling the item source.
template<class Policy>
class BasicHierarchyToolbarLayout final {
    using Appearance=typename Policy::Appearance;using Snapshot=typename Policy::Snapshot;
    using Geometry=typename Policy::Geometry;using Color=typename Policy::Color;
    using ButtonConfiguration=typename Policy::ButtonConfiguration;using ButtonModel=typename Policy::ButtonModel;
public:
    [[nodiscard]] static bool validate(const Appearance& value)noexcept {
        const auto bounded=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
        const auto color=[&](Color c){return bounded(c.red,0,1)&&bounded(c.green,0,1)&&bounded(c.blue,0,1)&&bounded(c.alpha,0,1);};
        ButtonConfiguration button;button.palette=value.palette;button.height=value.slot_size;
        button.icon_size=value.icon_size;button.font_size=value.font_size;button.corner_radius=value.corner_radius;
        return ButtonModel::validate(button)&&color(value.background)&&bounded(value.minimum_width,1,8192)&&
            bounded(value.width,0,8192)&&bounded(value.height,0,8192)&&bounded(value.horizontal_padding,0,8192)&&
            bounded(value.vertical_padding,0,8192)&&bounded(value.spacing,0,8192)&&bounded(value.background_opacity,0,1)&&
            bounded(Policy::slot_padding(value),0,8192)&&bounded(Policy::icon_text_spacing(value),0,8192)&&
            2*value.vertical_padding+value.slot_size<=8192;
    }
    [[nodiscard]] static std::optional<Geometry> geometry(const Snapshot& snapshot,const Appearance& value,
            const std::vector<double>& text_widths){
        if(!validate(value)||snapshot.items.size()>256||text_widths.size()!=snapshot.items.size())return std::nullopt;
        std::vector<double> widths(snapshot.items.size());double total=0;std::size_t visible=0;
        for(std::size_t i=0;i<snapshot.items.size();++i){
            if(!std::isfinite(text_widths[i])||text_widths[i]<0||text_widths[i]>8192)return std::nullopt;
            const auto& item=snapshot.items[i];if(!item.visible)continue;++visible;
            const auto icon=item.icon_name.empty()?0.:value.icon_size;
            const auto text=item.text.empty()?0.:text_widths[i];
            widths[i]=(std::max)(value.slot_size,Policy::slot_padding(value)+icon+text+
                (icon>0&&text>0?Policy::icon_text_spacing(value):0.));total+=widths[i];
        }
        const auto natural=2*value.horizontal_padding+total+(visible>1?(visible-1)*value.spacing:0.);
        if(natural>8192)return std::nullopt;
        Geometry result;result.width=value.width?value.width:(std::max)(value.minimum_width,natural);
        result.height=value.height?value.height:value.slot_size+2*value.vertical_padding;result.slots.resize(snapshot.items.size());
        const auto inset=(std::min)(value.horizontal_padding,result.width/2),available=(std::max)(0.,result.width-2*inset);
        auto gap=visible>1?(std::min)(value.spacing,available/double(visible-1)):0.;
        if(value.distribute_spacing&&visible>1&&available>=total+gap*(visible-1))gap=(available-total)/(visible-1);
        const auto remaining=(std::max)(0.,available-gap*(visible?visible-1:0));
        const auto scale=total?(std::min)(1.,remaining/total):0.;
        const auto top=(std::min)(value.vertical_padding,result.height/2);
        const auto height=(std::min)(value.slot_size,(std::max)(0.,result.height-2*top));
        double x=inset;bool first=true;
        for(std::size_t i=0;i<snapshot.items.size();++i)if(snapshot.items[i].visible){
            if(!first)x+=gap;first=false;result.slots[i]={x,(result.height-height)/2,widths[i]*scale,height};x+=widths[i]*scale;
        }
        return result;
    }
};
using HierarchyToolbarLayout=BasicHierarchyToolbarLayout<DefaultHierarchyToolbarLayoutPolicy>;
} // namespace ii::gui
