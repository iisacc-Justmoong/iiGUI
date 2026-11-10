#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
namespace ii::gui {
template<class Policy>
class BasicTooltip final {
    using TooltipConfiguration=typename Policy::Configuration;using TooltipInteraction=typename Policy::Interaction;
    using TooltipPlacement=typename Policy::Placement;using TooltipPhase=typename Policy::Phase;using TooltipTransition=typename Policy::Transition;
    using TooltipRect=typename Policy::Rect;using TooltipPoint=typename Policy::Point;using TooltipSize=typename Policy::Size;using TooltipLayout=typename Policy::Layout;
    using Milliseconds=std::chrono::milliseconds;
    TooltipConfiguration configuration_;
    TooltipInteraction interaction_;
    std::optional<std::int64_t> last_time_,opening_since_,hiding_since_,shown_since_;
    bool visible_=false,latched_=false,manual_=false;
    static constexpr std::int64_t duration_limit=86400000;
    [[nodiscard]] static bool metric(double value,double upper=8192)noexcept{
        return std::isfinite(value)&&value>=0&&value<=upper;
    }
    [[nodiscard]] static bool coordinate(double value)noexcept{
        return std::isfinite(value)&&std::abs(value)<=1000000000.;
    }
    [[nodiscard]] static bool duration(Milliseconds value)noexcept{
        return value.count()>=0&&value.count()<=duration_limit;
    }
    [[nodiscard]] bool accept_time(std::int64_t value)noexcept{
        if(value<0||value>(std::numeric_limits<std::int64_t>::max)()-duration_limit||
           (last_time_&&value<*last_time_))return false;
        last_time_=value;return true;
    }
    [[nodiscard]] bool engaged()const noexcept{
        return interaction_.target_present&&interaction_.target_available&&interaction_.target_engaged;
    }
    void hide()noexcept{visible_=false;manual_=false;shown_since_.reset();opening_since_.reset();hiding_since_.reset();}
public:
    [[nodiscard]] static bool validate(const TooltipConfiguration& value)noexcept{
        switch(value.preferred){case TooltipPlacement::automatic:case TooltipPlacement::above:case TooltipPlacement::below:
            case TooltipPlacement::left:case TooltipPlacement::right:break;default:return false;}
        return metric(value.maximum_width)&&value.maximum_width>0&&metric(value.maximum_height)&&value.maximum_height>0&&
            metric(value.content_padding)&&metric(value.edge_margin)&&metric(value.corner_radius)&&
            metric(value.tail_length)&&metric(value.tail_width)&&duration(value.delay)&&duration(value.timeout)&&
            duration(value.hide_delay)&&duration(value.animation_duration);
    }
    bool set_configuration(TooltipConfiguration value)noexcept{
        if(!validate(value))return false;
        configuration_=value;
        if(!configuration_.automatic){opening_since_.reset();hiding_since_.reset();}
        return true;
    }
    [[nodiscard]] const TooltipConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] bool visible()const noexcept{return visible_;}
    [[nodiscard]] TooltipPhase phase()const noexcept{
        if(visible_)return hiding_since_?TooltipPhase::waiting_to_hide:TooltipPhase::shown;
        return opening_since_?TooltipPhase::waiting_to_open:TooltipPhase::hidden;
    }
    [[nodiscard]] std::optional<std::int64_t> next_deadline()const noexcept{
        std::optional<std::int64_t> result;
        const auto consider=[&](std::optional<std::int64_t> start,Milliseconds elapsed){
            if(start){const auto deadline=*start+elapsed.count();if(!result||deadline<*result)result=deadline;}
        };
        if(!visible_)consider(opening_since_,configuration_.delay);
        else {
            if(configuration_.timeout.count()>0)consider(shown_since_,configuration_.timeout);
            consider(hiding_since_,configuration_.hide_delay);
        }
        return result;
    }
    [[nodiscard]] std::optional<TooltipTransition> advance(std::int64_t now,TooltipInteraction input)noexcept{
        if(!accept_time(now))return std::nullopt;
        interaction_=input;TooltipTransition change;
        const bool active=engaged();if(!active)latched_=false;
        if(visible_&&interaction_.target_present&&!interaction_.target_available){
            hide();latched_=false;change.closed=true;return change;
        }
        if(visible_&&configuration_.timeout.count()>0&&shown_since_&&now>=*shown_since_+configuration_.timeout.count()){
            hide();latched_=active;change.closed=true;return change;
        }
        if(visible_){
            if(manual_||!configuration_.automatic||active||interaction_.bubble_hovered)hiding_since_.reset();
            else {
                if(!hiding_since_)hiding_since_=now;
                if(now>=*hiding_since_+configuration_.hide_delay.count()){hide();latched_=false;change.closed=true;}
            }
        }else if(configuration_.automatic&&active&&!latched_){
            if(!opening_since_)opening_since_=now;
            if(now>=*opening_since_+configuration_.delay.count()){
                opening_since_.reset();visible_=true;manual_=false;shown_since_=now;latched_=true;change.opened=true;
            }
        }else opening_since_.reset();
        return change;
    }
    [[nodiscard]] std::optional<TooltipTransition> open(std::int64_t now)noexcept{
        if(!accept_time(now))return std::nullopt;
        TooltipTransition change;if(!visible_){visible_=true;shown_since_=now;change.opened=true;}
        opening_since_.reset();hiding_since_.reset();latched_=engaged();manual_=true;return change;
    }
    [[nodiscard]] std::optional<TooltipTransition> close(std::int64_t now)noexcept{
        if(!accept_time(now))return std::nullopt;
        TooltipTransition change;change.closed=visible_;hide();latched_=engaged();return change;
    }
    [[nodiscard]] std::optional<TooltipLayout> layout(TooltipRect available,TooltipPoint anchor,TooltipSize content,
            std::optional<TooltipRect> anchor_bounds=std::nullopt)const noexcept{
        const auto origin=anchor_bounds.value_or(available);
        if(!coordinate(available.x)||!coordinate(available.y)||!metric(available.width)||!metric(available.height)||
           !coordinate(origin.x)||!coordinate(origin.y)||!metric(origin.width)||!metric(origin.height)||
           !coordinate(anchor.x)||!coordinate(anchor.y)||!metric(content.width,1000000)||!metric(content.height,1000000)||
           anchor.x<origin.x||anchor.x>origin.x+origin.width||anchor.y<origin.y||anchor.y>origin.y+origin.height)
            return std::nullopt;
        const double ix=(std::min)(configuration_.edge_margin,available.width/2),iy=(std::min)(configuration_.edge_margin,available.height/2);
        const double left=available.x+ix,right=available.x+available.width-ix,top=available.y+iy,bottom=available.y+available.height-iy;
        if(right<=left||bottom<=top)return std::nullopt;
        const double desired_width=(std::min)(configuration_.maximum_width,(std::max)(1.,content.width+2*configuration_.content_padding));
        const double desired_height=(std::min)(configuration_.maximum_height,(std::max)(1.,content.height+2*configuration_.content_padding));
        const std::array<TooltipPlacement,4> choices{TooltipPlacement::above,TooltipPlacement::below,TooltipPlacement::right,TooltipPlacement::left};
        std::array<TooltipPlacement,4> order=choices;
        if(configuration_.preferred!=TooltipPlacement::automatic){
            const auto it=std::find(order.begin(),order.end(),configuration_.preferred);
            std::rotate(order.begin(),it,it+1);
        }
        std::optional<TooltipLayout> best;double best_area=-1;
        for(const auto side:order){
            double x0=left,x1=right,y0=top,y1=bottom;
            if(side==TooltipPlacement::above)y1=(std::min)(y1,anchor.y-configuration_.tail_length);
            if(side==TooltipPlacement::below)y0=(std::max)(y0,anchor.y+configuration_.tail_length);
            if(side==TooltipPlacement::left)x1=(std::min)(x1,anchor.x-configuration_.tail_length);
            if(side==TooltipPlacement::right)x0=(std::max)(x0,anchor.x+configuration_.tail_length);
            if(x1<=x0||y1<=y0)continue;
            const double width=(std::min)(desired_width,x1-x0),height=(std::min)(desired_height,y1-y0);
            TooltipRect body;
            body.width=width;body.height=height;
            body.x=side==TooltipPlacement::left?x1-width:side==TooltipPlacement::right?x0:std::clamp(anchor.x-width/2,x0,(std::max)(x0,x1-width));
            body.y=side==TooltipPlacement::above?y1-height:side==TooltipPlacement::below?y0:std::clamp(anchor.y-height/2,y0,(std::max)(y0,y1-height));
            TooltipLayout candidate;candidate.placement=side;candidate.body=body;candidate.tip=anchor;
            const auto x=(std::min)(body.x,anchor.x),y=(std::min)(body.y,anchor.y);
            candidate.frame={x,y,(std::max)(body.x+width,anchor.x)-x,(std::max)(body.y+height,anchor.y)-y};
            candidate.radius=(std::min)({configuration_.corner_radius,width/2,height/2});
            if(side==TooltipPlacement::above||side==TooltipPlacement::below){
                const auto half=(std::min)(configuration_.tail_width/2,(std::max)(0.,width/2-candidate.radius));
                const auto low=body.x+candidate.radius+half,high=(std::max)(low,body.x+width-candidate.radius-half);
                const auto center=std::clamp(anchor.x,low,high);
                const auto edge=side==TooltipPlacement::above?body.y+height:body.y;
                candidate.tail_start={center-half,edge};candidate.tail_end={center+half,edge};
            }else{
                const auto half=(std::min)(configuration_.tail_width/2,(std::max)(0.,height/2-candidate.radius));
                const auto low=body.y+candidate.radius+half,high=(std::max)(low,body.y+height-candidate.radius-half);
                const auto center=std::clamp(anchor.y,low,high);
                const auto edge=side==TooltipPlacement::left?body.x+width:body.x;
                candidate.tail_start={edge,center-half};candidate.tail_end={edge,center+half};
            }
            const auto area=width*height;if(!best||area>best_area){best=candidate;best_area=area;}
            if(width==desired_width&&height==desired_height)break;
        }
        return best;
    }
};
} // namespace ii::gui
