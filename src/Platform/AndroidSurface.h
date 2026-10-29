#pragma once
#include <iiGUI/Host.h>
#include <android/input.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>

namespace ii::gui::detail {
android_app* android_application() noexcept;
struct AndroidSurfaceParent final {
    android_app* app=nullptr;
    ANativeWindow* window=nullptr; // Borrowed until APP_CMD_TERM_WINDOW.
    double scale=1;
    bool active=false;
};
enum class AndroidSurfaceEventKind { input, window, activation, resize };
struct AndroidSurfaceEvent final {
    AndroidSurfaceEventKind kind;
    AInputEvent* input=nullptr;
};
}
