#pragma once
#include <iiGUI/NativeText.h>
#include <iiGUI/Image.h>
#include <bit>
namespace ii::gui::detail {
// The plan is already validated and ordered with dependencies first. A cached
// source is reusable only while its own drawing and every referenced source
// are unchanged. Target/scale changes invalidate all reachable sources.
inline std::array<bool,maximum_canvas_scenes> scene_rebuilds(
        const CanvasFrame& frame,const CanvasScenePlan& plan,const CanvasFrame* previous,
        const std::array<bool,maximum_canvas_scenes>& available,bool target_changed){
    std::array<bool,maximum_canvas_scenes> dirty{};
    for(std::size_t i=0;i<plan.count;++i){
        const auto index=plan.order[i];
        const auto& scene=frame.scenes[index];
        bool changed=target_changed||!previous||index>=previous->scenes.size()||
            !available[index]||scene!=previous->scenes[index];
        for(const auto& layer:scene.layers)if(layer.backdrop){
            const auto& material=*layer.backdrop;
            if(material.background_scene)changed=changed||dirty[*material.background_scene];
            if(material.content_scene)changed=changed||dirty[*material.content_scene];
        }
        dirty[index]=changed;
    }
    return dirty;
}
// Fits Vulkan's guaranteed 128-byte push-constant minimum. Float arrays also
// match the D3D11 constant-buffer layout. Flags: shadow 1, mask 2, underlay 4,
// capture 8, underlay tint 16, background scene 32, content scene 64.
// New fields are ignored by ordinary panel/text stages.
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
    if(!shadow){const auto& m=*layer.backdrop;if(m.underlay)flags|=4;if(m.capture)flags|=8;if(m.underlay_tint)flags|=16;if(m.background_scene)flags|=32;if(m.content_scene)flags|=64;const auto b=transform_rect(m.underlay_bounds,t);
        result.underlay[0]=b.x;result.underlay[1]=b.y;result.underlay[2]=b.width;result.underlay[3]=b.height;const auto color=m.border;result.border[0]=color.red;result.border[1]=color.green;result.border[2]=color.blue;result.border[3]=color.alpha;
        result.base[0]=m.base.red;result.base[1]=m.base.green;result.base[2]=m.base.blue;result.base[3]=m.base.alpha;result.highlight[0]=m.inner_highlight_opacity;result.highlight[1]=m.inner_highlight_offset_y*t.scale;if(m.underlay_tint){const auto color=*m.underlay_tint;result.highlight[2]=half_pair(color.red,color.green);result.highlight[3]=half_pair(color.blue,color.alpha);}
    }result.style[3]=float(flags);return result;
}

struct SceneGraphRequirements final {
    std::uint64_t scene_bytes=0,backdrop_bytes=0,shadow_bytes=0,asset_bytes=0;
    std::vector<std::shared_ptr<const RasterImage>> assets;
    [[nodiscard]] std::uint64_t total_bytes()const noexcept{return scene_bytes+backdrop_bytes+shadow_bytes+asset_bytes;}
};
// Conservative Metal effect accounting: full transparent source attachments,
// cropped material targets, shadow work targets and unique immutable images.
// The caller supplies already validated source frames in scene-index order.
inline std::optional<SceneGraphRequirements> scene_graph_requirements(const CanvasFrame& frame,const CanvasScenePlan& plan,
        std::span<const CanvasFrame> sources,double scale,std::uint32_t width,std::uint32_t height){
    if(plan.count>maximum_canvas_scenes||!std::isfinite(scale)||scale<=0||width<1||height<1||width>8192||height>8192)return std::nullopt;
    SceneGraphRequirements result;result.scene_bytes=std::uint64_t(width)*height*4*plan.count;
    const auto add=[&](const auto& image){if(image&&std::ranges::find(result.assets,image)==result.assets.end())result.assets.push_back(image);};
    bool transparent_image=false;
    const auto account=[&](const CanvasFrame& input)->bool{
        for(const auto& view:canvas_layers(input)){
            const auto& t=view.content_transform;
            if(view.backdrop){
                const auto& m=*view.backdrop;const auto bounds=transform_rect(m.bounds,t);const auto padding=m.blur_radius*t.scale;
                auto region=view;region.content_transform={};region.clip=Rect{bounds.x-padding,bounds.y-padding,bounds.width+2*padding,bounds.height+2*padding};
                const auto r=clip_pixels(region,scale,width,height);result.backdrop_bytes+=std::uint64_t(r.width)*r.height*(padding>0?12:4);
                add(m.mask);add(m.underlay);transparent_image=true;
            }
            if(view.shadow){
                const auto& s=*view.shadow;const auto bounds=transform_rect(s.bounds,t);const auto padding=std::ceil(s.blur_radius*t.scale*scale);
                const auto w=std::ceil(bounds.width*scale+2*padding),h=std::ceil(bounds.height*scale+2*padding);
                if(!std::isfinite(w)||!std::isfinite(h)||w<1||h<1||w>8192||h>8192)return false;
                result.shadow_bytes+=std::uint64_t(w)*std::uint64_t(h)*(s.blur_radius>0?3:1);add(s.mask);
            }
        }
        return true;
    };
    for(std::size_t i=0;i<plan.count;++i){const auto index=plan.order[i];if(index>=sources.size()||!account(sources[index]))return std::nullopt;}
    if(!account(frame))return std::nullopt;
    if(transparent_image)result.assets.push_back({});
    for(const auto& image:result.assets)result.asset_bytes+=image?image->rgba().size():4;
    return result;
}
}
