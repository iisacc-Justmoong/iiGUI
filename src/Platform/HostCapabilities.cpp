#include <iiGUI/Host.h>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
namespace ii::gui {
HostCapabilities host_capabilities() noexcept {
#if defined(__EMSCRIPTEN__)
    return {HostBackend::web,true,true,false,false,true,true,false,false};
#elif defined(__ANDROID__)
    return {HostBackend::android,true,true,false,false,true,false,false,false};
#elif defined(IIGUI_WAYLAND)
    return {HostBackend::wayland,true,true,true,false,true,true,true,false};
#elif defined(_WIN32)
    return {HostBackend::win32,true,true,true,true,false,true,true,true};
#elif defined(__APPLE__) && TARGET_OS_IPHONE
    return {HostBackend::uikit,true,true,true,false,true,true,true,true};
#elif defined(__APPLE__)
    return {HostBackend::cocoa,true,true,true,true,true,true,true,true};
#elif defined(__linux__)
    return {HostBackend::x11,true,true,true,true,false,true,true,false};
#else
    return {};
#endif
}
}
