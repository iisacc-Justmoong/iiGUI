#include <iiGUI/iiGUI.h>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ii;
namespace {
template<class T>T take(core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(core::Result<void> r){if(!r){if(r.error().cause)std::rethrow_exception(r.error().cause);throw std::runtime_error(r.error().message);}}
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
std::shared_ptr<const gui::RasterImage> ring(){
    static auto image=[] {std::vector<std::uint8_t> p(64*64*4);for(int y=0;y<64;++y)for(int x=0;x<64;++x){auto i=(y*64+x)*4;auto d=std::hypot(x+.5-32,y+.5-32);p[i+2]=255;p[i+3]=d>21&&d<31?255:0;}return take(gui::rgba_image(64,64,std::move(p)));}();return image;
}
void click(gui::WidgetMount m,gui::Rect r){good(m.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=r.x+r.width/2,.y=r.y+r.height/2}));good(m.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=r.x+r.width/2,.y=r.y+r.height/2}));}
void contracts(){
    gui::Host host;good(host.run({.opened=[&]{auto model=std::make_shared<gui::ColorPickerModel>();auto m=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{
        auto root=take(b.hstack({},"root"));for(auto size:{22.,28.,36.})take(b.color_button(root,"s"+std::to_string(int(size)),{.model=model,.hue_ring=ring(),.size=size}));
        check(!b.color_button(root,"null",{}),"Null model accepted");check(!b.color_button(root,"asset",{.model=model}),"Visible ring without image accepted");check(!b.color_button(root,"nan",{.model=model,.hue_ring=ring(),.size=std::numeric_limits<double>::quiet_NaN()}),"NaN size accepted");return {};}));
        for(auto size:{22.,28.,36.}){auto w=take(m.find("s"+std::to_string(int(size))));auto s=take(w.snapshot());check(s.kind==gui::WidgetKind::button&&s.color_button&&s.size==gui::Size{size,size}&&s.color_state&&s.accessibility_name=="Choose color","Color preview did not retain button semantics/natural size/model");}
        auto first=take(m.find("s22"));good(first.set_color_button_size(36));check(take(first.snapshot()).size==gui::Size{36,36},"Resizing did not remeasure native button");check(!first.set_color_button_size(500)&&!first.set_color_button_size(-1),"Invalid size mutation accepted");good(first.set_show_hue_ring(false));check(!take(first.snapshot()).show_hue_ring,"Ring visibility mutation ignored");good(host.request_close());}}));
}
void input(){
    gui::Host host;std::string order;gui::Widget button;good(host.run({.opened=[&]{auto m=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{button=take(b.color_button({},"button",{.model=std::make_shared<gui::ColorPickerModel>(),.hue_ring=ring()},{.clicked=[&]{order+='c';},.pressed=[&]{order+='p';},.released=[&]{order+='r';},.canceled=[&]{order+='x';}}));return {};}));
        click(m,take(button.snapshot()).bounds);check(order=="prc","Pointer lifecycle order differs");order.clear();good(button.focus());good(m.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::space}));good(m.dispatch_input({.kind=gui::WidgetInputKind::key_up,.key=gui::WidgetKey::space}));check(order=="prc","Space did not activate button");order.clear();auto r=take(button.snapshot()).bounds;good(m.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=r.x+3,.y=r.y+3}));good(button.set_enabled(false));good(m.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=r.x+3,.y=r.y+3}));good(button.activate());check(order=="px"&&!take(button.snapshot()).enabled,"Disabled capture/action was not canceled");good(host.request_close());}}));
}
std::array<unsigned,4> pixel(const gui::PixelBuffer& p,gui::Size logical,double x,double y){auto px=std::min(p.width-1,unsigned(x/logical.width*p.width)),py=std::min(p.height-1,unsigned(y/logical.height*p.height));auto i=(std::size_t(py)*p.width+px)*4;return {p.rgba[i],p.rgba[i+1],p.rgba[i+2],p.rgba[i+3]};}
void rendering(){
    gui::Host host({.size={320,240}});good(host.run({.opened=[&]{auto model=std::make_shared<gui::ColorPickerModel>();good(model->set_color({1,0,0,1}));gui::Widget button;auto m=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{button=take(b.color_button({},"button",{.model=model,.hue_ring=ring(),.size=36}));return {};}));auto r=take(button.snapshot()).bounds;auto logical=take(host.snapshot()).size;auto image=take(host.capture_canvas());auto center=pixel(image,logical,r.x+r.width/2,r.y+r.height/2),blue=pixel(image,logical,r.x+r.width/2,r.y+5);
        check(center[0]>240&&center[1]<10&&center[2]<10,"Current-color circle not rendered");auto idle=pixel(image,logical,r.x+8,r.y+r.height/2);check(idle[0]>=30&&idle[0]<=33,"Idle surface differs");good(m.dispatch_input({.kind=gui::WidgetInputKind::pointer_move,.x=r.x+r.width/2,.y=r.y+r.height/2}));image=take(host.capture_canvas());auto hover=pixel(image,logical,r.x+8,r.y+r.height/2);check(hover[0]>=48&&hover[0]<=51,"Hover surface differs");good(m.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=r.x+r.width/2,.y=r.y+r.height/2}));image=take(host.capture_canvas());auto pressed=pixel(image,logical,r.x+8,r.y+r.height/2);check(pressed[0]>=20&&pressed[0]<=23,"Pressed surface differs");good(m.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=r.x+r.width/2,.y=r.y+r.height/2}));check(blue[2]>180&&blue[0]<100,"Authored ring not painted");good(button.set_show_hue_ring(false));image=take(host.capture_canvas());auto hidden=pixel(image,logical,r.x+r.width/2,r.y+5);check(hidden[2]<100,"Hidden ring remains visible");good(button.focus());image=take(host.capture_canvas());auto focused=pixel(image,logical,r.x+1,r.y+18);check(focused[2]>focused[1]+25,"Focus accent border missing");good(button.set_enabled(false));image=take(host.capture_canvas());auto disabled=pixel(image,logical,r.x+r.width/2,r.y+r.height/2);check(disabled[0]<220&&disabled[0]>60,"Whole-button disabled opacity missing");check(take(host.graphics_info()).api==gui::GraphicsApi::metal,"Color preview did not reach native GPU");good(host.request_close());}}));
}
void cache(){
    gui::Host host;good(host.run({.opened=[&]{auto a=std::make_shared<gui::ColorPickerModel>(),b=std::make_shared<gui::ColorPickerModel>();gui::Widget button;auto m=take(host.mount_widgets([&](gui::WidgetBuilder&w)->core::Result<void>{button=take(w.color_button({},"button",{.model=a,.hue_ring=ring()}));return {};}));(void)take(host.capture_canvas());auto count=take(button.snapshot()).color_button_rasterizations;for(int i=0;i<5;++i)(void)take(host.capture_canvas());check(take(button.snapshot()).color_button_rasterizations==count,"Stable color button rerasterized");good(a->set_color({0,1,0,.5}));(void)take(host.capture_canvas());check(take(button.snapshot()).color_state->hex=="00FF00"&&take(button.snapshot()).color_button_rasterizations==count+1,"Shared model did not invalidate one preview");good(button.set_color_model(b));good(a->set_color({1,0,0,1}));check(take(button.snapshot()).color_state->hex=="FFFFFF"&&take(button.color_model())==b,"Old model still controls preview");check(!take(m.find("button")).set_color_field(gui::ColorField::wheel),"Color button accepted field mutation");good(host.request_close());}}));
}
void lifetime(){
    gui::Host host;gui::Widget stale;std::weak_ptr<gui::ColorPickerModel> weak;int clicks=0;good(host.run({.opened=[&]{auto model=std::make_shared<gui::ColorPickerModel>();weak=model;take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{stale=take(b.color_button({},"button",{.model=model,.hue_ring=ring()},{.clicked=[&]{++clicks;good(host.unmount_widgets());}}));return {};}));model.reset();good(stale.activate());check(clicks==1&&weak.expired()&&!stale.snapshot()&&!stale.set_show_hue_ring(false),"Retired color button/model survived unmount");good(host.request_close());}}));
}
}
void circular(){
    gui::Host host({.size={320,240}});good(host.run({.opened=[&]{
        auto model=std::make_shared<gui::ColorPickerModel>();good(model->set_color({1,0,0,1}));
        gui::Widget button,glyph,faint,compact;int clicked=0;
        auto m=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{auto root=take(b.hstack({},"root"));button=take(b.color_button(root,"circle",{.model=model,.size=28,.show_hue_ring=false,.circular=true}));faint=take(b.color_button(root,"faint",{.model=model,.size=28,.show_hue_ring=false,.circular=true,.border_opacity=.15}));check(!b.color_button(root,"invalid-border",{.model=model,.show_hue_ring=false,.border_opacity=2}),"Invalid border opacity accepted");compact=take(b.slider(root,"compact-slider",{.thumb_diameter=12,.track_height=4,.range_inset=6,.height=12}));check(!b.slider(root,"bad-height",{.height=1}),"Slider smaller than its thumb accepted");glyph=take(b.button(root,"shape",{.icon=ring(),.icon_name="rectangle",.icon_mode=true,.horizontal_padding=1},[&]{++clicked;}));return {};}));
        check(!button.set_button_icon(ring()),"Color swatch accepted ordinary icon mutation");
        good(glyph.set_button_icon(ring(),"ellipse"));good(glyph.activate());
        check(take(glyph.snapshot()).icon_name=="ellipse"&&clicked==1,"Icon replacement lost identity or input callback");
        check(!glyph.set_button_icon({}),"Null icon accepted");
        check(take(compact.snapshot()).bounds.height==12,"Explicit slider height was ignored");
        check(take(glyph.snapshot()).bounds.width==18,"Explicit icon horizontal padding was ignored");
        auto r=take(button.snapshot()).bounds;auto logical=take(host.snapshot()).size;auto image=take(host.capture_canvas());
        auto center=pixel(image,logical,r.x+14,r.y+14),rim=pixel(image,logical,r.x+14,r.y+2),corner=pixel(image,logical,r.x+1,r.y+1);
        check(center[0]>245&&center[1]<8,"Circular color swatch center differs");
        check(rim[0]>240&&rim[1]>240,"Circular color swatch lost white rim");
        auto fr=take(faint.snapshot()).bounds;auto faint_rim=pixel(image,logical,fr.x+14,fr.y+2);
        check(faint_rim[1]>50&&faint_rim[1]<160,"Authored circular border opacity was ignored");
        check(corner[0]<150&&corner[1]<150,"Circular swatch painted square corners");
        good(model->set_color({0,1,0,1}));image=take(host.capture_canvas());center=pixel(image,logical,r.x+14,r.y+14);
        check(center[1]>245&&center[0]<8,"Circular swatch did not observe its color model");
        good(host.request_close());
    }}));
}

int main(int argc,char** argv){try{std::string_view name=argc>1?argv[1]:"contracts";if(name=="contracts")contracts();else if(name=="input")input();else if(name=="rendering"){rendering();circular();}else if(name=="cache")cache();else if(name=="lifetime")lifetime();else throw std::runtime_error("Unknown color button case");std::cout<<"COLOR_BUTTON_OK "<<name<<'\n';}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
