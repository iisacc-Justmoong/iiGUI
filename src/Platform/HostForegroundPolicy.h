#pragma once

#include <iiGUI/Host.h>

namespace ii::gui::detail {
[[nodiscard]] constexpr bool has_application_foreground(HostBackend backend) noexcept {
    return backend == HostBackend::cocoa || backend == HostBackend::win32 || backend == HostBackend::x11 || backend == HostBackend::uikit || backend == HostBackend::android || backend == HostBackend::web;
}
} // namespace ii::gui::detail
