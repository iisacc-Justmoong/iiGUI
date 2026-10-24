#include <iiGUI/iiGUI.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
void good(core::Result<void> result){if(!result)throw std::runtime_error(result.error().message);}
bool near(double a,double b){return std::abs(a-b)<.01;}
void run(const std::string& name){
    gui::Host host({.size={600,400}});gui::WidgetMount mount;gui::Widget row,slot,child,popup,other;unsigned canceled=0,clicked=0;
    good(host.run({.opened=[&]{
        mount=take(host.mount_widgets([&](auto& b)->core::Result<void>{
            auto root=take(b.vstack({},"root",{.spacing=0,.alignment=gui::StackAlignment::leading}));
            if(name=="minimum"||name=="implicit"){
                row=take(b.hstack(root,"row",{.spacing=0,.alignment=gui::StackAlignment::center,.insets=gui::Insets{8,10,8,10},.minimum_width=80,.minimum_height=56,.implicit_insets=gui::Insets{8,16,8,16}}));
                child=take(b.label(row,"label",{.text="A",.font_size=22,.line_height=22,.elide_right=true}));
            }else if(name=="intrinsic"){
                row=take(b.vstack(root,"row",{.spacing=0,.alignment=gui::StackAlignment::stretch,.minimum_width=80,.intrinsic_width=true}));child=take(b.label(row,"label",{.text="A",.line_height=13,.elide_right=true}));
            }else if(name=="hidden"){
                row=take(b.zstack(root,"row",{.alignment=gui::StackAlignment::leading,.measure_hidden_children=true}));child=take(b.vstack(row,"hidden"));good(child.set_extent({180,30}));good(child.set_visible(false));
            }else if(name=="typography"||name=="button_options"){
                child=take(b.button(root,"button",{.text="Menu",.tone=gui::ButtonTone::normal},[&]{++clicked;}));
                good(child.set_button_options({.text="Menu",.tone=gui::ButtonTone::normal,.height=32,.horizontal_padding=10,.style=gui::ButtonStyle{.line_height=12,.text_role=gui::TextRole::description,.font_size=12}}));
            }else{
                row=take(b.vstack(root,"host",{.spacing=0}));slot=take(b.vstack(row,"slot",{.spacing=0}));
                good(b.set_default_content(row,slot));
                if(name=="nested"){
                    other=take(b.zstack(slot,"inner",{.alignment=gui::StackAlignment::leading}));good(b.set_default_content(slot,other));
                }
                child=take(b.button(row,"button",{.text="Action"},[&]{++clicked;}));good(child.set_control_events({.canceled=[&]{++canceled;if(name=="retirement")good(host.unmount_widgets());}}));
                popup=take(b.popover(row,"popup"));
                if(name=="validation"){
                    check(!b.set_default_content(slot,row)&&!b.set_default_content(row,row)&&!b.set_default_content(row,child)&&!b.set_default_content(row,{}),"Invalid content slot or cycle accepted");
                    auto invalid=gui::StackOptions{.minimum_width=std::numeric_limits<double>::quiet_NaN()};check(!row.set_stack_options(invalid),"Invalid minimum accepted");
                    check(!child.set_button_options({.text="Changed",.style=gui::ButtonStyle{.font_size=0}})&&take(child.snapshot()).text=="Action","Invalid button replacement partially committed");
                }
            }
            return {};
        }));
        if(name=="minimum"){
            auto snapshot=take(row.snapshot());check(near(snapshot.implicit_size.width,80)&&near(snapshot.implicit_size.height,56),"Minimum implicit metrics failed");good(row.set_extent({40,30}));snapshot=take(row.snapshot());check(near(snapshot.bounds.width,40)&&near(snapshot.bounds.height,30),"Explicit extent did not override implicit minimum");
        }else if(name=="implicit"){
            good(row.set_extent({240,100}));const auto r=take(row.snapshot()).bounds,c=take(child.snapshot()).bounds;check(near(c.x,r.x+8)&&near(c.y,r.y+39),"Implicit insets replaced actual content insets");
        }else if(name=="intrinsic"){
            const auto r=take(row.snapshot()).bounds,c=take(child.snapshot()).bounds;check(near(r.width,80)&&near(c.width,80),"Content stretch forced an intrinsic container to fill its parent");
        }else if(name=="hidden"){
            auto snapshot=take(row.snapshot());check(near(snapshot.implicit_size.width,180)&&near(snapshot.implicit_size.height,30)&&!take(child.snapshot()).visible,"Hidden slot metrics were dropped or hidden content was exposed");
        }else if(name=="typography"||name=="button_options"){
            auto snapshot=take(child.snapshot());check(near(snapshot.font_size,12)&&near(snapshot.bounds.height,32),"Native button typography/geometry replacement failed");good(child.activate());
            good(child.set_button_options({.text="Compact",.tone=gui::ButtonTone::normal,.height=28,.horizontal_padding=8,.style=gui::ButtonStyle{.line_height=12,.text_role=gui::TextRole::description,.font_size=12}}));good(child.activate());check(clicked==2&&near(take(child.snapshot()).bounds.height,28),"Button replacement lost its existing callback or geometry");
        }else{
            check(take(child.snapshot()).parent_id==(name=="nested"?"inner":"slot")&&take(popup.snapshot()).parent_id=="host","Default slot redirected a popup owner or lost ordinary children");
            if(name=="cancellation"||name=="retirement"){
                good(child.focus());const auto r=take(child.snapshot()).bounds;good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=r.x+r.width/2,.y=r.y+r.height/2}));good(row.set_enabled(false));check(canceled==1,"Container disable did not cancel a descendant press");
                if(name=="retirement")check(!child.snapshot(),"Cancellation receiver did not retire its scope");else{check(!take(child.snapshot()).enabled&&!take(child.snapshot()).focused&&!take(child.snapshot()).pressed ,"Disabled ancestor retained active input");good(child.activate());check(clicked==0,"Disabled ancestor dispatched activation");good(row.set_enabled(true));good(child.activate());check(clicked==1,"Ancestor re-enable did not restore requested child state");}
            }
        }
        good(host.request_close());
    }}));
}
}
int main(int argc,char** argv){try{run(argc>1?argv[1]:"minimum");std::cout<<"IIGUI_CONTAINER_LAYOUT_OK\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
