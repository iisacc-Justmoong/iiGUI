#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
namespace ii::gui {
template<class Policy>class BasicMobileNavigationMotion final {
    using MobileIndicatorGeometry=typename Policy::Geometry;
public:
    using TimePoint=std::chrono::steady_clock::time_point;
    [[nodiscard]] bool active()const noexcept{return active_;}
    [[nodiscard]] MobileIndicatorGeometry geometry()const noexcept{return visible_;}
    bool reset(int index,double width)noexcept{
        if(index<0||index>4||!std::isfinite(width)||width<0||width>8192)return false;
        visible_={double(index)*width,double(index+1)*width,Policy::touch_size};
        from_=target_=visible_;active_=false;initialized_=true;return true;
    }
    bool retarget(int index,double width,bool enabled,std::chrono::milliseconds duration,TimePoint now)noexcept{
        if(index<0||index>4||!std::isfinite(width)||width<0||width>8192||duration<duration.zero()||duration>std::chrono::hours(24))return false;
        if(!initialized_||!enabled||duration==duration.zero())return reset(index,width);
        (void)advance(now);const MobileIndicatorGeometry next{double(index)*width,double(index+1)*width,Policy::touch_size};
        if(next==target_)return true;
        from_=visible_;target_=next;start_=last_=now;duration_=duration;active_=from_!=target_;
        if(!active_)visible_=target_;return true;
    }
    bool advance(TimePoint now)noexcept{
        if(!active_)return false;if(now<last_)now=last_;last_=now;
        const auto elapsed=std::chrono::duration<double,std::milli>(now-start_).count();
        const auto total=double(duration_.count());if(elapsed>=total){visible_=target_;active_=false;return false;}
        const auto split=(std::min)(Policy::stretch_duration,total*Policy::stretch_fraction);const bool forward=target_.left+target_.right>=from_.left+from_.right;
        const MobileIndicatorGeometry stretch{from_.left+(target_.left-from_.left)*(forward?Policy::leading_forward:Policy::trailing_forward),
            from_.right+(target_.right-from_.right)*(forward?Policy::trailing_forward:Policy::leading_forward),Policy::touch_size-Policy::stretch_amount};
        const auto interpolate=[](MobileIndicatorGeometry a,MobileIndicatorGeometry b,double p){return MobileIndicatorGeometry{
            a.left+(b.left-a.left)*p,a.right+(b.right-a.right)*p,a.height+(b.height-a.height)*p};};
        if(elapsed<=split){const auto u=1-elapsed/split;visible_=interpolate(from_,stretch,1-u*u*u);}
        else{const auto x=(elapsed-split)/(total-split)-1;constexpr double overshoot=Policy::overshoot;
            visible_=interpolate(stretch,target_,1+(overshoot+1)*x*x*x+overshoot*x*x);}
        return true;
    }
private:
    MobileIndicatorGeometry visible_,from_,target_;
    TimePoint start_{},last_{};
    std::chrono::milliseconds duration_{};
    bool initialized_=false,active_=false;
};
} // namespace ii::gui
