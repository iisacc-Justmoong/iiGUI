#pragma once
#include <iiGUI/NativeCanvas.h>
#include <algorithm>
#include <span>

namespace ii::gui::detail {
// Native shaping/wrapping uses logical units. Rasterization uses device pixels.
float measure_text(const TextRun&);
float measure_text_width(const TextRun&);
// Top-left, straight-alpha RGBA8 encoded sRGB, uploaded to an sRGB GPU texture.
PixelBuffer rasterize_text(const CanvasFrame&, double scale);
struct CanvasLayerView final {
    std::span<const Panel> panels;
    std::span<const TextRun> texts;
    std::span<const ImageRun> images;
    std::optional<Rect> clip=std::nullopt;
    std::optional<BackdropMaterial> backdrop=std::nullopt;
    std::optional<BoxShadow> shadow=std::nullopt;
    ContentTransform content_transform{};
    bool images_before_text=false;
    [[nodiscard]] bool foreground() const noexcept { return !texts.empty() || !images.empty(); }
};
inline std::vector<CanvasLayerView> canvas_layers(const CanvasFrame& f) {
    std::vector<CanvasLayerView> out{{f.panels,f.texts,f.images,std::nullopt,std::nullopt,std::nullopt,{},f.images_before_text}};
    for(const auto& l:f.overlays) out.push_back({l.panels,l.texts,l.images,l.clip,l.backdrop,l.shadow,l.content_transform,l.images_before_text});
    return out;
}
inline bool panel_only(const CanvasLayerView& layer) noexcept {
    return !layer.panels.empty()&&!layer.foreground()&&!layer.backdrop&&!layer.shadow;
}
// A layer's clip and transform only affect its own drawing. An otherwise empty
// overlay cannot alter adjacent layers and is not a batching boundary.
inline bool empty_drawing(const CanvasLayerView& layer) noexcept {
    return layer.panels.empty()&&!layer.foreground()&&!layer.backdrop&&!layer.shadow;
}
inline bool compatible_panels(const CanvasLayerView& a,const CanvasLayerView& b) noexcept {
    return panel_only(a)&&panel_only(b)&&a.clip==b.clip&&a.content_transform==b.content_transform;
}
inline CanvasLayerView canvas_layer_view(const CanvasLayer& layer) {
    return {layer.panels,layer.texts,layer.images,layer.clip,layer.backdrop,layer.shadow,layer.content_transform,layer.images_before_text};
}
// The base layer is always a separate batch because content backdrops capture it
// before the first overlay. Foreground, effect, clipping and transform changes
// are ordering boundaries; only adjacent panel-only layers can be combined.
inline std::size_t canvas_batch_count(std::span<const CanvasLayer> overlays) {
    std::size_t count=1;
    std::optional<CanvasLayerView> previous;
    for(const auto& layer:overlays){
        const auto view=canvas_layer_view(layer);
        if(empty_drawing(view))continue;
        if(!previous||!compatible_panels(*previous,view))++count;
        previous=view;
    }
    return count;
}
struct CanvasBatchLayers final {
    struct Range final {std::size_t begin=0,end=0,count=0;};
    std::vector<CanvasLayerView> originals,views;
    std::vector<Range> ranges;
    [[nodiscard]] std::span<const CanvasLayerView> panel_layers(std::size_t index) const noexcept {
        const auto& range=ranges[index];return {originals.data()+range.begin,range.end-range.begin};
    }
    [[nodiscard]] std::size_t panel_count(std::size_t index) const noexcept {return ranges[index].count;}
};
inline CanvasBatchLayers canvas_batch_layers(const CanvasFrame& frame) {
    CanvasBatchLayers result;result.originals=canvas_layers(frame);
    result.views.reserve(result.originals.size());result.ranges.reserve(result.originals.size());
    for(std::size_t i=0;i<result.originals.size();++i){
        const auto& view=result.originals[i];
        if(i>0&&empty_drawing(view))continue;
        if(i>1&&result.views.size()>1&&compatible_panels(result.views.back(),view)){
            auto& range=result.ranges.back();range.end=i+1;range.count+=view.panels.size();
        }else{
            result.views.push_back(view);result.ranges.push_back({i,i+1,view.panels.size()});
        }
    }
    return result;
}
// A retained GPU buffer owns a value snapshot, not a borrow of the frame.
// The snapshot changes only after a replacement GPU allocation succeeds.
struct PanelKey final {
    bool valid=false;
    ContentTransform transform{};
    std::vector<Panel> panels;
    [[nodiscard]] bool matches(std::span<const CanvasLayerView> layers) const {
        if(!valid||layers.empty()||transform!=layers.front().content_transform)return false;
        std::size_t offset=0;
        for(const auto& layer:layers){
            if(layer.panels.size()>panels.size()-offset||
               !std::equal(layer.panels.begin(),layer.panels.end(),panels.begin()+std::ptrdiff_t(offset)))return false;
            offset+=layer.panels.size();
        }
        return offset==panels.size();
    }
    void assign(std::span<const CanvasLayerView> layers) {
        std::size_t count=0;for(const auto& layer:layers)count+=layer.panels.size();
        std::vector<Panel> next;next.reserve(count);
        for(const auto& layer:layers)next.insert(next.end(),layer.panels.begin(),layer.panels.end());
        panels.swap(next);transform=layers.front().content_transform;valid=true;
    }
    void clear() noexcept {std::vector<Panel>{}.swap(panels);valid=false;}
};
inline std::vector<GpuPanel> pack_panel_batch(std::span<const CanvasLayerView> layers,std::size_t count) {
    std::vector<GpuPanel> result;result.reserve(count);
    for(const auto& layer:layers)append_packed_panels(result,layer.panels,layer.content_transform);
    return result;
}
// Internal offscreen input. Its transparent clear is never submitted as a
// presentable CanvasFrame; valid_canvas keeps requiring an opaque window.
inline CanvasFrame canvas_scene_frame(const CanvasScene& source){
    CanvasFrame result{.viewport=source.viewport,.background={0,0,0,0}};
    std::size_t begin=0;
    if(source.first_is_base){
        if(source.layers.empty())throw std::invalid_argument("Missing source base layer");
        const auto& base=source.layers.front();result.panels=base.panels;result.texts=base.texts;result.images=base.images;result.images_before_text=base.images_before_text;begin=1;
    }
    result.overlays.assign(source.layers.begin()+std::ptrdiff_t(begin),source.layers.end());return result;
}
// Resolution-aware effect storage, bounded for small views and 4K surfaces.
inline std::uint64_t effect_texture_budget(std::uint32_t w,std::uint32_t h){return std::clamp<std::uint64_t>(std::uint64_t(w)*h*32,64ull*1024*1024,256ull*1024*1024);}
struct PixelClip final { std::uint32_t x,y,width,height; };
inline PixelClip clip_pixels(CanvasLayerView l,double scale,std::uint32_t width,std::uint32_t height) {
    if(!l.clip) return {0,0,width,height};
    const auto r=transform_rect(*l.clip,l.content_transform);
    if(r.width<=0||r.height<=0)return {0,0,0,0};
    const auto left=std::uint32_t(std::clamp(std::floor(r.x*scale),0.0,double(width)));
    const auto top=std::uint32_t(std::clamp(std::floor(r.y*scale),0.0,double(height)));
    const auto right=std::uint32_t(std::clamp(std::ceil((double(r.x)+r.width)*scale),double(left),double(width)));
    const auto bottom=std::uint32_t(std::clamp(std::ceil((double(r.y)+r.height)*scale),double(top),double(height)));
    return {left,top,right-left,bottom-top};
}
inline PixelClip scene_clip_pixels(CanvasLayerView layer,std::optional<Rect> crop,double scale,std::uint32_t width,std::uint32_t height){
    auto clip=clip_pixels(layer,scale,width,height);
    if(!crop)return clip;
    if(crop->width==0||crop->height==0)return {0,0,0,0};
    CanvasLayerView boundary{};boundary.clip=crop;
    const auto source=clip_pixels(boundary,scale,width,height);
    const auto right=std::min(clip.x+clip.width,source.x+source.width),bottom=std::min(clip.y+clip.height,source.y+source.height);
    clip.x=std::max(clip.x,source.x);clip.y=std::max(clip.y,source.y);
    clip.width=right>clip.x?right-clip.x:0;clip.height=bottom>clip.y?bottom-clip.y:0;return clip;
}
struct ForegroundKey final {
    bool valid=false;
    Rect viewport;
    double scale=0;
    std::vector<TextRun> texts;
    std::vector<ImageRun> images;
    ContentTransform transform{};
    bool images_before_text=false;
    bool matches(Rect r,double s,CanvasLayerView l,bool transformed=false) const {
        return valid && viewport==r && scale==s && images_before_text==l.images_before_text && transform==(transformed?l.content_transform:ContentTransform{}) && std::ranges::equal(texts,l.texts) && std::ranges::equal(images,l.images);
    }
    void assign(Rect r,double s,CanvasLayerView l,bool transformed=false) { images_before_text=l.images_before_text;transform=transformed?l.content_transform:ContentTransform{};valid=true;viewport=r;scale=s;texts.assign(l.texts.begin(),l.texts.end());images.assign(l.images.begin(),l.images.end()); }
};
struct ForegroundRaster final {PixelBuffer pixels;PixelClip region;};
ForegroundRaster rasterize_foreground_region(Rect,CanvasLayerView,double,bool transformed=false);
PixelBuffer rasterize_foreground(Rect viewport,CanvasLayerView,double scale);
PixelBuffer rasterize_transformed_foreground(Rect viewport,CanvasLayerView,double scale);
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
