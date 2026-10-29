#include <iiGUI/ColorSurface.h>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace ii::gui::detail {
namespace {
struct Point final {double x,y;};
double unit(double v){return std::clamp(v,0.0,1.0);}
double hue(double h){h=std::fmod(h,360);return h<0?h+360:h;}
double saturation(double y){return unit(y/.45);}
double brightness(double y){return 1-unit((y-.43)/.57);}
double spectrum_position(double s,double v) {
    const double overlap=(s/.45+(1/.57-v)/.57)/(1/(.45*.45)+1/(.57*.57));
    const double candidates[]{std::clamp(.45*s,0.0,.43),std::clamp(overlap,.43,.45),std::clamp(1-.57*v,.45,1.0)};
    auto distance=[&](double y){return std::pow(saturation(y)-s,2)+std::pow(brightness(y)-v,2);};double result=candidates[0];
    for(auto y:candidates)if(distance(y)<distance(result))result=y;return result;
}
Point local(Rect r,double x,double y) {const auto scale=std::max(.001,std::min(r.width,r.height)/232.0);return {(x-r.x-(r.width-232*scale)/2)/scale,(y-r.y-(r.height-232*scale)/2)/scale};}
Point weights(Point p,bool clamp) {
    auto weights=[](Point q){const auto h=(q.x-74)/126;return Point{h,(q.y-43-73*h)/146};};auto w=weights(p);
    if(clamp && (w.x<0 || w.y<0 || w.x+w.y>1)) {
        constexpr Point vertices[]{ {74,43},{200,116},{74,189} };Point closest{};double best=1e100;
        for(int i=0;i<3;++i) {auto a=vertices[i],b=vertices[(i+1)%3];const auto dx=b.x-a.x,dy=b.y-a.y,t=unit(((p.x-a.x)*dx+(p.y-a.y)*dy)/(dx*dx+dy*dy));
            Point c{a.x+t*dx,a.y+t*dy};auto d=std::hypot(p.x-c.x,p.y-c.y);if(d<best){best=d;closest=c;}}
        w=weights(closest);
    }return {unit(w.x),unit(w.y)};
}
WidgetColor hsv(double h,double s,double v) {
    h=hue(h)/60;const int i=int(h);const double f=h-i,p=v*(1-s),q=v*(1-s*f),t=v*(1-s*(1-f));
    switch(i%6){case 0:return {v,t,p,1};case 1:return {q,v,p,1};case 2:return {p,v,t,1};case 3:return {p,q,v,1};case 4:return {t,p,v,1};default:return {v,p,q,1};}
}
std::shared_ptr<const RasterImage> selector() {
    static auto image=[] {std::vector<std::uint8_t> p(64*64*4);
        for(int y=0;y<64;++y)for(int x=0;x<64;++x){const auto d=std::hypot(x+.5-32,y+.5-32),edge=std::abs(d-24);const auto a=unit(6.5-edge);const bool white=edge<=3;const auto i=(y*64+x)*4;p[i]=p[i+1]=p[i+2]=white?255:0;p[i+3]=std::uint8_t(std::lround(a*(white?240:190)));}
        return *rgba_image(64,64,std::move(p));}();return image;
}
std::shared_ptr<const RasterImage> thumb() {
    static auto image=[] {std::vector<std::uint8_t> p(72*32*4);
        for(int y=0;y<32;++y)for(int x=0;x<72;++x){const auto qx=std::max(std::abs(x+.5-36)-29.0,0.0),qy=std::max(std::abs(y+.5-16)-5.0,0.0);auto d=std::hypot(qx,qy)-6;const auto a=unit(.5-d);const bool border=d> -1.5;const auto i=(y*72+x)*4;p[i]=p[i+1]=border?0:235;p[i+2]=border?0:239;p[i+3]=std::uint8_t(std::lround(a*(border?180:255)));}
        return *rgba_image(72,32,std::move(p));}();return image;
}
std::shared_ptr<const RasterImage> analytic_ring() {
    static auto image=[] {
        std::vector<std::uint8_t> pixels(448*448*4);
        for(int y=0;y<448;++y)for(int x=0;x<448;++x){const auto dx=(x+.5)/2-112,dy=(y+.5)/2-112,r=std::hypot(dx,dy);auto c=hsv(hue(90-std::atan2(dy,dx)*180/std::numbers::pi),1,1);const auto i=(y*448+x)*4;
            pixels[i]=std::uint8_t(std::lround(c.red*255));pixels[i+1]=std::uint8_t(std::lround(c.green*255));pixels[i+2]=std::uint8_t(std::lround(c.blue*255));pixels[i+3]=std::uint8_t(std::lround(unit(std::min(112-r,r-87)*2+.5)*255));}
        return *rgba_image(448,448,std::move(pixels));
    }();return image;
}
}
bool ColorSurface::begin(Rect r,double x,double y) {
    drag_part=0;
    if(options.field==ColorField::wheel){auto p=local(r,x,y);const auto radius=std::hypot(p.x-116,p.y-116),h=(p.x-74)/126,b=(p.y-43-73*h)/146;
        if(radius>=86 && radius<=116)drag_part=1;else if(h<0 || b<0 || h+b>1)drag_part=-1;}
    return drag_part>=0;
}
core::Result<void> ColorSurface::edit(Rect r,double px,double py) {
    if(drag_part<0)return {};auto model=options.model;auto state=model->state();if(!state)return std::unexpected(state.error());
    const auto field=options.field;double h=state->hue,s=state->saturation,v=state->brightness;const auto x=unit((px-r.x)/std::max(1.f,r.width)),y=unit((py-r.y)/std::max(1.f,r.height));
    switch(field) {
    case ColorField::wheel:{auto p=local(r,px,py);if(drag_part==1)h=hue(90-std::atan2(p.y-116,p.x-116)*180/std::numbers::pi);else {auto w=weights(p,true);v=1-w.y;s=v>0?unit(w.x/v):s;}break;}
    case ColorField::hue_saturation:h=x*360;s=1-y;break;case ColorField::saturation_brightness:s=x;v=1-y;break;
    case ColorField::spectrum:h=x*360;s=saturation(y);v=brightness(y);break;
    case ColorField::hue_rail:h=unit((py-r.y-3.5)/std::max(1.0,r.height-7.0))*360;break;
    case ColorField::brightness_rail:v=1-unit((py-r.y-3.5)/std::max(1.0,r.height-7.0));break;
    }return model->edit_hsv(h,s,v);
}
core::Result<void> ColorSurface::key(WidgetKey key,bool shift) {
    if(key!=WidgetKey::left && key!=WidgetKey::right && key!=WidgetKey::up && key!=WidgetKey::down)return {};
    auto model=options.model;auto state=model->state();if(!state)return std::unexpected(state.error());auto h=state->hue,s=state->saturation,v=state->brightness;
    const bool horizontal=key==WidgetKey::left || key==WidgetKey::right;const double d=key==WidgetKey::right || key==WidgetKey::up?1:-1;const auto field=options.field;
    if(field==ColorField::hue_rail)h=hue(h-d*(shift?10:1));else if(field==ColorField::brightness_rail)v+=d*(shift?.1:.01);
    else if(horizontal){if(field==ColorField::saturation_brightness)s+=d*(shift?.1:.01);else h=hue(h+d*(shift?10:1));}
    else if(field==ColorField::hue_saturation || (field==ColorField::wheel && shift))s+=d*.01;
    else if(field==ColorField::spectrum){auto y=unit(spectrum_position(s,v)-d*(shift?.05:.005));s=saturation(y);v=brightness(y);}
    else v+=d*.01;return model->edit_hsv(h,s,v);
}
void ColorSurface::paint(std::vector<ImageRun>& images,Rect r,double backing_scale) {
    auto value=options.model->state();if(!value)throw std::runtime_error(value.error().message);const auto& state=*value;
    const auto field=options.field;const bool wheel=field==ColorField::wheel,rail=field>=ColorField::hue_rail;
    const auto scale=std::min(r.width,r.height)/232.f;
    Rect target=wheel?Rect{r.x+(r.width-232*scale)/2+74*scale,r.y+(r.height-232*scale)/2+43*scale,126*scale,146*scale}:rail?Rect{r.x+(r.width-12)/2,r.y,12,r.height}:r;
    if(target.width<=0 || target.height<=0)return;
    const auto raster_scale=std::min(backing_scale,2048.0/std::max(target.width,target.height));
    const auto width=std::uint32_t(std::max(1.0,std::ceil(target.width*raster_scale))),height=std::uint32_t(std::max(1.0,std::ceil(target.height*raster_scale)));
    const auto dependency=wheel || field==ColorField::saturation_brightness?hue(state.hue):field==ColorField::hue_saturation?state.brightness:0;
    if(!domain || field!=cached_field || width!=cached_width || height!=cached_height || dependency!=cached_dependency) {
        std::vector<std::uint8_t> pixels(std::size_t(width)*height*4);
        for(std::uint32_t y=0;y<height;++y)for(std::uint32_t x=0;x<width;++x){const auto nx=width==1?0:double(x)/(width-1),ny=height==1?0:double(y)/(height-1);WidgetColor c;
            if(wheel){const auto black=ny-nx/2,v=unit(1-black);c=hsv(state.hue,v>0?unit(nx/v):0,v);const auto outside=std::max({-black,black+nx-1,-nx});c.alpha=unit(.5-outside*std::min(width,height));}
            else if(field==ColorField::hue_saturation)c=hsv(nx*360,1-ny,state.brightness);
            else if(field==ColorField::saturation_brightness)c=hsv(state.hue,nx,1-ny);
            else if(field==ColorField::spectrum)c=hsv(nx*360,saturation(ny),brightness(ny));
            else if(field==ColorField::hue_rail)c=hsv(ny*360,1,1);else c={1-ny,1-ny,1-ny,1};
            if(!wheel){const auto radius=std::min({4.0,double(target.width)/2,double(target.height)/2});const auto px=(x+.5)/width*target.width,py=(y+.5)/height*target.height;
                const auto dx=std::max(std::abs(px-target.width/2)-target.width/2+radius,0.0),dy=std::max(std::abs(py-target.height/2)-target.height/2+radius,0.0);c.alpha=unit(.5+(radius-std::hypot(dx,dy))*raster_scale);}
            const auto i=(std::size_t(y)*width+x)*4;pixels[i]=std::uint8_t(std::lround(c.red*255));pixels[i+1]=std::uint8_t(std::lround(c.green*255));pixels[i+2]=std::uint8_t(std::lround(c.blue*255));pixels[i+3]=std::uint8_t(std::lround(c.alpha*255));
        }
        auto image=rgba_image(width,height,std::move(pixels));if(!image)throw std::runtime_error(image.error().message);domain=*image;cached_field=field;cached_width=width;cached_height=height;cached_dependency=dependency;++rasterizations;
    }
    const auto opacity=options.enabled?1.f:.32f;
    if(wheel)images.push_back({{r.x+(r.width-232*scale)/2+4*scale,r.y+(r.height-232*scale)/2+4*scale,224*scale,224*scale},options.hue_ring?options.hue_ring:analytic_ring(),{},opacity});
    images.push_back({target,domain,{},opacity});
    auto cursor=[&](double x,double y,double diameter){images.push_back({{float(x-diameter/2),float(y-diameter/2),float(diameter),float(diameter)},selector(),{},opacity});};
    if(wheel){const auto radians=(90-state.hue)*std::numbers::pi/180;const auto ox=r.x+(r.width-232*scale)/2,oy=r.y+(r.height-232*scale)/2;
        cursor(ox+(116+100*std::cos(radians))*scale,oy+(116+100*std::sin(radians))*scale,14*scale);
        cursor(ox+(74*(1-state.saturation)*state.brightness+200*state.saturation*state.brightness+74*(1-state.brightness))*scale,
               oy+(43*(1-state.saturation)*state.brightness+116*state.saturation*state.brightness+189*(1-state.brightness))*scale,12*scale);
    }else if(rail){const auto p=field==ColorField::hue_rail?state.hue/360:1-state.brightness;const auto y=r.y+3.5+p*std::max(0.0,r.height-7.0);images.push_back({{r.x,float(y-4),r.width,8},thumb(),{},opacity});}
    else {const auto nx=field==ColorField::saturation_brightness?state.saturation:state.hue/360,ny=field==ColorField::hue_saturation?1-state.saturation:field==ColorField::saturation_brightness?1-state.brightness:spectrum_position(state.saturation,state.brightness);
        cursor(r.x+std::clamp(nx*r.width,6.0,std::max(6.0,r.width-6.0)),r.y+std::clamp(ny*r.height,6.0,std::max(6.0,r.height-6.0)),12);}
}
} // namespace ii::gui::detail
