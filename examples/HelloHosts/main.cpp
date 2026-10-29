#include "HelloHosts.generated.h"
#include <iiGUI/Application.h>
#include <iiGUI/Desktop.h>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace std::chrono_literals;
namespace {
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
struct State final {
    std::shared_ptr<lvui_generated::HelloHosts> view=std::make_shared<lvui_generated::HelloHosts>();
    std::weak_ptr<ii::gui::Host> host;
    ii::gui::Widget timer,worker;
    bool self_test=false,background=false,resized=false,closed=false;int ticks=0,frames=0;
};
}
ii::core::Result<void> ii::runtime::configure_application(Application& app){
    auto state=std::make_shared<State>();for(const auto& argument:app.arguments())if(argument=="--self-test")state->self_test=true;
    auto options=lvui_generated::HelloHosts::options();options.frame_loop=&app.frame_loop();
    auto host=gui::attach_application(app,options,{
        .opened=[state,&app]{
            auto host=state->host.lock();if(!host)throw std::runtime_error("Host retired before opening");auto mount=state->view->widgets();state->timer=take(mount.find("timer"));state->worker=take(mount.find("worker"));
            const auto greeting=take(take(mount.find("greeting")).snapshot());const auto gpu=take(host->graphics_info());if(greeting.native_class!="iiGUI.Label"||greeting.renderer!="iiGUI"||gpu.api==gui::GraphicsApi::none)throw std::runtime_error("Native C++ widget renderer did not attach");
            take(app.frame_loop().add([state](const auto&){++state->frames;}));
            auto ui=host->executor();auto scheduled=ui.every(250ms,[state,&app]{good(state->timer.set_text("Timer: "+std::to_string(++state->ticks)));if(state->self_test&&state->ticks==2&&state->frames==0)throw std::runtime_error("Background wake traffic starved display frames");if(state->self_test&&state->ticks>=4&&state->background&&state->frames>0){auto host=state->host.lock();auto stats=take(host->stats());auto pixels=take(host->capture_canvas());if(!state->resized||pixels.rgba.empty()||(gui::host_capabilities().display_frames&&(!stats.display_callbacks||!stats.display_synced)))throw std::runtime_error("Host lifecycle, frame or capture check failed");std::cout<<"HOST_SMOKE_OK backend="<<int(gui::host_capabilities().backend)<<" timer=true background=42 resize=true frames="<<state->frames<<" synced="<<stats.display_synced<<" capture=true"<<std::endl;good(app.quit());}});if(!scheduled)throw std::runtime_error("Timer rejected");
            auto background=app.post([state,ui]{auto posted=ui.post([state]{state->background=true;good(state->worker.set_text("Background: 42"));});if(!posted)throw std::runtime_error("Background result rejected");});if(!background)throw std::runtime_error("Background work rejected");
            if(state->self_test){auto load=app.post([ui,stop=app.stop_token()]{const auto until=ii::runtime::Clock::now()+800ms;while(!stop.stop_requested()&&ii::runtime::Clock::now()<until){if(!ui.post([]{}))break;std::this_thread::sleep_for(1ms);}});if(!load)throw std::runtime_error("Frame fairness workload rejected");}
            if(state->self_test){good(host->set_content_size({760,480}));auto watchdog=ui.after(5s,[]{throw std::runtime_error("Host smoke watchdog expired");});if(!watchdog)throw std::runtime_error("Watchdog rejected");}
            std::cout<<"HOST_SMOKE_READY backend="<<int(gui::host_capabilities().backend)<<" adapter="<<gpu.adapter<<" qt=false aot=true"<<std::endl;
        },.resized=[state](gui::Size){state->resized=true;},.closed=[state]{state->closed=true;std::cout<<"HOST_SMOKE_CLOSED"<<std::endl;}});
    if(!host)return std::unexpected(host.error());state->host=*host;
    return app.attach(gui::view_attachment("host-smoke",*host,state->view,
        std::function<void(lvui_generated::HelloHosts::Scope&)>([&app](auto& scope){scope.backend_0.on_close=[&app]{good(app.quit());};})));
}
