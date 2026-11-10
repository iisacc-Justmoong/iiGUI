#include <iiGUI/Host.h>
#include <stdexcept>
#include <thread>

int main() {
    ii::gui::Host host;
    int calls=0;
    if (!host.set_application_activity_handler([&](bool) { ++calls; }))
        throw std::runtime_error("Cannot configure application activity");
    if (!host.set_application_activity_handler([&](bool) { ++calls; }))
        throw std::runtime_error("Cannot replace application activity handler");
    bool wrong_thread=false;
    std::thread worker([&] {
        auto result=host.set_application_activity_handler({});
        wrong_thread=!result && result.error().code==ii::core::ErrorCode::wrong_executor;
    });
    worker.join();
    if (!wrong_thread || calls)
        throw std::runtime_error("Activity configuration violated its owner or emitted before run");
    ii::gui::Host child;
    if (!child.set_application_activity_handler([](bool) {}))
        throw std::runtime_error("Cannot configure child activity policy");
    auto rejected=child.show(host);
    if (rejected || rejected.error().code!=ii::core::ErrorCode::invalid_argument)
        throw std::runtime_error("A secondary host accepted an application activity handler");
}
