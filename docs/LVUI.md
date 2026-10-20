# LVUI views rendered by iiGUI

LVUI is a separate declarative UI language: familiar tags and inline styles
describe a view. iiGUI consumes the validated document directly. No browser,
WebView, HTML importer, JavaScript runtime or Qt dependency is involved.

The package boundary is application → iiGUI → LVUI → iiSDKCore Contracts;
iiGUI additionally uses Core Runtime for its native host. The LVUI package
contains the parser, immutable tree, style values and source diagnostics.
iiGUI owns layout, native typography, graphics and presentation. Neither LVUI
nor Core references iiGUI. The full initial language vocabulary and layout
rules are in LVUI's `docs/Language.md`, installed with that package.

## Application usage

```cpp
#include <iiGUI/iiGUI.h>
#include <stdexcept>

int main() {
    auto document = lvui::parse(R"(
      <main style="padding:24px; gap:16px; background:#101827; color:white">
        <h1>Hello LVUI</h1>
        <p>This is a native view.</p>
      </main>
    )");
    if (!document) throw std::runtime_error(document.error().message);
    ii::gui::Host host;
    bool opened = false;
    const auto paint = [&] {
        auto result = host.set_view(*document);
        if (!result) throw std::runtime_error(result.error().message);
    };
    auto result = host.run({
        .opened = [&] { opened = true; paint(); },
        .resized = [&](ii::gui::Size) { if (opened) paint(); }
    });
    return result ? 0 : 1;
}
```

Parse once and retain the owned document. `set_view` is a value-snapshot canvas
submission; re-submit on resize. Identical scenes reuse the retained GPU
attachment. An invalid layout fails before canvas replacement, leaving the
prior scene intact. Parsing can run without a native window, and layout can
run without a GPU, using the target's native fonts. Native font APIs may
require the target's graphical/font environment; the application example
performs layout on the UI thread.

`layout_view(document, logical_size)` also returns `ViewLayout::elements`,
containing the bounds for every element with an ID. This allows applications
and tests to inspect layout without exposing native window types. Parsed
documents are immutable; application state and re-parsing changed source
remain ordinary C++ responsibilities. There is no event-binding or scripting
implementation in this initial language surface.

## Native text in the canvas

`CanvasFrame::texts` accepts UTF-8 text runs with a logical content rectangle,
linear-light color, font size, normal/bold weight and left/center/right
alignment. Panels draw first and text draws above them. Each run wraps and
clips to its rectangle. LVUI's flow layout prevents overlapping siblings.

AppKit on macOS, DirectWrite/Direct2D with an in-memory WIC bitmap on Windows,
and Pango/Cairo on Linux shape and rasterize the glyphs at backing scale.
The native premultiplied encoded-sRGB pixels are unpremultiplied and uploaded
to an sRGB GPU texture. A native GPU pass decodes and premultiplies in linear
space and composites onto the same retained attachment as the panels. Native
text is rasterized on changed scenes; unchanged scenes reuse the finished
attachment. Vulkan retains its glyph image/upload allocation until reuse or
canvas destruction; Metal/Direct3D upload textures are transient. No capture/readback occurs
for ordinary presentation. This first implementation rasterizes a full
viewport text layer on scene changes, rather than retaining a glyph atlas.

`capture_canvas` includes both panels and text. Existing `Host::set_text`
continues to provide its separate legacy native label surface; it is not used
to display LVUI text. Fonts and glyph fallback vary across target systems.
Editable controls, IME, accessibility semantics, image elements, event handlers,
data binding, CSS selectors and arbitrary HTML compatibility are not implemented.

## Build and verify

First build/install the sibling LVUI package under its `build/install`. Then:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DiiSDKCore_DIR=/path/to/iiSDKCore/lib/cmake/iiSDKCore \
  -DLVUI_DIR=/path/to/LVUI/build/install/lib/cmake/LVUI \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/examples/HelloLVUI/HelloLVUI --self-test
```

HelloLVUI's `main.lvui` is embedded by CMake, so changing it triggers regeneration
and rebuilding. The executable uses no runtime file I/O or source-tree lookup.
Run without flags for an ordinary resizable window. `--capture` emits one ASCII PPM
GPU capture to stdout and closes; redirection/export belongs to the caller.
The application source has no platform conditionals. It can also be built
standalone with installed Core/LVUI/iiGUI packages.

Tests cover parser grammar/errors/limits in the LVUI package; iiGUI adds
box/percentage/padding/typography/wrapping contracts, native glyph pixels,
color conversion, rectangular clipping, resize, retained-scene reuse and
invalid replacement preservation. The relocated installed consumer invokes
both parser and renderer. Runtime results must be reported per platform;
macOS execution does not establish Windows or Linux execution.

Native API references: [AppKit attributed text drawing](https://developer.apple.com/documentation/foundation/nsattributedstring/draw%28in%3A%29)
and [Direct2D rendering into a WIC bitmap](https://learn.microsoft.com/en-us/windows/win32/direct2d/server-side-rendering-overview).

Renderer selection defaults to the platform accelerator with Vulkan fallback.
Use `host.set_view(document, ii::gui::Renderer::vulkan)` to force Vulkan.
HelloLVUI accepts `--vulkan` or `--native`; `--capture --vulkan` captures the
same view through Vulkan. See [renderer contracts](Renderers.md).
