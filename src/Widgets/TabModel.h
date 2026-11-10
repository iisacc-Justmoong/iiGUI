#pragma once
#include <iiGUI/ButtonModel.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace ii::gui {
struct TabPalette final {WidgetColor selected{},text{},disabled_text{},badge{},surface_selected{},hover{},pressed{},accent{};
 friend bool operator==(const TabPalette&,const TabPalette&)=default;};
struct TabConfiguration final {std::string text,accessibility_name,icon_name,badge;bool enabled=true,selected=false;TabStyle style=TabStyle::underline;
 double height=32,minimum_width=80,icon_size=18,horizontal_padding=12,spacing=8,font_size=13,line_height=13;unsigned font_weight=500;TabPalette palette;
 friend bool operator==(const TabConfiguration&,const TabConfiguration&)=default;};
struct TabInteraction final {bool enabled=true,hovered=false,pressed=false,focused=false;};
struct TabVisual final {WidgetColor background,text,badge,accent;bool focused=false,underline=false;};
struct TabRect final {double x=0,y=0,width=0,height=0;};struct TabGeometry final {TabRect surface,icon,label,badge,underline;double radius=0;};
struct DefaultTabModelPolicy final {using Configuration=TabConfiguration;using Interaction=TabInteraction;using Visual=TabVisual;
 using Geometry=TabGeometry;using Style=TabStyle;using Color=WidgetColor;
 [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return DefaultButtonPolicy::valid_text(value);}};
// Design policy is independent of the host's input, shaping and rasterization.
template<class Policy>class BasicTab final {
    using TabConfiguration=typename Policy::Configuration;using TabInteraction=typename Policy::Interaction;
    using TabVisual=typename Policy::Visual;using TabGeometry=typename Policy::Geometry;
    using TabStyle=typename Policy::Style;using StyleColor=typename Policy::Color;
    TabConfiguration value_;
public:
    [[nodiscard]] const TabConfiguration& configuration()const noexcept{return value_;}
    [[nodiscard]] static bool validate(const TabConfiguration& value)noexcept{
        const auto bounded=[](double n,double lo,double hi){return std::isfinite(n)&&n>=lo&&n<=hi;};
        const auto color=[&](StyleColor c){return bounded(c.red,0,1)&&bounded(c.green,0,1)&&bounded(c.blue,0,1)&&bounded(c.alpha,0,1);};
        if(!Policy::valid_text(value.text)||!Policy::valid_text(value.accessibility_name)||
           !Policy::valid_text(value.icon_name)||value.icon_name.empty()||value.icon_name.size()>256||
           value.icon_name.find_first_not_of(" \t\r\n")==std::string::npos||!Policy::valid_text(value.badge)||
           !bounded(value.height,1,8192)||!bounded(value.minimum_width,1,8192)||!bounded(value.icon_size,1,256)||
           !bounded(value.horizontal_padding,0,4096)||!bounded(value.spacing,0,8192)||!bounded(value.font_size,1,256)||
           !bounded(value.line_height,0,8192)||(value.font_weight!=400&&value.font_weight!=500&&value.font_weight!=600&&value.font_weight!=700))return false;
        if(value.style!=TabStyle::underline&&value.style!=TabStyle::surface)return false;
        const auto& p=value.palette;
        return color(p.selected)&&color(p.text)&&color(p.disabled_text)&&color(p.badge)&&
               color(p.surface_selected)&&color(p.hover)&&color(p.pressed)&&color(p.accent);
    }
    bool set_configuration(TabConfiguration value){if(!validate(value))return false;value_=std::move(value);return true;}
    [[nodiscard]] TabVisual visual(TabInteraction state)const noexcept{
        const bool enabled=value_.enabled&&state.enabled;const auto& p=value_.palette;
        TabVisual result{.background={0,0,0,0},.text=enabled?(value_.selected?p.selected:p.text):p.disabled_text,
                         .badge=enabled?p.badge:p.disabled_text,.accent=p.accent,.focused=enabled&&state.focused,
                         .underline=enabled&&value_.selected&&value_.style==TabStyle::underline};
        if(enabled){
            if(state.pressed)result.background=p.pressed;
            else if(state.hovered)result.background=p.hover;
            else if(value_.selected&&value_.style==TabStyle::surface)result.background=p.surface_selected;
        }
        return result;
    }
    [[nodiscard]] std::optional<TabGeometry> geometry(double width,double height,double label_width,double label_height,
                                                       double badge_width,double badge_height)const noexcept{
        const auto extent=[](double n){return std::isfinite(n)&&n>=0&&n<=32767;};
        if(!extent(width)||!extent(height)||!extent(label_width)||!extent(label_height)||!extent(badge_width)||!extent(badge_height))return std::nullopt;
        TabGeometry result;result.surface={0,0,width,height};
        result.radius=value_.style==TabStyle::surface?(std::min)({4.,width/2,height/2}):0;
        if(width==0||height==0)return result;
        const auto padding=(std::min)(value_.horizontal_padding,width/2);
        auto available=(std::max)(0.,width-2*padding);
        const auto icon=(std::min)({value_.icon_size,available,height});available-=icon;
        const bool label=!value_.text.empty()&&label_width>0&&label_height>0;
        const bool badge=!value_.badge.empty()&&badge_width>0&&badge_height>0;
        const auto label_gap=label?(std::min)(value_.spacing,available):0.;available-=label_gap;
        const auto badge_gap=badge?(std::min)(value_.spacing,available):0.;available-=badge_gap;
        const auto fitted_badge=badge?(std::min)(badge_width,available):0.;available-=fitted_badge;
        const auto fitted_label=label?(std::min)(label_width,available):0.;
        const auto total=icon+label_gap+fitted_label+badge_gap+fitted_badge;
        auto x=(width-total)/2;
        result.icon={x,(height-icon)/2,icon,icon};x+=icon+label_gap;
        const auto lh=(std::min)(label_height,height);result.label={x,(height-lh)/2,fitted_label,lh};x+=fitted_label+badge_gap;
        const auto bh=(std::min)(badge_height,height);result.badge={x,(height-bh)/2,fitted_badge,bh};
        if(value_.selected&&value_.style==TabStyle::underline)result.underline={0,(std::max)(0.,height-2),width,(std::min)(2.,height)};
        return result;
    }
};

using TabModel=BasicTab<DefaultTabModelPolicy>;
struct TabBarConfiguration final {double spacing=4,minimum_tab_width=120;int current_index=0;bool auto_select=true;std::string accessibility_name;
 friend bool operator==(const TabBarConfiguration&,const TabBarConfiguration&)=default;};
enum class TabFocusMove {first,last,previous,next};enum class TabActivation {rejected,invoked,selection_changed};
struct DefaultTabBarModelPolicy final {using Configuration=TabBarConfiguration;using FocusMove=TabFocusMove;using Activation=TabActivation;
 [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return DefaultButtonPolicy::valid_text(value);}};
// Index ordering is explicit. Activation and focus navigation are independent;
// callers own callback dispatch and may replace the model after an activation.
template<class Policy>class BasicTabBar final {
    using TabBarConfiguration=typename Policy::Configuration;using TabFocusMove=typename Policy::FocusMove;
    using TabActivation=typename Policy::Activation;
    TabBarConfiguration value_;
    std::vector<bool> enabled_;
public:
    [[nodiscard]] const TabBarConfiguration& configuration()const noexcept{return value_;}
    [[nodiscard]] std::size_t count()const noexcept{return enabled_.size();}
    [[nodiscard]] bool enabled(std::size_t index)const noexcept{return index<enabled_.size()&&enabled_[index];}
    [[nodiscard]] static bool validate(const TabBarConfiguration& value)noexcept{
        return std::isfinite(value.spacing)&&value.spacing>=0&&value.spacing<=32767&&
               std::isfinite(value.minimum_tab_width)&&value.minimum_tab_width>=1&&value.minimum_tab_width<=8192&&
               value.current_index>=-1&&value.current_index<4096&&Policy::valid_text(value.accessibility_name);
    }
    bool set_configuration(TabBarConfiguration value){if(!validate(value))return false;value_=value;return true;}
    bool set_items(std::vector<bool> enabled){if(enabled.size()>4096)return false;enabled_=std::move(enabled);return true;}
    [[nodiscard]] TabActivation activate(std::size_t index)noexcept{
        if(index>=enabled_.size()||!enabled_[index])return TabActivation::rejected;
        if(!value_.auto_select||value_.current_index==int(index))return TabActivation::invoked;
        value_.current_index=int(index);return TabActivation::selection_changed;
    }
    [[nodiscard]] std::optional<std::size_t> focus_target(std::optional<std::size_t> current,TabFocusMove move)const noexcept{
        if(enabled_.empty())return std::nullopt;
        const auto count=enabled_.size();
        std::size_t index=0;
        if(move==TabFocusMove::last)index=count-1;
        else if(move==TabFocusMove::previous)index=current&&*current<count?(*current+count-1)%count:count-1;
        else if(move==TabFocusMove::next)index=current&&*current<count?(*current+1)%count:0;
        const bool backwards=move==TabFocusMove::previous||move==TabFocusMove::last;
        for(std::size_t i=0;i<count;++i){if(enabled_[index])return index;index=backwards?(index+count-1)%count:(index+1)%count;}
        return std::nullopt;
    }
    [[nodiscard]] std::optional<double> equal_tab_width(double available_width)const noexcept{
        if(!std::isfinite(available_width)||available_width<0||available_width>32767)return std::nullopt;
        if(enabled_.empty())return 0.;
        const auto gaps=double(enabled_.size()-1)*value_.spacing;
        return (std::max)(value_.minimum_tab_width,(std::max)(0.,available_width-gaps)/double(enabled_.size()));
    }
};
using TabBarModel=BasicTabBar<DefaultTabBarModelPolicy>;
} // namespace ii::gui
