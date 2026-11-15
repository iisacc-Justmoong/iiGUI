#pragma once
#include <algorithm>
#include <cmath>
#include <optional>
namespace ii::gui {
// Computes clipped regions from an application-supplied design configuration.
template<class Policy>
class BasicPanelLayout final {
    using Configuration=typename Policy::Configuration;
    using Regions=typename Policy::Regions;
public:
    [[nodiscard]] static bool validate(const Configuration& value)noexcept{
        const auto bound=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
        const auto c=value.background;
        return bound(value.width,1,8192)&&bound(value.height,1,8192)&&bound(value.toolbar_height,0,1024)&&bound(value.footer_height,0,1024)&&
            bound(c.red,0,1)&&bound(c.green,0,1)&&bound(c.blue,0,1)&&bound(c.alpha,0,1);
    }
    [[nodiscard]] static std::optional<Regions> geometry(const Configuration& value,double width,double height)noexcept{
        const auto bound=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
        if(!validate(value)||!bound(width,0,8192)||!bound(height,0,8192))return std::nullopt;
        const auto toolbar=value.toolbar_visible?(std::min)(value.toolbar_height,height):0.;
        const auto footer=value.footer_visible?(std::min)(value.footer_height,height-toolbar):0.;
        const auto list=height-toolbar-footer;
        return Regions{.toolbar={0,0,width,toolbar},.list={0,toolbar,width,list},.footer={0,toolbar+list,width,footer},
            .toolbar_visible=width>0&&toolbar>0,.list_visible=width>0&&list>0,.footer_visible=width>0&&footer>0};
    }
};
} // namespace ii::gui
