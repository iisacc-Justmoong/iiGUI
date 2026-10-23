#pragma once
#include "NativeText.h"
namespace ii::gui::detail {
struct alignas(16) GpuEffect final {float view[4],bounds[4],tint[4],border[4],style[4];};
static_assert(sizeof(GpuEffect)==80);
inline GpuEffect effect_parameters(CanvasLayerView layer,Rect viewport,double scale,bool shadow){
    const auto& t=layer.content_transform;GpuEffect result{{viewport.width,viewport.height,float(scale),t.opacity},{},{},{},{}};
    const auto bounds=transform_rect(shadow?layer.shadow->bounds:layer.backdrop->bounds,t);const auto paint=shadow?layer.shadow->color:layer.backdrop->tint;
    result.bounds[0]=bounds.x;result.bounds[1]=bounds.y;result.bounds[2]=bounds.width;result.bounds[3]=bounds.height;
    result.tint[0]=paint.red;result.tint[1]=paint.green;result.tint[2]=paint.blue;result.tint[3]=paint.alpha;
    result.style[0]=(shadow?layer.shadow->radius:layer.backdrop->radius)*t.scale;result.style[1]=(shadow?layer.shadow->blur_radius:layer.backdrop->blur_radius)*t.scale;result.style[2]=shadow?0:layer.backdrop->border_width*t.scale;result.style[3]=shadow?1:0;
    if(!shadow){const auto border=layer.backdrop->border;result.border[0]=border.red;result.border[1]=border.green;result.border[2]=border.blue;result.border[3]=border.alpha;}return result;
}
}
