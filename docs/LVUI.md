# LVUI views rendered by iiGUI

The compiled component-language route is documented separately in
[NativeWidgets.md](NativeWidgets.md). `../LVUI/example/HelloWorld` AOT-compiles `.lvui`
into native C++23 and mounts iiGUI's widgets on a native surface. The earlier
HTML-shaped runtime-parsed canvas example is retained as API history.

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

## Prior document API example

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

iiGUI의 라이브러리, 패키지, 네이티브 예제와 테스트는 독립적으로 구성한다.
HelloWorld와 생성 뷰 검사는 iiGUI 0.6.0 이상을 설치한 뒤 LVUI 저장소에서 구성한다.
LVUI 0.1.1은 생성 코드에서 사용하는 최소 호스트 API를
LVUI_IIGUI_MIN_VERSION으로 공개한다.

```sh
cmake -S ../LVUI -B ../LVUI/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DiiGUI_DIR=/path/to/iiGUI/lib/cmake/iiGUI \
  -DiiSDKCore_DIR=/path/to/iiSDKCore/lib/cmake/iiSDKCore \
  -DLVUI_BUILD_HOST_EXAMPLES=ON -DLVUI_BUILD_IIGUI_INTEGRATION_TESTS=ON
cmake --build ../LVUI/build --target HelloWorld --parallel 4
```

현재 데스크톱 예제는 CMake에서 `HelloWorld.lvui`를 C++로 생성한다.
창 없는 조합 검사는 생성된 팩토리와 iiGUI 네이티브 위젯을 연결해
레이아웃·입력·콜백·수명을 확인한다.

Native API references: [AppKit attributed text drawing](https://developer.apple.com/documentation/foundation/nsattributedstring/draw%28in%3A%29)
and [Direct2D rendering into a WIC bitmap](https://learn.microsoft.com/en-us/windows/win32/direct2d/server-side-rendering-overview).

Renderer selection defaults to the platform accelerator with Vulkan fallback.
See [renderer contracts](Renderers.md) for Vulkan selection.
