# Native host boundary

## Dependency and platform boundary

`application -> iiGUI -> iiSDKCore Runtime -> C++23 / native runtime`.
Core does not reference iiGUI, AppKit or another feature SDK. iiSDKBuilder is
build-time orchestration, not an application runtime dependency. No GUI or
storage implementation is added to Core. iiGUI itself creates no files; future
application document/export behavior must use iiFileProvider.

The public Host, UiExecutor, option, callback, snapshot and statistics records
use composition and final C++ types. No NSObject, NSWindow, HWND, Display,
native handle or platform header crosses this boundary. Platform callbacks
remain private; they introduce no public base class or registry.

CMake selects exactly one backend: Host.mm on macOS, HostWindows.cpp on
Windows, or HostLinux.cpp on Linux. Each has a private NativeCanvas backend:
CanvasMetal.mm, CanvasWindows.cpp, or CanvasLinux.cpp. Linux links X11,
Pango/Cairo and EGL/GLES. Other systems are rejected explicitly.
HostCommon.h shares value validation and deadline rounding; Dispatch.cpp
remains the common Core scheduling endpoint.

## Execution

Host is constructed, run, inspected, mutated and destroyed on one owning UI
thread. Use the process main thread for portable applications: AppKit requires
it; Win32 and X11 allow a dedicated owning thread. Only executor() and
is_ui_thread() are cross-thread Host operations. On macOS the host owns a
standalone NSApplication run and rejects an existing delegate/run. Windows
and Linux reject a second concurrently running iiGUI Host. Embedded/multi-host
applications are outside this API.
`run()` is single use and rejects reentrancy. Destruction during `run()` or on
another thread is a lifetime violation and terminates; close through the API
and let `run()` return before destroying its owner.

UiExecutor is a weak, copyable Core TaskExecutor endpoint. `post`, `at`, `after`,
`every` and `cancel` can be used by producers on other threads. They delegate
admission, priorities, cancellation, queue capacity and periodic coalescing to
Core EventLoop. A zero/negative period is rejected by Core. Accepted pre-run
work stays queued until the window opens; the open callback consumes no user
queue slot, so startup also works with a completely full one-slot queue.

Each successful submission signals a native wake gate. A mutex protects its
attach/signal/detach lifetime. No worker retains a native window or calls its
GUI APIs. Each bounded dispatch invokes Core poll(PumpBudget); ready work and
the next Core deadline determine the next wake.

| Backend | Cross-thread wake | Deadline and native dispatch |
|---|---|---|
| macOS | Retained CFRunLoopSource and CFRunLoopWakeUp | One-shot CFRunLoopTimer; AppKit dispatch |
| Windows | Auto-reset event and coalesced private window message | MsgWaitForMultipleObjectsEx for event/input; WM_TIMER killed/rearmed per Core deadline, including native move/size modal loops |
| Linux | Nonblocking CLOEXEC eventfd; workers never touch Display | poll on the X connection and eventfd with a rounded-up Core deadline; at most 64 native events before another Core turn |

Windows retires the gate's HWND before native destruction and closes its wake
event only after the wait loop returns. Linux detaches/closes eventfd under
the producer gate lock. There is no permanent frame/polling timer. Windows
retains OS timer granularity; scheduling is cooperative, not hard real-time.

The initial native dispatch calls `opened` after the window has been shown,
before queued application work. Resize callbacks are delivered only after
`opened` starts. Window changes and all public callbacks execute on the owner
thread. A SerialQueue alone does not establish that thread affinity.

Only GUI scheduling is exposed here. Core I/O reactors can run in their own
execution domain and post results to UiExecutor; this release does not adopt
native I/O watches into the window-system loop. Long callbacks can delay
native input; move blocking computation to Runtime. There is no hard real-time
guarantee and no automatic rendering/frame loop.

## Window state and shutdown

Sizes describe content dimensions. macOS uses AppKit points; Windows uses
96-DPI logical units and temporarily adopts a per-monitor-v2 DPI context on
the owning thread, restoring the previous context at cleanup. WM_DPICHANGED
updates logical size and DirectWrite/Direct2D DPI. Linux reads the Xft.dpi
RESOURCE_MANAGER value (48..384 DPI, otherwise 96), reacts to property changes,
and uses scaled physical dimensions for Cairo text and the EGL canvas. It
provides one X11 desktop scale; direct Wayland/per-output fractional scaling
are not implemented. Pixel
rounding can produce a sub-unit difference on Windows. Finite positive sizes
up to 32767 and initial size >= minimum are required. Native resizing updates
cached size and text layout. UTF-8 is validated before replacement; titles
reject embedded NUL. Linux uses UTF8_STRING/_NET_WM_NAME and Pango/Cairo;
Windows uses DirectWrite layouts, retained until text/size/DPI changes.
Both support native complex-script shaping, bidirectional layout and font
fallback; glyph coverage depends on installed fonts. Input-method/editor
behavior is outside this read-only display contract. Equal text replacements
avoid paint work; Linux defers/coalesces text damage until a host dispatch.
Direct2D recreates its text render target on D2DERR_RECREATE_TARGET.

The GPU canvas is an explicit native child surface. CanvasState shares
validation/admission/coalescing among the hosts; GPU resources remain private
and UI-owned. An instanced draw renders ordered panels into a retained sRGB
attachment; GPU copy/blit presents it without CPU readback. Native resize or
backing-scale changes invalidate physical dimensions; expose reuses retained
content. There is no automatic frame timer. The canvas is released before
the host destroys its native window/display. See Graphics.md for limits,
hardware policy, shader math and failure behavior.

`request_close()` and the native close button use the same close-veto callback.
Command-Q on macOS, Alt-F4 on Windows and WM_DELETE_WINDOW on Linux request
this close. No normal close path calls exit().
Once accepted, close stops admission, cancels queued Core work, hides the
window, emits `closed` exactly once and stops the native application run.
Callbacks cannot submit new work after that barrier. The wake source and timer
are detached before native resources and delegates are released. Late weak
endpoints reject instead of reviving the host.

Callback exceptions are captured and returned as a Core error, with the C++
exception_ptr when available. They do not unwind across Objective-C/CF callback
boundaries, Win32 window procedures or native event dispatch. Failure bypasses
a close veto to release the host. Initial native
setup failure also cleans partial resources, but does not promise `opened` or
`closed` notifications for a window that never finished opening.

The host does not own background Runtime workers. The application closes its
own work scopes and joins workers after `run()` returns. Workers must not wait
synchronously for a UI reply during shutdown. A rejected UI delivery is an
expected outcome when the user closes before computation completes.

## References

- [AppKit and UIKit main-thread rules](https://developer.apple.com/documentation/technologyoverviews/uikit-appkit)
- [Core Foundation run-loop sources](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/Multithreading/RunLoopManagement/RunLoopManagement.html)
- [NSApplication](https://developer.apple.com/documentation/appkit/nsapplication)
- [Win32 event/input wait](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-msgwaitformultipleobjectsex)
- [Win32 thread DPI context](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setthreaddpiawarenesscontext)
- [Per-monitor-v2 DPI availability](https://learn.microsoft.com/en-us/windows/win32/hidpi/dpi-awareness-context)
- [Xlib window/event protocol](https://xorg.freedesktop.org/archive/current/doc/libX11/libX11/libX11.html)

An X11/XWayland server must remain available for the Linux host's lifetime.
Xlib's process-level protocol/connection error policy remains in effect;
recovering from an X server restart is outside this standalone host.

These platform APIs are used directly. Qt's platform abstraction is a problem
reference only; no Qt implementation, API hierarchy or platform plugin is used.

## Core host integration (iiGUI 0.4 / Core 0.16)

DispatchState owns its EventLoop and one Core HostLoop connection. A native gate
is attached first; HostLoop then receives a noexcept callback that signals it.
Core, rather than UiExecutor's submission wrapper, performs coalescing and all
submission/token/lifecycle/readiness notifications. The native host still owns
one-shot deadline timers and bounded event dispatch. No native handle is exposed
by Core. The dependency remains application -> iiGUI -> Core -> standard/native
runtime. Core imports neither iiGUI nor the graphics/text backends.

One Core readiness waiter parks when no unready I/O exists. Pending I/O uses the
existing poller's bounded waits; completions execute only on UI poll. Thread
shutdown and in-flight wake barriers finish before the native gate detaches.
HostStats.host preserves the final Core host counters after disconnection.
There is no permanent UI polling or frame timer.

Host::event_loop returns an owner-checked borrowed reference while open. It
supports Core FrameLoop/QueryPort integration; the bound Core dispatcher rejects
foreign pumping or another blocking driver. Use FrameLoop::step from a scheduled
UI callback, and keep the host alive until frame owners are destroyed. Direct
Core request_stop is terminal and closes the native host through normal cleanup;
it bypasses the application's close veto because the scheduler cannot restart.
Use Host::request_close when veto behavior is required.

Use weak UiExecutor/Core Sender values across worker boundaries. TaskScope timed
submission accepts UiExecutor without a concrete EventLoop adapter. Operation's
then_on retains the endpoint value and a weak receiver scope; a temporary local
executor therefore cannot dangle. Host closure stops queued delivery and late
weak submissions reject. Pending Operation::wait_for returns would_block while
the calling thread is connected to an external host; arbitrary blocking OS/C++
calls remain the application's responsibility.
