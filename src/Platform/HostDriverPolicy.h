#pragma once
#include <iiGUI/Host.h>
namespace ii::gui::detail {
[[nodiscard]] inline bool has_application_driver(HostBackend backend) noexcept {
    return backend == HostBackend::cocoa || backend == HostBackend::win32 ||
           backend == HostBackend::x11 || backend == HostBackend::wayland || backend == HostBackend::uikit;
}
// A driver still owns a native event-pump anchor, but never presents it.
[[nodiscard]] inline bool show_native_window(bool application_driver) noexcept {
    return !application_driver;
}
// X11 normally waits for MapNotify before opening. An unmapped driver cannot.
[[nodiscard]] inline bool deliver_opened_before_visible(bool application_driver) noexcept {
    return application_driver;
}
} // namespace ii::gui::detail
