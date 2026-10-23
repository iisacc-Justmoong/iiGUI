# Native GPU backdrop materials and authored widget metrics

`CanvasLayer::backdrop` applies a `BackdropMaterial` before the layer's panels,
text and images. Its rounded bounds, tint and optional inner border are logical
canvas coordinates; `Color` components remain linear-light sRGB. It uses the
same native surface as ordinary C++ widgets and creates no OS popup or control.

`BackdropSource::content` captures the base canvas before any overlay. This lets
a dialog blur application content without feeding its own dim layer, text or
previous popup pixels back into the blur. `previous_layers` captures the image
immediately before that layer. A zero blur radius retains a sharp image with
translucent tint; it is not an opaque replacement color. Radius values are
finite 0..64 logical units and correspond to a Gaussian sigma of radius/3.

## Metal implementation and reuse

The native Metal backend copies only the material region expanded by the blur
radius and clipped to the attachment. BGRA8 sRGB input is sampled into an
RGBA16Float linear blur destination by the system Metal Performance Shaders
Gaussian kernel. Rounded masking, tint and border are then composed on the GPU.
The normal rendering path has no CPU backdrop pixel readback.

Textures and kernels persist. A content material reuses its source and blur when
only overlay text, buttons or colors change. Changes to the base background,
panels, text, images, viewport or capture region invalidate the source; a sigma
change invalidates the blur. `previous_layers` recopies when a changed scene is
rendered because the accumulated image can have changed. Identical scenes still
skip rendering through the ordinary retained-canvas contract.

The sum of backdrop and [box-shadow](Shadows.md) working textures is bounded to
64 MiB. A sharp backdrop crop uses
4 bytes/pixel; a blurred crop uses 4-byte source plus 8-byte destination pixels.
Removed materials release these textures. This budget excludes the ordinary
attachment, swapchain, native driver and foreground caches.
`GraphicsInfo::backdrop_supported` reports support. `backdrop_copies`,
`backdrop_blurs` and `backdrop_texture_bytes` expose encoded pass counts and
retained working storage, not a GPU timing or physical display measurement.

This implementation currently supports the native Metal backend. Vulkan and
Direct3D11 reject a requested backdrop material explicitly. `set_canvas` admits
a value snapshot; deferred flush or capture observes renderer failure. Automatic
fallback never silently discards the material. These backend gaps remain part
of the cross-platform work.

## Stroke, text and layout contracts

`Panel::stroke_width == 0` fills its rounded rectangle. Positive values draw
only the inner ring with analytic antialiasing and linear-light alpha blending.
Metal, D3D11 and Vulkan shaders carry the same field in the existing 64-byte
panel record. The current macOS tests execute both Metal and Vulkan strokes;
the D3D11 implementation has not been run on this host.
Focused C++ buttons use the same resolved corner radius and a 1.5-unit inner
stroke, matching the authored LVRS FocusRing geometry without square corner
pixels. Full input-modality visibility, inherited focus customization and
focus motion remain part of the LVRS migration.

`TextRun::line_height` and `LabelOptions::line_height` select a fixed logical line
height when positive; zero preserves native font metrics. Label alignment,
optional color and 400/500/600/700 weight are explicit values. macOS measures
native layout fragments at the chosen width; Windows uses DirectWrite uniform
line spacing; Linux uses Pango absolute line height and requires Pango 1.50+ for
this option. Windows/Linux execution remains unverified here. The limit is
8192 logical units, and ordinary text-size and UTF-8 limits still apply.

`StackOptions::insets` optionally supplies distinct left/top/right/bottom edges;
otherwise `padding` keeps its original meaning. `ButtonStyle` supplies optional
state fills, text colors, border and line height. `PopoverOptions::fit_to_viewport`
allows a centered dialog to shrink below its minimum width, while the default
keeps the earlier minimum-width behavior. `opaque_base`, optional `blur_radius`,
border and corner limit let an owner select a material without changing popup
input, focus or lifetime rules. `ImageOptions::preserve_aspect` centers an image inside its padded area without
stretching; its default false preserves earlier image-view behavior.
Runtime setters invalidate the affected widget
layout or drawing; they do not rebuild its owning view.

`tests/backdrop.cpp` verifies real blurred pixels, overlay exclusion, rounded
masking, linear tint, reuse, release and explicit unsupported-backend errors.
`tests/alert_primitives.cpp` verifies fixed line metrics, asymmetric layout,
transparent outlined button states, event order and Metal/Vulkan stroke pixels.
