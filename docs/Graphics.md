# Native GPU canvas contracts (0.3)

The public Graphics.h is standard C++23 and uses no native/Qt types. Host
owns one optional canvas. Existing text-only applications do not initialize
Metal, D3D or Vulkan devices. [Renderer selection](Renderers.md)
uses Metal/Direct3D first, then Vulkan; Linux uses Vulkan directly. Core remains the runtime/scheduling dependency; Builder
orchestrates compilation and is not linked into the GUI application.

## Rendering and resource use

Each panel is a rectangle with a logical radius and two linear-light sRGB
colors. Six unit-square vertices per instance and a 64-byte panel record
produce ordered triangles in a single instanced draw. The fragment shader
evaluates rounded-rectangle signed distance and derivative-based coverage.
It interpolates the gradient in linear space and premultiplies by alpha.
ONE/ONE_MINUS_SRC_ALPHA blending occurs into an sRGB attachment, which
decodes the destination before blending and encodes the result afterward.
The canvas background is opaque, so captures are also opaque.

| Platform | Rendering | Retained attachment / presentation |
|---|---|---|
| macOS | Metal native device, compiled MSL vertex/fragment pipeline | Private BGRA8Unorm_sRGB texture; GPU blit to CAMetalLayer, display-synced, two drawables |
| Windows | D3D11 feature level 11.0, HLSL Shader Model 5 | Typeless RGBA8 texture with sRGB view; GPU copy to two-buffer DXGI flip-sequential swapchain, sync interval 1 |
| Vulkan (all desktop hosts; Linux default) | Native surface, Vulkan 1.1 SPIR-V pipelines | RGBA8 sRGB image; GPU blit to a native FIFO swapchain; fence and per-image semaphore synchronization |

Shaders are private driver-compiled GPU programs. The application itself is a
native C++ binary; no common application VM, interpreter, Qt plugin or WebView
is involved. Text uses AppKit/CoreText, DirectWrite/Direct2D or Pango/Cairo.
Linux does not depend on the GTK widget toolkit.

`set_canvas` validates and admits the latest value snapshot. The end of a
bounded Core dispatch flushes it once. A hundred replacements in one callback
render only the final value. Equal snapshots do not re-render or present.
Pipelines/device/attachment persist. Physical size changes reallocate the
attachment; input updates replace only the small instance data. This API does
not promise partial panel damage tracking or a general retained scene graph.
Panel rendering/presentation has no CPU pixel loop. Canvas text uses native
glyph rasterization and a pixel-format conversion/upload on changed scenes,
then a GPU text-composition pass; see [LVUI](LVUI.md). GPU copy/blit and
swapchain buffers still consume device bandwidth/storage.

An exposed Linux canvas presents its cached attachment. Cocoa/DWM retain the
last presented surface. Resize/backing changes request a flush; there is no
idle animation timer. Occluded Metal windows may temporarily have no drawable;
offscreen rendering/capture still works and presentation can resume on a later
native invalidation or changed scene. Continuous animation is application
scheduling, not an always-running SDK frame loop.

## Bounds, policy and errors

- Viewport/panel origins: finite, absolute logical value <=32767. Their width
  and height: finite, 0 < value <=8192. Viewport device-pixel origins must also
  fit +/-32767. Physical canvas width/height are ceil(logical*backing_scale),
  each <=8192; allocation can still fail on a lower-limit device or low memory.
- At most 4096 ordered panels. Radius is finite, 0..min(width,height)/2.
  Color components must be finite 0..1, and background alpha must equal 1.
  Panels clip to the viewport. Colors are SDR sRGB, not ICC/HDR/wide gamut.
- At most 4096 text runs, each at most 64 KiB of valid UTF-8 with no NUL,
  at most 1 MiB total. Font size is 8..256 logical units; weight is 400/500/600/700.
  Runs wrap, clip to their positive content rectangle, and paint after panels.
  Text uses the same finite geometry/color limits and is included in capture.
- `prefer_hardware`: prefer the native Metal/Direct3D accelerator, then Vulkan.
  Vulkan prefers classified hardware and permits software/unknown devices.
- `require_hardware`: reject software/unknown/virtual Vulkan devices; only
  integrated/discrete device reports satisfy this policy. Metal identifies its
  MTLDevice; Windows checks the native driver/adapter hardware flag. Driver
  reports do not guarantee physical isolation or performance.
- API selection is separate: automatic, native-only or explicit Vulkan. A
  native initialization/drawing failure in automatic mode retries through
  Vulkan and records the actual API and fallback reason. Failure of Vulkan
  itself uses normal guarded host cleanup. See [Renderers](Renderers.md).
- Text-only hosting does not require a GPU device. Failed canvas initialization
  returns `unsupported`; invalid input returns `invalid_argument` and physical
  coordinate overflow `out_of_range`. Rejected admission preserves the prior
  canvas. Vulkan device-loss recovery/hot-device migration are outside scope.

Canvas methods require the open host's owning UI thread. A missing canvas has
GraphicsApi::none, while capture returns not_found. Closing/clearing releases
native resources on that thread. clear is idempotent; frame_requests is the
host's cumulative admitted request counter. Other device counters reset when
the device is recreated/cleared. `graphics_info` inspects current counters and
does not implicitly flush deferred work.

## Capture and measurement

`capture_canvas` is an explicit synchronous GPU readback after flushing pending
work. It returns top-left-origin tightly packed RGBA8 sRGB bytes in memory;
it may stall the UI and must not be used for normal frame presentation. Metal
uses a shared staging buffer (256-byte aligned rows), D3D a staging texture,
and Vulkan an image-to-host buffer copy with explicit visibility/fence synchronization. Tests use captures to verify sRGB 0.5 -> about
188, alpha compositing, gradient direction and fractional corner coverage.
They also check coalescing, stable identical-scene render count, physical
resize, invalid update preservation, strict policy and teardown.

GraphicsInfo reports the actual selected API/adapter and physical size.
cached_texture_bytes counts the retained offscreen color attachment only
(width*height*4); it excludes swapchain buffers, driver/pipeline/instance data,
capture staging storage and text raster/upload storage. A counter or compiled binary is not proof of
target execution or a Qt performance comparison. See Verification.md for
the recorded hardware/runtime evidence and deferred platforms.

Optional [backdrop materials, inner strokes and fixed line metrics](BackdropMaterials.md)
extend these contracts. Backdrop working textures have a separate 64 MiB bound
and explicit capability/counters; they are not included in `cached_texture_bytes`.

`TextRun::clip` adds an optional paint clip in the coordinate space of `bounds`.
Native shaping, wrapping and ellipsis still use the original bounds. Core Text,
DirectWrite and Pango/Cairo apply both rectangles during rasterization; allocation
remains bounded by the actual canvas. Retained drawing and embedded popup
translation move both rectangles. `render_text` rebases the clip with its bounds.
Clips participate in validation, equality and foreground cache keys. Runtime crop
checks are deferred to the final verification batch.

The Vulkan effect test uses the public `CanvasLayer` type in `CanvasFrame::overlays`.
Compilation of that test is separate from executing its GPU pixel checks.
