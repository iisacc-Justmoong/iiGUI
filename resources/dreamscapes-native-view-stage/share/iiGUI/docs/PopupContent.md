# Activation-owned popup content

`Widget::set_popup_content(WidgetFactory)` is an alternative to statically mounted
Popover children. The factory constructs one root C++ View when a closed popup
opens. Repositioning an already open popup keeps that activation. Closing revokes
its handles and releases its owned widgets/subscriptions/objects before the
parent's closed notification. A subsequent open constructs a fresh activation.
The factory itself stays owned by the outer popup until replaced or unmounted.

```cpp
// Build an empty Popover in LVUI or WidgetBuilder, then configure it while closed.
auto result = popup.set_popup_content([](ii::gui::WidgetBuilder& b)
        -> ii::core::Result<void> {
    auto field = b.input_field({}, "field", {.placeholder = "Fresh editor"});
    if (!field) return std::unexpected(field.error());
    return {};
});
// Check every Result on the UI owner. After open:
// auto current = popup.popup_content("field");
// close_popup() revokes current; popup_content() without an ID returns its root.
```

Factories and static ordinary children are mutually exclusive. Change/clear a
factory while its popup is closed and the shared domain is idle. A closed popup
being constructed inside the currently staged activation may also set its own
factory before publication; it cannot open during construction or mutate a
previously published popup. A rejected candidate leaves the live tree intact. `retain`
inside the factory owns a C++ object for that content activation. Repeated opens,
rendering, callbacks, focus changes and model subscriptions do not reconstruct
the parent LVUI backend or application. No VM, source parser or OS control is
introduced. An embedded activation creates no NSView, native window or GPU context:
the original surface presents its flattened panels/text/images and popup layers.

Each activation uses a separate generation-owned state. Local IDs can repeat
between activations. `popup_content(local_id)` returns a weak handle for the
current activation only. Snapshot/accessibility IDs are qualified opaque targets;
the `@iiscope:` namespace is reserved for these targets. Old handles and old AX
targets cannot address a replacement. An inactive popup has no content item.
Native focus/IME resolves the actual focused descendant rather than the shell.

Frames, snapshots, pointer coordinates and input share the host's coordinate
domain. Owned nested menus can overflow the content box and still use the whole
window's placement bounds. Their own clipping remains independent from the
parent content clip. Text editing, child-first Escape, Tab, capture, modal outside
input and parent focus return retain their ordinary C++ behavior.

The shared domain admits at most 4096 live widget nodes, eight open popups and
eight embedded activation levels. Closed activations return their capacity;
factory candidates count against remaining capacity. Factory construction and
content retirement serialize admission across the domain. A failed/throwing
factory retires its candidate and publishes no partial content. The existing
outer mount survives. Reopening is supported from the parent's closed callback,
after content retirement, rather than during construction/destructor/blur work.

Ownership release is distinct from finishing an active C++ invocation. A callback
already executing may keep its captures/model alive until it returns. Those
objects are not forcibly destroyed while in use; their widget admission is already
closed. External shared owners follow normal C++ ownership. Use retained Core
TaskScope/weak receivers for content-owned asynchronous work; arbitrary external
jobs are not inferred or cancelled automatically. Teardown and factory failures
must not throw from user destructors.

`iiGUI.popup_content_*` covers lifecycle, 500 reopenings, native input/AX, retained
single-surface identity, stale targets, actual Metal pixels, nested menu overflow,
budget/failure/reentrancy, owner removal during blur and active capture retirement.
`LVRS/examples/NativeActivationGallery` uses LVUI for the outer view and a compiled
C++ ColorPicker factory for each open, with Core-owned parent/content lifetimes.

The dedicated [LVRS Modal](../../LVRS/docs/native/Modal.md) additionally keeps
static header/action chrome while using the anchored body path below.

## Retained chrome with a per-open body

`popup.set_popup_content_at(anchor, factory)` binds an empty, unfixed VStack
descendant in the same popup generation. Static widgets outside that anchor stay
owned by the mount. Each opening creates one body view; its measured size and
padding participate in the normal static layout. While inactive the anchor is
hidden and contributes no spacing. Body text/wrapping changes invalidate the
anchor and its ancestor measurements; resizing preserves the current activation.

Closing immediately retires body widgets, receivers and retained owners before
the popup's closed callback. Static chrome remains available through mount lookup.
With `PopoverOptions::transition`, chrome can finish its exit while the body has
already gone. Its resulting height is recomputed without the body, matching a
Loader whose `active` and `visible` become false on close. Reopening during exit
keeps the current reveal value and static handles but constructs a fresh body.
Whole-popup factories retain their original immediate retirement contract and
require an explicit body anchor to combine with native transitions.

`popup_content(id)` first searches the current body and then static chrome owned
by that popup; it requires an open popup. `popup_content()` returns the body root
when present, otherwise the static content root. The popup's own ID and nested
unrelated popup children are not exposed as content. Qualified stale body IDs
cannot address a replacement; static IDs do not change between openings.

Input/AX uses inherited frame scale for the body. Tab/Shift-Tab crosses the body
and static action row in layout order. Capture stays with the body or chrome
that accepted the press, stale hover is cleared across that boundary, and a
nested modal menu dismisses before an outside press can reach static actions.

Cancellation belongs to the popup generation that closed. If a closed callback
reopens the popup, even if it also closes that new opening immediately, the old
cancellation is suppressed. A current cancellation is delivered after the closed
notification. This policy applies to generic popups and does not depend on an
application's component library.

An application can supply `PopoverAdaptiveLayout` for retained popup content.
The host calls the layout provider first with an empty content measurement to
select a width, measures the retained content and named scroll viewport at that
width, and calls it again for the final viewport-relative rectangle. The final
width must match the first width. The host validates the rectangle and exposes
provider errors to callers; it rejects a failing layout before opening a popup.
The provider runs synchronously on the UI owner and must not access or mutate
the widget domain, including its parent. The host rejects reentrant UI access.
An optional bottom-slide transition is a hosting capability; placement and input
policy remain application-owned. Adaptive layout currently requires static
retained popup content and cannot be combined with explicit bounds or the older
sheet-layout option.
General mixed independent-popup policies and all nested transform/IME cases are
still covered by the native parity ledger rather than claimed complete here.

`iiGUI.anchored_content_*` checks repeated lifecycle, width-aware measurement,
dynamic body height, frame reversal/reduced motion, visual input coordinates,
focus crossing, actual GPU pixels, teardown/admission, pointer capture, nested
menus and unchanged explicit label colors during exit. GPU resources belonging
to already-submitted draws retire through the normal presentation path.
Bodies without focusable controls add no Tab stop. If a focus-loss callback
retires or replaces the view, pending focus is not delivered to its old body.
Initial focus follows layout order once; `focus_on_open=false` preserves the
caller's focus instead of letting body construction override that policy.
