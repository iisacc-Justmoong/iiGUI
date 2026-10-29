#include <iiGUI/NativeText.h>
#include <iiGUI/Image.h>
#include <array>

namespace ii::gui::detail {
namespace {
float decoded(float c){return c<=.04045f?c/12.92f:std::pow((c+.055f)/1.055f,2.4f);}
using Ink=std::array<float,4>;
void over(Ink& dst,Ink src){for(unsigned c=0;c<4;++c)dst[c]=src[c]+dst[c]*(1-src[3]);}
Ink ink(Color c){return {c.red*c.alpha,c.green*c.alpha,c.blue*c.alpha,c.alpha};}
Ink lerp(Ink a,Ink b,float f){for(unsigned c=0;c<4;++c)a[c]+=f*(b[c]-a[c]);return a;}
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
    for(auto& text:texts){text.bounds=transform_rect(text.bounds,t);if(text.clip)text.clip=transform_rect(*text.clip,t);text.font_size*=t.scale;text.line_height*=t.scale;text.color.alpha*=t.opacity;}
    for(auto& image:images){image.bounds=transform_rect(image.bounds,t);image.opacity*=t.opacity;image.radius*=t.scale;image.border_width*=t.scale;if(image.gradient)image.gradient->height*=t.scale;}
}
}
PixelBuffer rasterize_foreground(Rect viewport,CanvasLayerView layer,double scale){
    CanvasFrame text_frame;text_frame.viewport=viewport;text_frame.texts.assign(layer.texts.begin(),layer.texts.end());const auto [width,height]=canvas_pixels(text_frame,scale);
    auto out=!layer.images_before_text&&!layer.texts.empty()?rasterize_text(text_frame,scale):PixelBuffer{width,height,std::vector<std::uint8_t>(std::size_t(width)*height*4)};
    for(const auto& run:layer.images){
        const auto& image=*run.image;const auto bytes=image.rgba();const auto r=run.bounds;const auto uv=run.source;
        const auto x0=std::clamp(int(std::clamp(std::floor(r.x*scale),0.,double(width))),0,int(width)),y0=std::clamp(int(std::clamp(std::floor(r.y*scale),0.,double(height))),0,int(height));
        const auto x1=std::clamp(int(std::clamp(std::ceil((r.x+r.width)*scale),0.,double(width))),0,int(width)),y1=std::clamp(int(std::clamp(std::ceil((r.y+r.height)*scale),0.,double(height))),0,int(height));
        for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){
            const auto local_x=(x+.5)/scale-r.x,local_y=(y+.5)/scale-r.y;
            const auto mask=run.radius>0?coverage(local_x,local_y,r.width,r.height,run.radius,scale):1.f;if(mask<=0)continue;
            const auto sx=std::clamp((uv.x+local_x/r.width*uv.width)*image.width()-.5,0.,double(image.width()-1));
            const auto sy=std::clamp((uv.y+local_y/r.height*uv.height)*image.height()-.5,0.,double(image.height()-1));
            const auto ix=std::uint32_t(sx),iy=std::uint32_t(sy),jx=std::min(ix+1,image.width()-1),jy=std::min(iy+1,image.height()-1);const float fx=float(sx-ix),fy=float(sy-iy);Ink src{};
            for(int j=0;j<2;++j)for(int i=0;i<2;++i){const auto* p=bytes.data()+(std::size_t(j?jy:iy)*image.width()+(i?jx:ix))*4;const float w=(i?fx:1-fx)*(j?fy:1-fy),a=p[3]/255.f;for(unsigned c=0;c<3;++c)src[c]+=decoded(p[c]/255.f)*a*w;src[3]+=a*w;}
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
PixelBuffer rasterize_transformed_foreground(Rect viewport,CanvasLayerView layer,double scale){
    if(layer.content_transform.identity())return rasterize_foreground(viewport,layer,scale);
    std::vector<TextRun> texts(layer.texts.begin(),layer.texts.end());std::vector<ImageRun> images(layer.images.begin(),layer.images.end());transformed(texts,images,layer.content_transform);layer.texts=texts;layer.images=images;layer.content_transform={};return rasterize_foreground(viewport,layer,scale);
}
ForegroundRaster rasterize_foreground_region(Rect viewport,CanvasLayerView layer,double scale,bool apply_transform){
    std::vector<TextRun> texts(layer.texts.begin(),layer.texts.end());std::vector<ImageRun> images(layer.images.begin(),layer.images.end());if(apply_transform)transformed(texts,images,layer.content_transform);
    double left=viewport.width,top=viewport.height,right=0,bottom=0;
    const auto add=[&](Rect r){if(r.width<=0||r.height<=0)return;left=std::min(left,double(r.x));top=std::min(top,double(r.y));right=std::max(right,double(r.x)+r.width);bottom=std::max(bottom,double(r.y)+r.height);};
    for(const auto& text:texts)if(!text.text.empty()&&text.color.alpha>0)add(text.bounds);
    for(const auto& image:images)if(image.opacity>0)add(image.bounds);
    const auto [width,height]=canvas_pixels(CanvasFrame{.viewport=viewport},scale);
    const auto x=std::uint32_t(std::clamp(std::floor(left*scale),0.,double(width))),y=std::uint32_t(std::clamp(std::floor(top*scale),0.,double(height)));
    const auto r=std::uint32_t(std::clamp(std::ceil(right*scale),double(x),double(width))),b=std::uint32_t(std::clamp(std::ceil(bottom*scale),double(y),double(height)));
    if(r==x||b==y)return {PixelBuffer{1,1,std::vector<std::uint8_t>(4)},PixelClip{0,0,1,1}};
    const float ox=float(x/scale),oy=float(y/scale);
    for(auto& text:texts){text.bounds.x-=ox;text.bounds.y-=oy;if(text.clip){text.clip->x-=ox;text.clip->y-=oy;}}
    for(auto& image:images){image.bounds.x-=ox;image.bounds.y-=oy;}
    layer.texts=texts;layer.images=images;layer.content_transform={};auto pixels=rasterize_foreground({0,0,float((r-x)/scale),float((b-y)/scale)},layer,scale);const PixelClip region{x,y,pixels.width,pixels.height};return {std::move(pixels),region};
}
}
