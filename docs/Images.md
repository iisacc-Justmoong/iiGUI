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

## SVG 자산 캐시

`SvgImageCache`는 불변 자산의 공유 식별자와 출력 크기를 기준으로 래스터를
재사용한다. 기본 한도는 128개와 8 MiB이며, 개별 캐시는 한도를 지정할 수 있고
한도가 0이면 결과를 보관하지 않는다. 같은 식별자는 수명 동안 같은 SVG 바이트를
뜻해야 한다. 오류나 잘못된 출력 크기는 캐시에 넣지 않는다. 여러 스레드가 같은
자산을 요청하면 저장된 이미지 하나로 수렴하지만, 동시 캐시 미스의 디코딩 자체를
하나로 합치는 것은 보장하지 않는다.

`clear()`는 현재 항목을 해제하며 호출 전에 시작된 디코딩이 나중에 캐시를 다시
채우지 않게 한다. 이후 새 요청은 다시 저장할 수 있다. 앱이 디코더를 교체할 수
있으며 병렬 호출되는 디코더의 스레드 안전성은 공급자가 보장해야 한다.
`CachedSvgDecoder`는 공유 캐시를 자산 모듈의 디코더 계약에 연결한다. macOS arm64의
헤드리스 검사에서 캐시 한도·재사용·동시 요청·정리 중 요청 및 설치 소비자를 확인했다.

`CanvasFrame` draws its base panels, texts, then images. Each `CanvasLayer` in
`overlays` draws in that same order **after all content in the previous layer**.
Thus popup backgrounds can cover base text and icons. Up to 512 overlays are
accepted, in addition to the base layer; aggregate panel/text/image and text-byte
budgets are validated independently. `maximum_canvas_overlays` is the shared
validation and Vulkan descriptor limit. The limit accommodates retained widget
compositions without merging layers whose drawing order would otherwise change.
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
actual GPU layer order (including 160 layers), foreground cache reuse, selective
invalidation and storage release. Native Metal and Vulkan tests run on macOS; D3D11 source uses the same
contract but Windows execution is not covered by those macOS results.
The outer process timeout allows a cold app/font load from external storage;
the example's five-second callback watchdog starts after its view is mounted.
Sanitizer builds instrument iiGUI and the native SVG codec targets; an independently
installed Core shared library is not instrumented by that build option.
