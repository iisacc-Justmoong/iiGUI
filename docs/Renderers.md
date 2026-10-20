# Native rendering and Vulkan fallback

iiGUI uses the platform window system directly. LVUI describes the view;
iiGUI measures native fonts, lays out boxes and draws native GPU graphics.
Qt, a browser engine, WebView, SDL and a JavaScript application runtime are
not part of this rendering path.

| Platform | Window/text host | Automatic graphics selection |
|---|---|---|
| macOS | Cocoa/AppKit, native text shaping, NSView/CAMetalLayer | Metal, then Vulkan/MoltenVK on native accelerator failure |
| Windows | Win32, DirectWrite/Direct2D, native child HWND | Direct3D 11 hardware, then Vulkan on native accelerator failure |
| Linux | X11/XWayland, Pango/Cairo, native child Window | Vulkan directly |

The previous Linux EGL/GLES renderer is retired from this source. WARP is no
longer the automatic Windows fallback. `GraphicsApi::opengl_es3` retains its
old enum value for source compatibility; it does not enable a GLES backend.
Direct Wayland and mobile window systems are outside the desktop host scope.

## Selection and failures

`CanvasFrame::renderer` and `Host::set_view(document, renderer)` accept:

- `Renderer::automatic` (default): try the platform-specific accelerator;
  if initialization or a drawing operation fails, switch to Vulkan. Linux
  starts with Vulkan and reports this as its default, not as a failure.
- `Renderer::native`: require the platform-specific accelerator; return its
  failure instead of silently choosing another API. Linux rejects this mode.
- `Renderer::vulkan`: use Vulkan directly, including for verification.

```cpp
auto document = lvui::parse(source);
// Production default: Metal/Direct3D hardware first, Vulkan fallback.
auto shown = host.set_view(*document);
// Explicit Vulkan uses exactly the same document/layout/text contract.
auto checked_vulkan = host.set_view(*document, ii::gui::Renderer::vulkan);
```

After a successful automatic switch, the canvas keeps Vulkan until cleared or
its selection/acceleration policy changes. It does not repeatedly retry a
broken native device every frame. `GraphicsInfo::api`, `adapter`, `hardware`,
`fallback` and `fallback_reason` report the actual result. A failed native
attempt and failed Vulkan attempt are both retained in the returned error.
Resources are created/destroyed on the owning UI thread.

Acceleration policy is separate from API selection. `prefer_hardware` permits
a software/unknown Vulkan implementation when no classified GPU is available;
hardware adapters are preferred. `require_hardware` accepts only a reported
integrated or discrete Vulkan GPU. CPU, virtual and unknown device types cannot
satisfy the strict policy. Vulkan is an API fallback, not a guarantee that any
machine has a usable GPU/driver. Failure of Vulkan itself enters the host's
normal error/cleanup path; automatic recovery from Vulkan device loss and hot
device migration are not implemented.

On macOS, MoltenVK translates Vulkan into Metal. It provides a different API
and pipeline implementation, not a second physical graphics system. Missing
Metal hardware or a broken underlying Metal driver can prevent both paths.
This follows the [Vulkan portability model](https://docs.vulkan.org/guide/latest/portability_initiative.html).
When advertised, the renderer enables portability enumeration and the device's
portability-subset extension, following the
[Khronos contract](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_portability_enumeration.html).

## Vulkan implementation

The shared C++23 renderer uses `VK_KHR_swapchain` and a private native surface:
`VK_EXT_metal_surface`, `VK_KHR_win32_surface` or `VK_KHR_xlib_surface`.
Public headers expose no Vulkan or platform handle. The selected queue must
support both graphics and presentation. SDR sRGB surface/image formats and
GPU blits are checked before use.
Storage-buffer ranges and the actual sRGB image-usage/dimension capabilities
are checked against the selected device before allocation.

Instanced rounded panels use the same gradient, analytic coverage and
linear-light premultiplied blending equations as the Metal/Direct3D paths.
Native glyph rasterization uploads encoded-sRGB text to an sRGB image and
composites in linear light. Rendering targets a retained RGBA8 sRGB image;
GPU blits present it through a FIFO native swapchain. Equal scenes retain the
image. Explicit capture copies it to host-visible memory, with a fence and
transfer-to-host visibility barrier; normal presentation performs no readback.

One graphics command stream is owned by the UI thread. Fence completion bounds
resource reuse; presentation semaphores are allocated per swapchain image so
they are reused only after acquiring that image again. Resize drains the device
and recreates the swapchain/attachment. Zero-sized/temporarily unavailable
surfaces defer presentation; a later host invalidation/update can retry.
This is not an automatic animation timer or partial-damage renderer.

## Build and shader source

All desktop builds now require Vulkan 1.1+ development headers and a loader/
driver library. Install a target Vulkan SDK, or pass explicit target paths:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DiiSDKCore_DIR=/path/to/Core/lib/cmake/iiSDKCore \
  -DLVUI_DIR=/path/to/LVUI/lib/cmake/LVUI \
  -DVulkan_INCLUDE_DIR=/path/to/Vulkan/include \
  -DVulkan_LIBRARY=/path/to/Vulkan/lib/libvulkan.so \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/examples/HelloLVUI/HelloLVUI --self-test --vulkan
```

Use the target's Vulkan loader on Windows/Linux and MoltenVK on macOS; the
example library path above is illustrative, not a macOS path. Installed static
consumers receive `Vulkan::Vulkan` through CMake dependency discovery. If the
Vulkan SDK is outside normal search paths, provide the same two cache variables
to the consumer. Runtime loader paths remain a deployment responsibility;
verification removes inherited DYLD overrides and checks a relocated consumer.
No Vulkan dependency is added to LVUI or iiSDKCore.

GLSL sources are in `src/shaders`. Checked-in `VulkanShaders.h` contains their
SPIR-V, so consumers do not need a runtime or build-time shader compiler.
After editing GLSL, regenerate using the official Khronos tool:

```sh
python3 -B tools/compile_vulkan_shaders.py /path/to/glslang
```

The shader guard checks source hashes and SPIR-V magic. Renderer policy tests
inject deterministic private backend failures; real forced-Vulkan tests verify
panel colors/alpha/antialiasing, native glyph pixels/clipping, swapchain
presentation, resize, equal-scene reuse and the installed consumer. A policy
test is not evidence that a physical Metal/DirectX device was made unavailable.
Runtime evidence must name its actual platform/device and API.
