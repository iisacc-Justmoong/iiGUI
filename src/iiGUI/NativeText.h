#pragma once
#include "NativeCanvas.h"
#include <algorithm>

namespace ii::gui::detail {
// Native shaping/wrapping uses logical units. Rasterization uses device pixels.
float measure_text(const TextRun&);
// Top-left, straight-alpha RGBA8 encoded sRGB, uploaded to an sRGB GPU texture.
PixelBuffer rasterize_text(const CanvasFrame&, double scale);
inline float encoded_srgb(float linear) {
    return linear<=0.0031308f ? linear*12.92f : 1.055f*std::pow(linear,1.f/2.4f)-0.055f;
}
// Native surfaces use premultiplied encoded sRGB. Unpremultiply before GPU sRGB
// decoding, then the text shader premultiplies in linear light for composition.
inline void straight_rgba(PixelBuffer& image, bool bgra) {
    for (std::size_t i=0;i<image.rgba.size();i+=4) {
        auto* p=image.rgba.data()+i;
        if (bgra) std::swap(p[0],p[2]);
        if (p[3]) for (int c=0;c<3;++c)
            p[c]=static_cast<std::uint8_t>(std::min(255u,(unsigned(p[c])*255u+p[3]/2u)/p[3]));
        else p[0]=p[1]=p[2]=0;
    }
}
} // namespace ii::gui::detail
