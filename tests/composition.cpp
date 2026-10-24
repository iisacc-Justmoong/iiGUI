#include <iiGUI/iiGUI.h>
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
void layout(){
    gui::Host host;gui::Widget group,a,b;int clicks=0;
    auto result=host.run({.opened=[&]{
        auto mount=take(host.mount_widgets([&](gui::WidgetBuilder& w)->core::Result<void>{
            auto root=take(w.vstack({},"root",{.spacing=5}));group=take(w.absolute_stack(root,"group",{100,60}));
            a=take(w.button(group,"a",{.text="A"},[&]{++clicks;}));good(a.set_frame({10,12,50,22}));
            b=take(w.button(root,"b",{.text="B"}));return {};
        }));
        auto g=take(group.snapshot()),s=take(a.snapshot());check(g.size==gui::Size{100,60} && s.bounds.x==g.bounds.x+10 && s.bounds.y==12 && s.size==gui::Size{50,22},"Absolute coordinates or measured extent wrong");
        check(!b.set_frame({0,0,1,1}) && !a.set_frame({-1,0,1,1}),"Fixed frame accepted another layout or negative geometry");
        good(a.focus());good(group.set_visible(false));check(!take(a.snapshot()).visible && !take(a.snapshot()).focused,"Hidden descendants still visible/focused");
        auto activated=a.activate();check(!activated && clicks==0,"Hidden control activated");
        good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::tab}));check(take(b.snapshot()).focused,"Tab did not skip hidden subtree");
        good(group.set_visible(true));good(group.set_extent({0,0}));check(take(group.snapshot()).size==gui::Size{},"Empty absolute extent must remain zero");
        check(!group.set_extent({-1,0}),"Negative empty extent accepted");
        good(group.set_extent({120,80}));check(take(group.snapshot()).size==gui::Size{120,80},"Extent did not invalidate ancestor layout");
        good(a.activate());check(clicks==1,"Restored subtree lost its action");good(host.request_close());
    }});good(result);check(!a.set_visible(true),"Closed widget mutated");
}
void input(){
    gui::Host host;gui::Widget a,b;int commits=0,escapes=0;
    good(host.run({.opened=[&]{
        auto m=take(host.mount_widgets([&](gui::WidgetBuilder& w)->core::Result<void>{auto root=take(w.vstack({},"root"));good(root.set_escape_action([&]{++escapes;}));
            a=take(w.input_field(root,"a",{.text="12",.commit_on_focus_loss=true,.accessibility_name="Channel",.maximum_length=2},{},[&]{++commits;}));b=take(w.button(root,"b",{.text="Next"}));return {};}));
        good(a.set_text("한글x"));check(take(a.snapshot()).text=="한글","Length limit split a UTF-8 scalar");
        good(a.focus());good(a.focus());check(commits==0,"Repeated focus committed input");good(b.focus());check(commits==1,"Programmatic focus loss did not commit");
        good(a.focus());auto r=take(b.snapshot()).bounds;good(m.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=r.x+2,.y=r.y+2}));check(commits==2,"Pointer focus loss did not commit");
        good(m.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=r.x+2,.y=r.y+2}));
        good(m.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::escape}));check(escapes==1,"Escape did not route to owning group");
        good(a.focus());good(take(m.find("root")).focus());check(commits==3&&!take(a.snapshot()).focused,"Owning group did not retire input focus");
        good(a.focus());good(m.dispatch_input({.kind=gui::WidgetInputKind::focus_lost}));check(commits==4,"Native focus loss did not commit");good(host.request_close());
    }}));
    // A commit can retire the tree or redirect focus. The pending press must not
    // invoke an action from the previous input target after that callback.
    gui::Host retired;int stale=0;
    good(retired.run({.opened=[&]{auto m=take(retired.mount_widgets([&](gui::WidgetBuilder& w)->core::Result<void>{auto root=take(w.vstack({},"root"));a=take(w.input_field(root,"a",{.commit_on_focus_loss=true},{},[&]{good(retired.unmount_widgets());}));b=take(w.button(root,"b",{.text="Retire"},[&]{++stale;}));return {};}));
        good(a.focus());auto r=take(b.snapshot()).bounds;good(m.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=r.x+2,.y=r.y+2}));check(stale==0 && !a.snapshot(),"Commit left a stale live press");good(retired.request_close());}}));
}
void images(){
    auto red=take(gui::rgba_image(1,1,{255,0,0,255})),green=take(gui::rgba_image(1,1,{0,255,0,255}));gui::Host host;
    good(host.run({.opened=[&]{auto m=take(host.mount_widgets([&](gui::WidgetBuilder& w)->core::Result<void>{auto root=take(w.absolute_stack({},"root",{100,60}));auto image=take(w.image(root,"image",{.image=red,.width=40,.height=20,.accessibility_name="Comparison"}));good(image.set_frame({0,0,40,20}));good(image.set_image(green));auto slider=take(w.slider(root,"ramp",{.width=100,.thumb_diameter=12,.track_height=4,.range_inset=8,.background_image=red}));good(slider.set_frame({0,30,100,22}));return {};}));
        auto pixels=take(host.capture_canvas());check(pixels.width && pixels.height,"Image view did not render");auto n=take(take(m.find("image")).snapshot());check(n.native_class=="iiGUI.Image" && n.accessibility_name=="Comparison","Image metadata wrong");
        const auto offset=(std::size_t(5)*pixels.width+5)*4;check(pixels.rgba[offset+1]>240 && pixels.rgba[offset]<10,"Image mutation did not reach GPU");
        const auto scale=double(pixels.width)/take(host.snapshot()).size.width;const auto thumb=(std::size_t(41*scale)*pixels.width+std::size_t(50*scale))*4;check(pixels.rgba[thumb+1]>220&&pixels.rgba[thumb+2]>220,"Gradient track painted over its thumb");good(host.request_close());}}));
}
}
int main(int argc,char** argv){try{std::string name=argc>1?argv[1]:"layout";if(name=="layout")layout();else if(name=="input")input();else if(name=="images")images();else throw std::runtime_error("Unknown composition test");std::cout<<"NATIVE_COMPOSITION_OK "<<name<<'\n';}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
