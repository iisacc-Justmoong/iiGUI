#include <iiGUI/Host.h>
#include <iiGUI/Image.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
}
int main(){try{using namespace ii::gui;Host host({.title="Vulkan material verification",.size={320,240},.renderer=Renderer::vulkan});good(host.run({.opened=[&]{
    CanvasFrame frame;frame.viewport={0,0,320,240};frame.renderer=Renderer::vulkan;frame.background={1,1,1,1};frame.panels.push_back({{160,0,160,240},{0,0,0,1},{0,0,0,1}});
    CanvasLayer material;material.backdrop=BackdropMaterial{.bounds={100,40,120,100},.radius=12,.blur_radius=18,.border={0,1,0,1},.border_width=2};material.shadow=BoxShadow{.bounds={70,170,60,32},.radius=8,.blur_radius=12,.color={0,0,0,.8}};frame.overlays.push_back(material);
    CanvasLayer transformed;transformed.content_transform={.scale=.5f,.opacity=.5f,.offset_x=230,.offset_y=40};transformed.panels.push_back({{0,0,40,40},{1,0,0,1},{1,0,0,1}});transformed.clip=Rect{0,0,40,40};frame.overlays.push_back(transformed);
    CanvasLayer picture;picture.content_transform={.scale=.5f,.opacity=.5f,.offset_x=270,.offset_y=40};picture.images.push_back({{0,0,40,40},take(rgba_image(1,1,{0,0,255,255}))});frame.overlays.push_back(picture);
    CanvasLayer previous;previous.panels.push_back({{220,160,50,40},{1,1,0,1},{1,1,0,1}});frame.overlays.push_back(previous);
    CanvasLayer sample;sample.backdrop=BackdropMaterial{.bounds={225,165,20,20},.source=BackdropSource::previous_layers};frame.overlays.push_back(sample);
    good(host.set_canvas(frame));auto pixels=take(host.capture_canvas());const auto scale=take(host.snapshot()).backing_scale;
    const auto pixel=[&](int x,int y){return pixels.rgba.data()+(std::size_t(y*scale)*pixels.width+std::size_t(x*scale))*4;};
    auto blurred=pixel(159,90);check(blurred[0]>100&&blurred[0]<240&&std::abs(int(blurred[0])-int(blurred[1]))<3,"Vulkan backdrop did not blur the previous GPU scene");
    auto border=pixel(101,90);check(border[1]>200&&border[0]<80,"Vulkan material border is missing");
    auto shadow=pixel(72,185);check(shadow[0]>50&&shadow[0]<250,"Vulkan box shadow is missing");
    auto red=pixel(235,45);check(red[0]>170&&red[0]<205&&red[1]<5&&red[2]<5,"Vulkan panel transform or opacity is incorrect");
    auto blue=pixel(275,45);check(blue[2]>170&&blue[2]<205&&blue[0]<5&&blue[1]<5,"Vulkan image transform or opacity is incorrect");
    auto prior=pixel(230,175);check(prior[0]>250&&prior[1]>250&&prior[2]<5,"Previous-layer backdrop sampled the content-only snapshot");
    const auto stats=take(host.graphics_info());check(stats.api==GraphicsApi::vulkan&&stats.hardware&&stats.backdrop_supported&&stats.shadow_supported&&stats.content_transform_supported,"Vulkan material capabilities are not reported");
    good(host.set_canvas(frame));auto again=take(host.capture_canvas());check(again.rgba==pixels.rgba,"Retained material presentation changed its pixels");good(host.request_close());
}}));std::cout<<"VULKAN_EFFECTS_OK gpu_backdrop=true shadow=true transforms=true previous_layers=true retained=true\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
