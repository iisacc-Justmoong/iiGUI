#pragma once
#include <iiGUI/Application.h>

struct android_app;
namespace ii::gui {
// Invoke from NDK native_app_glue's android_main, on its native Looper thread.
// The borrowed Android activity outlives Core's Application and all attachments.
[[nodiscard]] IIGUI_EXPORT core::Result<int> run_android_application(android_app*,
    std::function<core::Result<void>(runtime::Application&)> configure);
}
