#pragma once
#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>
namespace ii::gui {
// Geometry is independent of a widget tree; all design dimensions are supplied by the application.
template<class Policy>struct BasicColorPickerLayout final {
 using Configuration=typename Policy::Configuration;using Layout=typename Policy::Layout;using Rect=typename Policy::Rect;
 using Mode=typename Policy::Mode;using Channel=typename Policy::Channel;using Kind=typename Policy::Kind;
 [[nodiscard]] static std::optional<Layout> compute(const Configuration& input){
  if(!Policy::valid(input))return std::nullopt;
  const auto m=Policy::metrics();Layout result;result.width=input.width;
  const auto x=input.padding,inner=input.width-input.padding*2;double y=input.padding;bool first=true;
  const auto section=[&](double height){if(!first)y+=m.section_gap;first=false;Rect bounds{x,y,inner,height};y+=height;return bounds;};
  std::vector<Channel> before;
  switch(input.mode){
   case Mode::wheel:before={Channel::hue,Channel::saturation,Channel::brightness};break;
   case Mode::grayscale:before={Channel::gray};break;
   case Mode::rgb:before={Channel::red,Channel::green,Channel::blue};break;
   case Mode::cmyk:before={Channel::cyan,Channel::magenta,Channel::yellow,Channel::black};break;
   case Mode::hue_saturation:case Mode::saturation_brightness:break;
  }
  if(!before.empty()){
   const auto group=section(before.size()*m.channel_height+(before.size()-1)*m.channel_gap);
   for(std::size_t i=0;i<before.size();++i)result.channels.push_back({before[i],{x,group.y+i*(m.channel_height+m.channel_gap),inner,m.channel_height}});
  }
  const bool plane=input.mode==Mode::hue_saturation||input.mode==Mode::saturation_brightness;
  const auto domain=section(plane?inner-m.plane_reserve:input.mode==Mode::wheel?(std::min)(inner,m.wheel_max):m.spectrum_height);
  result.surface=domain;
  if(plane){
   result.surface.width=inner-m.plane_reserve;result.rail={x+inner-m.rail_width,domain.y,m.rail_width,domain.height};
   if(input.mode==Mode::hue_saturation){result.surface_kind=Kind::hue_saturation;result.rail_kind=Kind::brightness_rail;}
   else{result.surface_kind=Kind::saturation_brightness;result.rail_kind=Kind::hue_rail;}
   const auto row=section(m.channel_height);result.channels.push_back({input.mode==Mode::hue_saturation?Channel::brightness:Channel::hue,row});
  }else if(input.mode!=Mode::wheel)result.surface_kind=Kind::spectrum;
  result.divider=section(m.divider_height);const auto values=section(m.values_height);
  result.comparison={x,values.y,m.comparison_width,m.values_height};
  result.hex_label={x+m.label_leading,values.y,m.hex_label_width,m.values_height};
  result.hex_field={x+m.hex_field_leading,values.y+m.field_top,(std::max)(m.hex_field_min,inner-m.hex_field_reserve),m.channel_height};
  result.alpha_label={x+inner-m.alpha_label_reserve,values.y,m.alpha_label_width,m.values_height};
  result.alpha_field={x+inner-m.alpha_field_reserve,values.y+m.field_top,m.alpha_field_width,m.channel_height};
  result.alpha_suffix={x+inner-m.suffix_reserve,values.y,m.suffix_width,m.values_height};
  if(input.show_recent_colors){const auto recent=section(m.recent_height);result.recent_label={x,recent.y,m.recent_label_width,m.recent_height};
   const auto count=input.recent_colors.size();if(count){const auto width=(std::max)(1.,(inner-m.recent_leading-(count-1)*m.recent_gap)/count);
    for(std::size_t i=0;i<count;++i)result.recent.push_back({x+m.recent_leading+i*(width+m.recent_gap),recent.y,width,m.recent_height});}}
  if(input.show_actions){const auto actions=section(m.action_height);result.eyedropper={x,actions.y,m.action_icon_width,m.action_height};
   result.apply={x+inner-m.action_button_width,actions.y,m.action_button_width,m.action_height};
   result.cancel={result.apply.x-m.action_button_width-m.action_button_gap,actions.y,m.action_button_width,m.action_height};
   result.color_space={x+m.color_space_leading,actions.y,(std::max)(0.,result.cancel.x-x-m.color_space_reserve),m.action_height};}
  result.height=y+input.padding;return result;
 }
};
} // namespace ii::gui
