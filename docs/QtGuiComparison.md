# Qt GUI comparison and implemented response

This audit compares the pre-0.3 iiGUI desktop text host to Qt's public GUI
contracts. Qt is a conceptual reference only: no Qt code, packages, types,
runtime, platform plugin or RHI dependency is added to iiGUI/Core/HelloGUI.
The inspected references are the official Qt 6 documentation on 2026-10-16.

Qt GUI supplies window-system integration, input/events, fonts/text, raster
painting and graphics interfaces. Qt Quick's retained scene graph is a
separate layer built above it; its batching/resource techniques are useful
references but must not be attributed to Qt GUI alone.

| Area | Identified iiGUI limitation | Implemented in iiGUI 0.3 | Remaining difference |
|---|---|---|---|
| Platform coverage | Three desktop hosts only; Linux X11 pixel scale fixed at 1 | Same C++23 GPU/host API and HelloGUI source on three desktops; live Xft.dpi logical scale on Linux | Qt supports more targets; direct Wayland, Android, iOS and WASM GUI hosts are absent |
| Text quality | Windows GDI text; Linux manual UTF-8-prefix wrapping lacked shaping/fallback | DirectWrite/Direct2D and Pango/Cairo retained text layouts; native fallback, bidi, complex-script shaping | Editable text/IME, selection on Windows/Linux, custom-control accessibility and complete input remain absent |
| Graphic quality | No graphics surface or GPU drawing path | Native sRGB GPU canvas, linear-light gradients and alpha, analytic rounded-edge AA, device-pixel sizing | General paths, images/codecs, rich painting, ICC content, wide gamut/HDR and 3D/compute remain absent |
| Hardware/resource use | CPU text-only paint, repeated Linux measurement/repaint | Metal, D3D11 or GLES3 native drivers; instanced drawing; retained pipeline/attachment; equal-state suppression; Core-turn coalescing; on-demand GPU allocation | No arbitrary scene graph, partial GPU damage, texture atlas, compute scheduler, threaded renderer or guaranteed device-loss recovery |
| Observability | No graphics/device information or pixel test path | Selected adapter/API, conservative hardware policy, actual storage/counters, explicit readback quality tests | No Qt-vs-iiGUI benchmark or claimed general performance superiority |

The desktop GPU backends deliberately match their platforms' broadly available
native interfaces. Metal is the maintained Apple path. D3D11 provides mature
native Windows support with flip-model presentation; adding D3D12 command
management is not required for this small instanced UI workload. Linux EGL/GLES3
uses the system driver with an sRGB surface, rather than adding a Vulkan loader,
shader-toolchain and synchronization layer before an observed need. The narrow
NativeCanvas boundary allows each implementation to be replaced without native
types entering the public API. New hardware capabilities should be added when
concrete drawing requirements justify them.

This is a real graphics/resource extension, not a claim that iiGUI replaces
all of Qt GUI or Qt Quick. It closes the absence of a GPU path and modern text
layout on the existing desktop scope. Windows/Linux executable and shader
execution must be confirmed on those desktops; cross compilation alone does
not establish image quality, DPI behavior or driver compatibility.

## Primary references

- [Qt GUI overview](https://doc.qt.io/qt-6/qtgui-overview.html)
- [Qt supported platforms](https://doc.qt.io/qt-6/supported-platforms.html)
- [Qt high DPI](https://doc.qt.io/qt-6/highdpi.html)
- [Qt GUI RHI window example](https://doc.qt.io/qt-6/qtgui-rhiwindow-example.html)
- [Qt Quick scene graph](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html)
- [Qt backing store](https://doc.qt.io/qt-6/qbackingstore.html)
- [Apple CAMetalLayer](https://developer.apple.com/documentation/quartzcore/cametallayer)
- [Direct2D resource/render targets](https://learn.microsoft.com/en-us/windows/win32/direct2d/render-targets-overview)
- [DXGI sRGB resource/view conversion](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/converting-data-color-space)
- [Pango architecture](https://www.gtk.org/docs/architecture/pango/)
- [OpenGL ES 3 specification](https://registry.khronos.org/OpenGL/specs/es/3.0/es_spec_3.0.pdf)
