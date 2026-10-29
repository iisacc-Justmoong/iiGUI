# LVUI views rendered by iiGUI

The compiled component-language route is documented separately in
[NativeWidgets.md](NativeWidgets.md). `examples/HelloWorld` AOT-compiles `.lvui`
into native C++23 and mounts iiGUI's own C++ widgets on a Cocoa surface. The document below describes
the earlier HTML-shaped runtime-parsed GPU canvas API, which remains available.

LVUI는 별도 모듈이다. 앱이 LVUI 문서 어댑터와 iiGUI 호스트를 조합하면
LVUI가 계산한 프레임을 iiGUI가 표시한다. 데스크톱 경로에 브라우저 엔진,
WebView, JavaScript 런타임이나 Qt는 필요하지 않다.

이 문서의 브라우저·JavaScript 부재 설명은 데스크톱 렌더링 경로에 해당한다.
새 Web 대상은 C++23 AOT Wasm과 브라우저 API bridge를 사용하며 DOM 위젯을 만들지
않는다. Android·직접 Wayland·Web 호스트의 계약과 제한은
[HostPlatforms.md](HostPlatforms.md)에 정리되어 있다.

앱은 LVUI와 iiGUI를 각각 선택한다. LVUI 문서 어댑터가 레이아웃을 소유하며,
iiGUI는 범용 텍스트 측정·그래픽·표시를 제공한다. iiGUI 소스와 패키지는
LVUI 타입을 참조하거나 LVUI를 찾지 않는다. 전체 문서 규칙은 LVUI의
`docs/Language.md`와 `docs/NativeDocument.md`에 있다.
이전 `Host::set_view(lvui::Document, ...)` 이진 심볼을 사용하던 앱은 LVUI
어댑터 헤더와 명시적 LVUI 링크로 다시 빌드해야 한다.

## Application usage

```cpp
#include <iiGUI/iiGUI.h>
#include <LVUI/NativeDocument.h>
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

`lvui::native::layout_document(document, logical_size)`는 프레임과 ID가 있는
요소의 경계를 반환한다. 파싱된 문서는 불변 값이며 앱 상태와 소스의 재파싱은
앱 책임이다. 이 초기 문서 경로에는 이벤트 바인딩과 스크립트가 없다.

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

기본 iiGUI 빌드에는 LVUI가 필요하지 않다. 별도 HelloLVUI 앱까지 빌드하려면
앱이 사용할 LVUI 패키지를 준비하고 선택적 예제를 활성화한다. LVUI 생성 위젯의
기존 통합 검사를 iiGUI 빌드에 추가하려면 `IIGUI_BUILD_LVUI_INTEGRATION_TESTS=ON`을
명시하고 LVUI의 빌드된 컴파일러와 런타임을 제공한다.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DiiSDKCore_DIR=/path/to/iiSDKCore/lib/cmake/iiSDKCore \
  -DIIGUI_BUILD_LVUI_EXAMPLES=ON \
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

LVUI의 창 없는 조합 검사는 박스·백분율·패딩·글꼴·줄바꿈과 잘못된 입력을
확인한다. 플랫폼 GPU·창·스크린 리더 실행은 이 검사와 별도로 보고한다.

Native API references: [AppKit attributed text drawing](https://developer.apple.com/documentation/foundation/nsattributedstring/draw%28in%3A%29)
and [Direct2D rendering into a WIC bitmap](https://learn.microsoft.com/en-us/windows/win32/direct2d/server-side-rendering-overview).

Renderer selection defaults to the platform accelerator with Vulkan fallback.
Use `host.set_view(document, ii::gui::Renderer::vulkan)` to force Vulkan.
HelloLVUI accepts `--vulkan` or `--native`; `--capture --vulkan` captures the
same view through Vulkan. See [renderer contracts](Renderers.md).
