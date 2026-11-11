#pragma once

#include <algorithm>

namespace ii::gui::detail {
enum class WindowResizeEdge { none, top_left, top, top_right, right, bottom_right, bottom, bottom_left, left };

// Coordinates and margins share the same units; a point outside the window is
// never treated as a resize handle. Narrow windows retain non-overlapping edges.
[[nodiscard]] constexpr WindowResizeEdge window_resize_edge(double x,double y,double width,double height,
                                                              double horizontal,double vertical) noexcept {
    if(!(width>0 && height>0 && horizontal>0 && vertical>0 && x>=0 && y>=0 && x<width && y<height))
        return WindowResizeEdge::none;
    const auto hx=std::min(horizontal,width/2),hy=std::min(vertical,height/2);
    const bool left=x<hx,right=x>=width-hx,top=y<hy,bottom=y>=height-hy;
    if(top&&left)return WindowResizeEdge::top_left;
    if(top&&right)return WindowResizeEdge::top_right;
    if(bottom&&left)return WindowResizeEdge::bottom_left;
    if(bottom&&right)return WindowResizeEdge::bottom_right;
    if(top)return WindowResizeEdge::top;
    if(bottom)return WindowResizeEdge::bottom;
    if(left)return WindowResizeEdge::left;
    if(right)return WindowResizeEdge::right;
    return WindowResizeEdge::none;
}
} // namespace ii::gui::detail
