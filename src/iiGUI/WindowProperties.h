#pragma once
#include "Host.h"
#include "Appearance.h"
#include <iiSDKCore/Core/Text/Utf8.h>
#include <algorithm>
#include <cmath>

namespace ii::gui {
struct WindowProperties final {
    WindowGeometry geometry;
    std::optional<WidgetAppearance> appearance;
    friend bool operator==(const WindowProperties&,const WindowProperties&)=default;
};
struct WindowGeometryValue final {
    std::string title;
    Size size,minimum_size;
    friend bool operator==(const WindowGeometryValue&,const WindowGeometryValue&)=default;
};
[[nodiscard]] inline core::Result<void> validate_window_geometry(const WindowGeometry& value) {
    if(value.title&&(!core::valid_utf8(*value.title)||value.title->find('\0')!=value.title->npos||value.title->size()>65536))
        return core::fail(core::ErrorCode::invalid_argument,"Window title must be UTF-8 without NUL, up to 64 KiB");
    for(const auto& number:{value.width,value.height,value.minimum_width,value.minimum_height})
        if(number&&(!std::isfinite(*number)||*number<=0||*number>32767))
            return core::fail(core::ErrorCode::invalid_argument,"Window dimensions must be finite and in (0,32767]");
    if((value.width&&value.minimum_width&&*value.width<*value.minimum_width)||
       (value.height&&value.minimum_height&&*value.height<*value.minimum_height))
        return core::fail(core::ErrorCode::invalid_argument,"Window size must be at least its minimum size");
    return {};
}
// Resolve against the actual current window, rather than a previous declaration.
// A larger minimum grows an unconstrained dimension; an explicitly smaller size
// is rejected before any native changes are attempted.
[[nodiscard]] inline core::Result<WindowGeometryValue> resolve_window_geometry(WindowGeometryValue current,const WindowGeometry& update) {
    if(auto valid=validate_window_geometry(update);!valid)return std::unexpected(valid.error());
    if(update.title)current.title=*update.title;
    current.minimum_size={update.minimum_width.value_or(current.minimum_size.width),update.minimum_height.value_or(current.minimum_size.height)};
    current.size={update.width.value_or(std::max(current.size.width,current.minimum_size.width)),
                  update.height.value_or(std::max(current.size.height,current.minimum_size.height))};
    if(current.size.width<current.minimum_size.width||current.size.height<current.minimum_size.height)
        return core::fail(core::ErrorCode::invalid_argument,"Window size must be at least its minimum size");
    return current;
}
[[nodiscard]] inline bool valid_widget_appearance(const WidgetAppearance& value) {
    for(auto c:{value.window,value.primary,value.hover,value.pressed,value.disabled_background})
        for(auto channel:{c.red,c.green,c.blue,c.alpha})if(!std::isfinite(channel)||channel<0||channel>1)return false;
    for(auto alpha:value.text_alpha)if(!std::isfinite(alpha)||alpha<0||alpha>1)return false;
    for(auto number:{value.button_height,value.button_padding,value.button_radius})if(!std::isfinite(number)||number<0||number>32767)return false;
    return value.button_height>0&&!value.font_family.empty()&&value.font_family.size()<=4096&&
        core::valid_utf8(value.font_family)&&value.font_family.find('\0')==value.font_family.npos;
}
[[nodiscard]] inline core::Result<void> validate_window_properties(const WindowProperties& value) {
    if(auto valid=validate_window_geometry(value.geometry);!valid)return valid;
    if(value.appearance&&!valid_widget_appearance(*value.appearance))
        return core::fail(core::ErrorCode::invalid_argument,"Invalid Window appearance");
    return {};
}
[[nodiscard]] inline WindowProperties window_properties_delta(const WindowProperties& before,WindowProperties next) {
    auto& a=next.geometry;const auto& b=before.geometry;
    if(a.title==b.title)a.title.reset();
    if(a.width==b.width)a.width.reset();if(a.height==b.height)a.height.reset();
    if(a.minimum_width==b.minimum_width)a.minimum_width.reset();if(a.minimum_height==b.minimum_height)a.minimum_height.reset();
    if(next.appearance==before.appearance)next.appearance.reset();
    return next;
}
[[nodiscard]] inline bool empty_window_geometry(const WindowGeometry& v) {
    return !v.title&&!v.width&&!v.height&&!v.minimum_width&&!v.minimum_height;
}
} // namespace ii::gui
