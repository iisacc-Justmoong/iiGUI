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
// Resolution-aware effect storage, bounded for small views and 4K surfaces.
inline std::uint64_t effect_texture_budget(std::uint32_t w,std::uint32_t h){return std::clamp<std::uint64_t>(std::uint64_t(w)*h*32,64ull*1024*1024,256ull*1024*1024);}
struct PixelClip final { std::uint32_t x,y,width,height; };
inline PixelClip clip_pixels(CanvasLayerView l,double scale,std::uint32_t width,std::uint32_t height) {
    if(!l.clip) return {0,0,width,height};
    const auto r=transform_rect(*l.clip,l.content_transform);
    const auto left=std::uint32_t(std::clamp(std::floor(r.x*scale),0.0,double(width)));
    const auto top=std::uint32_t(std::clamp(std::floor(r.y*scale),0.0,double(height)));
    const auto right=std::uint32_t(std::clamp(std::ceil((double(r.x)+r.width)*scale),double(left),double(width)));
    const auto bottom=std::uint32_t(std::clamp(std::ceil((double(r.y)+r.height)*scale),double(top),double(height)));
    return {left,top,right-left,bottom-top};
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
