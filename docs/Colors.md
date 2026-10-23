# Native color model and fields

`ColorPickerModel` is native C++23 state shared by application code and iiGUI's
own color fields. It uses encoded sRGB `WidgetColor` and Core `Result`/typed query
contracts. There is no QColor, platform color well, system color panel or source
interpreter. Construct and use a model on its owning UI thread; worker results
return through the existing Core UI executor.

## Model contract

The neutral default is opaque white. `state()` returns current/previous RGBA,
uppercase six-digit RGB HEX, hue in degrees, saturation/brightness in [0,1] and
a revision. `set_color` tracks the initial previous color until an explicit
`set_previous_color`; `edit_color` preserves that comparison color. `accept`
copies current to previous; `cancel` edits back to previous.

`edit_hsv` takes degrees/unit/unit and clamps finite values. Achromatic RGB edits
preserve the selected hue. `channel_value` and `edit_channel` use degrees for hue,
0..255 for RGB, and percentages for saturation, brightness, CMYK, alpha and
subtractive grayscale. CMYK preserves latent ink choices at full black. Alpha
editing preserves latent HSV and CMYK. Ramps are opaque except the alpha ramp.

`edit_hex` accepts optional `#`, surrounding ASCII whitespace and six or eight
hexadecimal digits. Six preserve alpha; eight mean **RRGGBBAA**. Invalid text
returns false without changing revision or color. Nonfinite channels and invalid
encoded colors return errors. Reads and edits reject foreign threads. Revision
follows channel recalculation, including latent changes; updating only the previous
color does not increment it.

`observe(shared_ptr<const ColorChangePort>)` connects an explicit Core receiver
weakly. Close/drop the port to detach. Registration is idempotent per port and
bounded to 256 live receivers. Notifications contain flags and revision; receivers
select the values they need. Delivery is synchronous on the owner. Registration
during delivery starts with the next change; mutation from a receiver is busy.
Notification failure is reported after commit and must not cause a repeated edit.
There is no frame polling, global event bus or arbitrary C++ assignment tracking.
Callers retain the model throughout an active call.

## Self-rendered fields

`WidgetBuilder::color_surface` accepts a shared model, `ColorField`, dimensions
and an optional edited callback. Domains match the LVRS wheel/triangle, HS, SV,
spectrum, hue rail and brightness rail. Spectrum preserves the .43/.45 overlay
stops; wheel drags project to the nearest triangle edge. Pointer capture, clamping,
direction/Shift keys, disabled state and UI ownership are handled in C++.

`set_color_model`, `color_model` and `set_color_field` explicitly replace/query
native connections. Replacement detaches the old receiver and cancels its drag.
Unmounting revokes the weak connection; an edited callback can remove its own
tree. State is committed before the callback. Fields share the exact same model.

Backgrounds are immutable cached RGBA images: wheel/SV depend on hue, HS on
brightness, and spectrum/rails have no changing color dependency. Selection marks
use separate shared images. Cursor changes reuse the background. Keys include
native backing scale and pixel dimensions, capped at 2048 per dimension; logical
dimensions are 12..1024. Cache misses rasterize on the CPU before the existing
native GPU compositor. This is not a procedural GPU shader or atlas. Snapshots
expose `color_domain_rasterizations` for verification.

LVRS supplies its original unmodified embedded 448x448 Figma wheel PNG; standalone
iiGUI has an analytic ring. `png_image` uses the already pinned PlutoVG/STB memory
decoder. Header/dimensions are bounded before decode (8 MiB input, 2048x2048),
output is straight RGBA8, and no path or network resource is loaded. A private
PNG-only build of the pinned STB header preserves straight-alpha RGB and bounds
all codec allocations/reallocations to 96 MiB per call, including zlib expansion
beyond a misleading image header. Input and final RGBA storage are separate from
that codec budget. The STB MIT notice is included in the installed licenses.

macOS uses the same one-NSView input/presentation surface. Virtual color elements
expose current HEX, focus and rail increment/decrement. Native pointer events and
keys enter the common state machine. Native touch and Windows/Linux widget
adapters remain incomplete. Full ColorPicker composition, recent swatches,
eyedropper and all original input styles remain separate work.

`iiGUI.colors_*` verifies conversions, latent channels, HEX/alpha, owner/receiver
lifetimes, PNG bounds, six domains, cache dependencies, Cocoa pointer/AX events,
self-unmount and actual Metal pixels. LVRS NativeColorGallery exercises the same
public model/fields as an installed consumer with source/compiler offline.

## Current-color action button

`WidgetBuilder::color_button` makes an own-rendered button using a nonnull
`ColorPickerModel`, optional ring image, 8..256 natural size and ordinary
`ControlEvents`. A visible ring requires an image. It retains standard button
pointer/keyboard/accessibility activation and capture cancellation. It does not
construct a picker or popup. LVRS supplies the separate original button PNG and
its 22/28/36 sizes; a shared model reflects edits without reconstructing the button.

The preview composites the state background, normal/focused border, ring and
alpha color circle into one cached image. Disabled opacity applies to that whole
image. It preserves a centered circular well in rectangular allocations. Small
surfaces use 4x sampling, bounded to 2048 pixels per side; GPU composition uses the
existing image path. Equal visual keys reuse the image. Snapshot diagnostics
expose `color_button_rasterizations`. This is cached CPU rasterization followed by
native GPU drawing, not a procedural GPU shader.

`Widget::set_color_model`, `set_color_button_size` and `set_show_hue_ring` change
the same native button. Its receiver is weak and tied to the widget generation;
replacement detaches the old model and teardown retires the observer/model. No
pending callback owns a retired tree. `iiGUI.color_buttons_*` tests actual Metal
pixels, hover/pressed/focus/disabled states, lifecycle, model replacement and reuse.
State transitions and spring press/rebound animation remain incomplete.
