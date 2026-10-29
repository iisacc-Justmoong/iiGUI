#include "HelloWorld.generated.h"
#include <iiGUI/Application.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace std::chrono_literals;
namespace {
using View = lvui_generated::HelloWorld;
void check(bool value,const char* why) { if (!value) throw std::runtime_error(why); }
template<class T> T take(ii::core::Result<T> result) {
    if (!result) {
        if (result.error().cause) std::rethrow_exception(result.error().cause);
        throw std::runtime_error(result.error().message);
    }
    return std::move(*result);
}
void good(ii::core::Result<void> result) {
    if (result) return;
    if (result.error().cause) std::rethrow_exception(result.error().cause);
    throw std::runtime_error(result.error().message);
}
template<class T> void submitted(const T& value) { check(bool(value),"Core submission rejected"); }
struct HelloState final {
    bool self_test=false,background=false,resized=false,closed=false,clicked=false;
    int ticks=0;
    std::shared_ptr<ii::gui::Host> host;
    std::shared_ptr<View> view=std::make_shared<View>();
    ii::gui::Widget greeting,timer,worker;
};
}

// There is no application-owned main() or worker pool. The Core entrypoint
// constructs Application, calls this configuration, then owns exec/teardown.
ii::core::Result<void> ii::runtime::configure_application(Application& app) {
    auto state=std::make_shared<HelloState>();
    if (app.arguments().size()==2 && app.arguments()[1]=="--self-test") state->self_test=true;
    else if (app.arguments().size()!=1) return core::fail(core::ErrorCode::invalid_argument,"Usage: HelloWorld [--self-test]");
    const auto ui=app.event_loop().sender();
    state->host=take(gui::attach_application(app,View::options(),{
        .opened=[state,ui,&app] {
            auto mounted=state->view->widgets();
            state->greeting=take(mounted.find("greeting"));
            state->timer=take(mounted.find("timer")); state->worker=take(mounted.find("worker"));
            check(&take(state->host->event_loop()).get()==&app.event_loop(),"GUI did not borrow the Core-owned event loop");
            check(take(state->host->snapshot()).visible,"Native window is not visible");
            for (const auto& item:take(mounted.snapshot())) {
                check(item.attached && item.styled && item.size.width>0 && item.size.height>0,"LVRS native widget is not displayed");
                std::cout<<"NATIVE_WIDGET id="<<item.id<<" class="<<item.native_class
                    <<" width="<<item.size.width<<" height="<<item.size.height<<" font="<<item.font_family<<'\n';
            }
            auto greeting=take(state->greeting.snapshot());
            check(greeting.native_class=="iiGUI.Label" && greeting.renderer=="iiGUI" && greeting.font_family=="Pretendard","Self-rendered LVRS typography not attached to final app");
            const auto graphics=take(state->host->graphics_info());
            check(graphics.api==gui::GraphicsApi::metal || graphics.api==gui::GraphicsApi::direct3d11 ||
                  graphics.api==gui::GraphicsApi::vulkan,"Self-rendered widgets did not use a native accelerator");
            std::cout<<"NATIVE_GRAPHICS host="<<HELLO_WORLD_NATIVE_HOST<<" api="<<unsigned(graphics.api)
                     <<" hardware="<<graphics.hardware<<" adapter="<<graphics.adapter<<'\n';
            submitted(ui.every(250ms,[state] {
                good(state->timer.set_text("Timer: "+std::to_string(++state->ticks)));
                if (state->self_test && state->ticks>=2 && state->background) {
                    auto root=take(take(state->view->widgets().find("root")).snapshot());
                    check(state->resized && std::abs(root.size.width-820)<1,"Native stack did not resize");
                    auto greet=take(state->view->widgets().find("greet"));
                    good(greet.activate()); good(greet.activate());
                    check(HelloBackend::created==1 && HelloBackend::greetings==2 && HelloBackend::destroyed==0,"Backend recreated between actions");
                    check(take(state->greeting.snapshot()).text=="Hello World · LVUI!","Native action did not update text");
                    good(take(state->view->widgets().find("close")).activate());
                }
            }));
            submitted(app.post([state,ui] {
                int result=0; for (int i=0;i<7;++i) result+=6;
                auto delivery=ui.post([state,result] {
                    good(state->worker.set_text("Background result: "+std::to_string(result)));
                    state->background=result==42;
                });
                if (!delivery && state->self_test) throw std::runtime_error("Core result delivery rejected");
            }));
            if (state->self_test) {
                submitted(ui.after(5s,[] { throw std::runtime_error("Core native widget watchdog"); }));
                good(state->host->set_content_size({820,460}));
            }
        },
        .resized=[state](gui::Size size) { state->resized=state->resized || (std::abs(size.width-820)<1 && std::abs(size.height-460)<1); },
        .closed=[state] {
            state->closed=true;
            check(HelloBackend::destroyed==0,"Native window destroyed the Core-owned C++ backend too early");
            auto late=state->greeting.set_text("late");
            check(!late && late.error().code==core::ErrorCode::closed,"Closing did not revoke native handles");
        }}));
    auto verification=app.attach({"verification",[](Application&) -> core::Result<void> { return {}; },[state]() -> core::Result<void> {
        check(HelloBackend::created==1 && HelloBackend::destroyed==1,"Core attachment did not retire one backend");
        if (state->self_test) {
            check(state->ticks>=2 && state->background && state->resized && state->closed && state->clicked,"Lifecycle milestone missing");
            std::cout<<"HELLO_WORLD_AOT_OK native="<<HELLO_WORLD_NATIVE_HOST<<" runtime=iiSDKCore widget=C++23 theme=LVRS timer=true background=42 resize=true actions=2 closed=true scopes=1/1 core_entry=true\n";
        }
        return {};
    }});
    if (!verification) return verification;
    return app.attach(gui::view_attachment("main-view",state->host,state->view,
        std::function<void(View::Scope&)>{[state,&app](View::Scope& scope) {
            scope.backend_0.on_greet=[state] { good(state->greeting.set_text("Hello World · LVUI!")); };
            scope.backend_0.on_close=[state,&app] { state->clicked=true; good(app.quit()); };
        }}));
}
