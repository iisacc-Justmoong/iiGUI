#include <iiGUI/Android.h>
#include <android_native_app_glue.h>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
enum class Mode { success, failed, rejected, exception, unknown_exception };
Mode mode = Mode::success;
int calls = 0, configured = 0, finishes = 0;
android_app* received = nullptr;
std::vector<std::string> logs;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}

namespace ii::runtime {
ii::core::Result<void> configure_application(Application&) { ++configured; return {}; }
}

namespace ii::gui {
core::Result<int> run_android_application(android_app* native,
        std::function<core::Result<void>(runtime::Application&)> configure) {
    ++calls;
    received = native;
    if (mode == Mode::exception) throw std::runtime_error("entry exception");
    if (mode == Mode::unknown_exception) throw 4;
    if (mode == Mode::failed) {
        native->destroyRequested = 1; // The normal iiGUI host already finished the Activity.
        return core::fail(core::ErrorCode::failed, "application failed",
                          std::make_exception_ptr(std::runtime_error("configuration cause")));
    }
    if (mode == Mode::rejected)
        return core::fail(core::ErrorCode::invalid_argument, "native activity rejected");
    runtime::Application application({}, {.workers = {.workers = 1}});
    if (auto result = configure(application); !result) return std::unexpected(result.error());
    return 7;
}
}

extern "C" int __android_log_write(int, const char*, const char* text) {
    logs.emplace_back(text);
    return 0;
}
extern "C" void ANativeActivity_finish(ANativeActivity*) { ++finishes; }

int main() {
    ANativeActivity activity;
    android_app app{&activity, 0};
    android_main(&app);
    check(calls == 1 && received == &app && configured == 1 && logs.empty() && finishes == 0,
          "Native entry did not pass the application and its configuration to iiGUI");

    mode = Mode::failed;
    android_main(&app);
    check(logs.size() == 2 && logs[0] == "application failed" && logs[1] == "configuration cause" && finishes == 0,
          "Application failure or its cause was lost, or a completed host was finished twice");

    logs.clear();
    app.destroyRequested = 0;
    mode = Mode::rejected;
    android_main(&app);
    check(logs.size() == 1 && logs[0] == "native activity rejected" && finishes == 1,
          "An early host rejection left the activity running");

    logs.clear();
    mode = Mode::exception;
    android_main(&app);
    check(logs.size() == 1 && logs[0] == "entry exception" && finishes == 2,
          "An unexpected entry exception escaped or left the activity running");

    logs.clear();
    mode = Mode::unknown_exception;
    app.destroyRequested = 1;
    android_main(&app);
    check(logs.size() == 1 && finishes == 2,
          "An unknown exception escaped or finished an already-destroying activity");
    android_main(nullptr);
    check(finishes == 2, "A null activity was finished after an exception");
}
