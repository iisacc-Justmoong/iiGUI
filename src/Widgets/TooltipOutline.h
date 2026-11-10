#pragma once
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
namespace ii::gui {
template<class Policy>
class BasicTooltipOutline final {
    using TooltipLayout=typename Policy::Layout;using TooltipPlacement=typename Policy::Placement;using TooltipPoint=typename Policy::Point;
public:
[[nodiscard]] static std::optional<std::string> build(const TooltipLayout& value){
    const auto finite=[](double n){return std::isfinite(n)&&std::abs(n)<=1000000000.;};
    const auto& b=value.body;const auto& f=value.frame;
    if(!finite(b.x)||!finite(b.y)||!finite(b.width)||!finite(b.height)||!finite(f.x)||!finite(f.y)||
       !finite(f.width)||!finite(f.height)||!finite(value.radius)||!finite(value.tip.x)||!finite(value.tip.y)||
       !finite(value.tail_start.x)||!finite(value.tail_start.y)||!finite(value.tail_end.x)||!finite(value.tail_end.y)||
       b.width<=0||b.height<=0||f.width<=0||f.height<=0||f.width>8192||f.height>8192||value.radius<0||
       value.radius>(std::min)(b.width,b.height)/2||b.x<f.x-1e-5||b.y<f.y-1e-5||b.x+b.width>f.x+f.width+1e-5||b.y+b.height>f.y+f.height+1e-5||
       value.tip.x<f.x-1e-5||value.tip.x>f.x+f.width+1e-5||value.tip.y<f.y-1e-5||value.tip.y>f.y+f.height+1e-5)
        return std::nullopt;
    const auto in_range=[](double n,double low,double high){return n>=low-1e-5&&n<=high+1e-5;};
    const auto equal=[](double a,double c){return std::abs(a-c)<1e-5;};
    switch(value.placement){
    case TooltipPlacement::above:case TooltipPlacement::below:{
        const auto edge=value.placement==TooltipPlacement::above?b.y+b.height:b.y;
        if(!equal(value.tail_start.y,edge)||!equal(value.tail_end.y,edge)||
           (value.placement==TooltipPlacement::above&&value.tip.y<edge-1e-5)||
           (value.placement==TooltipPlacement::below&&value.tip.y>edge+1e-5)||
           !in_range(value.tail_start.x,b.x+value.radius,b.x+b.width-value.radius)||
           !in_range(value.tail_end.x,value.tail_start.x,b.x+b.width-value.radius))return std::nullopt;
        break;
    }
    case TooltipPlacement::left:case TooltipPlacement::right:{
        const auto edge=value.placement==TooltipPlacement::left?b.x+b.width:b.x;
        if(!equal(value.tail_start.x,edge)||!equal(value.tail_end.x,edge)||
           (value.placement==TooltipPlacement::left&&value.tip.x<edge-1e-5)||
           (value.placement==TooltipPlacement::right&&value.tip.x>edge+1e-5)||
           !in_range(value.tail_start.y,b.y+value.radius,b.y+b.height-value.radius)||
           !in_range(value.tail_end.y,value.tail_start.y,b.y+b.height-value.radius))return std::nullopt;
        break;
    }
    default:return std::nullopt;
    }
    const auto x=b.x-f.x,y=b.y-f.y,right=x+b.width,bottom=y+b.height,r=value.radius;
    const TooltipPoint tip{value.tip.x-f.x,value.tip.y-f.y},start{value.tail_start.x-f.x,value.tail_start.y-f.y},
        end{value.tail_end.x-f.x,value.tail_end.y-f.y};
    std::ostringstream path;path.imbue(std::locale::classic());path<<std::setprecision(12);
    const auto line=[&](double a,double c){path<<" L "<<a<<' '<<c;};
    const auto curve=[&](double a,double c,double d,double e){path<<" Q "<<a<<' '<<c<<' '<<d<<' '<<e;};
    path<<"M "<<x+r<<' '<<y;
    if(value.placement==TooltipPlacement::below){line(start.x,start.y);line(tip.x,tip.y);line(end.x,end.y);}
    line(right-r,y);curve(right,y,right,y+r);
    if(value.placement==TooltipPlacement::left){line(start.x,start.y);line(tip.x,tip.y);line(end.x,end.y);}
    line(right,bottom-r);curve(right,bottom,right-r,bottom);
    if(value.placement==TooltipPlacement::above){line(end.x,end.y);line(tip.x,tip.y);line(start.x,start.y);}
    line(x+r,bottom);curve(x,bottom,x,bottom-r);
    if(value.placement==TooltipPlacement::right){line(end.x,end.y);line(tip.x,tip.y);line(start.x,start.y);}
    line(x,y+r);curve(x,y,x+r,y);path<<" Z";return path.str();
}
};
} // namespace ii::gui
