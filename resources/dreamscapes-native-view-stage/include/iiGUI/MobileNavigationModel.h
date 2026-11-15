#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <unordered_set>
namespace ii::gui {
template<class Policy>struct BasicMobileNavigationModel final {
 using Configuration=typename Policy::Configuration;using Item=typename Policy::Item;
 using Layout=typename Policy::Layout;using Platform=typename Policy::Platform;
[[nodiscard]] static bool validate(const Configuration& c){
    const auto bounded=[](double n,double lo,double hi){return std::isfinite(n)&&n>=lo&&n<=hi;};
    if(!bounded(c.width,1,8192)||!bounded(c.bottom_safe_inset,0,8192-2*Policy::outer-Policy::group_height)||c.items.size()>5||
       c.current_index < -1 || (c.current_index>=0&&std::size_t(c.current_index)>=c.items.size())||
       !Policy::valid_text(c.accessibility_name)||
       (c.platform!=Platform::automatic&&c.platform!=Platform::ios&&c.platform!=Platform::android))return false;
    for(const auto channel:{c.focus_color.red,c.focus_color.green,c.focus_color.blue,c.focus_color.alpha})if(!bounded(channel,0,1))return false;
    const auto item=[](const Item& v){return !v.key.empty()&&v.key.size()<=256&&!v.icon_name.empty()&&v.icon_name.size()<=256&&
        (!v.text.empty()||!v.accessibility_name.empty())&&
        Policy::valid_text(v.key)&&Policy::valid_text(v.text)&&Policy::valid_text(v.icon_name)&&Policy::valid_text(v.accessibility_name)&&
        v.badge.size()<=256&&Policy::valid_text(v.badge)&&Policy::valid_text(v.badge_accessibility_name);};
    std::unordered_set<std::string> keys;
    for(const auto& v:c.items)if(!item(v)||!keys.insert(v.key).second)return false;
    return !c.search||(item(*c.search)&&!keys.contains(c.search->key));
}
 [[nodiscard]] static Layout layout(const Configuration& c,double width){
  const auto available=(std::max)(0.,width-2*Policy::outer);
  const auto search_space=c.search?Policy::group_height+Policy::gap:0.;
  const auto max_group=(std::max)(0.,available-search_space);const auto count=double(c.items.size());
  const auto tab=count?(std::max)(Policy::touch_size,(std::min)(Policy::tab_width,(max_group-2*Policy::inset)/count)):0.;
  const auto content=count*tab;const auto group=count?(std::min)(max_group,content+2*Policy::inset):0.;
  return {.height=Policy::group_height+2*Policy::outer+c.bottom_safe_inset,.group_x=Policy::outer,.group_y=Policy::outer,.group_width=group,
   .viewport_width=(std::max)(0.,group-2*Policy::inset),.content_width=content,.tab_width=tab,
   .search_x=(std::max)(Policy::outer,width-Policy::outer-Policy::group_height)};
 }
};
} // namespace ii::gui
