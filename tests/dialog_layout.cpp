#include <iiGUI/iiGUI.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
void geometry(){
    gui::Host host({.size={1000,700}});gui::Widget popup,title,body,icon;int made=0;
    good(host.run({.opened=[&]{
        auto mount=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{
            auto root=take(b.vstack({},"root"));popup=take(b.popover(root,"dialog",{.width=760,.padding=20,.margins=24,.modal=true,.placement=gui::PopoverPlacement::centered,.minimum_width=520,.maximum_width=980,.minimum_height=210,.vertical_offset=-16}));
            return popup.set_popup_content([&](gui::WidgetBuilder& c)->core::Result<void>{++made;auto content=take(c.vstack({},"content",{.spacing=20,.alignment=gui::StackAlignment::stretch}));auto header=take(c.hstack(content,"header",{.spacing=16,.alignment=gui::StackAlignment::leading}));icon=take(c.absolute_stack(header,"icon",{72,72}));auto text=take(c.vstack(header,"texts",{.spacing=8,.alignment=gui::StackAlignment::leading}));good(text.set_flex(1));title=take(c.label(text,"title",{.text="A title",.font_size=22}));body=take(c.label(text,"description",{.text=std::string(40,'a')+" "+std::string(40,'b')+" "+std::string(40,'c'),.font_size=13}));take(c.button(content,"action",{.text="OK",.height=20,.corner_radius=4}));return {};});
        }));
        good(popup.open_at(900,650));auto a=take(popup.snapshot()).bounds;auto t=take(title.snapshot()).bounds;auto i=take(icon.snapshot()).bounds;auto d=take(body.snapshot()).bounds;
        check(a.width==760&&std::abs(a.x-120)<1&&std::abs(a.y-((700-a.height)/2-16))<1,"Dialog was not centered above midpoint");
        check(t.x>=i.x+72+15&&std::abs(t.y-i.y)<1&&d.x==t.x&&d.x+d.width<=a.x+a.width-19,"Header flex alignment escaped content width");
        good(host.set_content_size({600,700}));auto narrow=take(popup.snapshot()).bounds;check(narrow.width==552&&made==1,"Resize recreated content or ignored side padding");check(take(body.snapshot()).bounds.height>d.height,"Width-dependent text height did not grow");
        check(!title.set_flex(-1),"Invalid flex accepted");good(host.request_close());
    }}));
}
void stacks(){
    gui::Host host({.size={600,500}});gui::Widget text,fixed,spacer;
    good(host.run({.opened=[&]{auto mount=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{auto root=take(b.vstack({},"root",{.padding=10}));auto row=take(b.hstack(root,"row",{.spacing=16,.alignment=gui::StackAlignment::leading}));fixed=take(b.absolute_stack(row,"fixed",{72,72}));text=take(b.label(row,"text",{.text="Short"}));good(text.set_flex(1));auto overflow=take(b.hstack(root,"overflow",{.spacing=0,.alignment=gui::StackAlignment::leading}));spacer=take(b.spacer(overflow,"minimum",600));take(b.button(overflow,"button",{.text="Action"}));return {};}));
        auto a=take(text.snapshot()).bounds;check(std::abs(a.width-492)<1&&std::abs(a.y-take(fixed.snapshot()).bounds.y)<1,"Ordinary host stack ignored flex/alignment");good(text.set_text(std::string(30,'a')+" "+std::string(30,'b')+" "+std::string(30,'c')));auto wide=take(text.snapshot()).bounds;good(host.set_content_size({340,500}));auto narrow=take(text.snapshot()).bounds;check(narrow.height>wide.height&&narrow.width<wide.width,"Nested host constraints did not invalidate text layout");check(take(spacer.snapshot()).bounds.width>=600,"Flexible spacer lost its minimum length");good(host.request_close());
    }}));
}
void cancellation(){
    gui::Host host;gui::Widget popup;int closed=0,canceled=0;
    good(host.run({.opened=[&]{auto mount=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{auto root=take(b.vstack({},"root"));popup=take(b.popover(root,"dialog",{.width=320,.height=180,.modal=true,.placement=gui::PopoverPlacement::centered},{.closed=[&]{++closed;},.canceled=[&]{++canceled;}}));take(b.button(popup,"action",{.text="OK"}));return {};}));
        good(popup.open_at(0,0));good(popup.close_popup());check(closed==1&&canceled==0,"Programmatic close emitted cancellation");good(popup.open_at(0,0));good(popup.cancel_popup());check(closed==2&&canceled==1,"Explicit cancel contract failed");good(popup.cancel_popup());check(canceled==1,"Closed popup canceled twice");good(popup.open_at(0,0));good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::escape}));check(closed==3&&canceled==2,"Escape did not cancel");good(popup.open_at(0,0));good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=1,.y=1}));good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=1,.y=1}));check(closed==4&&canceled==3,"Outside dismissal did not cancel");good(host.request_close());
    }}));
}
}
int main(int argc,char** argv){try{std::string name=argc>1?argv[1]:"geometry";if(name=="geometry")geometry();else if(name=="cancellation")cancellation();else if(name=="stacks")stacks();else throw std::runtime_error("Unknown dialog layout case");std::cout<<"DIALOG_LAYOUT_OK "<<name<<'\n';}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
