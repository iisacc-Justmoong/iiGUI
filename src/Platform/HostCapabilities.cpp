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
bool supports_custom_window_chrome() noexcept {
    const auto presentation=window_chrome_presentation(host_capabilities().backend);
    return presentation.native_controls||presentation.client_controls;
}
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__) || (defined(__APPLE__) && TARGET_OS_IPHONE)
core::Result<void> Host::begin_window_drag() {
    return core::fail(core::ErrorCode::unsupported,"Custom window chrome is unavailable on this host");
}
#endif
}
