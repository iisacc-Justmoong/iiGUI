# Verification

The current scope covers Windows/Win32, macOS/AppKit and Linux/X11 backends.
Windows/Linux execution was explicitly deferred by the user on 2026-10-16.
Their evidence is cross compilation, native-image inspection, host-side source
checks and relocated installed-consumer compilation. It is not real target
GUI execution. macOS runs the shared lifecycle suite, HelloGUI and installed
consumer on the actual host. Native tests require an interactive desktop;
cross compilation or header inspection alone is not GUI evidence.

## Automated checks

| Test | Actual behavior checked |
|---|---|
| contracts | UTF-8/NUL/native-size/budget/capacity validation; main-thread construction on AppKit, owner-thread construction elsewhere; inactive/empty endpoint errors |
| lifecycle | Visible native window; Core timer; Runtime result on UI thread; 820×520 resize; closed snapshot; reentrancy rejection |
| wakeup | A worker wakes an idle native loop without a frame timer or user input |
| earlier_deadline | A worker posts an earlier timer while a long native timer is armed |
| close_veto | Native close veto and later acceptance; cancelled timer does not run |
| failure | Callback exception becomes a returned error and releases the window |
| close_failure | A throwing close-veto callback returns an error and still releases the window |
| shutdown_race | Late weak endpoint rejects after the host has been destroyed |
| producer_race | Two producers race close; bounded admission and no UI delivery after the close barrier |
| full_queue | A full one-slot queue does not prevent native startup or lose accepted pre-run work |
| wrong_thread | Foreign run, inspect, resize, retitle and close operations return wrong_executor |
| graphics_contracts | Pure geometry/color/radius/panel-count/policy validation and inactive GPU endpoint errors |
| graphics | Actual native device and sRGB pixel readback, linear blend/gradient/rounded AA, 100-to-1 update coalescing, identical-scene reuse, resize, strict policy and teardown |
| core_io | A real pipe fixture acknowledges startup and waits for release after I/O admission; readiness wakes the native host, delivers on UI and joins its waiter before teardown |
| core_operation | Tracked completion owns a temporary weak UI endpoint; scoped timed UI submission runs; removal cancels an earlier scope's timer |
| core_frame | Borrowed Core loop rejects foreign/blocking drivers; update then prepare_present reaches actual GPU capture for three frames |
| core_stop | Direct Core submission and terminal request_stop close the native host through normal cleanup |
| boundary | Public C++/Core boundary; no Qt/SDL package or include; no direct native file creation |
| tooling | Verifier output stays under build/, foreign execution requires explicit omission, all kits share sources, cross pkg-config cannot inherit host package paths |

HelloGUI's separate self-test checks a 250 ms Core timer, a tracked worker
result of 42 through then_on, an 840×540 logical-unit resize owned by TaskScope,
Core FrameLoop preparation, native GPU capture and a normal return after close.
The copied installed consumer checks the same categories through installed
headers/targets, with its own application source.

The initial red run intentionally failed contracts and lifecycle against the
unimplemented host. Its XML remains in `build/red-tests.xml`. Subsequent build,
test, package and binary evidence is written below `build/`. Sanitizer runs
instrument the new GUI implementation and its tests; they do not imply that
Apple's system frameworks are instrumented.

## Reproduction

Use tools/verify_desktop.py from README.md on each native desktop. It runs
Builder, the 19 host checks (17 native cases and two source/tooling checks),
HelloGUI and the relocated consumer. Each run gets
new logs and a report under build/verify-desktop/<platform>/runs/. The
--compile-only option marks executable tests not_run and still compiles both
the native test binary and relocated consumer. A missing desktop is an error
when native execution is requested; it is never silently converted to a pass.

Follow README.md for source/Builder builds and the relocated consumer. To test
the new host under ASan/UBSan, configure another build under `build/` with
`-DIIGUI_SANITIZE=ON`, build, and run the lifecycle, wakeup, failure and shutdown
tests there. Qt package discovery remains disabled. Native window observation,
manual resizing/closing, and loaded-library inspection supplement these tests.

## Limits

This release does not establish a complete widget toolkit, editable Unicode
text/IME, custom-control accessibility, a general 2D/3D renderer, mobile lifecycle support,
store deployment, code-signing identity or long-running production stability.
Global installed SDK packages and existing running product apps are preserved.

## Historical macOS 0.1 result (2026-10-16)

Environment: macOS 27.0.1, arm64, Apple Clang 21, C++23 and private Objective-C++23.
iiSDKBuilder 0.1.1 built iiSDKCore 0.15.0, iiGUI 0.1.0 and HelloGUI successfully.
All eight configure/build/install steps in the selected macOS kit succeeded;
Builder verified the application's arm64 Mach-O executable.

| Configuration / consumer | Passed | Failed | Evidence below build/ |
|---|---:|---:|---|
| Debug host regression | 10 | 0 | host-tests.xml |
| Builder Release host regression | 10 | 0 | release-tests.xml |
| ASan + UBSan host regression | 10 | 0 | sanitizer-tests.xml |
| HelloGUI native self-test | 1 | 0 | hello-gui-tests.xml |
| Relocated installed-package consumer | 1 | 0 | installed-consumer/tests.xml |

These are 32 passing CTest entries, including repetition of the same ten host
checks in three configurations. The source boundary check is included in those
ten; the count is not a number of independent GUI features.

The app was installed at `build/HelloGUI.app` and run again with loader tracing.
It returned 0 after three timer ticks, a background result of 42 delivered on
the UI thread, an 840×540 resize and normal close. Six direct dependencies are
Apple AppKit/CoreFoundation/Foundation, libc++, libSystem and libobjc. The
loaded-image trace contained no Qt or SDL library. Core and iiGUI are static
in this app; its execution does not need the source tree or a Qt SDK.

Native computer-use observation also showed the actual HelloGUI window:
the display changed from `working…` to `42`, the timer advanced, and macOS
zoom/restore changed content size from 680×440 to 2560×1378 and back. The
display recorded 42 native resize callbacks after restoration. Command-Q
terminated the app, confirmed by both process inspection and app inventory.
The app is reopened for the final handoff after validation.

Machine-readable aggregation is `build/verification.json`; Builder's report is
`build/hello-gui/report.json`. No global SDK install was replaced. Installation
evidence refers to the isolated, relocated prefix under build/.

## Desktop backends 0.2 result (2026-10-16)

The Windows and Linux native backends were implemented against the unchanged
C++23 Host/UiExecutor API. The same HelloGUI main.cpp was compiled for all
three platforms. Windows/Linux desktop execution was omitted at the user's
explicit request, not silently skipped by the test suite.

| Target | Compiler / image | Builder | Host executable tests | Relocated consumer |
|---|---|---|---|---|
| Windows x64 | LLVM-MinGW Clang 23.1.2; PE32+ x64 | Core → iiGUI → HelloGUI, all 8 steps passed | Test binary compiled; 2 build-host checks passed; native execution not run | Installed/moved, configured and linked; execution not run |
| Linux ARM64 | Clang 21 with GCC 14/libstdc++ and Debian 13 sysroot; ELF AArch64 | All 8 steps passed | Test binary compiled; 2 build-host checks passed; native execution not run | Installed/moved, configured and linked; execution not run |
| macOS ARM64 | Apple Clang 21; Mach-O arm64 | All 8 steps passed | 13 checks passed; HelloGUI self-test passed | Installed/moved, configured, linked and executed successfully |

The macOS ASan/UBSan configuration also passed all 13 checks with no reported
sanitizer errors. Instrumentation covers iiGUI and its test executable, not
the prebuilt Core archive or system frameworks. Counts repeat the same suite
across configurations and do not represent distinct features.

Each current run report is under build/verify-desktop/<platform>/runs/.
Windows/Linux report status is built_execution_deferred and targetTests,
helloGUI and installedConsumerExecution remain not_run. Their two passing
host checks are the Python boundary/tooling checks running on macOS, not
Windows/Linux GUI tests. Existing consumers still request iiGUI 0.1 to verify
the package compatibility of the unchanged API.

Installed-consumer configure/build processes clear inherited compiler search
paths and resolve both packages inside their relocated prefix. Native system
libraries still come from the target toolchain/sysroot. The Linux test image
uses Debian 13's ABI; it is not claimed as a portable prebuilt binary for every
Linux distribution. The Windows image uses the MinGW native ABI; MSVC and
other CPU variants remain unverified. HelloGUI and the installed consumer use
the Windows GUI subsystem while retaining the same ordinary C++ main source.

The aggregate current evidence is build/desktop-verification.json. Original
0.1 reports above are retained separately. No global SDK install or existing
running product application was replaced. Run tools/verify_desktop.py on a
Windows/Linux desktop without --compile-only to perform the deferred native
test and installed-consumer execution.

## Graphics and native text 0.3 result (2026-10-16)

The same C++23 Graphics/Host API and HelloGUI scene now build through Builder
for all three desktop targets. The 0.3 reports are isolated from the 0.1/0.2
artifacts under build/modern-verification/.

| Target | Builder configure/build/install steps | Target execution | Relocated installed consumer |
|---|---|---|---|
| macOS ARM64, Apple Clang 21 | All 8 passed | 15 host checks and HelloGUI self-test passed | Compiled, linked and executed, including native GPU capture |
| Windows x64, LLVM-MinGW Clang 23.1.2 | All 8 passed | Explicitly not run; 2 build-host source/tooling checks passed | Installed/moved, compiled and linked; execution not run |
| Linux ARM64, Clang 21 + GCC 14 / Debian 13 sysroot | All 8 passed | Explicitly not run; 2 build-host source/tooling checks passed | Installed/moved, compiled and linked; execution not run |

The macOS ASan/UBSan build also passed all 15 checks with no reported sanitizer
errors. It instruments iiGUI and the tests, not the prebuilt Core archive,
Apple drivers or system libraries. These counts repeat the same checks across
configurations and do not count unique features.

Actual Apple M1 Max Metal readback recorded hardware=1, a 384x256 device-pixel
canvas at 2x scale, sRGB midpoint 188, 71 partially covered corner pixels,
and scene_renders=1 after 100 admitted requests. Identical scene updates did
not increase that count; resize and strict-hardware/teardown checks passed.
HelloGUI also reported Apple M1 Max hardware=true, four scene renders and
4,039,168 bytes in its retained color attachment at the measured self-test
point. That byte count excludes swapchain/driver/capture storage.

Native computer-use observation separately showed the real Metal gradient
panels and shaped Korean/Arabic/Devanagari/emoji text, a running timer and the
background result of 42. Zoom expanded content from 760x540 to 2560x1313 at
2x, with 21 resize callbacks; restoring returned it to 760x540 with 40 callbacks.
The GPU scene filled its resized viewport. This is observed macOS behavior,
not Windows/Linux visual evidence or a general text-conformance test.

Windows/Linux test executables include the GPU quality/resource checks but
were never executed or emulated. Their driver initialization, runtime shader
compilation, actual hardware selection, glyph output, DPI changes, presentation
and device-pixel readback remain unverified on those desktops. Windows
evidence uses the MinGW native ABI; an MSVC build is still unverified. The
Linux sysroot is Debian 13 ARM64, not a claim of a universal Linux binary.
Native compilation remains required for other distributions/architectures.

The Linux build exposed an inherited macOS PKG_CONFIG_PATH contaminating
target Cairo include paths. The verifier and relocated package script now
clear that path for toolchain builds; tooling tests cover the isolation while
preserving native-host custom package paths. The sysroot's official Debian
package URLs and SHA256 values are recorded in
build/linux-toolchain/modern-packages.json. No host library is substituted to
make the cross link pass.

Current run reports and logs:

- macOS: build/modern-verification/macos/runs/20261016T074132.699692Z/
- Windows: build/modern-verification/windows/runs/20261016T074132.699766Z/
- Linux: build/modern-verification/linux/runs/20261016T074725.898044Z/
- Sanitizers: build/modern-sanitizer/modern-sanitizer-tests.xml
- Aggregate/current-source and binary audit: build/modernization-verification.json

Global SDK installations, previous evidence and existing running product apps
are preserved. No Qt performance comparison, full Qt GUI/Quick parity, direct
Wayland/mobile/web GUI implementation or GPU device-loss recovery is asserted.

## Core host integration 0.4 result (2026-10-16)

iiGUI 0.4 requires Core 0.16 Runtime. All three native backends now attach a
Core HostLoop after their native wake gate is ready, then disconnect/join Core
before detaching that gate. Direct Core producers and TaskToken cancellation
use the same native wake path as UiExecutor. Core owns no window/GPU/font API
and introduces no reverse dependency on iiGUI.

| Validation | Result | Evidence |
|---|---|---|
| macOS ARM64 Builder chain | All eight Core/GUI/HelloGUI configure/build/install steps passed | build/foundation-verification/macos/report.json |
| macOS native host suite | 19/19 passed, including four Core integration cases | macOS run host-tests.xml |
| HelloGUI with Operation, TaskScope and FrameLoop | 1/1 passed, actual Metal capture and normal close | macOS run hello-gui-tests.xml |
| Relocated GUI package consumer | 1/1 executed successfully; background=42, GPU capture, Core frames and scoped timer | macOS run installed-consumer/tests.xml |
| ASan + UBSan across Core and iiGUI | 19/19 GUI checks passed; both libraries instrumented | build/core-foundation-asan/sanitizer-tests.xml |
| Core Debug / ASan+UBSan / selected TSan | 40/40, 40/40 and 5/5 passed | ../iiSDKCore/build/gui-foundation*/ |
| Copied relocated Core Runtime+Values / Contracts / Values consumers | 17/17, 2/2 and 3/3 passed | ../iiSDKCore/build/gui-package/latest.json |
| Windows x64 and Linux ARM64 Builder chains | All eight steps passed per platform; test executables and relocated GUI consumers compiled/linked | build/foundation-verification/{windows,linux}/ |

The new pipe tests use a startup acknowledgement and release after I/O admission
so loader startup cannot masquerade as readiness latency. Once released, the
child writes after 120 ms. Native readiness must deliver on the UI owner within
one second against a three-second request timeout, with no periodic UI timer.
The native host's final counters confirm a readiness wait occurred and the Core
connection was closed before teardown. The other integration cases verify an
owned temporary completion endpoint, receiver timer cancellation, update then
prepare_present with actual GPU readback, owner affinity and terminal Core stop.

HelloGUI and the copied GUI consumer both use the public integration API. The
consumer still requests iiGUI 0.1 to verify compatibility with existing source
consumers while receiving Core 0.16 transitively. Its compile inputs refer only
to copied app source and relocated package headers, with Qt discovery disabled.
No source-tree or global installed SDK header is substituted.

One Core readiness thread per default connection parks when there is no pending
I/O. Pending I/O retains the existing maximum 20 ms cancellation/probe wait;
callbacks and graphics remain on the UI owner. Core Operation::wait_for returns
would_block for a pending result on that external host thread. These contracts
do not intercept arbitrary blocking application calls or imply hard real-time
scheduling, native display-vsync pacing, or an IOCP backend.

The final runs are:

- macOS: build/foundation-verification/macos/runs/20261016T084414.196306Z/
- Windows: build/foundation-verification/windows/runs/20261016T084414.196380Z/
- Linux: build/foundation-verification/linux/runs/20261016T084414.196389Z/
- Current-source/package/binary aggregation: build/core-integration-verification.json

Windows/Linux target tests, HelloGUI execution and installed-consumer execution
remain explicitly not_run at the user's request; their two passing source and
tooling checks run on the macOS build host. Core's additional iOS, Android and
WASM compilations are not iiGUI mobile/web backends or target execution proof.
Global SDK installs, previous validation runs and running product applications
remain untouched. Source/native-runtime and cross-compilation evidence are kept
separate in both module reports.
