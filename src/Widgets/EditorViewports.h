#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace ii::gui {
struct EditorPoint final {
    double x=0,y=0;
    friend bool operator==(const EditorPoint&,const EditorPoint&)=default;
};
struct EditorCanvasViewportConfiguration final {
    double scale=1,offset_x=0,offset_y=0;
    double rotation_radians=0;
    bool flip_x=false,flip_y=false;
    friend bool operator==(const EditorCanvasViewportConfiguration&,const EditorCanvasViewportConfiguration&)=default;
};
// Screen and document coordinates are logical pixels. The app owns content.
class EditorCanvasViewport final {
public:
    [[nodiscard]] const EditorCanvasViewportConfiguration& configuration()const noexcept{return value_;}
    bool configure(EditorCanvasViewportConfiguration value)noexcept{
        if(!std::isfinite(value.scale)||value.scale<.0001||value.scale>8192||!coordinate(value.offset_x)||!coordinate(value.offset_y)||
           !std::isfinite(value.rotation_radians)||std::abs(value.rotation_radians)>1e6)return false;
        value_=value;cos_=std::cos(value.rotation_radians);sin_=std::sin(value.rotation_radians);return true;
    }
    [[nodiscard]] EditorPoint document_point(EditorPoint screen)const noexcept{
        const auto x=screen.x-value_.offset_x,y=screen.y-value_.offset_y;
        return {(cos_*x+sin_*y)/value_.scale*(value_.flip_x?-1:1),(-sin_*x+cos_*y)/value_.scale*(value_.flip_y?-1:1)};
    }
    [[nodiscard]] EditorPoint screen_point(EditorPoint document)const noexcept{
        const auto x=document.x*value_.scale*(value_.flip_x?-1:1),y=document.y*value_.scale*(value_.flip_y?-1:1);
        return {cos_*x-sin_*y+value_.offset_x,sin_*x+cos_*y+value_.offset_y};
    }
    bool pan(double x,double y)noexcept{
        if(!coordinate(x)||!coordinate(y))return false;auto next=value_;next.offset_x+=x;next.offset_y+=y;return configure(next);
    }
    bool zoom_at(double factor,EditorPoint screen)noexcept{
        if(!std::isfinite(factor)||factor<=0||!coordinate(screen.x)||!coordinate(screen.y))return false;
        const auto document=document_point(screen);auto next=value_;next.scale*=factor;auto candidate=*this;
        if(!candidate.configure(next))return false;const auto mapped=candidate.screen_point(document);
        next.offset_x+=screen.x-mapped.x;next.offset_y+=screen.y-mapped.y;return configure(next);
    }
private:
    static bool coordinate(double value)noexcept{return std::isfinite(value)&&std::abs(value)<=1e15;}
    EditorCanvasViewportConfiguration value_;
    double cos_=1,sin_=0;
};
struct EditorTimelineViewportConfiguration final {
    std::int64_t frame_count=0;
    double pixels_per_frame=1,scroll=0,width=0;
    friend bool operator==(const EditorTimelineViewportConfiguration&,const EditorTimelineViewportConfiguration&)=default;
};
// Half-open frame range. Consumers ask their own model only for this interval.
struct EditorFrameRange final {
    std::int64_t first=0,end=0;
    friend bool operator==(const EditorFrameRange&,const EditorFrameRange&)=default;
};
// An unspecified rate still permits frame ticks. Time divisions require a
// positive numerator and denominator from the owning document.
struct EditorTimelineFrameRate final {
    std::uint32_t numerator=0,denominator=1;
    friend bool operator==(const EditorTimelineFrameRate&,const EditorTimelineFrameRate&)=default;

    // Return the first frame in the half-open range that is nearest to a
    // time division. Half-frame ties go to the later frame. The modular
    // calculation avoids multiplying a long frame index by the rate.
    [[nodiscard]] std::optional<std::int64_t> first_time_boundary(
            EditorFrameRange range,std::uint32_t divisions_per_second)const noexcept{
        if(!numerator||!denominator||!divisions_per_second||range.first<0||range.end<=range.first)return {};
        const auto divisions=std::uint64_t(denominator)*divisions_per_second;
        const auto remainder=(std::uint64_t(range.first)%numerator)*(divisions%numerator)%numerator;
        if(2*remainder<=divisions)return range.first;
        const auto offset=(numerator-remainder+divisions/2)/divisions;
        if(offset>=std::uint64_t(range.end-range.first))return {};
        return range.first+std::int64_t(offset);
    }
};
class EditorTimelineViewport final {
public:
    [[nodiscard]] const EditorTimelineViewportConfiguration& configuration()const noexcept{return value_;}
    bool configure(EditorTimelineViewportConfiguration value)noexcept{
        if(value.frame_count<0||value.frame_count>1'000'000'000'000LL||!std::isfinite(value.pixels_per_frame)||value.pixels_per_frame<.0001||value.pixels_per_frame>8192||
           !std::isfinite(value.scroll)||value.scroll<0||!std::isfinite(value.width)||value.width<0||value.width>8192)return false;
        value.scroll=std::clamp(value.scroll,0.,maximum_scroll(value));value_=value;return true;
    }
    [[nodiscard]] EditorFrameRange visible_frames()const noexcept{
        const auto total=value_.frame_count;if(total==0)return {};
        const auto first=std::clamp(std::int64_t(std::floor(value_.scroll/value_.pixels_per_frame)),std::int64_t(0),total);
        const auto end=value_.width==0?first:std::clamp(std::int64_t(std::ceil((value_.scroll+value_.width)/value_.pixels_per_frame)),first,total);
        return {first,end};
    }
    // Frame boundaries use the same coordinate as the clip at that frame.
    [[nodiscard]] double boundary_x(std::int64_t frame)const noexcept{
        return double(frame)*value_.pixels_per_frame-value_.scroll;
    }
    // Locate the first boundary at or after a viewport coordinate. One past
    // the terminal boundary is a valid sentinel for a half-open iteration.
    [[nodiscard]] std::int64_t boundary_at_or_after(double x)const noexcept{
        if(!std::isfinite(x))return value_.frame_count+1;
        const auto last=value_.frame_count;
        auto frame=std::int64_t(std::clamp(std::ceil((value_.scroll+x)/value_.pixels_per_frame),0.,double(last+1)));
        if(frame>0&&boundary_x(frame-1)>=x)--frame;
        if(frame<=last&&boundary_x(frame)<x)++frame;
        return frame;
    }
    // A ruler includes the terminal boundary when it lands at the right edge.
    // The content query remains a half-open range of frame cells.
    [[nodiscard]] EditorFrameRange visible_boundaries()const noexcept{
        if(value_.frame_count==0||value_.width==0)return {};
        const auto first=boundary_at_or_after(0);
        auto end=boundary_at_or_after(value_.width);
        if(end<=value_.frame_count&&boundary_x(end)<=value_.width)++end;
        return {first,end};
    }
    [[nodiscard]] std::optional<std::int64_t> frame_at(double x)const noexcept{
        if(!std::isfinite(x)||std::abs(x)>1e15||value_.frame_count==0)return {};
        const auto frame=std::floor((value_.scroll+x)/value_.pixels_per_frame);
        return std::int64_t(std::clamp(frame,0.,double(value_.frame_count-1)));
    }
    bool scroll_by(double delta)noexcept{
        if(!std::isfinite(delta)||std::abs(delta)>1e15)return false;auto next=value_;
        next.scroll=std::clamp(value_.scroll+delta,0.,maximum_scroll(value_));return configure(next);
    }
    bool zoom_at(double factor,double anchor)noexcept{
        if(!std::isfinite(factor)||factor<=0||!std::isfinite(anchor)||std::abs(anchor)>8192)return false;
        const auto frame=(value_.scroll+anchor)/value_.pixels_per_frame;auto next=value_;next.pixels_per_frame*=factor;
        if(!std::isfinite(next.pixels_per_frame)||next.pixels_per_frame<.0001||next.pixels_per_frame>8192)return false;
        next.scroll=std::clamp(frame*next.pixels_per_frame-anchor,0.,maximum_scroll(next));return configure(next);
    }
private:
    static double maximum_scroll(const EditorTimelineViewportConfiguration& v)noexcept{return (std::max)(0.,double(v.frame_count)*v.pixels_per_frame-v.width);}
    EditorTimelineViewportConfiguration value_;
};
} // namespace ii::gui
