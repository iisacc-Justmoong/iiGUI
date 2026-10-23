# Centered dialog layout and cancellation

`PopoverPlacement::centered` places an own-rendered popup in its host window.
The existing anchored mode retains its positioning and viewport clamping.
Centered width is `min(maximum_width, max(minimum_width, min(width, viewport_width - 2*margins)))`.
`width` is the preferred outer width, or the natural width when zero. Height
uses the content measured at the chosen width, padding and `minimum_height`;
an explicit `height` wins. `vertical_offset` shifts the centered frame. Minimum
dimensions can intentionally overflow a smaller window. This preserves the
authored dialog policy rather than silently shrinking a minimum size.

`cylinder` resolves the surface radius to half its smaller dimension. A custom
`backdrop_color` supplies the dim layer. These remain C++ GPU panels on the
existing surface; no OS dialog, panel, popover or control is created.

`StackOptions::alignment` selects center, leading, trailing or stretch on the
cross axis. Existing stacks default to center. `Widget::set_flex(weight)` gives
a horizontal child a relative share of available main-axis space; zero uses its
natural width. A spacer still consumes extra main-axis space. Width-dependent
measurement propagates wrapped label heights through nested stacks, and a stable
layout caches measurements until its constraints or content change.

Buttons can override height and corner radius through optional construction
fields. Image views can add padding, a rounded background and a border; these
are GPU geometry around the immutable image. Default options preserve existing
widget appearance.

`close_popup()` is a programmatic close. `cancel_popup()`, outside dismissal,
Escape and a native accessibility cancellation are user cancellation paths.
`PopoverEvents::canceled` runs after content retirement and `closed`, once for
the actual open activation. Closing an already closed popup emits neither event.
Removing or hiding its owner uses ordinary teardown without a cancellation.
Active callback captures can retire after their own invocation returns; see
[content ownership](PopupContent.md). Child Escape handlers still have priority.

`tests/dialog_layout.cpp` verifies centered placement, resize without recreation,
header flex sizing, wrapped height and close/cancel distinction. LVRS's Modal
tests additionally verify action ordering, fresh content and actual Metal pixels.
