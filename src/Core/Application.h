#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Runtime/Application.h>

namespace ii::gui {
// Bind an OS host to Core's owned event loop and application lifecycle. Core
// owns the installed callbacks; this layer supplies only the native driver.
[[nodiscard]] IIGUI_EXPORT core::Result<std::shared_ptr<Host>> attach_application(
    runtime::Application&, HostOptions = {}, HostCallbacks = {});

template<class View, class... Configuration>
[[nodiscard]] runtime::ApplicationAttachment view_attachment(std::string name,
        std::shared_ptr<Host> host, std::shared_ptr<View> view, Configuration... configuration) {
    return {std::move(name),
        [host,view,... configuration=std::move(configuration)](runtime::Application&) -> core::Result<void> {
            if (!host || !view) return core::fail(core::ErrorCode::invalid_argument,"Native view attachment requires a host and C++ widget");
            return view->attach(*host,configuration...);
        },
        [host,view]() -> core::Result<void> {
            if (!host || !view) return {};
            return view->detach(*host);
        }};
}
} // namespace ii::gui
