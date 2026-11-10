#include <iiGUI/Host.h>
#include "../src/Platform/HostForegroundPolicy.h"
#include <stdexcept>
#include <thread>

int main() {
    using ii::core::ErrorCode;
    using ii::gui::HostBackend;
    for (const auto backend : {HostBackend::cocoa, HostBackend::win32, HostBackend::x11, HostBackend::uikit, HostBackend::android, HostBackend::web})
        if (!ii::gui::detail::has_application_foreground(backend))
            throw std::runtime_error("A supported foreground backend was not advertised");
    for (const auto backend : {HostBackend::wayland, HostBackend::none})
        if (ii::gui::detail::has_application_foreground(backend))
            throw std::runtime_error("A backend without foreground events was advertised");
    const auto backend = ii::gui::host_capabilities().backend;
    const bool expected = backend == HostBackend::cocoa || backend == HostBackend::win32 || backend == HostBackend::x11 || backend == HostBackend::uikit || backend == HostBackend::android || backend == HostBackend::web;
    if (ii::gui::supports_application_foreground() != expected)
        throw std::runtime_error("Foreground capability disagrees with the host backend");

    ii::gui::Host host;
    int delivered = 0;
    auto result = host.set_application_foreground_handler([&](bool) { ++delivered; });
    if (expected ? !result : (bool(result) || result.error().code != ErrorCode::unsupported))
        throw std::runtime_error("Foreground handler did not report platform support");
    if (!host.set_application_foreground_handler({}))
        throw std::runtime_error("Removing an unstarted foreground handler failed");
    bool wrong_thread = false;
    std::thread worker([&] {
        auto foreign = host.set_application_foreground_handler({});
        wrong_thread = !foreign && foreign.error().code == ErrorCode::wrong_executor;
    });
    worker.join();
    if (!wrong_thread || delivered)
        throw std::runtime_error("Foreground registration violated ownership or delivered before run");
    if (backend == HostBackend::cocoa || backend == HostBackend::win32 || backend == HostBackend::x11) {
        ii::gui::Host child;
        if (!child.set_application_foreground_handler([](bool) {}))
            throw std::runtime_error("Cannot configure a child foreground handler");
        auto rejected = child.show(host);
        if (rejected || rejected.error().code != ErrorCode::invalid_argument)
            throw std::runtime_error("A secondary host accepted an application foreground handler");
    }
}
