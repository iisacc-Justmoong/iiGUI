#import <AppKit/AppKit.h>
#include <iiGUI/Host.h>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>

namespace {
void stop(NSApplication* app) {
    [app stop:nil];
    [app postEvent:[NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSZeroPoint
        modifierFlags:0 timestamp:0 windowNumber:0 context:nil subtype:0 data1:0 data2:0] atStart:YES];
}
struct State final {
    std::unique_ptr<ii::gui::Host> host;
    std::string error;
    bool opened=false, posted=false, closed=false, inside_callback=false;
    __strong id<NSApplicationDelegate> delegate=nil;
    __strong NSMenu* menu=nil;
};
}

int main() {
    @autoreleasepool {
        NSApplication* app=NSApplication.sharedApplication;
        [app setActivationPolicy:NSApplicationActivationPolicyProhibited];
        auto state=std::make_shared<State>();
        state->delegate=(id<NSApplicationDelegate>)[NSObject new];
        app.delegate=state->delegate;
        app.mainMenu=[NSMenu new];
        state->host=std::make_unique<ii::gui::Host>();
        if(!state->host->set_application_driver(true)) return 1;
        dispatch_async(dispatch_get_main_queue(), ^{
            state->menu=app.mainMenu;
            auto opened=[state,app] {
                state->opened=true;state->inside_callback=true;
                if(app.delegate!=state->delegate || app.mainMenu!=state->menu)
                    state->error="External host changed the owning application's delegate or menu";
                const auto menu_result=state->host->set_app_menu({});
                if(menu_result || menu_result.error().code!=ii::core::ErrorCode::invalid_argument || app.mainMenu!=state->menu)
                    state->error="External host took ownership of the application menu";
                const auto snapshot=state->host->snapshot();
                if(!snapshot || snapshot->visible)state->error="External driver became visible";
                auto posted=state->host->executor().post([state]{state->posted=true;});
                if(!posted)state->error="External host rejected an immediate task";
                auto timer=state->host->executor().after(std::chrono::milliseconds(10),[state]{
                    if(!state->host->request_close())state->error="External driver did not close";
                });
                if(!timer)state->error="External host rejected a timer";
                state->inside_callback=false;
            };
            auto closed=[state,app] {
                state->closed=true;
                if(state->inside_callback || !app.running || app.delegate!=state->delegate || app.mainMenu!=state->menu)
                    state->error="External host close interfered with the owning application";
                const auto snapshot=state->host->snapshot();
                if(!snapshot || !snapshot->closed)state->error="External host did not retire before notification";
                state->host.reset();
                stop(app);
            };
            auto shown=state->host->show_external({.opened=std::move(opened),.closed=std::move(closed)});
            if(!shown){state->error=shown.error().message;stop(app);}
        });
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,5*NSEC_PER_SEC),dispatch_get_main_queue(), ^{
            if(!state->closed){state->error="External event loop timed out";if(state->host)(void)state->host->request_close();stop(app);}
        });
        [app run];
        if(!state->error.empty() || !state->opened || !state->posted || !state->closed){
            std::fprintf(stderr,"%s\n",state->error.empty()?"External host callbacks were lost":state->error.c_str());
            return 1;
        }
        state->host.reset();
        return 0;
    }
}
