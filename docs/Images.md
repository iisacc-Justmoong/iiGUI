# Native images, SVG assets and ordered layers

`rgba_image` transfers an encoded-sRGB straight-alpha RGBA8 buffer into immutable
`RasterImage` storage. Dimensions must be 1..2048 and the byte count must match.
`ImageRun` specifies logical bounds, an optional **linear** color tint, and opacity.
The rasterizer bilinearly samples in premultiplied linear light and clips to the
surface. The GPU samples the resulting sRGB texture and blends in linear light.
There is no mutable pixel view or implicit resource-file access.

`svg_image` accepts <=1 MiB UTF-8 SVG and rasterizes using the native C++
[LunaSVG 3.5.0](https://github.com/sammycage/lunasvg) asset codec, pinned to commit
`83c58df8103dc7dca423dfd824992af94d49bed6`. Its PlutoVG dependency is pinned by
the upstream submodule. Both MIT license texts ship under `share/iiGUI/licenses`.
The initial source build obtains these dependencies; an installed consumer uses
their installed CMake packages and libraries without fetching sources.

External SVG file resources and automatic system-font loading are disabled at
compile time. Paths, clipping, masks, gradients and embedded data images are
handled by the codec. Filters, animation, scripts, foreign objects and external
document declarations return `unsupported` instead of being silently ignored.
The decoder handles asset geometry, not a UI language or runtime program.

`CanvasFrame` draws its base panels, texts, then images. Each `CanvasLayer` in
`overlays` draws in that same order **after all content in the previous layer**.
Thus popup backgrounds can cover base text and icons. Up to 16 overlays are
accepted; aggregate panel/text/image and text-byte budgets are validated.
This is a drawing contract. Menu behavior, focus trapping, outside-click dismissal
and scrolling are separate widget responsibilities and are not supplied by a layer.

`CanvasLayer::clip` is an optional logical rectangle relative to the canvas.
Metal/Vulkan/D3D11 apply hardware scissoring to all panels, text and images in
that layer. A zero-size rectangle hides the layer. Bounds are intersected with
the attachment and rounded outwards to device pixels. Image UVs remain unchanged.
Clip-only changes reuse the foreground texture and do not rerasterize glyphs or
upload image pixels. Self-rendered menus use this contract for scrollable rows.

Metal, Vulkan and D3D11 keep one GPU foreground texture per active nonempty layer.
Text, image identity, tint, opacity, bounds, scale or viewport changes invalidate
that layer. Panel-only changes reuse it. Identical frames reuse the scene attachment;
removing a layer releases its foreground texture. Vulkan uses a distinct descriptor
set per layer, so later descriptor updates cannot redirect an earlier draw.

`iiGUI.images_*` tests cover SVG pixels, invalid inputs, clipping, linear blending,
actual GPU layer order, foreground cache reuse, selective invalidation and storage
release. Native Metal and Vulkan tests run on macOS; D3D11 source uses the same
contract but Windows execution is not covered by those macOS results.
The outer process timeout allows a cold app/font load from external storage;
the example's five-second callback watchdog starts after its view is mounted.
Sanitizer builds instrument iiGUI and the native SVG codec targets; an independently
installed Core shared library is not instrumented by that build option.
