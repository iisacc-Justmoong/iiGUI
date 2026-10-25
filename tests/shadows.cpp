#include <iiGUI/iiGUI.h>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
gui::CanvasFrame scene(){
    gui::CanvasFrame frame{.viewport={0,0,512,384},.background={1,1,1,1},.renderer=gui::Renderer::native};
    frame.overlays.emplace_back().shadow=gui::BoxShadow{.bounds={140,80,200,160},.radius=24,.blur_radius=32,.color={0,0,0,.28}};
    frame.overlays.emplace_back().panels.push_back({{140,64,200,160},{1,1,1,1},{1,1,1,1},24});
    return frame;
}
void validation(){
    auto frame=scene();check(gui::valid_canvas(frame),"Valid shadow rejected");
    frame.overlays[0].shadow->blur_radius=65;check(!gui::valid_canvas(frame),"Unbounded blur accepted");
    frame=scene();frame.overlays[0].shadow->radius=81;check(!gui::valid_canvas(frame),"Invalid corner radius accepted");
    frame=scene();frame.overlays[0].shadow->color.alpha=-1;check(!gui::valid_canvas(frame),"Invalid alpha accepted");
    frame=scene();frame.overlays[0].shadow->bounds.x=std::numeric_limits<float>::quiet_NaN();check(!gui::valid_canvas(frame),"Invalid shadow geometry accepted");
}
void pixels(){
    gui::Host host({.size={512,384},.minimum_size={320,220}});
    good(host.run({.opened=[&]{
        auto frame=scene();good(host.set_canvas(frame));auto image=take(host.capture_canvas());
        const auto scale=take(host.snapshot()).backing_scale;
        auto pixel=[&](const gui::PixelBuffer& p,double x,double y){return &p.rgba[(std::size_t(y*scale)*p.width+std::size_t(x*scale))*4];};
        check(pixel(image,240,238)[0]<250&&pixel(image,240,266)[0]>252&&pixel(image,240,50)[0]>252,"Blurred downward shadow or transparent padding missing");
        auto first=take(host.graphics_info());
        check(first.shadow_supported&&first.shadow_masks==1&&first.shadow_blurs==1&&first.shadow_texture_bytes>0,"Native shadow counters or storage invalid");
        good(host.set_canvas(frame));take(host.capture_canvas());
        check(take(host.graphics_info()).shadow_blurs==first.shadow_blurs,"Identical scene regenerated shadow");
        frame.overlays[0].shadow->bounds.x+=12;frame.overlays[0].shadow->bounds.y+=10;
        good(host.set_canvas(frame));take(host.capture_canvas());
        auto moved=take(host.graphics_info());
        check(moved.shadow_masks==first.shadow_masks&&moved.shadow_blurs==first.shadow_blurs,"Moving a shape rerasterized its invariant shadow mask");
        frame.overlays[0].shadow->color={1,0,0,.5};good(host.set_canvas(frame));auto tinted=take(host.capture_canvas());
        auto red=pixel(tinted,250,242);
        check(red[0]>245&&red[1]<red[0]-8&&std::abs(int(red[1])-red[2])<3,"Shadow tint did not composite in linear light");
        check(take(host.graphics_info()).shadow_blurs==first.shadow_blurs,"Changing shadow color regenerated geometry");
        frame.overlays[0].shadow->radius=40;good(host.set_canvas(frame));take(host.capture_canvas());
        check(take(host.graphics_info()).shadow_blurs==first.shadow_blurs+1,"Changed radius reused stale mask");
        auto count=take(host.graphics_info()).shadow_blurs;
        frame.background={.2,1,.2,1};good(host.set_canvas(frame));take(host.capture_canvas());
        check(take(host.graphics_info()).shadow_blurs==count,"Background change regenerated independent shadow");
        frame.overlays.clear();good(host.set_canvas(frame));take(host.capture_canvas());
        check(take(host.graphics_info()).shadow_texture_bytes==0,"Removed shadow retained working textures");
        good(host.request_close());
    }}));
}
void budget(){
    gui::Host host;
    good(host.run({.opened=[&]{
        gui::CanvasFrame frame{.viewport={0,0,1100,1100},.renderer=gui::Renderer::native};
        for(int i=0;i<16;++i)frame.overlays.emplace_back().shadow=gui::BoxShadow{.bounds={0,0,1100,1100},.radius=40,.blur_radius=64,.color={0,0,0,.28}};
        check(gui::valid_canvas(frame),"Budget test failed value validation");
        good(host.set_canvas(frame));auto image=host.capture_canvas();
        check(!image,"GPU working texture budget admitted an oversized scene");
        check(image.error().message.find("effect budget")!=std::string::npos,image.error().message.c_str());
        const auto info=take(host.graphics_info());
        constexpr std::uint64_t mib=1024*1024;
        check(info.effect_texture_budget_bytes>=64*mib&&info.effect_texture_budget_bytes<=256*mib,"Reported adaptive effect budget is outside its documented bounds");
        check(info.shadow_masks==0&&info.shadow_texture_bytes==0&&info.effect_image_texture_bytes==0,"Rejected scene allocated GPU effect working textures");
        good(host.request_close());
    }}));
}
void vulkan(){gui::Host host;good(host.run({.opened=[&]{auto f=scene();f.renderer=gui::Renderer::vulkan;good(host.set_canvas(f));auto shadow=take(host.capture_canvas());check(take(host.graphics_info()).shadow_supported,"Vulkan shadow capability is missing");f.overlays[0].shadow.reset();good(host.set_canvas(f));check(take(host.capture_canvas()).rgba!=shadow.rgba,"Vulkan ignored the shadow");good(host.request_close());}}));}
}
int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"pixels";if(name=="validation")validation();else if(name=="pixels")pixels();else if(name=="budget")budget();else if(name=="vulkan")vulkan();else throw std::runtime_error("Unknown shadow case");std::cout<<"SHADOW_GPU_OK "<<name<<'\n';}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
