#pragma once
#include <iiGUI/HostCommon.h>
#include <cmath>
#include <cstdint>
#include <optional>
namespace ii::gui::detail {
// Wayland toplevel and viewport destinations are integer surface coordinates.
inline Size wayland_surface_size(Size value) {
    if(!valid_size(value))throw std::out_of_range("Invalid Wayland surface size");
    return {std::max(1.,std::round(value.width)),std::max(1.,std::round(value.height))};
}
// Compositors may send zero to let the client choose a dimension. Never adopt
// negative, unbounded or NaN dimensions into GPU allocation state.
inline Size wayland_configure_size(Size current,int width,int height) {
    if(width<0||height<0||width>32767||height>32767)throw std::out_of_range("Wayland configure exceeds logical coordinate budget");
    if(width)current.width=width;if(height)current.height=height;
    return wayland_surface_size(current);
}
inline int wayland_buffer_scale(int value) {
    if(value<1||value>8)throw std::out_of_range("Wayland buffer scale must be 1..8");
    return value;
}
// Integer output scale remains the fallback until the compositor sends a
// preferred fractional scale. Mode changes matter even when the ratio is equal:
// viewporter and wl_surface have different buffer-scale requirements.
class WaylandScaleState final {
    int output_=1;
    std::optional<std::uint32_t> preferred_;
public:
    [[nodiscard]] double scale()const noexcept{return preferred_?double(*preferred_)/120.:double(output_);}
    [[nodiscard]] int buffer_scale()const noexcept{return preferred_?1:output_;}
    [[nodiscard]] int cursor_scale()const noexcept{return std::max(1,int(std::ceil(scale())));}
    [[nodiscard]] bool fractional()const noexcept{return preferred_.has_value();}
    bool output(int value){value=wayland_buffer_scale(value);const bool changed=!preferred_&&value!=output_;output_=value;return changed;}
    bool preferred(std::uint32_t value){if(!value||value>960)throw std::out_of_range("Invalid Wayland preferred scale");if(preferred_==value)return false;preferred_=value;return true;}
    bool clear_preferred()noexcept{if(!preferred_)return false;preferred_.reset();return true;}
};
inline std::uint32_t wayland_scaled_pixels(double logical,double scale){
    const double pixels=std::round(logical*scale);
    if(!std::isfinite(logical)||logical<=0||!std::isfinite(scale)||scale<=0||!std::isfinite(pixels)||pixels<1||pixels>8192)
        throw std::out_of_range("Wayland physical dimension exceeds canvas budget");
    return static_cast<std::uint32_t>(pixels);
}
inline std::uint32_t wayland_fractional_pixels(int logical,std::uint32_t numerator){
    if(!numerator||numerator>960)throw std::out_of_range("Invalid Wayland preferred scale");
    return wayland_scaled_pixels(logical,double(numerator)/120.);
}
}
