#include <iiGUI/Android.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <exception>

namespace {
void log_error(const char* message) noexcept {
    __android_log_write(ANDROID_LOG_ERROR, "iiGUI", message);
}
void finish_if_running(android_app* native) noexcept {
    if (native && native->activity && !native->destroyRequested)
        ANativeActivity_finish(native->activity);
}
}

extern "C" void android_main(android_app* native) {
    try {
        const auto result = ii::gui::run_android_application(native, ii::runtime::configure_application);
        if (result) return;
        log_error(result.error().message.c_str());
        if (result.error().cause) {
            try { std::rethrow_exception(result.error().cause); }
            catch (const std::exception& cause) { log_error(cause.what()); }
            catch (...) { log_error("Unknown Android application failure"); }
        }
        finish_if_running(native);
    } catch (const std::exception& error) {
        log_error(error.what());
        finish_if_running(native);
    } catch (...) {
        log_error("Unknown Android application entry failure");
        finish_if_running(native);
    }
}
