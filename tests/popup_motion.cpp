#include <iiGUI/iiGUI.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace ii;
using namespace std::chrono_literals;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T> void good(const T& r){if(!r){if constexpr(requires{r.error().cause;}){if(r.error().cause)std::rethrow_exception(r.error().cause);throw std::runtime_error(r.error().message);}else throw std::runtime_error("Task admission failed");}}
template<class T>T take(core::Result<T> r){good(r);return std::move(*r);}
gui::PopoverOptions options(std::shared_ptr<gui::MotionPreferences> preferences={}){
    gui::PopoverOptions o{.width=260,.height=160,.padding=0,.margins=0,.corner_radius=20,.surface_opacity=1,.surface_color=gui::WidgetColor{1,1,1,1},.modal=true,.dim=true,.clip_content=true,.placement=gui::PopoverPlacement::centered};
    o.transition=gui::PopoverTransition{.enter_duration=420ms,.exit_duration=150ms,.enter_easing=animation::Easing::out_back,.exit_easing=animation::Easing::in_cubic,.overshoot=1.45,.collapsed_scale=.92,.preferences=std::move(preferences)};return o;
}
struct Tree final {
    gui::Widget root,popup,button,outside;gui::WidgetMount mount;int clicks=0,closed=0;
    void attach(gui::Host& host,gui::PopoverOptions o){mount=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{
        root=take(b.absolute_stack({},"root",{640,420}));outside=take(b.button(root,"outside",{.text="Outside"},[this]{++clicks;}));good(outside.set_frame({10,10,100,30}));
        popup=take(b.popover(root,"popup",std::move(o),{.closed=[this]{++closed;}}));auto content=take(b.absolute_stack(popup,"content",{260,160}));
        button=take(b.button(content,"action",{.text="Action"},[this]{++clicks;}));good(button.set_frame({0,60,80,30}));return {};}));}
};
void lifecycle(){
    gui::Host host;Tree tree;std::uint64_t stopped_callbacks=0;int phase=0;
    runtime::Clock::time_point idle_since{};
    good(host.run({.opened=[&]{
        tree.attach(host,options());check(!take(host.stats()).display_synced,"Idle View allocated a display source");
        good(tree.popup.open_at(0,0));auto start=take(tree.popup.snapshot());check(start.opened&&start.transitioning&&start.reveal_progress==0,"Enter did not start from the collapsed state");
        // Timers schedule observations, never infer how many native frames have
        // arrived. A delayed display pulse may finish a real-time track at once.
        good(host.executor().every(10ms,[&]{
            auto state=take(tree.popup.snapshot());auto stats=take(host.stats());
            if(phase==0){
                if(state.transitioning||state.reveal_progress!=1||stats.display_synced)return;
                check(stats.display_callbacks>0&&take(host.graphics_info()).scene_renders>0,"Native motion did not reach a display/GPU frame");
                stopped_callbacks=stats.display_callbacks;idle_since=runtime::Clock::now();phase=1;
            }else if(phase==1){
                if(runtime::Clock::now()-idle_since<60ms)return;
                check(stats.display_callbacks==stopped_callbacks,"Idle motion kept driving frames");
                good(tree.popup.close_popup());auto exit=take(tree.popup.snapshot());
                check(!exit.opened&&exit.visible&&exit.exiting&&exit.transitioning&&exit.input_blocked&&tree.closed==1,"Close did not revoke input before the exit pixels retired");phase=2;
            }else if(phase==2){
                if(state.visible||state.transitioning||stats.display_synced)return;
                check(state.reveal_progress==0&&stats.display_callbacks>stopped_callbacks,"Exit did not retire through native display frames");phase=3;good(host.request_close());
            }
        }));
    }}));check(phase==3,"Native popup motion did not finish its lifecycle");
}
void deterministic(const std::string& name){
    runtime::EventLoop events;runtime::FrameLoop frames(events);gui::Host host({.event_loop=&events,.frame_loop=&frames});Tree tree;auto prefs=std::make_shared<gui::MotionPreferences>();
    good(host.run({.opened=[&]{tree.attach(host,options(prefs));good(tree.outside.focus());good(tree.popup.open_at(0,0));auto now=runtime::Clock::now();
        (void)frames.step(now+210ms);auto middle=take(tree.popup.snapshot());check(middle.reveal_progress>1.04&&middle.reveal_progress<1.08,"Core OutBack did not overshoot on the frame boundary");
        if(name=="reverse"){
            auto before=middle.reveal_progress;good(tree.popup.close_popup());auto reversing=take(tree.popup.snapshot());check(reversing.reveal_progress==before&&reversing.exiting,"Close jumped to a new start value");
            (void)frames.step(now+280ms);auto exit=take(tree.popup.snapshot());check(exit.reveal_progress>0&&exit.reveal_progress<before,"Exit did not use its own cubic track");
            auto value=exit.reveal_progress;good(tree.popup.open_at(0,0));check(take(tree.popup.snapshot()).reveal_progress==value,"Reopen discarded the current animated value");
            (void)frames.step(now+900ms);check(take(tree.popup.snapshot()).reveal_progress==1&&!take(tree.popup.snapshot()).transitioning,"Reopened track did not settle");
        }else if(name=="preferences"){
            good(prefs->set_reduced_motion(true));check(take(tree.popup.snapshot()).reveal_progress==1&&!take(tree.popup.snapshot()).transitioning,"Live reduced motion did not complete the current track synchronously");
            good(tree.popup.close_popup());check(!take(tree.popup.snapshot()).visible,"Reduced motion retained an exit frame");good(prefs->set_reduced_motion(false));good(prefs->set_speed(4));good(tree.popup.open_at(0,0));
            (void)frames.step(now+320ms);check(take(tree.popup.snapshot()).reveal_progress==1,"Shared motion speed was not applied");
            good(prefs->set_enabled(false));good(tree.popup.close_popup());check(!take(tree.popup.snapshot()).transitioning,"Disabled motion registered an exit");
        }else if(name=="input"){
            // Shrink far enough to distinguish the visual left edge from its logical edge.
            good(tree.popup.close_popup());(void)frames.step(now+410ms);good(tree.popup.open_at(0,0));
            auto logical=take(tree.button.snapshot());auto visual=logical.visual_bounds;check(visual.width<logical.bounds.width&&visual.x>logical.bounds.x,"Motion did not expose transformed accessibility bounds");
            good(tree.mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=visual.x+visual.width/2,.y=visual.y+visual.height/2}));
            good(tree.mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=visual.x+visual.width/2,.y=visual.y+visual.height/2}));check(tree.clicks==1,"Visual control coordinates did not map to its logical input");
            good(tree.popup.close_popup());check(!tree.button.activate(),"Exit accepted direct action");check(tree.clicks==1&&take(tree.outside.snapshot()).focused,"Exit accepted input or lost restored focus");
        }else if(name=="paused"){
            (void)frames.step(runtime::Clock::now()+500ms);good(frames.set_time_scale(0));frames.set_paused(true);good(tree.popup.close_popup());
            (void)frames.step(runtime::Clock::now()+700ms);check(!take(tree.popup.snapshot()).visible,"Game pause froze unscaled UI motion");
        }else if(name=="teardown"){
            auto before=frames.stats().active_ticks;good(host.unmount_widgets());(void)frames.step(runtime::Clock::now()+500ms);check(!tree.popup.snapshot()&&frames.stats().active_ticks<before,"Unmount retained a Core motion callback or stale widgets");
        }else if(name=="hidden"){
            good(tree.popup.close_popup());check(take(tree.popup.snapshot()).exiting,"Exit was not active before parent hide");good(tree.root.set_visible(false));auto hidden=take(tree.popup.snapshot());check(!hidden.visible&&!hidden.transitioning,"Hidden owner retained an exit track");good(tree.root.set_visible(true));check(!take(tree.popup.snapshot()).visible,"Showing owner resurrected an old exit");
        }else throw std::runtime_error("Unknown deterministic motion test");
        good(host.request_close());
    }}));
}
void limits(){
    runtime::EventLoop events;runtime::FrameLoop frames(events,{.capacity=1});gui::Host host({.event_loop=&events,.frame_loop=&frames});
    good(host.run({.opened=[&]{auto old=take(host.mount_widgets([](gui::WidgetBuilder& b)->core::Result<void>{auto label=b.label({},"old",{.text="Preserved"});if(!label)return std::unexpected(label.error());return {};}));
        auto rejected=host.mount_widgets([](gui::WidgetBuilder& b)->core::Result<void>{auto popup=b.popover({},"tooManyTicks",options());if(!popup)return std::unexpected(popup.error());return {};});
        check(!rejected&&rejected.error().code==core::ErrorCode::full&&bool(old.find("old")),"Frame capacity failure replaced the previous View");good(host.request_close());}}));
    gui::Host bounded;std::vector<gui::Widget> popups;
    good(bounded.run({.opened=[&]{
        auto mount=take(bounded.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{auto root=take(b.vstack({},"root"));for(int i=0;i<17;++i){auto o=options();o.transition->enter_duration=0ms;o.transition->exit_duration=1s;popups.push_back(take(b.popover(root,"popup"+std::to_string(i),o)));}return {};}));
        for(int i=0;i<8;++i)good(popups[i].open_at(0,0));for(int i=0;i<8;++i)good(popups[i].close_popup());
        for(int i=8;i<16;++i)good(popups[i].open_at(0,0));good(popups[8].close_popup());
        auto result=popups[16].open_at(0,0);check(!result&&result.error().code==core::ErrorCode::full,"Exiting visuals bypassed the domain's sixteen-popup limit");
        good(bounded.unmount_widgets());good(bounded.request_close());
    }}));
}
}
int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"lifecycle";if(name=="lifecycle")lifecycle();else if(name=="limits")limits();else deterministic(name);std::cout<<"POPUP_MOTION_OK "<<name<<'\n';}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
