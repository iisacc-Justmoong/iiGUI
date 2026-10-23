#include <iiGUI/iiGUI.h>
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace ii;
using namespace std::chrono_literals;
namespace {
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
template<class T> void good(const T& r) {
    if(!r) {
        if constexpr(requires {r.error().cause;}) {if(r.error().cause) std::rethrow_exception(r.error().cause);throw std::runtime_error(r.error().message);}
        else throw std::runtime_error("Core frame task submission rejected");
    }
}
void display(bool fail) {
    runtime::EventLoop events;runtime::FrameLoop frames(events,{.fixed_step=10ms});
    gui::Host host({.title="iiGUI display-synchronized Core frames",.event_loop=&events,.frame_loop=&frames});
    std::uint64_t updates=0,fixed=0,renders=0;runtime::Clock::duration elapsed{};bool closed=false;
    good(frames.add([&](const runtime::FrameContext& c) {
        check(host.is_ui_thread(),"Display callback escaped Core's UI owner");
        check(c.frame==++updates && c.unscaled_delta>=runtime::Clock::duration::zero(),"Frame context sequence or delta invalid");
        elapsed=c.unscaled_elapsed;
        if(fail) throw std::runtime_error("frame callback failure");
        gui::CanvasFrame canvas{.viewport={0,0,80,80},.background={0,0,0,1}};
        const float red=c.frame%2?1:0;
        canvas.panels.push_back({{8,8,64,64},{red,0,0,1},{red,0,0,1},8});good(host.set_canvas(canvas));
    }));
    good(frames.add([&](const auto&){++fixed;},{.phase=runtime::FramePhase::fixed_update}));
    good(frames.add([&](const auto& c){if(c.frame>=12) {auto g=host.graphics_info();good(g);renders=g->scene_renders;good(host.request_close());}},{.phase=runtime::FramePhase::end,.clock=runtime::TickClock::unscaled}));
    auto result=host.run({.opened=[&]{
        gui::CanvasFrame initial{.viewport={0,0,80,80},.background={0,0,0,1}};
        good(host.set_canvas(initial));good(host.capture_canvas());
        // CTest bounds process liveness. Completion below requires actual
        // native frames; a wall timer cannot predict their delivery under load.
    },.closed=[&]{closed=true;}});
    check(closed,"Display host did not retire");
    if(fail) { check(!result && result.error().cause && frames.stats().failed_frames==1,"Frame failure escaped native exception barrier");return; }
    good(result);auto stats=host.stats();good(stats);
    check(updates>=12 && fixed>0 && elapsed>0ms,"Native display source did not drive app/game frame phases");
    check(stats->display_callbacks==frames.stats().frames && !stats->display_synced && stats->display_interval>0ms,"Display cadence not accounted or not stopped");
    // Canvas teardown happens on close; the frame tests inspect actual GPU pixels separately.
    check(!host.graphics_info() && renders>=5,"Frame updates did not reach GPU, or closed host exposed content");
    std::cout<<"DISPLAY_FRAME_OK frames="<<updates<<" fixed="<<fixed<<" cadence_hz="<<1/std::chrono::duration<double>(stats->display_interval).count()
        <<" update_hz="<<double(updates-1)/std::chrono::duration<double>(elapsed).count()<<" stopped=true\n";
}
void contracts() {
    runtime::EventLoop events,other;runtime::FrameLoop frames(events);
    bool rejected=false;try{gui::Host host({.event_loop=&other,.frame_loop=&frames});}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"Frame source accepted another event domain");
    gui::Host idle;good(idle.run({.opened=[&]{check(idle.stats()->display_callbacks==0 && !idle.stats()->display_synced,"Idle app allocated a frame source");good(idle.request_close());}}));
}
void widgets() {
    runtime::EventLoop events;runtime::FrameLoop frames(events);
    gui::Host host({.title="iiGUI coalesced frame presentation",.event_loop=&events,.frame_loop=&frames});
    gui::Widget text;std::uint64_t presentations=0,delivered=0;
    good(frames.add([&](const auto& c){delivered=c.frame;if(c.frame%4==0) good(text.set_text("Frame: "+std::to_string(c.frame)));}));
    good(frames.add([&](const auto& c){if(c.frame>=24){auto info=host.graphics_info();good(info);presentations=info->presentations;good(host.request_close());}},
        {.phase=runtime::FramePhase::end,.clock=runtime::TickClock::unscaled}));
    good(host.run({.opened=[&] {
        auto mount=host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{auto w=b.label({},"frame",{.text="Frame: 0"});if(!w)return std::unexpected(w.error());text=*w;return {};});
        good(mount); // The outer process timeout bounds missing native frames.
    }}));
    check(delivered>=24 && presentations<=delivered/4+5,"Unchanged frames or duplicate OS invalidation submitted redundant widget presentations");
    std::cout<<"COALESCED_FRAME_OK callbacks="<<delivered<<" presentations="<<presentations<<'\n';
}
}
int main(int argc,char** argv) {
    try {const std::string name=argc>1?argv[1]:"display";if(name=="display") display(false);else if(name=="failure") display(true);else if(name=="contracts") contracts();else if(name=="widgets") widgets();else throw std::runtime_error("Unknown frame host test");}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
