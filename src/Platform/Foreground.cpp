#include <iiGUI/NativeText.h>
#include <iiGUI/Image.h>
#include "../Graphics/GpuImage.h"
#include <array>

namespace ii::gui::detail {
namespace {
float decoded(float c){return c<=.04045f?c/12.92f:std::pow((c+.055f)/1.055f,2.4f);}
using Ink=std::array<float,4>;
void over(Ink& dst,Ink src){for(unsigned c=0;c<4;++c)dst[c]=src[c]+dst[c]*(1-src[3]);}
Ink ink(Color c){return {c.red*c.alpha,c.green*c.alpha,c.blue*c.alpha,c.alpha};}
Ink lerp(Ink a,Ink b,float f){for(unsigned c=0;c<4;++c)a[c]+=f*(b[c]-a[c]);return a;}
const std::array<float,256>& srgb_to_linear(){
    static const auto table=[] {std::array<float,256> result{};for(unsigned i=0;i<result.size();++i)result[i]=decoded(i/255.f);return result;}();
    return table;
}
Ink image_texel(const RasterImage& image,int x,int y){
    const auto* p=image.rgba().data()+(std::size_t(y)*image.width()+std::size_t(x))*4;
    if(p[3]==0)return {};
    const auto& linear=srgb_to_linear();const auto a=p[3]/255.f;
    return {linear[p[0]]*a,linear[p[1]]*a,linear[p[2]]*a,a};
}
Ink sample_image(const RasterImage& image,double sx,double sy,double rx,double ry){
    const auto bytes=image.rgba();Ink result{};
    if(rx<=1&&ry<=1){
        const auto ix=std::uint32_t(sx),iy=std::uint32_t(sy),jx=std::min(ix+1,image.width()-1),jy=std::min(iy+1,image.height()-1);
        const float fx=float(sx-ix),fy=float(sy-iy);
        for(int j=0;j<2;++j)for(int i=0;i<2;++i){const auto* p=bytes.data()+(std::size_t(j?jy:iy)*image.width()+(i?jx:ix))*4;const float w=(i?fx:1-fx)*(j?fy:1-fy),a=p[3]/255.f;for(unsigned c=0;c<3;++c)result[c]+=decoded(p[c]/255.f)*a*w;result[3]+=a*w;}
        return result;
    }
    // Widen the bilinear kernel by the source footprint when reducing an
    // image. This avoids aliasing without changing the linear-light contract.
    const auto& linear=srgb_to_linear();
    double total=0;std::array<double,4> sums{};
    const int x0=int(std::floor(sx-rx))+1,x1=int(std::ceil(sx+rx))-1;
    const int y0=int(std::floor(sy-ry))+1,y1=int(std::ceil(sy+ry))-1;
    for(int y=y0;y<=y1;++y){const auto wy=std::max(0.,1-std::abs(sy-y)/ry);const auto iy=std::clamp(y,0,int(image.height())-1);
        for(int x=x0;x<=x1;++x){const auto w=wy*std::max(0.,1-std::abs(sx-x)/rx);if(w<=0)continue;
            const auto ix=std::clamp(x,0,int(image.width())-1);const auto* p=bytes.data()+(std::size_t(iy)*image.width()+std::size_t(ix))*4;
            const auto a=p[3]/255.;for(unsigned c=0;c<3;++c)sums[c]+=linear[p[c]]*a*w;sums[3]+=a*w;total+=w;}}
    if(total>0)for(unsigned c=0;c<4;++c)result[c]=float(sums[c]/total);return result;
}
void blend(std::uint8_t* p,const Ink& src){
    if(src[3]<=0)return;const float a=p[3]/255.f;Ink result{decoded(p[0]/255.f)*a,decoded(p[1]/255.f)*a,decoded(p[2]/255.f)*a,a};over(result,src);
    for(unsigned c=0;c<3;++c)p[c]=std::uint8_t(std::clamp(std::lround(encoded_srgb(result[c]/result[3])*255),0l,255l));p[3]=std::uint8_t(std::clamp(std::lround(result[3]*255),0l,255l));
}
float coverage(double x,double y,double w,double h,double radius,double scale){
    if(w<=0||h<=0)return 0;const auto qx=std::abs(x-w*.5)-(w*.5-radius),qy=std::abs(y-h*.5)-(h*.5-radius);
    const auto distance=std::hypot(std::max(0.,qx),std::max(0.,qy))+std::min(std::max(qx,qy),0.)-radius;return float(std::clamp(.5-distance*scale,0.,1.));
}
void transformed(std::vector<TextRun>& texts,std::vector<ImageRun>& images,ContentTransform t){
    if(t.identity())return;
    for(auto& text:texts){text.bounds=transform_rect(text.bounds,t);if(text.clip)text.clip=transform_rect(*text.clip,t);text.font_size*=t.scale;text.line_height*=t.scale;text.letter_spacing*=t.scale;text.color.alpha*=t.opacity;}
    for(auto& image:images){image.bounds=transform_rect(image.bounds,t);image.opacity*=t.opacity;image.radius*=t.scale;image.border_width*=t.scale;if(image.gradient)image.gradient->height*=t.scale;}
}
PixelBuffer rasterize_foreground_impl(Rect viewport,CanvasLayerView layer,double scale,bool align_device_pixels){
    CanvasFrame text_frame;text_frame.viewport=viewport;text_frame.texts.assign(layer.texts.begin(),layer.texts.end());const auto [width,height]=canvas_pixels(text_frame,scale);
    auto out=!layer.images_before_text&&!layer.texts.empty()?rasterize_text(text_frame,scale):PixelBuffer{width,height,std::vector<std::uint8_t>(std::size_t(width)*height*4)};
    for(const auto& run:layer.images){
        const auto& image=*run.image;const auto r=run.bounds;const auto uv=run.source;
        const auto aligned=align_device_pixels?device_image_pixels(run,r,scale):std::nullopt;
        const double rx=std::clamp(double(uv.width)*image.width()/(r.width*scale),1.,double(image.width())),
                     ry=std::clamp(double(uv.height)*image.height()/(r.height*scale),1.,double(image.height()));
        const auto x0=int(std::clamp(aligned?double(aligned->x):std::floor(r.x*scale),0.,double(width)));
        const auto y0=int(std::clamp(aligned?double(aligned->y):std::floor(r.y*scale),0.,double(height)));
        const auto x1=int(std::clamp(aligned?double(aligned->x+aligned->width):std::ceil((r.x+r.width)*scale),0.,double(width)));
        const auto y1=int(std::clamp(aligned?double(aligned->y+aligned->height):std::ceil((r.y+r.height)*scale),0.,double(height)));
        for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){
            const auto local_x=(x+.5-(aligned?aligned->x:r.x*scale))/scale;
            const auto local_y=(y+.5-(aligned?aligned->y:r.y*scale))/scale;
            const auto mask=run.radius>0?coverage(local_x,local_y,r.width,r.height,run.radius,scale):1.f;if(mask<=0)continue;
            Ink src;
            if(aligned)src=image_texel(image,aligned->source_x+(x-aligned->x),aligned->source_y+(y-aligned->y));
            else {
                const auto sx=std::clamp((uv.x+local_x/r.width*uv.width)*image.width()-.5,0.,double(image.width()-1));
                const auto sy=std::clamp((uv.y+local_y/r.height*uv.height)*image.height()-.5,0.,double(image.height()-1));
                src=sample_image(image,sx,sy,rx,ry);
            }
            if(run.tint){const auto c=*run.tint;src={c.red*src[3]*c.alpha,c.green*src[3]*c.alpha,c.blue*src[3]*c.alpha,src[3]*c.alpha};}
            if(run.gradient&&run.gradient->height>0){const auto& g=*run.gradient;const auto t=float(std::clamp((local_y-(r.height-g.height))/g.height,0.,1.));auto paint=t<g.middle_position?lerp(ink(g.top),ink(g.middle),t/g.middle_position):lerp(ink(g.middle),ink(g.bottom),(t-g.middle_position)/(1-g.middle_position));over(src,paint);}
            if(run.border_width>0&&run.border.alpha>0){const auto b=run.border_width;const auto inside=coverage(local_x-b,local_y-b,r.width-2*b,r.height-2*b,std::max(0.f,run.radius-b),scale);auto paint=ink(run.border);for(auto& c:paint)c*=std::clamp(1-inside/mask,0.f,1.f);over(src,paint);}
            for(auto& c:src)c*=mask*run.opacity;blend(out.rgba.data()+(std::size_t(y)*width+x)*4,src);
        }
    }
    if(layer.images_before_text&&!layer.texts.empty()){
        auto text=rasterize_text(text_frame,scale);for(std::size_t i=0;i<text.rgba.size();i+=4){const auto* p=text.rgba.data()+i;if(!p[3])continue;const float a=p[3]/255.f;blend(out.rgba.data()+i,{decoded(p[0]/255.f)*a,decoded(p[1]/255.f)*a,decoded(p[2]/255.f)*a,a});}
    }
    return out;
}
}
PixelBuffer rasterize_foreground(Rect viewport,CanvasLayerView layer,double scale){
    return rasterize_foreground_impl(viewport,layer,scale,true);
}
PixelBuffer rasterize_transformed_foreground(Rect viewport,CanvasLayerView layer,double scale){
    if(layer.content_transform.identity())return rasterize_foreground(viewport,layer,scale);
    std::vector<TextRun> texts(layer.texts.begin(),layer.texts.end());std::vector<ImageRun> images(layer.images.begin(),layer.images.end());transformed(texts,images,layer.content_transform);layer.texts=texts;layer.images=images;return rasterize_foreground_impl(viewport,layer,scale,layer.content_transform.scale==1);
}
ForegroundRaster rasterize_foreground_region(Rect viewport,CanvasLayerView layer,double scale,bool apply_transform){
    std::vector<TextRun> texts(layer.texts.begin(),layer.texts.end());std::vector<ImageRun> images(layer.images.begin(),layer.images.end());if(apply_transform)transformed(texts,images,layer.content_transform);
    const bool align_device_pixels=!apply_transform||layer.content_transform.scale==1;
    const auto [width,height]=canvas_pixels(CanvasFrame{.viewport=viewport},scale);
    double left=width,top=height,right=0,bottom=0;
    const auto add=[&](double x,double y,double r,double b){left=std::min(left,x);top=std::min(top,y);right=std::max(right,r);bottom=std::max(bottom,b);};
    const auto add_logical=[&](Rect r){if(r.width>0&&r.height>0)add(r.x*scale,r.y*scale,(double(r.x)+r.width)*scale,(double(r.y)+r.height)*scale);};
    for(const auto& text:texts)if(!text.text.empty()&&text.color.alpha>0)add_logical(text.bounds);
    for(const auto& image:images)if(image.opacity>0){
        if(const auto p=align_device_pixels?device_image_pixels(image,image.bounds,scale):std::nullopt)
            add(p->x,p->y,p->x+p->width,p->y+p->height);
        else add_logical(image.bounds);
    }
    const auto x=std::uint32_t(std::clamp(std::floor(left),0.,double(width))),y=std::uint32_t(std::clamp(std::floor(top),0.,double(height)));
    const auto r=std::uint32_t(std::clamp(std::ceil(right),double(x),double(width))),b=std::uint32_t(std::clamp(std::ceil(bottom),double(y),double(height)));
    if(r==x||b==y)return {PixelBuffer{1,1,std::vector<std::uint8_t>(4)},PixelClip{0,0,1,1}};
    const float ox=float(x/scale),oy=float(y/scale);
    for(auto& text:texts){text.bounds.x-=ox;text.bounds.y-=oy;if(text.clip){text.clip->x-=ox;text.clip->y-=oy;}}
    for(auto& image:images){
        if(const auto p=align_device_pixels?device_image_pixels(image,image.bounds,scale):std::nullopt){
            image.bounds.x=float((p->x-double(x))/scale);
            image.bounds.y=float((p->y-double(y))/scale);
        }else{image.bounds.x-=ox;image.bounds.y-=oy;}
    }
    // Converting an integer extent through the logical float coordinate type must
    // not add a trailing pixel when the canvas rounds the result outward.
    const auto logical_extent=[scale](std::uint32_t n){
        const auto extent=float(n/scale);
        return extent*scale>n?std::nextafter(extent,0.f):extent;
    };
    layer.texts=texts;layer.images=images;
    auto pixels=rasterize_foreground_impl({0,0,logical_extent(r-x),logical_extent(b-y)},layer,scale,align_device_pixels);
    const PixelClip region{x,y,pixels.width,pixels.height};return {std::move(pixels),region};
}
}
