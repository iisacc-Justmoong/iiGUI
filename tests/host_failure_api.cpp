#include <iiGUI/Host.h>
#include <stdexcept>
#include <thread>

int main() {
    ii::gui::Host host;
    int calls = 0;
    if (!host.set_failure_handler([&](const ii::core::Error&) { ++calls; }) ||
        !host.set_failure_handler([&](const ii::core::Error&) { ++calls; }))
        throw std::runtime_error("Cannot configure or replace the native failure handler");
    bool wrong_thread = false;
    std::thread worker([&] {
        auto result = host.set_failure_handler({});
        wrong_thread = !result && result.error().code == ii::core::ErrorCode::wrong_executor;
    });
    worker.join();
    if (!wrong_thread || calls) throw std::runtime_error("Failure handler violated owner-thread or early-delivery rules");
}
