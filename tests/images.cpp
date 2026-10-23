#include <iiGUI/iiGUI.h>
#include "NativeText.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
using namespace ii;
namespace {
int assertions=0;
void check(bool value,const char* message) { ++assertions;if(!value) throw std::runtime_error(message); }
template<class T> void good(const T& value,const char* message) {
    if(!value) { if(value.error().cause) std::rethrow_exception(value.error().cause);throw std::runtime_error(std::string(message)+": "+value.error().message); }
    ++assertions;
}
std::shared_ptr<const gui::RasterImage> solid(std::uint8_t r,std::uint8_t g,std::uint8_t b,std::uint8_t a=255) {
    auto result=gui::rgba_image(1,1,{r,g,b,a});good(result,"immutable RGBA image");return *result;
}
int pixel(const gui::PixelBuffer& b,int x,int y,int c=0) { return b.rgba[(std::size_t(y)*b.width+x)*4+c]; }
void contracts() {
    static_assert(std::is_same_v<decltype(std::declval<const gui::RasterImage&>().rgba()),std::span<const std::uint8_t>>);
    check(!gui::rgba_image(0,1,{}),"zero size rejected");
    check(!gui::rgba_image(1,1,{1,2,3}),"truncated RGBA rejected");
    check(!gui::rgba_image(2049,1,{}),"image dimensions bounded");
    gui::CanvasFrame frame{.viewport={0,0,80,80}};
    frame.images.push_back({{1,1,16,16},solid(255,0,0)});
    check(gui::valid_canvas(frame),"image admitted through common canvas API");
    auto bad=frame;bad.images[0].image.reset();check(!gui::valid_canvas(bad),"null image rejected");
    bad=frame;bad.images[0].opacity=std::numeric_limits<float>::quiet_NaN();check(!gui::valid_canvas(bad),"NaN opacity rejected");
    bad=frame;bad.images[0].opacity=1.01f;check(!gui::valid_canvas(bad),"opacity range enforced");
    bad=frame;bad.images[0].tint=gui::Color{2,0,0,1};check(!gui::valid_canvas(bad),"linear tint range enforced");
    bad=frame;bad.images[0].bounds.width=0;check(!gui::valid_canvas(bad),"image bounds enforced");
    bad=frame;bad.overlays.resize(64);check(gui::valid_canvas(bad),"bounded popup/exit composition accepted");
    bad.overlays.resize(65);check(!gui::valid_canvas(bad),"overlay layer count bounded");
    bad=frame;bad.overlays.resize(1);bad.overlays[0].clip=gui::Rect{0,0,-1,10};check(!gui::valid_canvas(bad),"negative layer clip rejected");
    bad.overlays[0].clip=gui::Rect{0,0,0,0};check(gui::valid_canvas(bad),"empty layer clip is valid hidden content");
    bad=frame;bad.overlays.resize(1);bad.overlays[0].images=bad.images;bad.overlays[0].images[0].image.reset();
    check(!gui::valid_canvas(bad),"overlay has the same validation as base content");
    gui::detail::ForegroundKey key;auto view=gui::detail::canvas_layers(frame)[0];
    key.assign(frame.viewport,2,view);check(key.matches(frame.viewport,2,view),"immutable foreground key retained");
    frame.panels.push_back({{0,0,10,10},{1,0,0,1},{1,0,0,1},0});
    check(key.matches(frame.viewport,2,gui::detail::canvas_layers(frame)[0]),"background changes preserve foreground key");
    frame.images[0].tint=gui::Color{0,1,0,1};
    check(!key.matches(frame.viewport,2,gui::detail::canvas_layers(frame)[0]),"tint changes invalidate foreground key");
}
void svg() {
    constexpr auto clipped=R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32"><defs><clipPath id="c"><circle cx="16" cy="16" r="14"/></clipPath><linearGradient id="g"><stop stop-color="red"/><stop offset="1" stop-color="blue"/></linearGradient></defs><rect width="32" height="32" clip-path="url(#c)" fill="url(#g)"/></svg>)svg";
    auto image=gui::svg_image(clipped,32,32);good(image,"native SVG clips and gradient");
    const auto p=(*image)->rgba();auto at=[&](int x,int y,int c){return p[(y*32+x)*4+c];};
    check(at(0,0,3)==0 && at(16,16,3)==255,"SVG clip preserves transparent corners");
    check(at(6,16,0)>at(25,16,0) && at(6,16,2)<at(25,16,2),"gradient varies across clipped geometry");
    auto mask=gui::svg_image(R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32"><defs><mask id="m"><rect width="16" height="32" fill="white"/></mask></defs><rect width="32" height="32" fill="red" mask="url(#m)"/></svg>)svg",32,32);
    good(mask,"native SVG luminance mask");check((*mask)->rgba()[(16*32+8)*4+3]>250 && (*mask)->rgba()[(16*32+24)*4+3]==0,"mask clips actual pixels");
    for(auto tag:{"filter","script","foreignObject","animate","animateTransform","set"}) {
        const auto doc=std::string("<svg width='10' height='10'><")+tag+"/></svg>";
        auto result=gui::svg_image(doc);check(!result && result.error().code==core::ErrorCode::unsupported,"unsupported SVG semantics fail explicitly");
    }
    good(gui::svg_image("<svg width='10' height='10'><!-- <filter/> --><rect width='10' height='10'/></svg>"),"comment is not interpreted as an element");
    check(!gui::svg_image("<svg"),"malformed SVG rejected");
    check(!gui::svg_image("<svg width='0' height='10'/>"),"nonpositive intrinsic size rejected");
    check(!gui::svg_image("<svg width='10' height='10'/>",0,10),"invalid SVG output rejected");
    check(!gui::svg_image(std::string(1024*1024+1,'x')),"SVG byte budget enforced");
}
void composition() {
    gui::CanvasFrame frame{.viewport={0,0,8,8}};
    frame.images={{{0,0,8,8},solid(255,0,0,128)},{{0,0,8,8},solid(0,0,255,128)}};
    auto rendered=gui::detail::rasterize_foreground(frame.viewport,gui::detail::canvas_layers(frame)[0],1);
    check(std::abs(pixel(rendered,4,4,3)-192)<=1,"foreground alpha is source-over");
    check(std::abs(pixel(rendered,4,4,0)-156)<=2 && std::abs(pixel(rendered,4,4,2)-213)<=2,"image blending occurs in linear light");
    frame.images={{{-4,-4,8,8},solid(255,0,0),gui::Color{0,1,0,1},.5f}};
    rendered=gui::detail::rasterize_foreground(frame.viewport,gui::detail::canvas_layers(frame)[0],2);
    check(pixel(rendered,2,2,0)==0 && pixel(rendered,2,2,1)==255 && std::abs(pixel(rendered,2,2,3)-128)<=1,"tint replaces color while preserving coverage and opacity");
    check(pixel(rendered,12,12,3)==0,"image clips to its bounds on a HiDPI surface");
}
void gpu(gui::Renderer renderer) {
    gui::Host host({.title="iiGUI own image/layer/cache verification"});
    gui::CanvasFrame frame{.viewport={10,10,96,96},.background={0,0,0,1}};frame.renderer=renderer;
    frame.images={{{16,16,64,64},solid(255,0,0)}};
    frame.overlays.push_back({.panels={{{28,28,40,40},{0,1,0,1},{0,1,0,1},0}},.images={{{38,38,16,16},solid(0,0,255)}}});
    good(host.run({.opened=[&] {
        good(host.set_canvas(frame),"image frame admitted");auto captured=host.capture_canvas();good(captured,"GPU image capture");
        const auto scale=host.snapshot()->backing_scale;
        auto at=[&](int x,int y,int c){return pixel(*captured,int(x*scale),int(y*scale),c);};
        check(at(20,20,0)>250 && at(32,32,1)>250 && at(44,44,2)>250,"overlays cover base foreground; overlay foreground covers its panel");
        auto info=host.graphics_info();good(info,"GPU cache statistics");
        if(renderer==gui::Renderer::vulkan) check(info->api==gui::GraphicsApi::vulkan,"explicit Vulkan image path");
        check(info->foreground_rasterizations==2,"each changed layer rasterizes once");
        check(info->foreground_texture_bytes==std::uint64_t(captured->width)*captured->height*8,"foreground VRAM accounted per active layer");
        frame.overlays[0].panels[0].top={1,1,0,1};frame.overlays[0].panels[0].bottom={1,1,0,1};
        good(host.set_canvas(frame),"overlay background changes");captured=host.capture_canvas();good(captured,"updated background capture");
        check(at(32,32,0)>250 && at(32,32,1)>250,"background-only change reaches GPU");
        check(host.graphics_info()->foreground_rasterizations==2,"background-only updates reuse both foreground textures");
        frame.overlays[0].images[0].tint=gui::Color{1,0,1,1};good(host.set_canvas(frame),"icon tint changes");
        captured=host.capture_canvas();good(captured,"tinted GPU image");
        check(at(44,44,0)>250 && at(44,44,2)>250,"tint reaches GPU");
        check(host.graphics_info()->foreground_rasterizations==3,"only changed foreground rerasterizes");
        frame.overlays[0].clip=gui::Rect{40,40,8,8};good(host.set_canvas(frame),"hardware layer clip admitted");
        captured=host.capture_canvas();good(captured,"clipped GPU layer");
        check(at(32,32,0)>250 && at(32,32,1)<5 && at(44,44,2)>250 && at(50,44,2)<5,"layer clip must cover both panels and foreground images");
        check(host.graphics_info()->foreground_rasterizations==3,"clip-only update rerasterized cached foreground");
        frame.overlays.clear();good(host.set_canvas(frame),"popup dismissed");good(host.capture_canvas(),"base content restored");
        check(host.graphics_info()->foreground_texture_bytes==std::uint64_t(captured->width)*captured->height*4,"dismissal releases overlay foreground storage");
        good(host.request_close(),"image host closes");
    }}),"native image rendering lifecycle");
}
}
int main(int argc,char** argv) {
    try { const std::string which=argc>1?argv[1]:"contracts";
        if(which=="contracts") contracts();else if(which=="svg") svg();else if(which=="composition") composition();
        else if(which=="native") gpu(gui::Renderer::native);else if(which=="vulkan") gpu(gui::Renderer::vulkan);else throw std::runtime_error("unknown case");
        std::cout<<"IMAGE_OK "<<which<<" assertions="<<assertions<<'\n';return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
