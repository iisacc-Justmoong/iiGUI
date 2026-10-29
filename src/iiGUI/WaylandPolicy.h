#pragma once
#include "HostCommon.h"
namespace ii::gui::detail {
// Compositors may send zero to let the client choose a dimension. Never adopt
// negative, unbounded or NaN dimensions into GPU allocation state.
inline Size wayland_configure_size(Size current,int width,int height) {
    if(width<0||height<0||width>32767||height>32767)throw std::out_of_range("Wayland configure exceeds logical coordinate budget");
    if(width)current.width=width;if(height)current.height=height;
    if(!valid_size(current))throw std::out_of_range("Invalid Wayland configure size");
    return current;
}
inline int wayland_buffer_scale(int value) {
    if(value<1||value>8)throw std::out_of_range("Wayland buffer scale must be 1..8");
    return value;
}
}
