#include <iiGUI/iiGUI.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(core::Result<void> r){if(!r){if(r.error().cause)std::rethrow_exception(r.error().cause);throw std::runtime_error(r.error().message);}}
void z_order(){
    gui::Host host({.size={400,300}});gui::Widget front;gui::WidgetMount mount;int back_calls=0,front_calls=0;
    good(host.run({.opened=[&]{
        mount=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{auto root=take(b.zstack({},"root"));auto back=take(b.zstack(root,"back"));front=take(b.button(root,"front",{.text="Front",.corner_radius=0,.style=gui::ButtonStyle{.fill=gui::WidgetColor{1,0,0,1}}},[&]{++front_calls;}));take(b.button(back,"lateBack",{.text="Back",.corner_radius=0,.style=gui::ButtonStyle{.fill=gui::WidgetColor{0,0,1,1}}},[&]{++back_calls;}));return {};}));
        auto r=take(front.snapshot()).bounds;auto image=take(host.capture_canvas());auto scale=take(host.snapshot()).backing_scale;const auto* p=&image.rgba[(std::size_t((r.y+2)*scale)*image.width+std::size_t((r.x+r.width/2)*scale))*4];check(p[0]>250&&p[2]<3,"Tree paint order did not keep later sibling in front");
        good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=r.x+r.width/2,.y=r.y+r.height/2}));good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=r.x+r.width/2,.y=r.y+r.height/2}));check(front_calls==1&&back_calls==0,"Pointer targeting used allocation order instead of painted sibling order");good(host.request_close());
    }}));
}
void run(std::string name){
    gui::Host host({.size={600,480}});gui::Widget scroll,first,last,inner,popup,item;gui::WidgetMount mount;int calls=0;
    good(host.run({.opened=[&]{
        mount=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{
            auto root=take(b.vstack({},"root",{.spacing=12,.padding=20,.alignment=gui::StackAlignment::leading}));
            scroll=take(b.scroll_view(root,"scroll",{.width=400,.height=120}));
            auto content=take(b.vstack(scroll,"content",{.spacing=8,.alignment=gui::StackAlignment::stretch}));
            for(int i=0;i<12;++i){auto w=take(b.button(content,"row"+std::to_string(i),{.text="Row "+std::to_string(i)},[&]{++calls;}));if(!i)first=w;last=w;}
            if(name=="popup"){popup=take(b.menu(first,"popup"));item=take(b.menu_item(popup,"item",{.label="Escaping viewport",.has_child_items=false}));}
            if(name=="nested"){
                inner=take(b.scroll_view(content,"inner",{.height=40}));auto body=take(b.vstack(inner,"innerBody",{.spacing=0,.alignment=gui::StackAlignment::stretch}));
                for(int i=0;i<10;++i)take(b.label(body,"inner"+std::to_string(i),{.text="Nested content"}));
            }
            take(b.label(root,"footer",{.text="Outside viewport"}));return {};
        }));
        auto r=take(scroll.snapshot());check(r.bounds.width==400&&r.bounds.height==120&&r.content_height>300,"Scroll viewport did not retain authored size and content extent");
        check(!take(last.snapshot()).visible&&take(last.snapshot()).clip_bounds==std::optional{r.bounds},"Offscreen child/clip was exposed as visible");
        if(name=="popup"){
            good(popup.open_at(r.bounds.x+100,r.bounds.y+r.bounds.height+30));check(take(item.snapshot()).visible&&take(item.snapshot()).clip_bounds!=std::optional{r.bounds},"Owned popup was clipped by ancestor scroll viewport");check(take(scroll.snapshot()).scroll_offset==0,"Popup autofocus scrolled its owner viewport");
        }else if(name=="geometry"){
            good(scroll.set_scroll_offset(100000));auto end=take(scroll.snapshot());check(std::abs(end.scroll_offset-(end.content_height-end.bounds.height))<.01,"Scroll offset was not clamped");
            good(scroll.set_scroll_options({.width=400,.height=420}));check(take(scroll.snapshot()).scroll_offset<=std::max(0.,take(scroll.snapshot()).content_height-420),"Resize did not reclamp scroll");
            good(scroll.set_scroll_offset(0));check(take(first.snapshot()).visible,"Scroll to origin lost first child");
            check(!first.set_scroll_offset(1)&&!scroll.set_scroll_offset(std::nan("")),"Invalid scroll target or nonfinite offset accepted");
        }else if(name=="input"){
            good(mount.dispatch_input({.kind=gui::WidgetInputKind::scroll,.x=r.bounds.x+40,.y=r.bounds.y+40,.scroll_y=85}));check(take(scroll.snapshot()).scroll_offset==85,"Native wheel missed viewport");
            good(last.focus());check(take(last.snapshot()).visible&&take(scroll.snapshot()).scroll_offset>85,"Keyboard focus did not reveal scrolled child");
            good(first.focus());check(take(scroll.snapshot()).scroll_offset==0,"Focus did not reveal first row");
            good(scroll.set_scroll_options({.width=400,.height=120,.enabled=false}));good(mount.dispatch_input({.kind=gui::WidgetInputKind::scroll,.x=r.bounds.x+40,.y=r.bounds.y+40,.scroll_y=85}));check(take(scroll.snapshot()).scroll_offset==0,"Disabled scrolling changed offset");
        }else if(name=="rendering"){
            auto before=take(host.capture_canvas());good(scroll.set_scroll_offset(70));auto after=take(host.capture_canvas());check(before.rgba!=after.rgba,"Native scroll did not change GPU pixels");
            auto scale=take(host.snapshot()).backing_scale;const auto y=std::size_t((r.bounds.y+r.bounds.height+5)*scale),x=std::size_t((r.bounds.x+4)*scale);const auto pos=(y*after.width+x)*4;
            check(before.rgba[pos]==after.rgba[pos]&&before.rgba[pos+1]==after.rgba[pos+1]&&before.rgba[pos+2]==after.rgba[pos+2],"Scroll foreground escaped viewport clipping");
        }else if(name=="nested"){
            good(inner.focus());auto bounds=take(inner.snapshot()).bounds;good(mount.dispatch_input({.kind=gui::WidgetInputKind::scroll,.x=bounds.x+10,.y=bounds.y+10,.scroll_y=20}));check(take(inner.snapshot()).scroll_offset==20,"Nested wheel did not choose innermost viewport");
            good(inner.set_scroll_offset(100000));good(scroll.set_scroll_offset(std::max(0.,take(scroll.snapshot()).scroll_offset-20)));bounds=take(inner.snapshot()).bounds;auto outer=take(scroll.snapshot()).scroll_offset;good(mount.dispatch_input({.kind=gui::WidgetInputKind::scroll,.x=bounds.x+10,.y=bounds.y+10,.scroll_y=30}));check(take(scroll.snapshot()).scroll_offset>outer,"Nested edge did not bubble to outer viewport");
        }else throw std::runtime_error("Unknown scroll case");
        good(host.unmount_widgets());check(!scroll.snapshot()&&!last.snapshot(),"Scroll handles survived unmount");good(host.request_close());
    }}));
}
}
int main(int argc,char**argv){try{const std::string name=argc>1?argv[1]:"geometry";if(name=="z_order")z_order();else run(name);std::cout<<"SCROLL_VIEW_OK\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
