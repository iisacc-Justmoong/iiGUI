#include <iiGUI/iiGUI.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
gui::CanvasFrame scene(){
    gui::CanvasFrame frame{.viewport={0,0,256,160},.background={0,0,0,1},.renderer=gui::Renderer::native};
    auto& content=frame.overlays.emplace_back();content.content_transform={.origin_x=120,.origin_y=80,.scale=.5,.opacity=.5};
    content.panels.push_back({{40,30,160,100},{1,1,1,1},{1,1,1,1},16});
    content.images.push_back({{120,60,40,40},take(gui::rgba_image(1,1,{0,255,0,255})),{},1});
    return frame;
}
void validation(){
    auto frame=scene();check(gui::valid_canvas(frame),"Valid content transform rejected");
    frame.overlays[0].content_transform.scale=0;check(!gui::valid_canvas(frame),"Singular scale accepted");
    frame=scene();frame.overlays[0].content_transform.opacity=1.1;check(!gui::valid_canvas(frame),"Invalid opacity accepted");
    frame=scene();frame.overlays[0].content_transform.origin_x=40000;check(!gui::valid_canvas(frame),"Unbounded origin accepted");
}
void pixels(){
    gui::Host host;
    good(host.run({.opened=[&]{
        auto frame=scene();good(host.set_canvas(frame));auto first=take(host.capture_canvas());
        const auto scale=take(host.snapshot()).backing_scale;
        auto pixel=[&](const gui::PixelBuffer& image,double x,double y){return &image.rgba[(std::size_t(y*scale)*image.width+std::size_t(x*scale))*4];};
        auto panel=pixel(first,100,80),image=pixel(first,125,80);
        check(panel[0]>180&&panel[0]<195&&pixel(first,45,35)[0]<3&&pixel(first,80.5,55.5)[0]<3,"Scaled translucent panel/radius incorrect");
        check(image[0]>130&&image[0]<145&&image[1]>217&&image[1]<232,"Foreground failed independent scale and opacity composition");
        auto count=take(host.graphics_info()).foreground_rasterizations;
        frame.overlays[0].clip=gui::Rect{80,30,120,100};good(host.set_canvas(frame));auto clipped=take(host.capture_canvas());
        check(pixel(clipped,90,80)[0]<3&&pixel(clipped,110,80)[0]>180,"Clip did not follow its content transform");
        check(take(host.graphics_info()).foreground_rasterizations==count,"Transforming clip invalidated foreground raster");
        good(host.request_close());
    }}));
}
void edges(){
    gui::Host host;
    good(host.run({.opened=[&]{
        gui::CanvasFrame frame{.viewport={0,0,64,64},.background={0,0,0,1},.renderer=gui::Renderer::native};
        auto& layer=frame.overlays.emplace_back();layer.content_transform.scale=.92;
        layer.images.push_back({{8,8,16,16},take(gui::rgba_image(1,1,{255,0,0,255})),{},1});
        good(host.set_canvas(frame));auto image=take(host.capture_canvas());const auto scale=take(host.snapshot()).backing_scale;
        const auto x=std::size_t(std::floor(8*.92*scale)),y=std::size_t(16*.92*scale);
        const auto alpha=std::clamp((double(x)+.5)/.92-(8*scale-.5),0.,1.);
        const auto expected=255*(alpha<=.0031308?alpha*12.92:1.055*std::pow(alpha,1/2.4)-.055);
        const auto* pixel=&image.rgba[(y*image.width+x)*4];
        check(std::abs(pixel[0]-expected)<4&&pixel[1]<3&&pixel[2]<3,"Resampling straight alpha created a dark fringe");
        good(host.request_close());
    }}));
}
void reuse(){
    gui::Host host;
    good(host.run({.opened=[&]{
        gui::CanvasFrame frame{.viewport={0,0,256,160},.background={0,0,0,1},.renderer=gui::Renderer::native};
        frame.overlays.emplace_back().texts.push_back({{20,30,200,60},"Native retained text",{1,1,1,1},18});
        good(host.set_canvas(frame));take(host.capture_canvas());const auto first=take(host.graphics_info());
        for(int i=0;i<32;++i){frame.overlays[0].content_transform={.origin_x=128,.origin_y=80,.scale=float(.92+i*.004),.opacity=float(.4+i*.015)};good(host.set_canvas(frame));take(host.capture_canvas());}
        auto last=take(host.graphics_info());
        check(last.content_transform_supported&&last.scene_renders>=first.scene_renders+32&&last.foreground_rasterizations==first.foreground_rasterizations&&last.foreground_texture_bytes==first.foreground_texture_bytes,"Per-frame composition rerasterized or reallocated text");
        good(host.request_close());
    }}));
}
void vulkan(){gui::Host host;good(host.run({.opened=[&]{auto frame=scene();frame.renderer=gui::Renderer::vulkan;good(host.set_canvas(frame));auto transformed=take(host.capture_canvas());check(take(host.graphics_info()).content_transform_supported,"Vulkan transform capability is missing");frame.overlays[0].content_transform={};good(host.set_canvas(frame));check(take(host.capture_canvas()).rgba!=transformed.rgba,"Vulkan ignored the content transform");good(host.request_close());}}));}
void translation(){
    gui::Host host;good(host.run({.opened=[&]{
        auto frame=scene();auto& layer=frame.overlays[0];layer.content_transform={.offset_x=20,.offset_y=30};layer.clip=gui::Rect{40,30,160,100};
        good(host.set_canvas(frame));auto image=take(host.capture_canvas());auto scale=take(host.snapshot()).backing_scale;
        auto pixel=[&](double x,double y){return &image.rgba[(std::size_t(y*scale)*image.width+std::size_t(x*scale))*4];};
        check(pixel(45,35)[0]<3&&pixel(70,70)[0]>250&&pixel(160,110)[1]>250&&pixel(160,110)[0]<3,"Panel, image and clip did not share translation");
        const auto count=take(host.graphics_info()).foreground_rasterizations;
        for(int i=0;i<12;++i){layer.content_transform.offset_y=30+i*.35f;good(host.set_canvas(frame));take(host.capture_canvas());}
        check(take(host.graphics_info()).foreground_rasterizations==count,"Translation rerasterized retained foreground");
        layer.content_transform.offset_y=std::nanf("");check(!gui::valid_canvas(frame),"Nonfinite translation accepted");good(host.request_close());
    }}));
}
}
int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"pixels";if(name=="validation")validation();else if(name=="pixels")pixels();else if(name=="edges")edges();else if(name=="reuse")reuse();else if(name=="translation")translation();else if(name=="vulkan")vulkan();else throw std::runtime_error("Unknown content transform case");std::cout<<"CONTENT_TRANSFORM_GPU_OK "<<name<<'\n';}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
