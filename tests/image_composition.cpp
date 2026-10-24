#include <iiGUI/iiGUI.h>
#include <iiGUI/Text.h>
#include "../src/iiGUI/NativeText.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
template<class T>T take(core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
std::shared_ptr<const gui::RasterImage> solid(std::uint8_t r,std::uint8_t g,std::uint8_t b,std::uint8_t a=255){return take(gui::rgba_image(1,1,{r,g,b,a}));}
unsigned at(const gui::PixelBuffer& b,unsigned x,unsigned y,unsigned c){return b.rgba[(std::size_t(y)*b.width+x)*4+c];}
void pure(const std::string& name){
    gui::CanvasFrame frame{.viewport={0,0,16,16}};frame.images={{{0,0,16,16},solid(255,0,0)}};
    if(name=="crop"){std::vector<std::uint8_t> rgba{255,0,0,255,0,255,0,255,0,0,255,255,255,255,0,255};frame.images[0].image=take(gui::rgba_image(4,1,rgba));frame.images[0].source={.25,0,.5,1};auto pixels=gui::detail::rasterize_foreground(frame.viewport,gui::detail::canvas_layers(frame)[0],1);check(at(pixels,4,8,1)>200&&at(pixels,4,8,0)<80&&at(pixels,11,8,2)>200,"Normalized source crop did not sample the middle image region");}
    else if(name=="mask"||name=="border"){frame.images[0].radius=8;if(name=="border"){frame.images[0].border_width=2;frame.images[0].border={0,1,0,1};}auto pixels=gui::detail::rasterize_foreground(frame.viewport,gui::detail::canvas_layers(frame)[0],2);check(at(pixels,0,0,3)==0&&at(pixels,16,16,0)>250,"Circular mask erased center ink or leaked corners");if(name=="border")check(at(pixels,2,16,1)>200&&at(pixels,2,16,0)<80,"Border was hidden by image ink");}
    else if(name=="gradient"){frame.images[0].image=solid(0,0,0,0);frame.images[0].gradient=gui::ImageGradient{.height=16};auto pixels=gui::detail::rasterize_foreground(frame.viewport,gui::detail::canvas_layers(frame)[0],1);check(at(pixels,8,0,3)<20&&at(pixels,8,15,3)>235,"Gradient did not composite over transparent image samples");}
    else if(name=="region"||name=="transform"){frame.viewport={0,0,800,600};frame.images[0].bounds={100,50,50,20};auto layer=gui::detail::canvas_layers(frame)[0];if(name=="transform")layer.content_transform={.scale=.5,.offset_x=10,.offset_y=20};auto raster=gui::detail::rasterize_foreground_region(frame.viewport,layer,2,name=="transform");check(raster.region.x==(name=="transform"?120u:200u)&&raster.region.y==(name=="transform"?90u:100u),"Foreground origin did not follow device coordinates");check(raster.pixels.width==(name=="transform"?50u:100u)&&raster.pixels.height==(name=="transform"?20u:40u)&&raster.pixels.rgba.size()<800*600,"Sparse foreground allocated a full viewport texture");check(at(raster.pixels,2,2,0)>250,"Cropped foreground lost its source ink");}
    else if(name=="cache"){auto layer=gui::detail::canvas_layers(frame)[0];gui::detail::ForegroundKey key;key.assign(frame.viewport,2,layer);layer.content_transform.offset_x=20;check(key.matches(frame.viewport,2,layer),"GPU-only motion invalidated untransformed foreground pixels");layer.images_before_text=true;check(!key.matches(frame.viewport,2,layer),"Foreground order was omitted from the cache key");key.assign(frame.viewport,2,layer,true);layer.content_transform.offset_x=21;check(!key.matches(frame.viewport,2,layer,true),"CPU-transformed foreground ignored motion");}
    else if(name=="validation"){auto bad=frame;bad.images[0].source={-.1,0,1,1};check(!gui::valid_canvas(bad),"Out-of-bounds crop accepted");bad=frame;bad.images[0].radius=9;check(!gui::valid_canvas(bad),"Invalid mask radius accepted");bad=frame;bad.images[0].gradient=gui::ImageGradient{.height=std::numeric_limits<float>::quiet_NaN()};check(!gui::valid_canvas(bad),"NaN image gradient accepted");}
    else throw std::runtime_error("Unknown pure image case");
}
void window(const std::string& name){
    gui::Host host({.size={300,200}});gui::WidgetMount mount;gui::Widget root,shape,last;gui::WidgetPaintState painted;unsigned calls=0,clicked=0;bool rejected=false;
    good(host.run({.opened=[&]{
        if(name=="order"){gui::CanvasFrame frame{.viewport={0,0,64,32}};frame.texts={{.bounds={0,0,64,32},.text="X",.font_size=24}};frame.images={{{0,0,64,32},solid(255,0,0)}};auto legacy=gui::detail::rasterize_foreground(frame.viewport,gui::detail::canvas_layers(frame)[0],1);frame.images_before_text=true;auto ordered=gui::detail::rasterize_foreground(frame.viewport,gui::detail::canvas_layers(frame)[0],1);check(legacy.rgba!=ordered.rgba,"Text was hidden behind a foreground image");}
        else if(name=="metrics"){auto wide=take(gui::measure_text_layout({.text="WWW",.font_size=13})),narrow=take(gui::measure_text_layout({.text="iii",.font_size=13}));check(wide.width>narrow.width&&wide.height>0,"Public native text metrics did not use shaped glyph widths");check(!gui::measure_text_layout({.text="x",.font_size=0}),"Invalid text metrics admitted");}
        else{
            mount=take(host.mount_widgets([&](auto& b)->core::Result<void>{auto row=take(b.vstack({},"row",{.spacing=4,.alignment=gui::StackAlignment::leading}));root=take(b.action_surface(row,"action",{.width=80,.height=40,.checkable=true,.tab_focusable=name!="focus",.pointer_focusable=name!="focus"},[&]{++clicked;}));shape=take(b.absolute_stack(root,"shape",{80,40}));
                good(shape.set_paint_handler([&](const gui::WidgetPaintState& state)->core::Result<std::shared_ptr<const gui::WidgetDrawing>>{++calls;painted=state;if(name=="paint_error")return core::fail(core::ErrorCode::failed,"Intentional paint failure");if(name=="mutation"){auto result=root.set_checked(false);rejected=!result&&result.error().code==core::ErrorCode::busy;}auto d=std::make_shared<gui::WidgetDrawing>();d->panels={{{0,0,80,40},state.checked?gui::Color{0,1,0,1}:gui::Color{1,0,0,1},state.checked?gui::Color{0,1,0,1}:gui::Color{1,0,0,1},0}};return std::shared_ptr<const gui::WidgetDrawing>{d};},root));
                if(name=="slot"){good(b.set_default_content(root,shape));take(b.label(root,"content",{.text="Content"}));}
                last=take(b.button(row,"last",{.text="Last"}));if(name=="paint"){check(!shape.set_paint_handler({},last),"Non-ancestor paint owner accepted");}
                return {};
            }));
            if(name=="paint_error"){check(!host.capture_canvas(),"Paint error escaped the capture Result boundary");good(shape.set_paint_handler({}));}
            else{take(host.capture_canvas());check(calls>0&&painted.size==gui::Size{80,40},"Paint provider did not receive native local geometry");
                if(name=="paint"||name=="checked"||name=="mutation"){good(root.activate());take(host.capture_canvas());check(clicked==1&&painted.checked,"Checkable action state was not exposed to its paint provider");if(name=="mutation")check(rejected&&take(root.snapshot()).checked,"UI mutation during paint was not rejected");if(name=="checked"){good(root.set_checked(false));check(!take(root.action_options()).checked&&clicked==1,"Programmatic check state invoked a click or was not retained");}}
                else if(name=="slot")check(painted.content_children==1&&take(take(mount.find("content")).snapshot()).parent_id=="shape","Paint content count did not follow the default slot");
                else if(name=="focus"){good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::tab}));check(take(last.snapshot()).focused&&!take(root.snapshot()).focused,"No-tab action remained in keyboard traversal");auto r=take(root.snapshot()).bounds;good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=r.x+5,.y=r.y+5}));good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=r.x+5,.y=r.y+5}));check(!take(root.snapshot()).focused,"No-pointer-focus action stole focus");}
                else if(name=="retirement"){good(host.unmount_widgets());check(!shape.request_repaint(),"Paint handle survived its generation");}
                else throw std::runtime_error("Unknown paint case");
            }
        }
        good(host.request_close());
    }}));
}
}
int main(int argc,char** argv){try{std::string name=argc>1?argv[1]:"crop";if(name=="crop"||name=="mask"||name=="border"||name=="gradient"||name=="region"||name=="transform"||name=="cache"||name=="validation")pure(name);else window(name);std::cout<<"IIGUI_IMAGE_COMPOSITION_OK\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
