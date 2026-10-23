#include <iiGUI/iiGUI.h>
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void run(std::string name){
    gui::Host host({.size={640,420}});gui::WidgetMount mount;gui::Widget action,child;
    int outer=0,inner=0,canceled=0;
    gui::ActionOptions options{.text="Fallback native",.role=gui::ActionRole::link,.text_role=gui::TextRole::description,.font_size=12,.line_height=12,.padding={14,10,14,10},.minimum_height=36,.corner_radius=8,.text_color=gui::WidgetColor{.1,.5,1,1},.hover_text_color=gui::WidgetColor{1,1,1,.9},.pressed_text_color=gui::WidgetColor{.08,.4,.8,1},.disabled_text_color=gui::WidgetColor{1,1,1,.6}};
    good(host.run({.opened=[&]{
        mount=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{auto root=take(b.vstack({},"root",{.spacing=8,.alignment=gui::StackAlignment::leading}));action=take(b.action_surface(root,"action",options,[&]{++outer;if(name=="retirement")good(host.unmount_widgets());}));
            if(name=="children"||name=="child_input"||name=="disabled"){auto content=take(b.hstack(action,"content",{.spacing=8}));take(b.label(content,"title",{.text="Custom child"}));child=take(b.button(content,"child",{.text="Child action"},[&]{++inner;}));good(child.set_control_events({.canceled=[&]{++canceled;}}));}return {};}));
        const auto initial=take(action.snapshot());check(initial.kind==gui::WidgetKind::action_surface&&initial.action_role==gui::ActionRole::link,"Action surface was replaced by an ordinary control");
        const auto pointer=[&](gui::WidgetInputKind kind,gui::Rect r){good(mount.dispatch_input({.kind=kind,.x=r.x+3,.y=r.y+3}));};
        if(name=="geometry"){
            check(initial.implicit_size.height==36&&initial.font_size==12&&initial.bounds.width>28,"Authored action padding/minimum height were lost");auto next=take(action.action_options());next.width=240;next.height=90;good(action.set_action_options(next));auto r=take(action.snapshot()).bounds;check(r.width==240&&r.height==90,"Explicit action geometry did not replace natural size");
        }else if(name=="fallback"){
            check(!initial.custom_content,"Empty slot hid fallback text");auto before=take(host.capture_canvas());good(action.set_text(""));auto empty=take(host.capture_canvas());check(before.rgba!=empty.rgba,"Fallback text was not rendered");
        }else if(name=="children"){
            check(initial.custom_content&&take(child.snapshot()).parent_id=="content","Native child tree was not retained");auto before=take(host.capture_canvas());good(action.set_text("A different hidden fallback"));auto after=take(host.capture_canvas());check(before.rgba==after.rgba,"Fallback text painted over custom children");
        }else if(name=="child_input"){
            auto r=take(child.snapshot()).bounds;pointer(gui::WidgetInputKind::pointer_down,r);pointer(gui::WidgetInputKind::pointer_up,r);check(inner==1&&outer==0,"Parent action stole a child's click");pointer(gui::WidgetInputKind::pointer_down,initial.bounds);pointer(gui::WidgetInputKind::pointer_up,initial.bounds);check(outer==1,"Parent padding did not remain actionable");
        }else if(name=="keyboard"){
            good(action.focus());good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::enter}));check(outer==0&&take(action.snapshot()).pressed,"Enter activated before release");good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_up,.key=gui::WidgetKey::enter}));check(outer==1,"Enter release did not activate the action");
        }else if(name=="colors"){
            check(initial.foreground_color==options.text_color,"Default action color changed");pointer(gui::WidgetInputKind::pointer_move,initial.bounds);check(take(action.snapshot()).foreground_color==options.hover_text_color,"Hover color missing");pointer(gui::WidgetInputKind::pointer_down,initial.bounds);check(take(action.snapshot()).foreground_color==options.pressed_text_color,"Pressed color missing");good(action.set_enabled(false));check(take(action.snapshot()).foreground_color==options.disabled_text_color,"Disabled color missing");
        }else if(name=="disabled"){
            good(child.focus());good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::space}));good(action.set_enabled(false));check(!take(child.snapshot()).enabled&&!take(child.snapshot()).focused&&canceled==1,"Disabled action did not cancel descendant focus/press");good(child.activate());good(action.activate());check(inner==0&&outer==0,"Disabled action subtree admitted clicks");
        }else if(name=="underline"){
            auto before=take(host.capture_canvas());auto next=take(action.action_options());next.underline=true;good(action.set_action_options(next));auto after=take(host.capture_canvas());check(take(action.snapshot()).text_underline&&before.rgba!=after.rgba,"Underline did not reach native text painting");
        }else if(name=="validation"){
            auto invalid=options;invalid.padding.left=-1;check(!action.set_action_options(invalid)&&take(action.action_options()).padding.left==14,"Invalid options partially changed geometry");invalid=options;invalid.text_color=gui::WidgetColor{2,0,0,1};check(!action.set_action_options(invalid),"Invalid action color accepted");
        }else if(name=="retirement"){
            good(action.activate());check(!action.snapshot()&&outer==1,"Action callback could not retire its owner");
        }else throw std::runtime_error("Unknown action surface case");
        good(host.request_close());
    }}));
}
}
int main(int argc,char** argv){try{run(argc>1?argv[1]:"geometry");std::cout<<"ACTION_SURFACE_OK\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
