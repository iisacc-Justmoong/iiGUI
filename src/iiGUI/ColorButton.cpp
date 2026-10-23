#include "ColorButton.h"
#include <array>
#include <cmath>
#include <stdexcept>
#include <tuple>
namespace ii::gui::detail {
namespace {
double unit(double x){return std::clamp(x,0.,1.);}
// Interpolate premultiplied samples so transparent edge RGB cannot darken a ring.
std::array<double,4> sample(const RasterImage& image,double u,double v){
    const auto w=image.width(),h=image.height();const auto x=std::clamp(u*w-.5,0.,double(w-1)),y=std::clamp(v*h-.5,0.,double(h-1));
    const auto left=unsigned(x),top=unsigned(y),right=std::min(left+1,w-1),bottom=std::min(top+1,h-1);std::array<double,4> result{};
    for(auto [px,py,weight]:{std::tuple{left,top,(1-(x-left))*(1-(y-top))},std::tuple{right,top,(x-left)*(1-(y-top))},std::tuple{left,bottom,(1-(x-left))*(y-top)},std::tuple{right,bottom,(x-left)*(y-top)}}){
        const auto at=(std::size_t(py)*w+px)*4;const double alpha=image.rgba()[at+3]/255.;for(unsigned k=0;k<3;++k)result[k]+=image.rgba()[at+k]/255.*alpha*weight;result[3]+=alpha*weight;
    }return result;
}
}
std::shared_ptr<const RasterImage> ColorButton::paint(Size size,bool hovered,bool pressed,bool focused,WidgetColor primary){
    const Key key{size,current,primary,options.show_hue_ring,options.circular,hovered,pressed,focused,options.border_opacity};if(key_==key)return image_;
    const double scale=std::min(4.,2048/std::max(size.width,size.height));const auto width=std::max(1u,unsigned(std::ceil(size.width*scale))),height=std::max(1u,unsigned(std::ceil(size.height*scale)));std::vector<std::uint8_t> pixels(std::size_t(width)*height*4);
    const WidgetColor base=pressed?WidgetColor{.083941631,.083941631,.086313099,1}:hovered?WidgetColor{.1935568452,.1978072077,.1999323964,1}:WidgetColor{.1230252981,.1252803057,.1275353134,1};
    const auto cx=size.width/2,cy=size.height/2,corner=std::min({4.,cx,cy}),well=std::max(0.,std::min(size.width,size.height)-8),radius=well*18/28/2,border=focused?2.:.5;const WidgetColor stroke=focused?primary:WidgetColor{1,1,1,.2};
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
        if(options.circular){
            const double px=(x+.5)/scale-cx,py=(y+.5)/scale-cy;
            const double distance=std::hypot(px,py),radius=std::min(cx,cy)-1;
            const double coverage=unit(.5+(radius-distance)*scale);
            const double inner=unit(.5+(radius-2-distance)*scale);
            const WidgetColor rim=focused?primary:WidgetColor{1,1,1,1};
            const auto at=(std::size_t(y)*width+x)*4;
            const double color[]{current.red,current.green,current.blue},edge[]{rim.red,rim.green,rim.blue};
            const double rim_alpha=focused?1:options.border_opacity;
            const double alpha=rim_alpha*(1-inner)+current.alpha*inner;
            for(unsigned k=0;k<3;++k)pixels[at+k]=std::uint8_t(std::lround(unit(alpha>0?(edge[k]*rim_alpha*(1-inner)+color[k]*current.alpha*inner)/alpha:0)*255));
            pixels[at+3]=std::uint8_t(std::lround(coverage*alpha*255));continue;
        }
        const double px=(x+.5)/scale,py=(y+.5)/scale,qx=std::abs(px-cx)-(cx-corner),qy=std::abs(py-cy)-(cy-corner);
        const double distance=std::hypot(std::max(qx,0.),std::max(qy,0.))+std::min(std::max(qx,qy),0.)-corner,coverage=unit(.5-distance*scale);if(!coverage)continue;
        std::array<double,3> rgb{base.red,base.green,base.blue};const double edge=unit(.5+(border+distance)*scale)*stroke.alpha;
        const double channels[]{stroke.red,stroke.green,stroke.blue};for(unsigned k=0;k<3;++k)rgb[k]=rgb[k]*(1-edge)+channels[k]*edge;
        if(well>0&&options.show_hue_ring&&options.hue_ring&&std::abs(px-cx)<well/2&&std::abs(py-cy)<well/2){auto ring=sample(*options.hue_ring,(px-cx)/well+.5,(py-cy)/well+.5);for(unsigned k=0;k<3;++k)rgb[k]=rgb[k]*(1-ring[3])+ring[k];}
        const double circle=unit(.5+(radius-std::hypot(px-cx,py-cy))*scale)*current.alpha;const double color[]{current.red,current.green,current.blue};
        const auto at=(std::size_t(y)*width+x)*4;for(unsigned k=0;k<3;++k)pixels[at+k]=std::uint8_t(std::lround(unit(rgb[k]*(1-circle)+color[k]*circle)*255));pixels[at+3]=std::uint8_t(std::lround(coverage*255));
    }
    auto image=rgba_image(width,height,std::move(pixels));if(!image)throw std::runtime_error(image.error().message);image_=*image;key_=key;++rasterizations;return image_;
}
}
