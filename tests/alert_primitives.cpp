#include <iiGUI/iiGUI.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(core::Result<void> r){if(!r){if(r.error().cause)std::rethrow_exception(r.error().cause);throw std::runtime_error(r.error().message);}}
void layout(){gui::Host host({.size={800,700}});good(host.run({.opened=[&]{gui::Widget root,title,body;auto m=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{root=take(b.vstack({},"root",{.spacing=14,.alignment=gui::StackAlignment::stretch,.insets=gui::Insets{24,46,24,36}}));title=take(b.label(root,"title",{.text="Alert Dialog",.font_size=26,.role=gui::TextRole::title,.alignment=gui::TextAlignment::center,.line_height=26}));body=take(b.label(root,"body",{.text="First line\nSecond line",.font_size=13,.role=gui::TextRole::body,.line_height=13}));return {};}));check(take(title.snapshot()).size.height==26&&take(body.snapshot()).size.height==26&&take(root.snapshot()).size.height==148,"Fixed line metrics or asymmetric padding differ from authored layout");good(body.set_text("One line"));check(take(body.snapshot()).size.height==13&&take(root.snapshot()).size.height==135,"Fixed text update did not shrink layout");good(root.set_stack_options({.spacing=14,.alignment=gui::StackAlignment::stretch,.insets=gui::Insets{12,20,12,10}}));check(take(title.snapshot()).bounds.x==12&&take(title.snapshot()).bounds.y==20&&take(root.snapshot()).size.height==83,"Stack options did not invalidate constraints");take(host.capture_canvas());good(host.request_close());}}));}
void style(){gui::Host host;good(host.run({.opened=[&]{gui::Widget button;auto appearance=gui::lvrs_appearance();appearance.window={.2,.4,.6,1};auto m=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{auto root=take(b.absolute_stack({},"root",{640,420}));button=take(b.button(root,"button",{.text="",.tone=gui::ButtonTone::normal,.height=56,.corner_radius=16,.style=gui::ButtonStyle{.fill=gui::WidgetColor{0,0,0,0},.hover_fill=gui::WidgetColor{1,1,1,.05},.pressed_fill=gui::WidgetColor{1,1,1,.09},.disabled_fill=gui::WidgetColor{.2,.2,.2,1},.border=gui::WidgetColor{1,1,1,.5},.border_width=1}}));good(button.set_frame({20,20,100,56}));return {};},appearance));auto scale=take(host.snapshot()).backing_scale;auto pixel=[&](const gui::PixelBuffer& im,double x,double y){return &im.rgba[(std::size_t(y*scale)*im.width+std::size_t(x*scale))*4];};auto a=take(host.capture_canvas());auto center=pixel(a,70,48),border=pixel(a,70,20.5);check(std::abs(int(center[0])-51)<3&&std::abs(int(center[2])-153)<3&&border[0]>150,"Outline filled its center or failed linear GPU stroke");good(m.dispatch_input({.kind=gui::WidgetInputKind::pointer_move,.x=70,.y=48}));auto b=take(host.capture_canvas());check(pixel(b,70,48)[0]>center[0]+15,"Hover color did not apply");good(button.set_enabled(false));auto c=take(host.capture_canvas());auto disabled=pixel(c,70,48);check(std::abs(int(disabled[0])-disabled[1])<2&&std::abs(int(disabled[0])-51)<3,"Disabled fill did not override state");check(!button.set_button_style({.border_width=-1}),"Invalid border admitted");good(host.request_close());}}));}
void events(){gui::Host host;std::string order;good(host.run({.opened=[&]{gui::Widget button;auto m=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{auto root=take(b.vstack({},"root"));button=take(b.button(root,"button",{.text="Action"},[&]{order+='a';}));good(button.set_control_events({.pressed=[&]{order+='p';},.released=[&]{order+='r';},.canceled=[&]{order+='c';}}));return {};}));auto r=take(button.snapshot()).bounds;good(m.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=r.x+2,.y=r.y+2}));good(m.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=r.x+2,.y=r.y+2}));check(order=="pra","Control events did not precede accepted action");good(host.request_close());}}));}
void focus() {
    gui::Host host;
    good(host.run({.opened=[&] {
        gui::Widget button;
        auto appearance=gui::lvrs_appearance();
        appearance.window={.2,.4,.6,1};
        appearance.primary={0,1,0,1};
        auto mount=take(host.mount_widgets([&](gui::WidgetBuilder& builder)->core::Result<void> {
            auto root=take(builder.absolute_stack({},"root",{640,420}));
            button=take(builder.button(root,"button",{.text="",.height=56,.corner_radius=16,
                .style=gui::ButtonStyle{.fill=gui::WidgetColor{.2,.2,.2,1}}}));
            good(button.set_frame({20,20,160,56}));
            return {};
        },appearance));
        auto before=take(host.capture_canvas());
        good(button.focus());
        auto after=take(host.capture_canvas());
        const auto scale=take(host.snapshot()).backing_scale;
        auto pixel=[&](const gui::PixelBuffer& image,double x,double y) {
            return &image.rgba[(std::size_t(y*scale)*image.width+std::size_t(x*scale))*4];
        };
        auto old_corner=pixel(before,20.5,20.5),corner=pixel(after,20.5,20.5);
        auto ring=pixel(after,100,21),center=pixel(after,100,48);
        check(std::abs(int(old_corner[0])-corner[0])<3&&std::abs(int(old_corner[1])-corner[1])<3&&std::abs(int(old_corner[2])-corner[2])<3,
              "Button focus painted a square corner outside its authored radius");
        check(ring[0]<10&&ring[1]>240&&ring[2]<10,"Authored 1.5-unit focus stroke missing");
        check(std::abs(int(center[0])-51)<3&&std::abs(int(center[1])-51)<3,"Focus stroke filled the button center");
        good(host.request_close());
    }}));
}
void stroke(bool vulkan){gui::Host host;good(host.run({.opened=[&]{gui::CanvasFrame f{.viewport={0,0,200,120},.background={0,0,0,1},.renderer=vulkan?gui::Renderer::vulkan:gui::Renderer::native};f.panels.push_back({{20,20,160,80},{1,1,1,.5},{1,1,1,.5},20,4});good(host.set_canvas(f));auto im=take(host.capture_canvas());auto scale=take(host.snapshot()).backing_scale;auto p=[&](double x,double y){return im.rgba[(std::size_t(y*scale)*im.width+std::size_t(x*scale))*4];};check(p(100,60)<3&&p(100,21)>170&&p(100,21)<200&&p(21,21)<3,"Rounded native/Vulkan stroke geometry or linear alpha failed");good(host.request_close());}}));}
}
int main(int argc,char** argv){try{std::string n=argc>1?argv[1]:"layout";if(n=="layout")layout();else if(n=="style")style();else if(n=="events")events();else if(n=="focus")focus();else if(n=="stroke_native")stroke(false);else if(n=="stroke_vulkan")stroke(true);else throw std::runtime_error("Unknown alert primitive case");std::cout<<"ALERT_PRIMITIVES_OK "<<n<<'\n';}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
