#pragma once

#include <iiGUI/Host.h>

namespace ii::gui::detail {
[[nodiscard]] constexpr HostCapabilities host_capabilities_for(HostBackend backend) noexcept {
    switch (backend) {
    case HostBackend::web: return {backend,true,true,false,false,true,true,false,false};
    case HostBackend::android: return {backend,true,true,false,false,true,true,false,false};
    case HostBackend::wayland: return {backend,true,true,true,true,true,true,true,true};
    case HostBackend::win32: return {backend,true,true,true,true,false,true,true,true};
    case HostBackend::uikit: return {backend,true,true,true,true,true,true,true,true};
    case HostBackend::cocoa: return {backend,true,true,true,true,true,true,true,true};
    case HostBackend::x11: return {backend,true,true,true,true,false,true,true,true};
    case HostBackend::none: return {};
    }
    return {};
}
} // namespace ii::gui::detail
