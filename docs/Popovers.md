# Popovers with own-rendered content

Centered placement, width-aware dialog layout and cancellation events are documented
in [Dialog layout](DialogLayout.md). Anchored defaults and retained static content
continue to use the contracts below.

`WidgetBuilder::popover` creates an overlay in the existing C++ widget tree. It
owns one ordinary content widget, normally a stack, and may also own nested menus
or popovers. Content is measured while closed and remains mounted between opens.
Opening a popup does not construct another native window, OS control or backend.
An explicit [content factory](PopupContent.md) selects per-open content lifetime;
it is separate from the default retained static content described here.
The macOS adapter uses the same NSView and virtual accessibility peers; rendering
uses the existing native GPU canvas layers, including the content's color domains.

```cpp
auto popup = builder.popover(parent, "editor", {}, {.closed = on_closed});
// Check each Result, then add one stack/content view under *popup.
// After publishing the mount, on its UI owner:
// popup->open_for(anchor, 0, anchor_height + 8);
// popup->close_popup();
```

`PopoverOptions` defaults to automatic width/height, 12 logical units of padding,
8 units of edge margin, radius 12, a 25 percent surface coating, nonmodal input,
no dimming, focus on open and no content clipping. Width/height zero use the
content's natural size plus padding. `WidgetSnapshot::implicit_size` exposes
natural metrics even before the popup has been arranged. Explicit sizes are outer
dimensions. Placement flips or clamps at window edges; `open_for` follows its
anchor when the owner tree is laid out again. Descendant/self anchor cycles are
rejected. Oversized explicitly clipped content can scroll and reveal focused
descendants. Unclipped content can extend outside the content box.

Opening/closing callbacks execute on the tree's owner. `open_at`, `open_for` and
`close_popup` share the existing weak, generation-checked widget handle. Closing
a parent closes its descendant popups, commits a focused editor when configured,
and restores its previous focus when it still owns focus. Ordinary hide/unmount
does not turn a closed popup's content into a new activation. A callback may retire
the tree; no remaining callback or captured press then reaches that retired tree.
Reopening in a close callback protects the new activation from the old press.

The default close policies are Escape and a press outside all open popups.
The `dismiss_on_context_request` override can distinguish a secondary outside press
from an ordinary outside press. When it is absent, both follow
`dismiss_on_outside_press`, preserving the default behavior.
A context request inside a descendant popup is also inside each of its popup
ancestors for dismissal purposes, even when the descendant lies outside their
visual rectangles.
A focused descendant's Escape handler runs first. This allows a ColorPicker to
restore its model and notify its owner before the owner closes the popup. Tab
wraps through eligible content; text editing, sliders and color dragging retain
their normal behavior. Captured drag releases outside do not become outside
presses. A nonmodal outside press closes eligible popovers and reaches the main
content; modal input consumes that press. Pressing inside another open popup does
not automatically dismiss the first. `dim` adds a visual overlay independently of
`modal`. Native window focus loss clears focus/capture but keeps popover content.

The popup itself does not accept/cancel a document model. Applications decide
whether close means rollback or commit. `LVRS/examples/NativePopoverGallery`
demonstrates a shared ColorPicker with owner-controlled apply/cancel/outside close,
one scoped backend, Core worker delivery and display-driven frames.

`tests/popovers.cpp` covers lifecycle, policy, placement, nested menus, actual Metal
pixels, retirement and reentrancy. Existing menu/widget tests cover the shared
input/presentation path. Full independent-popup accessibility/focus behavior and
Windows/Linux widget adapters remain incomplete. The current surface uses an
opaque authored base plus coating; original backdrop blur, radial coating, grain
and spring transitions are still pending. Options are construction constants.

Qt's [Popup contract](https://doc.qt.io/qt-6/qml-qtquick-controls-popup.html) was
consulted for interaction requirements. This is an independent C++ implementation
and introduces no Qt dependency.
