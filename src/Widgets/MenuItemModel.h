#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace ii::gui {
struct MenuItemPalette final {
    WidgetColor text{},disabled_text{},detail{},selected{},hover{},pressed{},focus{};
    friend bool operator==(const MenuItemPalette&,const MenuItemPalette&)=default;
};
struct MenuItemConfiguration final {
    std::string label="Label",shortcut,key_placeholder,icon_name="procedure",accessibility_name;
    MenuItemState state=MenuItemState::normal;
    MenuDirection direction=MenuDirection::automatic;
    bool enabled=true,compact=false,key_visible=true,show_icon_slot=true,show_chevron=true,has_children=true,expanded=false;
    double width=0,height=0,icon_size=16,chevron_size=16,horizontal_padding=4,spacing=8;
    double font_size=13,line_height=13;
    unsigned font_weight=500;
    MenuItemPalette palette;
    friend bool operator==(const MenuItemConfiguration&,const MenuItemConfiguration&)=default;
};
struct MenuItemRect final {double x=0,y=0,width=0,height=0;};
struct MenuItemInteraction final {bool enabled=true,hovered=false,pressed=false,focused=false;};
struct MenuItemVisual final {WidgetColor background,text,detail,focus;bool enabled=true,focused=false;};
struct MenuItemGeometry final {MenuItemRect surface,icon,label,shortcut,chevron;double radius=0;};
struct DefaultMenuItemModelPolicy final {
    using Configuration=MenuItemConfiguration;using Interaction=MenuItemInteraction;using Visual=MenuItemVisual;
    using Geometry=MenuItemGeometry;using State=MenuItemState;using Direction=MenuDirection;using Color=WidgetColor;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept {
        return value.size()<=65536&&value.find('\0')==std::string_view::npos&&ii::core::valid_utf8(value);
    }
};
// The framework owns menu item state precedence, validation and geometry.
template<class Policy>
class BasicMenuItem final {
    using MenuItemConfiguration=typename Policy::Configuration;
    using MenuItemInteraction=typename Policy::Interaction;
    using MenuItemVisual=typename Policy::Visual;
    using MenuItemGeometry=typename Policy::Geometry;
    using MenuEntryState=typename Policy::State;
    using MenuDirection=typename Policy::Direction;
    using StyleColor=typename Policy::Color;
    MenuItemConfiguration value_;
public:
    [[nodiscard]] const MenuItemConfiguration& configuration()const noexcept{return value_;}
    [[nodiscard]] static bool validate(const MenuItemConfiguration& value)noexcept{
        const auto bound=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
        const auto color=[&](StyleColor c){return bound(c.red,0,1)&&bound(c.green,0,1)&&bound(c.blue,0,1)&&bound(c.alpha,0,1);};
        if(!Policy::valid_text(value.label)||!Policy::valid_text(value.shortcut)||!Policy::valid_text(value.key_placeholder)||
           !Policy::valid_text(value.icon_name)||value.icon_name.size()>256||!Policy::valid_text(value.accessibility_name)||
           !bound(value.width,0,8192)||!bound(value.height,0,8192)||!bound(value.icon_size,1,256)||!bound(value.chevron_size,1,256)||
           !bound(value.horizontal_padding,0,4096)||!bound(value.spacing,0,8192)||!bound(value.font_size,1,256)||!bound(value.line_height,0,8192)||
           (value.font_weight!=400&&value.font_weight!=500&&value.font_weight!=600&&value.font_weight!=700))return false;
        if(value.state!=MenuEntryState::normal&&value.state!=MenuEntryState::selected&&value.state!=MenuEntryState::inactive)return false;
        if(value.direction!=MenuDirection::automatic&&value.direction!=MenuDirection::right&&value.direction!=MenuDirection::left&&
           value.direction!=MenuDirection::up&&value.direction!=MenuDirection::down)return false;
        const auto& p=value.palette;
        return color(p.text)&&color(p.disabled_text)&&color(p.detail)&&color(p.selected)&&color(p.hover)&&color(p.pressed)&&color(p.focus);
    }
    bool set_configuration(MenuItemConfiguration value){if(!validate(value))return false;value_=std::move(value);return true;}
    [[nodiscard]] double width()const noexcept{return value_.width>0?value_.width:(value_.compact?141:161);}
    [[nodiscard]] double height()const noexcept{return value_.height>0?value_.height:(value_.compact?18:24);}
    [[nodiscard]] std::string_view shortcut()const noexcept{
        if(!value_.key_visible)return {};
        return value_.shortcut.empty()?std::string_view(value_.key_placeholder):std::string_view(value_.shortcut);
    }
    [[nodiscard]] MenuDirection direction()const noexcept{
        return value_.direction==MenuDirection::automatic?(value_.expanded?MenuDirection::down:MenuDirection::right):value_.direction;
    }
    [[nodiscard]] MenuItemVisual visual(MenuItemInteraction state)const noexcept{
        const auto& p=value_.palette;const bool enabled=value_.enabled&&state.enabled&&value_.state!=MenuEntryState::inactive;
        MenuItemVisual result{.background={0,0,0,0},.text=enabled?p.text:p.disabled_text,.detail=enabled?p.detail:p.disabled_text,
                              .focus=p.focus,.enabled=enabled,.focused=enabled&&state.focused};
        if(value_.state==MenuEntryState::selected)result.background=p.selected;
        else if(enabled&&state.pressed)result.background=p.pressed;
        else if(enabled&&state.hovered)result.background=p.hover;
        return result;
    }
    [[nodiscard]] std::optional<MenuItemGeometry> geometry(double width,double height,double label_width,double label_height,
                                                            double shortcut_width,double shortcut_height)const noexcept{
        const auto extent=[](double n){return std::isfinite(n)&&n>=0&&n<=32767;};
        if(!extent(width)||!extent(height)||!extent(label_width)||!extent(label_height)||!extent(shortcut_width)||!extent(shortcut_height))return std::nullopt;
        MenuItemGeometry result;result.surface={0,0,width,height};result.radius=value_.compact?0:(std::min)({4.,width/2,height/2});
        if(!width||!height)return result;
        const auto padding=(std::min)(value_.horizontal_padding,width/2);double left=padding,right=width-padding;
        if(value_.show_icon_slot){const auto size=(std::min)({value_.icon_size,height,right-left});result.icon={left,(height-size)/2,size,size};
            left+=size;if(left<right)left+=(std::min)(value_.spacing,right-left);}
        if(value_.show_chevron&&value_.has_children){const auto size=(std::min)({value_.chevron_size,height,(std::max)(0.,right-left)});right-=size;
            result.chevron={right,(height-size)/2,size,size};if(right>left)right-=(std::min)(value_.spacing,right-left);}
        if(!shortcut().empty()&&shortcut_width>0&&shortcut_height>0){const auto size=(std::min)(shortcut_width,(std::max)(0.,right-left));right-=size;
            const auto h=(std::min)(shortcut_height,height);result.shortcut={right,(height-h)/2,size,h};if(right>left)right-=(std::min)(value_.spacing,right-left);}
        if(!value_.label.empty()&&label_width>0&&label_height>0){const auto h=(std::min)(label_height,height);
            result.label={left,(height-h)/2,(std::min)(label_width,(std::max)(0.,right-left)),h};}
        return result;
    }
};
using MenuItemModel=BasicMenuItem<DefaultMenuItemModelPolicy>;
} // namespace ii::gui
