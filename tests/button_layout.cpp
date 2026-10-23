#include <iiGUI/iiGUI.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
bool near(double a,double b){return std::abs(a-b)<.01;}
std::shared_ptr<const gui::RasterImage> solid(bool red){std::vector<std::uint8_t> pixels(4*4*4,255);for(unsigned i=0;i<16;++i){pixels[i*4]=red?255:0;pixels[i*4+1]=0;pixels[i*4+2]=red?0:255;}return take(gui::rgba_image(4,4,std::move(pixels)));}
void run(const std::string& name){
    gui::Host host({.size={300,200}});gui::WidgetMount mount;gui::Widget button;unsigned clicks=0;
    gui::ButtonOptions options{.text="",.tone=gui::ButtonTone::normal,.icon=solid(true),.indicator=solid(false),.icon_size=18,.icon_mode=true,.height=22,.left_padding=4,.right_padding=2,.content_spacing=-2,.indicator_size=18};
    good(host.run({.opened=[&]{
        mount=take(host.mount_widgets([&](auto& b)->core::Result<void>{auto row=take(b.hstack({},"root",{.spacing=0,.alignment=gui::StackAlignment::leading}));button=take(b.button(row,"button",options,[&]{++clicks;}));return {};}));
        auto snapshot=take(button.snapshot());check(near(snapshot.implicit_size.width,40)&&near(snapshot.bounds.height,22),"Asymmetric icon/indicator metrics were not used");
        if(name=="spacing"){options.content_spacing=4;good(button.set_button_options(options));check(near(take(button.snapshot()).implicit_size.width,46),"Content spacing was not included in natural width");}
        else if(name=="retained"){good(button.activate());options.width=80;options.left_padding=8;good(button.set_button_options(options));good(button.activate());check(clicks==2&&near(take(button.snapshot()).bounds.width,80),"Explicit width replacement lost control identity or callbacks");}
        else if(name=="validation"){auto invalid=options;invalid.content_spacing=-257;check(!button.set_button_options(invalid),"Unbounded negative spacing accepted");invalid=options;invalid.left_padding=std::numeric_limits<double>::quiet_NaN();check(!button.set_button_options(invalid),"NaN left inset accepted");invalid=options;invalid.width=0;check(!button.set_button_options(invalid),"Zero explicit button width accepted");check(near(take(button.snapshot()).implicit_size.width,40),"Invalid replacement changed existing button metrics");}
        else if(name=="pixels"){auto image=take(host.capture_canvas());const auto r=take(button.snapshot()).bounds;const auto color=[&](double x,double y,unsigned channel){const auto px=std::size_t((r.x+x)*image.width/300),py=std::size_t((r.y+y)*image.height/200);return image.rgba[(py*image.width+px)*4+channel];};check(color(6,11,0)>200&&color(6,11,2)<50&&color(30,11,2)>200&&color(30,11,0)<50,"Icon/indicator ink did not follow asymmetric content placement");}
        else if(name!="asymmetry")throw std::runtime_error("Unknown button layout case");
        good(host.request_close());
    }}));
}
}
int main(int argc,char** argv){try{run(argc>1?argv[1]:"asymmetry");std::cout<<"IIGUI_BUTTON_LAYOUT_OK\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
