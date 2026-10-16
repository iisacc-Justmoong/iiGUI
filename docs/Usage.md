# C++23 host usage

```cmake
find_package(iiGUI 0.4 CONFIG REQUIRED)
add_executable(MyApp WIN32 MACOSX_BUNDLE main.cpp)
target_link_libraries(MyApp PRIVATE iiGUI::iiGUI)
if(MSVC)
    target_link_options(MyApp PRIVATE /ENTRY:mainCRTStartup)
endif()
```

```cpp
#include <iiGUI/iiGUI.h>
#include <iiSDKCore/Runtime/Runtime.h>
#include <chrono>
#include <stdexcept>

int main() {
    using namespace std::chrono_literals;
    ii::gui::Host host({.title="My native app"});
    auto ui = host.executor();
    ii::runtime::Runtime workers({.workers=1});
    auto result = host.run({.opened=[&] {
        if (!workers.post([&, ui] {
            const int answer = 6 * 7;
            (void)ui.post([&, answer] {
                (void)host.set_text("Answer: " + std::to_string(answer));
            });
        })) throw std::runtime_error("Worker admission failed");
        if (!ui.after(2s, [&] { (void)host.request_close(); }))
            throw std::runtime_error("Timer admission failed");
    }});
    auto joined = workers.join();
    return result && joined ? 0 : 1;
}
```

`UiExecutor` also satisfies Core's structural TaskExecutor concept and can be
used for explicit reply delivery and scoped posts. Do not send UI callbacks
directly to an unrelated EventLoop: it would bypass this host's native wakeup.
Use UiExecutor for both immediate work and timer changes.

Always check Submission/Result where application correctness depends on it.
The runnable HelloGUI example handles admission errors and has an explicit
self-test watchdog. It does not persist generated files or application data.

HelloGUI's main.cpp is identical on Windows, macOS and Linux, without native
headers or OS conditionals. Link iiGUI::iiGUI; its installed target supplies
C++23 and transitive Core/native dependencies. The installed consumer retains
find_package(iiGUI 0.1) to check source and package compatibility.

Windows retains ordinary C++ main in a GUI-subsystem executable, without a
separate console window. MinGW supplies its native CRT entry adapter; MSVC
selects mainCRTStartup explicitly. CRT initialization and C++ static-object
lifetime are preserved. No separate WinMain source is required.

The verifier places HelloGUI under
build/verify-desktop/<platform>/kits/<platform>/Release/app/build:
HelloGUI.exe on Windows, HelloGUI on Linux, and
HelloGUI.app/Contents/MacOS/HelloGUI on macOS. Run with --self-test for a real
GUI lifecycle check; any missing milestone returns a nonzero exit code.

## Native GPU canvas

Call on the UI owner after `opened`:

```cpp
ii::gui::CanvasFrame scene{
    .viewport={28,180,480,240},
    .background={0.01f,0.02f,0.04f,1},
    .acceleration=ii::gui::Acceleration::require_hardware
};
scene.panels.push_back({
    .bounds={16,16,448,208},
    .top={0.02f,0.12f,0.85f,1}, .bottom={0.01f,0.65f,0.2f,1}, .radius=20
});
auto admitted=host.set_canvas(std::move(scene));
auto device=host.graphics_info();
// Optional synchronous test/preview readback, not part of ordinary presentation:
auto pixels=host.capture_canvas();
```

Colors above are linear-light sRGB components. Viewport coordinates are logical
content coordinates, panels are viewport-local, and radius uses logical units.
The application places text and canvas in separate areas; the native canvas
occludes the text in its rectangle. Recompute viewport in the resize callback.
`set_canvas` accepts a value snapshot; submit worker results through UiExecutor.
Accepted requests coalesce, and a synchronous capture first flushes the latest
request. Inspect `scene_renders` after a host dispatch or capture. Handle an
`unsupported` initialization result by choosing the app's text/error UI or a
permitted software policy. `clear_canvas` releases the device and attachments.
Captures stay in memory; file export belongs to iiFileProvider.

## Core operations, scopes and frames

```cpp
ii::runtime::OperationOwner work_owner;
ii::runtime::TaskScope view;
// workers is a Runtime that outlives work_owner; call inside/after opened.
auto task=work_owner.submit<int>(workers, [](ii::runtime::WorkContext& context)
    -> ii::core::Result<int> {
    if (auto ready=context.checkpoint(); !ready) return std::unexpected(ready.error());
    return 6*7;
});
(void)task.then_on(host.executor(), view.sender(), [&](auto result) {
    if (result->value) (void)host.set_text(std::to_string(*result->value));
});
auto ui=host.executor();
(void)view.sender().at(ui, ii::runtime::Clock::now()+std::chrono::milliseconds(100), []{});
```

Close tracked owners/scopes and join workers after run returns. For frames,
construct FrameLoop(host.event_loop()->get()) on the open UI thread and call
step from a UiExecutor timer. HelloGUI uses prepare_present to prepare the GPU
scene; the native host submits it after Core dispatch. Destroy FrameLoop after
run returns while Host remains alive. The installed consumer checks the same
path through relocated packages. Channel watch/read/write callbacks use the same
UI endpoint and ownership rules; use Runtime/AsyncIO for blocking transfers.
