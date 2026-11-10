#include <iiGUI/Host.h>
#include "HostCapabilitiesPolicy.h"
#include "HostDriverPolicy.h"
#include "HostForegroundPolicy.h"
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
namespace ii::gui {
void require_application_driver_abi_v1() noexcept {}
HostCapabilities host_capabilities() noexcept {
#if defined(__EMSCRIPTEN__)
    return detail::host_capabilities_for(HostBackend::web);
#elif defined(__ANDROID__)
    return detail::host_capabilities_for(HostBackend::android);
#elif defined(IIGUI_WAYLAND)
    return detail::host_capabilities_for(HostBackend::wayland);
#elif defined(_WIN32)
    return detail::host_capabilities_for(HostBackend::win32);
#elif defined(__APPLE__) && TARGET_OS_IPHONE
    return detail::host_capabilities_for(HostBackend::uikit);
#elif defined(__APPLE__)
    return detail::host_capabilities_for(HostBackend::cocoa);
#elif defined(__linux__)
    return detail::host_capabilities_for(HostBackend::x11);
#else
    return {};
#endif
}
bool supports_application_driver() noexcept {
    return detail::has_application_driver(host_capabilities().backend);
}
bool supports_application_foreground() noexcept {
    return detail::has_application_foreground(host_capabilities().backend);
}
}
