#include <iiGUI/Host.h>
#include <windows.h>
#include <chrono>
#include <memory>
#include <string>

// This Windows-only integration check uses a hidden driver, so it does not
// display an application window. The surrounding message loop remains owned by
// the test, just as it would be owned by an embedding application.
int main() {
    using ii::gui::Host;
    std::unique_ptr<Host> host=std::make_unique<Host>();
    std::string failure;
    bool opened=false, posted=false, closed=false, inside_native_callback=false;
    const auto original_dpi=GetThreadDpiAwarenessContext();
    if(!host->set_application_driver(true)) return 1;
    auto result=host->show_external({
        .opened=[&] {
            opened=true;
            auto state=host->snapshot();
            if(!state || state->visible) failure="External driver became visible";
            if(!host->executor().post([&]{posted=true;})) failure="Immediate task was rejected";
            if(!host->executor().after(std::chrono::milliseconds(10),[&]{
                inside_native_callback=true;
                if(!host->request_close()) failure="External host rejected close";
                // Reenter the foreign message loop while the Core task is still
                // active. The close observer must wait for this callback.
                MSG nested{};
                for(int count=0;count<16 && PeekMessageW(&nested,nullptr,0,0,PM_REMOVE);++count) {
                    if(nested.message==WM_QUIT) { failure="Nested loop received an early quit"; break; }
                    TranslateMessage(&nested);
                    DispatchMessageW(&nested);
                }
                if(closed) failure="Close ran inside a nested native callback";
                inside_native_callback=false;
            })) failure="Requested timer was rejected";
        },
        .closed=[&] {
            closed=true;
            const auto state=host->snapshot();
            if(inside_native_callback || !state || !state->closed) failure="Close was not safely deferred";
            host.reset();
            PostQuitMessage(0);
        }
    });
    if(!result) return 2;
    if(!AreDpiAwarenessContextsEqual(original_dpi,GetThreadDpiAwarenessContext())) return 3;
    MSG message{};
    int status=0;
    while((status=GetMessageW(&message,nullptr,0,0))>0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if(status<0 || !failure.empty() || !opened || !posted || !closed || host) return 4;
    return 0;
}
