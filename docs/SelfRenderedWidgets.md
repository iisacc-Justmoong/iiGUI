# Self-rendered C++ widgets

Native color state and six own-rendered domains are documented in [Colors.md](Colors.md).
Fixed authored layout, image-backed controls and focus-loss commits are documented
in [Composition.md](Composition.md).

The widget implementation now follows the explicit requirement that iiGUI owns
rendering. Earlier AppKit-widget validation records describe the superseded
implementation. Cocoa controls are no longer used for mounted widgets or legacy
Host text content on macOS.

`WidgetState.cpp` owns C++ nodes, logical layout, measurement caches, geometry,
hit testing, capture, hover/press, focus order, checked/radio state, numeric values,
UTF-8 editing and action dispatch. It produces CanvasFrame panels, text and images.
The existing native GPU renderer presents them: Metal on macOS, with the existing
Vulkan fallback. Native text shaping/rasterization is a font service, not a widget.

`NativeWidgetsMac.mm` now contains one NSView input/presentation surface per tree
and virtual NSAccessibilityElement peers. It creates no NSButton, NSTextField,
NSStackView, NSControl or NSLayoutConstraint. Cocoa translates pointer/key events
and committed/preedit IME text; iiGUI targets widgets and implements their behavior.
The platform adapter handles window coordinates and reports virtual widget
geometry/roles/actions to accessibility tools. No C++ widget corresponds to an OS
control object. The legacy `native_class` snapshot field reports `iiGUI.Label`,
`iiGUI.Button`, etc.; `renderer` is explicitly `iiGUI`.

Current primitives include vertical/horizontal/overlay stacks, spacers, labels,
five button tones, checkboxes, radio groups, switches, basic text inputs, sliders
and interval progress, icon/help/dropdown button visuals and icon-bearing tabs.
TabBar uses equal-width children, single selection, disabled-tab reconciliation,
and focus-only Left/Right/Home/End navigation; activation changes selection.
ComboBox is a 97×20 clickable field with an 18×18 stepper indicator, not an
implicitly constructed menu. Stepper emits +1/-1 from its clicked half or an
explicit direction; it does not own a numeric value or run a repeat timer.
Single-arrow controls ignore the opposite direction. Up/Down keys emit once per
key event, and virtual increment/decrement accessibility actions use the same
C++ dispatch. Enter/Space and pointer activation dispatch pressed, released,
clicked, then stepped. Outside release omits the click; disabling, Escape,
focus loss and moving focus cancel capture once. A release callback may unmount
its generation without a stale click following it.
SegmentedControl is a styled horizontal container with independent child actions,
as in the original LVRS label/icon segmented controls. It supports separate
horizontal/vertical padding, spacing, an opaque fill/border, rounded/cylindrical
shape and optional borderless tone propagation. It does not infer selection.
Self-rendered [menus](Menus.md) add popup ownership, regular/context rows and
dividers, context binding, scrolling, placement, input capture and focus return.
[Popovers](Popovers.md) mount ordinary native C++ widget content in the same GPU
overlay path, with modal/nonmodal input, close policies and retained content.
An explicit [activation factory](PopupContent.md) can instead build/retire a
generation of C++ content per open while reusing the same native GPU surface.
The disabled tone remains effectively disabled when `set_enabled(true)` is called;
the Boolean setter cannot bypass the authored tone contract. The borderless
Stepper pressed surface uses LVRS's opaque accent-muted mix.
Mutations and input require the UI owner; stale handles
cannot affect a successor tree. Unchanged text skips mutation/layout invalidation;
layout caches intrinsic metrics, redraw requests coalesce on the OS surface, and
identical GPU scenes reuse retained attachments.
Foreground textures are keyed by immutable image identity, text, geometry and scale;
panel-only hover/background changes do not reshape text, rasterize icons or upload
their pixels. `foreground_rasterizations` and `foreground_texture_bytes` expose
actual work and active texture storage. See [Images and layers](Images.md).
An empty Host allocates no widget surface or GPU renderer. Explicit text content,
widget mounting or canvas submission creates presentation resources on demand.
Font snapshots report the resolved native family after bundle-font registration.
Applications and games can opt into the same Core frame loop with a native
[display-synchronized source](DisplayFrames.md); duration timers remain independent.

Tests reject OS controls/layout in the actual content tree, deliver actual Cocoa
mouse/key events through the surface, activate virtual accessibility peers, and
read GPU pixels for primary/disabled LVRS states. They also cover capture cancel,
keyboard focus, scalar-safe UTF-8 deletion, IME preedit vs commit, radio exclusion,
range clamping and unchanged-scene reuse.
Value mutation rejects labels/buttons; inputs report virtual focus and editable
values. Some OS accessibility clients still report read-only virtual peers as
writable because of the shared value-setter bridge; that metadata discrepancy
remains to be corrected. The single input surface handles Command-V and Cocoa
paste commands without creating a native field editor or OS text control.

This supersedes the previous claim that native OS controls are the widget backend.
Full LVRS parity remains in progress in `LVRS/catalog/native/parity.json`. The OS
input/presentation adapter currently supports macOS; Windows/Linux mounting still
returns unsupported. Full IME replacement ranges, pointer text selection, clipboard
and advanced input contracts remain incomplete. Self-rendering does not imply
those remaining behaviors have already been implemented.
