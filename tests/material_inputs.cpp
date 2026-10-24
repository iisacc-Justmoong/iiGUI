#include <iiGUI/iiGUI.h>
#ifndef IIGUI_INSTALLED_CONSUMER
#include "../src/iiGUI/GpuEffects.h"
#endif
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
std::shared_ptr<const gui::RasterImage> mask(){std::vector<std::uint8_t> pixels(8*8*4);for(unsigned y=0;y<8;++y)for(unsigned x=0;x<4;++x){auto at=(y*8+x)*4;pixels[at]=pixels[at+1]=pixels[at+2]=pixels[at+3]=255;}return take(gui::rgba_image(8,8,std::move(pixels)));}
void pure(std::string name){gui::CanvasFrame f{.viewport={0,0,256,192}};f.overlays={{.backdrop=gui::BackdropMaterial{.bounds={32,32,192,128},.capture=false}}};
 if(name=="validation"){auto& m=*f.overlays[0].backdrop;m.mask=mask();check(gui::valid_canvas(f),"Native alpha-mask material rejected");m.border_width=1;check(!gui::valid_canvas(f),"Unsupported custom-mask border was silently accepted");m.border_width=0;m.underlay=mask();check(!gui::valid_canvas(f),"Missing underlay rectangle accepted");m.underlay_bounds=m.bounds;check(gui::valid_canvas(f),"Valid preblur input rejected");m.inner_highlight_opacity=2;check(!gui::valid_canvas(f),"Invalid highlight opacity accepted");}
#ifndef IIGUI_INSTALLED_CONSUMER
 else if(name=="parameters"){auto& layer=f.overlays[0];auto& m=*layer.backdrop;m.mask=mask();m.underlay=mask();m.underlay_bounds={10,20,30,40};m.base={.1,.2,.3,.4};m.inner_highlight_offset_y=2;m.underlay_tint=gui::Color{1,0,0,1};layer.content_transform={.scale=.5,.offset_x=10,.offset_y=20};auto p=gui::detail::effect_parameters(gui::detail::canvas_layers(f)[1],f.viewport,2,false);check(sizeof(p)==128&&p.style[3]==22&&p.bounds[0]==26&&p.underlay[0]==15&&p.underlay[1]==30&&p.highlight[1]==1,"GPU push layout lost mask/input/transform flags");check(gui::detail::unit_half(1)==0x3c00&&gui::detail::unit_half(0)==0,"Half-color packing changed endpoint bits");}
#endif
 else if(name=="large_image"){auto image=take(gui::rgba_image(4096,1,std::vector<std::uint8_t>(4096*4,255)));check(image->width()==4096,"Wide native material image rejected");check(!gui::rgba_image(8193,1,{})&&!gui::svg_image("<svg width='1' height='1'/>",8192,8192),"Unbounded native image admitted");}
 else throw std::runtime_error("Unknown pure material case");
}
void window(std::string name){gui::Host host({.size={256,192}});gui::Widget root;gui::WidgetMount mount;unsigned calls=0;good(host.run({.opened=[&]{
 if(name.starts_with("frame_")){mount=take(host.mount_widgets([&](auto& b)->core::Result<void>{root=take(b.vstack({},"root"));good(root.set_frame_handler([&](runtime::Clock::time_point)->core::Result<bool>{++calls;if(name=="frame_retirement"){good(host.unmount_widgets());good(host.request_close());return false;}if(name=="frame_rearm"&&calls==1){good(root.request_frames());return false;}if(calls>=3){good(host.request_close());return false;}return true;}));return {};}));check(calls==0,"Inactive frame callback ran during construction");good(root.request_frames());return;}
 if(name=="provider"){mount=take(host.mount_widgets([&](auto& b)->core::Result<void>{auto parent=take(b.vstack({},"parent",{.padding=12,.alignment=gui::StackAlignment::leading}));root=take(b.absolute_stack(parent,"surface",{120,80}));good(root.set_paint_handler([](const auto& state)->core::Result<std::shared_ptr<const gui::WidgetDrawing>>{check(state.backing_scale>0,"Provider omitted native backing scale");auto d=std::make_shared<gui::WidgetDrawing>();d->backdrop=gui::BackdropMaterial{.bounds={0,0,120,80},.base={1,0,0,1},.capture=false};return std::shared_ptr<const gui::WidgetDrawing>{d};}));return {};}));take(host.capture_canvas());good(host.request_close());return;}
 gui::CanvasFrame f{.viewport={0,0,256,192},.background={0,0,1,1}};if(name.starts_with("vulkan_"))f.renderer=gui::Renderer::vulkan;const auto kind=name.starts_with("vulkan_")?name.substr(7):name;auto coverage=mask();
 if(kind=="shadow"){f.overlays={{.shadow=gui::BoxShadow{.bounds={32,32,192,128},.blur_radius=0,.color={0,0,0,1},.mask=coverage}}};}
 else{f.overlays={{.backdrop=gui::BackdropMaterial{.bounds={32,32,192,128},.base={1,0,0,1},.capture=false}}};auto& m=*f.overlays[0].backdrop;if(kind=="mask"||kind=="cache")m.mask=coverage;else if(kind=="underlay"){m.base={0,0,0,0};m.underlay=take(gui::rgba_image(1,1,{255,255,255,128}));m.underlay_bounds={32,32,192,128};m.underlay_tint=gui::Color{0,1,0,1};}else if(kind=="highlight"){m.base={0,0,0,1};m.inner_highlight_opacity=1;m.inner_highlight_offset_y=4;}else throw std::runtime_error("Unknown GPU material case");}
 good(host.set_canvas(f));auto pixels=take(host.capture_canvas());const auto sx=pixels.width/256.,sy=pixels.height/192.;const auto at=[&](double x,double y,unsigned c){return pixels.rgba[(std::size_t(y*sy)*pixels.width+std::size_t(x*sx))*4+c];};
 if(kind=="mask"){check(at(64,96,0)>240&&at(192,96,2)>240,"Native material alpha mask leaked or erased body");}
 else if(kind=="shadow")check(at(64,96,2)<20&&at(192,96,2)>240,"Custom shadow ignored its alpha geometry");
 else if(kind=="underlay")check(at(128,96,1)>100&&at(128,96,2)>100&&at(128,96,0)<20,"Transparent preblur input lost linear alpha/tint");
 else if(kind=="highlight")check(at(128,33,0)>200&&at(128,70,0)<20,"Inner highlight did not subtract shifted coverage");
 else if(kind=="cache"){auto first=take(host.graphics_info());good(host.set_canvas(f));take(host.capture_canvas());check(take(host.graphics_info()).effect_image_uploads==first.effect_image_uploads,"Identical mask uploaded again");f.overlays[0].backdrop->tint={0,1,0,.25};good(host.set_canvas(f));take(host.capture_canvas());check(take(host.graphics_info()).effect_image_uploads==first.effect_image_uploads,"Tint-only update uploaded immutable mask");}
 good(host.request_close());
 }}));if(name=="frame_stop"||name=="frame_rearm")check(calls==3,"Frame callback did not reuse/stop its Core source");if(name=="frame_retirement")check(calls==1&&!root.request_frames(),"Frame owner survived retirement");}
}
int main(int argc,char** argv){try{std::string name=argc>1?argv[1]:"validation";if(name=="validation"||name=="parameters"||name=="large_image")pure(name);else window(name);std::cout<<"IIGUI_MATERIAL_INPUTS_OK\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
