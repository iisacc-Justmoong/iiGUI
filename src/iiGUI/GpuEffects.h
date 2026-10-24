#pragma once
#include "NativeText.h"
#include <bit>
namespace ii::gui::detail {
// Fits Vulkan's guaranteed 128-byte push-constant minimum. Float arrays also
// match the D3D11 constant-buffer layout. Flags: shadow 1, mask 2, underlay 4,
// capture 8, underlay tint 16. New fields are ignored by ordinary panel/text stages.
struct alignas(16) GpuEffect final {float view[4],bounds[4],tint[4],border[4],style[4],underlay[4],base[4],highlight[4];};
static_assert(sizeof(GpuEffect)==128);
// Four unit-range colors use two half pairs in otherwise unused slots. The
// maximum channel error is below 1/1024; the push block stays at 128 bytes.
inline std::uint32_t unit_half(float f){if(f<.00006103515625f)return std::uint32_t(std::round(f*16777216.f));const auto bits=std::bit_cast<std::uint32_t>(f)+0x1000u;return (((bits>>23)-112u)<<10)|((bits>>13)&1023u);}
inline float half_pair(float a,float b){return std::bit_cast<float>(unit_half(a)|(unit_half(b)<<16));}

inline GpuEffect effect_parameters(CanvasLayerView layer,Rect viewport,double scale,bool shadow){
    const auto& t=layer.content_transform;GpuEffect result{{viewport.width,viewport.height,float(scale),t.opacity},{},{},{},{},{},{},{}};
    const auto bounds=transform_rect(shadow?layer.shadow->bounds:layer.backdrop->bounds,t);const auto paint=shadow?layer.shadow->color:layer.backdrop->tint;
    result.bounds[0]=bounds.x;result.bounds[1]=bounds.y;result.bounds[2]=bounds.width;result.bounds[3]=bounds.height;
    result.tint[0]=paint.red;result.tint[1]=paint.green;result.tint[2]=paint.blue;result.tint[3]=paint.alpha;
    result.style[0]=(shadow?layer.shadow->radius:layer.backdrop->radius)*t.scale;result.style[1]=(shadow?layer.shadow->blur_radius:layer.backdrop->blur_radius)*t.scale;result.style[2]=shadow?0:layer.backdrop->border_width*t.scale;
    unsigned flags=shadow?1:0;if(shadow?bool(layer.shadow->mask):bool(layer.backdrop->mask))flags|=2;
    if(!shadow){const auto& m=*layer.backdrop;if(m.underlay)flags|=4;if(m.capture)flags|=8;if(m.underlay_tint)flags|=16;const auto b=transform_rect(m.underlay_bounds,t);
        result.underlay[0]=b.x;result.underlay[1]=b.y;result.underlay[2]=b.width;result.underlay[3]=b.height;const auto color=m.border;result.border[0]=color.red;result.border[1]=color.green;result.border[2]=color.blue;result.border[3]=color.alpha;
        result.base[0]=m.base.red;result.base[1]=m.base.green;result.base[2]=m.base.blue;result.base[3]=m.base.alpha;result.highlight[0]=m.inner_highlight_opacity;result.highlight[1]=m.inner_highlight_offset_y*t.scale;if(m.underlay_tint){const auto color=*m.underlay_tint;result.highlight[2]=half_pair(color.red,color.green);result.highlight[3]=half_pair(color.blue,color.alpha);}
    }result.style[3]=float(flags);return result;
}
}
