#include <iiGUI/Host.h>
#include <atomic>
#include <thread>

int main() {
    ii::gui::Host host;
    auto result=host.external_parent_resized();
    if (result || (result.error().code!=ii::core::ErrorCode::closed &&
                   result.error().code!=ii::core::ErrorCode::unsupported)) return 1;
#if !defined(__EMSCRIPTEN__)
    std::atomic_bool rejected=false;
    std::thread other([&] {
        auto foreign=host.external_parent_resized();
        rejected=!foreign && foreign.error().code==ii::core::ErrorCode::wrong_executor;
    });
    other.join();
    return rejected?0:2;
#else
    return 0;
#endif
}
