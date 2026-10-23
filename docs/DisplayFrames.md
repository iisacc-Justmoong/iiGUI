# One frame runtime for applications and games

`runtime::Application::frame_loop()` owns Core's native ordered frame callbacks.
`HostOptions::frame_loop` optionally connects that loop to the window's display.
It must borrow the same EventLoop as the Host and outlive the Host. Without this
option an event-driven app stays idle until native widget motion requests frames.
Widget motion borrows an existing frame loop or creates an on-demand loop over
the Host's EventLoop; it stops the display source after the final animated frame.
This does not start continuous frame delivery for an idle application. See
[native motion](NativeMotion.md) for the lifetime, preferences and GPU contract.
Use one driver for a bound loop; do not also call its timer-based `run` or manual
`poll`/`step` while the display host owns its cadence.

```cpp
auto& frames=app.frame_loop();
auto update=frames.add([](const ii::runtime::FrameContext& frame) {
    // Native app animation or game update; delta is scaled, unscaled_delta is real time.
}, {.phase=ii::runtime::FramePhase::update});
auto physics=frames.add([](const ii::runtime::FrameContext& frame) {
    // frame.delta is the configured fixed_step, independent of display refresh.
}, {.phase=ii::runtime::FramePhase::fixed_update});
auto options=View::options();
options.frame_loop=&frames;
auto host=ii::gui::attach_application(app,options,callbacks);
```

On macOS 14+ the Host creates NSView's CADisplayLink and runs it on the main run
loop's common modes. Callbacks follow the display containing the view. The native
presentation-target timestamps drive Core's monotonic frame clock; moving to a
different refresh rate changes the source cadence instead of retaining a hardcoded
16 ms interval. Core processes bounded tasks and advances frame phases. The Host
reserves one frame callback slot in `present` to flush the C++ widget/GPU surface;
`end` follows that presentation work and the work is included in Core's measured
frame budget. App/game producers update in `update`/`prepare_present`.
No Cocoa control is created by this connection.
In frame mode, widget mutations mark the surface dirty and the native OS paint
request joins that same presentation phase. Unchanged widgets submit no GPU
presentation; size/backing-scale changes still invalidate. This avoids racing an
extra OS-triggered draw against each display callback. Game producers that change
the scene each frame keep submitting at the native cadence.

The host invalidates the display link before releasing widgets and on every
cleanup path. A callback exception crosses the native barrier as a failed Host
result. Core closes frame registrations and releases callback captures after
joining owned workers and before deactivating application attachments.

| Contract | Meaning |
|---|---|
| `UiExecutor::every(duration)` | Elapsed-time scheduling, unrelated to vertical synchronization. A 16 ms timer requests 62.5 calls/s, not 60 display frames/s. |
| `FrameLoop::add`, interval zero | Once per eligible phase of each host-driven frame. |
| Tick interval | Throttle to eligible frame boundaries; no timer-like burst catch-up. |
| `fixed_update` | Fixed-step simulation with bounded catch-up and dropped-step accounting. |
| `TickClock::scaled` | Gameplay/animation time scale and pause. |
| `TickClock::unscaled` | UI and real-time frame work while scaled updates are paused. |

`HostStats::display_callbacks` counts actual delivered display callbacks and
`display_interval` records the native target interval. The former divided by
elapsed native frame time gives delivered update rate. Graphics scene renders,
presentation requests, and physical scanout/completion are distinct. No timer or
display callback counter is advertised as proof that a GPU frame physically
appeared on screen. OS load and visibility can reduce delivered callbacks.

The view's display source may suspend while hidden or off-display. Normal Core
timers and background jobs remain independent. Core clamps scaled resume delta
and bounds fixed-step catch-up; unscaled elapsed time retains the real gap.
The current native display connection requires macOS 14+. Windows/Linux accept
the same Core frame phases, but this Host option is explicitly rejected until
their native presentation cadence adapters are implemented; it never silently
substitutes a duration timer for synchronization.

`iiGUI.frame_host_*` checks real display callbacks, fixed/variable phases, GPU
updates, owner/event-domain validation, exception shutdown and no-source idle
behavior. `LVRSNativeGallery` displays a sampled native frame/update rate instead
of a 200 ms timer count. Its label refreshes four times per second to avoid
rebuilding text on every display pulse, while frame callbacks follow the display.

Frame integration tests finish after observing the required actual display
callbacks. Process liveness is bounded by the outer CTest timeout; an internal
four-second timer no longer infers whether twelve or twenty-four frames should
have arrived. The assertions on frame phases, failures, rendering and redundant
presentation remain in place. Instrumented startup bounds are documented in
[NativeMotion](NativeMotion.md).

Apple documents the [view-bound display link](https://developer.apple.com/documentation/appkit/nsview/displaylink(target:selector:))
and [presentation-target timestamp](https://developer.apple.com/documentation/quartzcore/cadisplaylink/targettimestamp).
