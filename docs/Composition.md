# Native control composition

`WidgetBuilder::absolute_stack(parent, id, Size)` owns a fixed logical content
extent. It participates as one measured child in surrounding stack layout. Its
direct children use `Widget::set_frame(Rect)` in coordinates relative to that
container. Nested absolute stacks permit reusable composed controls without OS
layout managers. Geometry must be finite, nonnegative and at most 32767 units;
extent dimensions must be positive. This is explicit authored placement, not a
constraint solver. Window resize does not automatically rewrite those frames.

`set_extent` updates the measured extent. `set_visible(false)` excludes the
subtree from normal layout, painting, targeting, Tab focus and accessibility.
It clears active focus/capture and closes owned popups. Handles remain valid in
the same generation and can show the subtree again. A replacement or closed mount
revokes them. All mutation follows the existing UI owner-thread contract.

`WidgetBuilder::image` displays an immutable `RasterImage` with logical dimensions
and an accessibility name. `set_image` replaces images on image views, button
backgrounds or slider tracks. Pointer identity preserves the existing foreground
cache contract. It does not load files or create an OS image/control object.
Image layers are composited above panels; image-backed sliders therefore composite
a cached thumb image above their track rather than paint a panel behind it.

Slider options expose `thumb_diameter`, `track_height` and `range_inset` for real
authored control sizes. The inset is also used for pointer-to-value mapping.
The default metrics retain the existing slider appearance. A background image
replaces its track/fill drawing; range, keyboard, snapping and value callbacks
remain owned by the same slider implementation.

`InputOptions::commit_on_focus_loss` optionally invokes the existing submit action
when focus leaves an editable field. It defaults to false. Focus assignment occurs
before the callback; a callback may redirect focus or unmount the tree, and a
pending pointer action must not activate the previous target afterward. An Enter
submission does not itself move focus. Consumers that commit only edited values
track dirtiness in their owning component; programmatic `set_text` emits no user
edit. Native focus loss, Tab, explicit focus changes and popup opening use this
contract. Tree destruction does not manufacture a final commit.

`InputOptions::maximum_length` limits Unicode scalar count when nonzero, without
splitting UTF-8. Zero retains only the existing 64 KiB byte limit. Initial text,
programmatic replacement and user insertion respect it. It is not a grapheme
cluster or UTF-16 code-unit limit. Accessibility names for inputs and sliders are
explicit options. Existing IME replacement-range/clipboard limitations remain.

`set_escape_action` routes Escape to the closest focused ancestor with an action,
after existing popup dismissal. The handler belongs to that widget generation;
it does not create a process-wide keyboard hook. The LVRS ColorPicker uses these
primitives to reuse iiGUI fields, sliders, buttons, images and color domains.

`iiGUI.composition_layout`, `composition_input` and `composition_images` test
geometry, hidden focus, restoring a subtree, Unicode length bounds, focus-loss
commit and reentrant retirement, image replacement and actual GPU thumb ordering.

Programmatic `focus()` also accepts stack containers. This removes focus from a
child editor and allows its owning component to receive Escape without adding a
new Tab stop. Labels and images remain nonfocusable. ColorPicker uses container
focus for Apply/Cancel and explicit swatch focus for programmatic/AX activation,
so an old editor cannot keep displaying a stale value after those actions.
