#include <iiGUI/ColorPicker.h>
#include "ColorSurface.h"
#include <fstream>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
using namespace ii;
namespace {
void check(bool v,const char* message) { if(!v) throw std::runtime_error(message); }
template<class T> T take(core::Result<T> r) { if(!r) throw std::runtime_error(r.error().message);return std::move(*r); }
void good(core::Result<void> r) { if(!r) throw std::runtime_error(r.error().message); }
bool near(double a,double b) { return std::abs(a-b)<1e-6; }
void model() {
    gui::ColorPickerModel m;auto s=take(m.state());
    check(s.color==gui::WidgetColor{1,1,1,1} && s.previous_color==s.color && s.hex=="FFFFFF","Neutral model default differs");
    good(m.set_color({1,0,0,.4}));check(take(m.state()).previous_color==gui::WidgetColor{1,0,0,.4},"Implicit previous color did not follow initial assignment");
    good(m.edit_hsv(240,1,1));s=take(m.state());check(near(s.color.blue,1) && near(s.color.alpha,.4),"HSV edit lost blue or alpha");
    good(m.edit_hsv(240,0,.5));good(m.edit_color({.2,.2,.2,.4}));check(near(take(m.state()).hue,240),"Achromatic edit forgot the selected hue");
    good(m.set_previous_color({0,1,0,.25}));good(m.set_color({1,0,0,1}));good(m.cancel());s=take(m.state());
    check(s.color==gui::WidgetColor{0,1,0,.25},"Cancel did not restore explicit previous color");
    good(m.edit_color({0,0,1,.5}));good(m.accept());check(take(m.state()).previous_color==gui::WidgetColor{0,0,1,.5},"Accept did not update previous color");
    auto invalid=m.edit_hsv(std::numeric_limits<double>::quiet_NaN(),1,1);check(!invalid,"Nonfinite HSV accepted");
    invalid=m.set_color({2,0,0,1});check(!invalid,"Out-of-range encoded sRGB accepted");
    bool foreign=false;std::thread worker([&]{auto r=m.state();foreign=!r && r.error().code==core::ErrorCode::wrong_executor;});worker.join();check(foreign,"Foreign model read accepted");
}
void channels() {
    gui::ColorPickerModel m;good(m.edit_color({1,0,0,.25}));
    check(near(take(m.channel_value(gui::ColorChannel::red)),255) && near(take(m.channel_value(gui::ColorChannel::alpha)),25),"Channel units differ");
    good(m.edit_channel(gui::ColorChannel::cyan,100));good(m.edit_channel(gui::ColorChannel::black,100));
    good(m.edit_channel(gui::ColorChannel::magenta,40));check(near(take(m.channel_value(gui::ColorChannel::magenta)),40),"Black CMYK edit lost latent channel");
    good(m.edit_channel(gui::ColorChannel::black,0));check(near(take(m.state()).color.alpha,.25),"CMYK edit lost alpha");
    good(m.edit_channel(gui::ColorChannel::alpha,75));check(near(take(m.channel_value(gui::ColorChannel::magenta)),40),"Alpha edit rewrote CMYK selection");
    good(m.edit_hsv(360,1,1));check(near(take(m.state()).color.red,1) && near(take(m.state()).hue,360),"Hue endpoint did not wrap to red");
    auto ramp=take(m.channel_color(gui::ColorChannel::hue,1));check(ramp==gui::WidgetColor{1,0,0,1},"Hue ramp endpoint differs");
    check(!m.edit_channel(static_cast<gui::ColorChannel>(99),1),"Unknown channel accepted");
    good(m.edit_channel(gui::ColorChannel::gray,100));check(take(m.state()).hex=="000000","Grayscale edit is not subtractive K");
}
void hex() {
    gui::ColorPickerModel m;good(m.edit_color({1,1,1,.25}));
    check(take(m.edit_hex(" \t#00e71f\n")),"Trimmed six-digit HEX rejected");auto s=take(m.state());
    check(s.hex=="00E71F" && near(s.color.alpha,.25),"Six-digit HEX did not preserve alpha");
    check(take(m.edit_hex("11223380")) && near(take(m.state()).color.alpha,128.0/255),"Eight-digit HEX is not RRGGBBAA");
    const auto before=take(m.state());
    for(auto bad:{"#abc","#1122334455","11223g","# 112233","11223300junk"}) check(!take(m.edit_hex(bad)),"Malformed HEX accepted");
    check(take(m.state()).revision==before.revision && take(m.state()).color==before.color,"Invalid HEX mutated model");
}
void observers() {
    gui::ColorPickerModel m;int first=0,second=0;bool reentrant=false;
    auto receiver=std::make_shared<gui::ColorChangePort>([&](const gui::ColorPickerChange& c){++first;check(c.color_changed && c.edited,"Edited notification flags differ");auto r=m.edit_color({0,1,0,1});reentrant=!r && r.error().code==core::ErrorCode::busy;});
    auto other=std::make_shared<gui::ColorChangePort>([&](const auto&){++second;});
    good(m.observe(receiver));good(m.observe(receiver));good(m.observe(other));good(m.edit_color({1,0,0,1}));
    check(first==1 && second==1 && reentrant,"Explicit Core change receivers duplicated or recursed");
    receiver.reset();good(m.edit_color({0,0,1,1}));check(first==1 && second==2,"Expired receiver was retained");
    auto throwing=std::make_shared<gui::ColorChangePort>([](const auto&){throw std::runtime_error("receiver failure");});good(m.observe(throwing));
    auto r=m.edit_color({0,1,0,1});check(!r && take(m.state()).color==gui::WidgetColor{0,1,0,1} && second==3,"Notification failure rolled back a committed edit");
}
void png() {
    std::ifstream file(COLOR_FIXTURE,std::ios::binary);std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(file)),{});auto image=take(gui::png_image(data));
    check(image->width()==2 && image->height()==2,"PNG dimensions differ");auto p=image->rgba();
    check(p[0]==255 && p[1]==0 && p[5]==255 && p[10]==255 && p[15]==128 && p[12]==255,"PNG channels or alpha differ");
    check(!gui::png_image({}),"Empty PNG accepted");auto bad=data;bad[0]=0;check(!gui::png_image(bad),"Invalid PNG signature accepted");
    bad=data;bad[16]=0xff;check(!gui::png_image(bad),"Unbounded PNG dimensions reached the decoder");
    check(!gui::png_image(std::span(data).first(36)),"Truncated PNG accepted");
    std::ifstream oversized(COLOR_EXPANSION_FIXTURE,std::ios::binary);std::vector<std::uint8_t> expansion((std::istreambuf_iterator<char>(oversized)),{});auto rejected=gui::png_image(expansion);
    check(!rejected && rejected.error().code==core::ErrorCode::out_of_range,"PNG compressed expansion bypassed the codec allocation budget");
}
void domain() {
    auto model=std::make_shared<gui::ColorPickerModel>();good(model->edit_hsv(0,1,1));gui::detail::ColorSurface surface;surface.options.model=model;
    surface.options.field=gui::ColorField::saturation_brightness;gui::CanvasFrame f{.viewport={0,0,100,100}};surface.paint(f,{8,8,64,64},2);
    check(gui::valid_canvas(f) && surface.rasterizations==1 && f.images.size()==2,"Color domain draw list invalid");
    auto image=f.images[0].image;auto pixels=image->rgba();const auto right=(image->width()-1)*4;
    check(pixels[0]==255 && pixels[1]==255 && pixels[right]==255 && pixels[right+1]==0,"Saturation/value domain endpoints differ");
    good(model->edit_hsv(0,.3,.8));f.images.clear();surface.paint(f,{8,8,64,64},2);
    check(surface.rasterizations==1 && image==f.images[0].image,"Moving only the selection rerasterized a stable domain");
    good(model->edit_hsv(120,.3,.8));f.images.clear();surface.paint(f,{8,8,64,64},2);check(surface.rasterizations==2 && image!=f.images[0].image,"Hue-dependent domain was not invalidated");
    for(auto field:{gui::ColorField::wheel,gui::ColorField::hue_saturation,gui::ColorField::spectrum,gui::ColorField::hue_rail,gui::ColorField::brightness_rail}) {
        surface.options.field=field;f.images.clear();surface.paint(f,{8,8,80,80},2);check(gui::valid_canvas(f),"One of the six color domains produced invalid ink");
    }
    surface.options.field=gui::ColorField::wheel;check(!surface.begin({0,0,232,232},0,0),"Wheel corner captured input");check(surface.begin({0,0,232,232},116,216),"Wheel ring rejected input");good(surface.edit({0,0,232,232},116,216));check(near(take(model->state()).hue,0),"Wheel red origin is not at the bottom");
    check(surface.begin({0,0,232,232},200,116),"Wheel hue vertex rejected input");good(surface.edit({0,0,232,232},400,116));check(near(take(model->state()).saturation,1) && near(take(model->state()).brightness,1),"Triangle drag did not project onto its nearest edge");
}
}
int main(int argc,char** argv) {
    try {std::string name=argc>1?argv[1]:"model";if(name=="model")model();else if(name=="channels")channels();else if(name=="hex")hex();else if(name=="observers")observers();else if(name=="png")png();else if(name=="domain")domain();else throw std::runtime_error("Unknown color case");std::cout<<"COLOR_PICKER_OK "<<name<<'\n';}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
