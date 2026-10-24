#include <iiGUI/NativeWidgets.h>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
using namespace ii;
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
void run(const std::string& name){
    runtime::EventLoop loop;gui::Host host({.size={640,420},.event_loop=&loop});gui::Widget target,parent,popup,embedded,modal,inside;gui::WidgetMount mount;
    std::vector<gui::WidgetInteraction> changes;std::shared_ptr<gui::WidgetInteractionPort> port,broken;unsigned clicks=0,inputs=0,canceled=0;bool released=false;
    good(host.run({.opened=[&]{
        mount=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{
            parent=take(b.absolute_stack({},"root",{640,420}));
            target=take(b.button(parent,"target",{.text="Origin",.height=40,.width=160},[&]{++clicks;}));good(target.set_frame({200,160,160,40}));
            good(target.set_input_handler([&](const auto& e)->core::Result<bool>{++inputs;if(name=="capture"){if(e.kind==gui::WidgetInputKind::pointer_down)return true;if(e.kind==gui::WidgetInputKind::pointer_up){released=true;return true;}if(e.kind==gui::WidgetInputKind::focus_lost)++canceled;}return false;}));
            popup=take(b.popover(parent,"bubble",{.width=200,.height=120,.padding=0,.margins=0,.surface_opacity=0,.focus_on_open=false,.opaque_base=false,.bounds=gui::Rect{180,80,200,120},.input_bounds=gui::Rect{0,0,200,70},.preserve_input=true}));
            if(name=="modal"){modal=take(b.popover(parent,"modal",{.width=200,.height=100,.modal=true,.focus_on_open=false}));auto body=take(b.vstack(popup,"bubbleBody"));inside=take(b.button(body,"bubbleAction",{.text="Action"}));}
            if(name=="embedded"){embedded=take(b.content_host(parent,"page",{300,200}));good(embedded.set_frame({20,40,300,200}));good(embedded.set_content([&](gui::WidgetBuilder& c)->core::Result<void>{auto root=take(c.absolute_stack({},"inner",{300,200}));target=take(c.button(root,"nested",{.text="Nested",.height=30,.width=100}));return target.set_frame({10,20,100,30});}));}
            if(name=="fanout"){broken=std::make_shared<gui::WidgetInteractionPort>([](const auto&){throw std::runtime_error("Subscriber failed");});good(target.observe_interaction(broken));}
            port=std::make_shared<gui::WidgetInteractionPort>([&](const auto& state){changes.push_back(state);});good(target.observe_interaction(port));if(name=="duplicate")good(target.observe_interaction(port));return {};
        }));
        if(name=="fanout"){auto r=target.snapshot();check(!r&&changes.size()==1,"One failed interaction receiver starved the next subscriber");broken.reset();good(target.set_enabled(false));take(target.snapshot());check(changes.size()==2&&!changes.back().enabled,"A committed state was lost after a subscriber failure");}
        else {
            take(target.snapshot());check(changes.size()==1&&changes[0].alive&&changes[0].enabled&&changes[0].visible,"Initial interaction state was not delivered");
            if(name=="initial"){check(changes[0].bounds==gui::Rect{200,160,160,40}&&changes[0].surface==gui::Size{640,420},"Interaction metadata is not in native surface coordinates");}
            else if(name=="hover"){good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_move,.x=220,.y=180}));check(changes.back().hovered&&inputs>0,"Interaction observation replaced ordinary input handling");}
            else if(name=="press"){good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=220,.y=180}));check(changes.back().pressed&&changes.back().focused,"Press/focus metadata was sampled before input committed");good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=220,.y=180}));check(clicks==1&&!changes.back().pressed,"Observer consumed a button click");}
            else if(name=="duplicate"){take(host.capture_canvas());take(target.snapshot());check(changes.size()==1,"Duplicate subscription or redraw emitted unchanged metadata");}
            else if(name=="hidden"){good(parent.set_visible(false));take(target.snapshot());check(!changes.back().visible,"Ancestor hiding left an observed target visible");}
            else if(name=="disabled"){good(parent.set_enabled(false));take(target.snapshot());check(!changes.back().enabled,"Ancestor disabling did not reach interaction metadata");}
            else if(name=="expired"){port.reset();good(target.set_enabled(false));take(target.snapshot());check(changes.size()==1,"Expired receiver was retained by a widget");}
            else if(name=="retirement"){auto old=target;good(host.unmount_widgets());check(!changes.back().alive&&!old.interaction(),"Retirement lost its final state or addressed a successor generation");}
            else if(name=="measure"){check(take(target.measure(100)).width<=100&&!target.measure(0)&&!target.measure(std::numeric_limits<double>::quiet_NaN()),"Native constrained measurement accepted invalid widths");}
            else if(name=="owner"){bool rejected=false;std::thread t([&]{auto r=target.interaction();rejected=!r&&r.error().code==core::ErrorCode::wrong_executor;});t.join();check(rejected,"Interaction metadata crossed its owner thread");}
            else if(name=="bounds"){good(popup.open_at(0,0));check(take(popup.snapshot()).bounds==gui::Rect{180,80,200,120},"Explicit bubble bounds were repositioned as a menu");}
            else if(name=="passthrough"){good(target.focus());good(popup.open_at(0,0));check(take(target.snapshot()).focused,"A passive bubble stole focus");good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=220,.y=180}));good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=220,.y=180}));check(clicks==1,"A painted tail intercepted the underlying button");}
            else if(name=="capture"){good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=220,.y=180}));good(popup.open_at(0,0));good(popup.close_popup());good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=500,.y=300}));check(released&&canceled==0,"Passive popup handoff canceled the origin's pointer capture");}
            else if(name=="invalid"){check(!target.observe_interaction({})&&!popup.set_popover_options({.preserve_input=true})&&!popup.set_popover_options({.focus_on_open=false,.bounds=gui::Rect{0,0,9000,20}}),"Invalid observation or passive popup contract was admitted");}
            else if(name=="embedded"){check(changes.back().bounds==gui::Rect{30,60,100,30},"Embedded target coordinates lost their containing origin");good(embedded.set_visible(false));take(parent.snapshot());check(!changes.back().visible,"Parent presentation did not flush an embedded observer after hiding");}
            else if(name=="modal"){good(modal.open_at(300,250));good(popup.open_at(0,0));check(take(target.snapshot()).input_blocked&&!target.activate()&&take(target.interaction()).input_blocked,"A passive bubble removed the underlying modal barrier");good(inside.activate());}
            else throw std::runtime_error("Unknown interaction case");
        }
        good(host.request_close());
    }}));
}
}
int main(int argc,char** argv){try{run(argc>1?argv[1]:"initial");std::cout<<"WIDGET_INTERACTION_OK\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
