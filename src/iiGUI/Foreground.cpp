#include "NativeText.h"
#include "Image.h"

namespace ii::gui::detail {
namespace {
float decoded(float c) { return c<=.04045f?c/12.92f:std::pow((c+.055f)/1.055f,2.4f); }
}
PixelBuffer rasterize_foreground(Rect viewport,CanvasLayerView layer,double scale) {
    CanvasFrame text_frame; text_frame.viewport=viewport; text_frame.texts.assign(layer.texts.begin(),layer.texts.end());
    const auto [width,height]=canvas_pixels(text_frame,scale);
    auto out=layer.texts.empty()?PixelBuffer{width,height,std::vector<std::uint8_t>(std::size_t(width)*height*4)}:rasterize_text(text_frame,scale);
    for(const auto& run:layer.images) {
        const auto& image=*run.image; const auto bytes=image.rgba(); const auto r=run.bounds;
        const auto x0=std::clamp(int(std::clamp(std::floor(r.x*scale),0.0,double(width))),0,int(width)), y0=std::clamp(int(std::clamp(std::floor(r.y*scale),0.0,double(height))),0,int(height));
        const auto x1=std::clamp(int(std::clamp(std::ceil((r.x+r.width)*scale),0.0,double(width))),0,int(width)), y1=std::clamp(int(std::clamp(std::ceil((r.y+r.height)*scale),0.0,double(height))),0,int(height));
        for(int y=y0;y<y1;++y) for(int x=x0;x<x1;++x) {
            const auto sx=std::clamp(((x+.5)/scale-r.x)/r.width*image.width()-.5,0.0,double(image.width()-1));
            const auto sy=std::clamp(((y+.5)/scale-r.y)/r.height*image.height()-.5,0.0,double(image.height()-1));
            const auto ix=std::uint32_t(sx),iy=std::uint32_t(sy),jx=std::min(ix+1,image.width()-1),jy=std::min(iy+1,image.height()-1);
            const float fx=float(sx-ix),fy=float(sy-iy); float src[4]{};
            for(int j=0;j<2;++j) for(int i=0;i<2;++i) {
                auto p=bytes.data()+(std::size_t(j?jy:iy)*image.width()+(i?jx:ix))*4;
                const float weight=(i?fx:1-fx)*(j?fy:1-fy),alpha=p[3]/255.f;
                for(int c=0;c<3;++c) src[c]+=decoded(p[c]/255.f)*alpha*weight;
                src[3]+=alpha*weight;
            }
            if(src[3]<=0) continue;
            float alpha=src[3]*run.opacity;
            if(run.tint) { const auto c=*run.tint; src[0]=c.red*src[3];src[1]=c.green*src[3];src[2]=c.blue*src[3];alpha*=c.alpha; }
            if(alpha<=0) continue;
            auto p=out.rgba.data()+(std::size_t(y)*width+x)*4; const auto dest_alpha=p[3]/255.f;
            const auto combined=alpha+dest_alpha*(1-alpha);
            for(int c=0;c<3;++c) {
                const auto value=(src[c]/src[3]*alpha+decoded(p[c]/255.f)*dest_alpha*(1-alpha))/combined;
                p[c]=std::uint8_t(std::clamp(std::lround(encoded_srgb(value)*255),0l,255l));
            }
            p[3]=std::uint8_t(std::clamp(std::lround(combined*255),0l,255l));
        }
    }
    return out;
}
} // namespace ii::gui::detail

namespace ii::gui::detail {
PixelBuffer rasterize_transformed_foreground(Rect viewport,CanvasLayerView layer,double scale){
    if(layer.content_transform.identity())return rasterize_foreground(viewport,layer,scale);
    const auto transform=layer.content_transform;std::vector<TextRun> texts(layer.texts.begin(),layer.texts.end());std::vector<ImageRun> images(layer.images.begin(),layer.images.end());
    for(auto& text:texts){text.bounds=transform_rect(text.bounds,transform);text.font_size*=transform.scale;text.line_height*=transform.scale;text.color.alpha*=transform.opacity;}
    for(auto& image:images){image.bounds=transform_rect(image.bounds,transform);image.opacity*=transform.opacity;}
    layer.texts=texts;layer.images=images;layer.content_transform={};return rasterize_foreground(viewport,layer,scale);
}
}
