# 위젯 하위 트리의 그리기 스냅샷

`Widget::snapshot_scene()`은 현재 표시 중인 위젯과 그 하위 트리의 그리기 명령을
`CanvasScene` 값으로 반환한다. 결과에는 창의 불투명 배경을 넣지 않으며, 같은 창에
있는 다른 형제 위젯의 명령도 넣지 않는다. 각 패널·글자·이미지, 재질·그림자 및
레이어 순서는 현재 위젯 장면에서 사용한 값이다. `viewport`와 좌표는 창의 논리
좌표이며, 상위의 클리핑과 불투명도, 임베디드 콘텐츠의 위치 및 팝업 변환을 함께
보존한다. 열린 팝업 자체를 선택하면 팝업에 속한 크롬과 자식도 포함한다.
선택한 루트의 경계 밖에 그려진 자식 명령도 스냅샷에 남지만 `bounds`에는 변환된
소스 위젯의 경계가 들어 있다. 소스 렌더러는 이 경계를 최종 픽셀 클리핑에 적용한다.
클리핑은 기존 scissor와 같이 물리 픽셀 경계로 바깥쪽 반올림하며 부분 픽셀 마스크는 아니다.
원래 프레임의 기본 레이어가 있으면 `first_is_base`로 구분하므로, 해당 장면 안의
`content` 배경 캡처도 후속 오버레이 이전의 기본 레이어를 읽는다.
별도 오버레이에 표시되는 팝업 포털은 논리적 조상 위젯의 장면에는 포함하지 않는다.
팝업 또는 팝업 안의 위젯을 직접 선택할 때만 그 팝업 경로를 따라간다.
`widget.is_in_scene_of(source)`는 같은 캡처 경계의 구조적 포함 관계를 조회한다.
팝업 본문은 팝업과 본문 앵커의 장면에 속하지만 창 콘텐츠의 장면에는 속하지 않는다.
표시·불투명도와 관계없는 구조적 결과이므로 나중에 다시 표시될 위젯의 순환 여부를
판단하는 데 사용할 수 있다. 서로 다른 트리의 위젯은 포함 관계가 아니다. 두 핸들은
모두 유효하고 호출 스레드가 소유자여야 한다. 이 조회만으로 여러 재질 간의 간접
순환까지 해결되는 것은 아니다.

```cpp
auto scene = source_widget.snapshot_scene();
if (!scene) return std::unexpected(scene.error());
for (const auto& layer : scene->layers) {
    // Inspect or forward the retained drawing commands.
}
```

이 작업은 위젯 소유 스레드에서 수행한다. 숨겨졌거나 상위 불투명도가 0인 트리는
빈 레이어를 반환한다. 닫혔거나 교체된 세대의 핸들은 오류를 반환하고, 나중에
만들어진 후속 세대를 가리키지 않는다. 스냅샷은 그 시점의 값과 불변 이미지 참조를
소유하며 위젯 트리나 운영체제 창을 유지하지 않는다. 레이아웃·뷰포트 통지는 기존
위젯 스냅샷과 같은 소유 스레드 정책에 따라 먼저 정리된다.
그리기 공급자가 실행되는 동안에는 같은 위젯 트리의 다른 임베디드 세대도 조회하거나
변경할 수 없다. 공급자의 실패는 오류로 보고하고 이 제한을 해제한다.

`CanvasFrame::scenes`에 스냅샷을 넣고 재질의 `background_scene`과 `content_scene`에
각각 인덱스를 지정할 수 있다. 두 장면은 기본색·선택적 기존 프레임 캡처 위에 배경,
콘텐츠 순서로 합성되고 그 위에 underlay가 놓인 뒤 흐려진다. 프레임과 각 장면은 같은
`viewport`를 사용하며 그래픽 좌표와 `bounds`는 그 뷰포트 기준이다. 위젯 스냅샷은
원점이 0인 창 콘텐츠 뷰포트를 사용하므로 그 좌표가 창 콘텐츠 좌표와 같다.

`plan_canvas_scenes()`는 참조된 장면을 의존성이 먼저 오도록 정렬하며 공유 장면을
한 번만 넣는다. 사용하지 않는 장면은 실행 계획에서 빠지지만 그 안의 유효하지 않은
참조·순환도 거절한다. 장면은 최대 64개, 장면당 레이어는 최대 512개, 전체 장면
레이어는 최대 4096개이다. `valid_canvas()`는 이 관계와 모든 장면의 좌표·효과·자원
한도를 함께 검증한다. 인덱스 기반 값 그래프는 소유 참조 순환을 만들지 않는다.

```cpp
auto captured = source_widget.snapshot_scene();
if (!captured) return std::unexpected(captured.error());
ii::gui::CanvasFrame frame{.viewport = captured->viewport};
frame.scenes.push_back(std::move(*captured));
ii::gui::CanvasLayer material;
material.backdrop = ii::gui::BackdropMaterial{
    .bounds = {40, 40, 180, 120}, .blur_radius = 12,
    .capture = false, .content_scene = 0
};
frame.overlays.push_back(std::move(material));
// Validate the frame and query scene_sources_supported before presenting it.
```

Metal, Vulkan, Direct3D 11과 WebGL2 구현은 계획된 장면을 투명 오프스크린 텍스처에 그리고 두 텍스처를
재질에 합성한다. `GraphicsInfo::scene_texture_bytes`로 장면 텍스처 크기를 보고한다.
창만 바뀐 경우에는 변경되지 않은 소스 장면의 텍스처를 유지한다. 소스의 내용이나
클립이 바뀌면 해당 장면과 그 장면을 입력으로 참조하는 후속 장면만 의존 순서대로
다시 그린다. 새 대상 생성, 화면 크기·배율 변경, 앞선 그리기 실패 뒤에는 필요한
장면을 다시 생성한다. Metal, Vulkan, Direct3D 11, WebGL2가 같은 변경 판정 계약을
사용한다. `scene_renders`는 실제 수행한 장면·창 패스 수를 집계하며 유지한 패스는
더하지 않는다. 소스 텍스처를 유지해도 창의 표시·최종 합성은 별도로 수행한다.
Metal은 장면·재질·그림자 작업
텍스처와 공유 효과 이미지를 예산에 포함한다. Vulkan은 장면 텍스처, 한 쌍의 공유
프레임 스냅샷과 공유 효과 이미지를 예산에 포함한다. Vulkan은 명령 기록 전에 각
장면과 창 레이어에 독립적인 디스크립터 범위를 부여하고, 작업 중인 이미지의 사용
전환을 기록한다. 큰 장면을 제거하면 과도하게 커진 디스크립터 풀도 줄인다.
Direct3D 11은 장면별 sRGB 렌더 대상과 전경 캐시를 분리하고 즉시 컨텍스트의 순서에
따라 두 스냅샷을 공유한다. 프레임 및 장면의 사전 곱셈 알파는 그대로 합성하고,
직선 알파로 업로드된 이미지만 알파를 곱한다. 장면 그래프의 예산에는 장면 대상,
공유 스냅샷과 중복을 제거한 효과 이미지를 포함한다.
이 경로는 [Direct3D의 sRGB 읽기·쓰기 변환](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/converting-data-color-space)과
[호환 리소스 복사 계약](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-copyresource)을 따른다.
WebGL2는 `EXT_color_buffer_float` 또는 `EXT_color_buffer_half_float`를 사용할 수 있으면 선형 16비트 부동소수점 대상을, 그렇지
않으면 선형 RGBA8 대상을 사용한다. 각 장면은 독립 framebuffer와 전경 캐시를 가지며
의존 순서대로 그려진다. 콘텐츠와 이전 레이어의 스냅샷은 필요한 종류만 지연 할당해
framebuffer 복사로 공유하고, 실제 해당 캡처를 사용하는 패스에서만 복사한다. 상단 기준
장면 좌표를 WebGL framebuffer의 하단 기준 좌표로 변환한다. 사용하지 않는 이미지
입력은 완전한 1픽셀 투명 텍스처로 채운다. 장면 대상과 스냅샷, 공유 효과 이미지 및
투명 텍스처는 효과 예산에 포함하며, 장면이 없는 일반 재질의 스냅샷에도 예산을
적용한다. WebGL은 요청마다 창을 합성하되 바뀌지 않은 소스 장면은 재사용한다.

### 기하 버퍼 보관과 패널 그리기 묶음

네 렌더러는 창과 각 소스 장면의 패널 인스턴스 기하·색상·변환을 GPU가 읽는 버퍼에
보관한다. 해당 묶음의 값이 유지되면 다른 레이어의 변화, 창 배경의 변화, 클립만의
변화 또는 렌더 대상의 크기·배율 변화 때문에 기하 데이터를 다시 올리지 않는다.
보이는 범위 밖으로 벗어났다가 다시 나타나도 기하 값이 같으면 버퍼를 재사용한다.
다만 레이어의 추가·삭제나 클립·변환 변경으로 묶음의 구성 자체가 바뀌면 영향을
받은 묶음은 다시 준비한다. 바뀐 패널 하나를 포함한 묶음은 전체 묶음 단위로 갱신하며,
나머지 묶음은 유지한다. 더 이상 사용하지 않는 레이어·장면과 렌더러의 버퍼는 해제한다.

연속된 패널 전용 레이어는 클립과 콘텐츠 변환이 같을 때 하나의 인스턴스 그리기로
합친다. 배열 순서는 유지하므로 투명 패널의 선후 관계도 유지된다. 기본 레이어와
오버레이 사이에는 콘텐츠 캡처 경계가 있으므로 합치지 않는다. 텍스트·이미지,
배경 재질, 그림자 또는 다른 클립·변환도 묶음의 경계이다. 이 규칙은 가능한 모든
그리기를 합치기 위한 전역 재정렬을 수행하지 않는다.

Metal과 Direct3D 11은 값이 바뀔 때 교체 버퍼를 새로 만들고 성공한 뒤 이전 버퍼를
놓는다. WebGL2도 실패한 교체 버퍼를 버리고 이전 버퍼를 유지한다. Vulkan은 이전
GPU 제출의 완료를 기다린 뒤에만 해당 호스트 매핑 버퍼를 수정한다. 버퍼 보관은
렌더러가 담당하며 프레임 값의 메모리를 빌려 장기 보관하지 않는다. 실제 물리 메모리의
배치는 플랫폼과 드라이버에 따르므로 전용 비디오 메모리 상주를 보장하지 않는다.

`GraphicsInfo::geometry_uploads`는 성공한 패널 기하 업로드의 누적 수이고,
`geometry_draw_batches`는 기록한 패널 인스턴스 그리기 호출의 누적 수이다. 기존
소스 장면이 유지되어 다시 그리지 않은 경우에는 두 수가 증가하지 않는다. 이 두
멤버의 추가는 `GraphicsInfo`의 C++ 이진 레이아웃을 바꾸므로 기존 바이너리 소비자는
새 헤더와 라이브러리로 다시 빌드해야 한다. 이 보관은 GPU 패널 버퍼에 대한 것이며,
현재 위젯 트리의 CPU 명령 수집이나 값 비교가 부분 갱신으로 바뀌었다는 뜻은 아니다.

변하지 않은 장면의 유지와 의존 부분만 다시 그리는 판단에는
[Qt Quick 장면 그래프의 배치·보관 전략](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph-renderer.html)을
참고했다. 구현은 iiGUI의 네이티브 렌더러가 소유하며 Qt 타입·호환 계층·런타임에
의존하지 않는다. 이 계약이나 헤드리스 시험은 Qt 대비 프레임 속도 또는 실제 GPU
픽셀 결과를 증명하지 않는다.

변경 판정 검사는 최초 장면, 창만 변경된 경우, 직접·간접 입력 변경, 사라진
캐시와 배율 변경을 확인한다. WebGL 모의 백엔드 검사는 유지된 텍스처의 재사용,
소스 변경의 전파, 전경 메모리 통계와 중간 업로드 실패 뒤의 재구성을 확인한다.
패널 묶음의 순서와 경계, GPU 버퍼 재사용과 단일 묶음 변경, 창과 소스 대상의
크기 변경, 화면 밖 클립, 업로드 실패 복구 및 자원 해제도 헤드리스 환경에서 검사한다.
2026-11-06 macOS 개발 빌드가 통과했고 관련 검사 13건 중 11건이 통과했다.
Direct3D·WebGL 셰이더 오프라인 검사 2건은 `glslangValidator`가 없어 건너뛰었다.
기존 빌드는 별도 준비된 iiSDKCore 헤더를 사용하므로, 현재 iiSDKCore 헤더로 Metal과
Vulkan 소스를 다시 구문 검사하고 현재 Core 라이브러리로 핵심 헤드리스 검사 3건을
별도로 통과시켰다. iPhoneOS Metal, Android NDK 26 Vulkan, Emscripten WebGL 소스의
대상별 구문 검사도 통과했다. Emscripten에서는 iiSDKCore의 기존 빌드 옵션에 따라
`-fexperimental-library`를 사용했다. 이는 각 플랫폼의 링크 또는 실제 GPU 실행 검증은 아니다.
macOS에서는 Metal·Vulkan 소스가 빌드되었다. 이 검사들은 실제 GPU 출력이나
Windows·Linux·Android·iOS 실기기의 성능 및 픽셀 검증을 대신하지 않는다.

`GraphicsInfo::scene_sources_supported`가 false인 렌더러는 해당 프레임을 미지원으로
거절한다. 자동 렌더러 선택에서 기본 네이티브 렌더러가 장면을 지원하지 않으면
프레임 발행 전에 Vulkan을 선택한다. 명시적으로 네이티브 렌더러를 지정했을 때는
조용히 다른 렌더러로 전환하지 않는다. 두 렌더러가 모두 지원하지 못하면 기존
렌더러를 보존하고 오류를 반환한다.

위젯 컨테이너는 `set_backdrop_sources(background, content)`로 자신의 재질에 두 위젯의
장면을 연결할 수 있다. 같은 창의 위젯만 허용하고 각 핸들은 세대에 대한 약한 참조다.
배경과 콘텐츠가 같은 위젯을 가리키거나 여러 재질이 소스를 공유하면 한 번만
캡처한다. 화면 장면을 만들 때 소스의 그리기 명령을 다시 수집하므로 같은 창에서의
내용·가시성·레이아웃·스크롤·팝업 변환 변경이 다음 프레임에 반영된다. 소스 세대가
종료되면 해당 입력은 비워지며 같은 ID의 후속 세대로 자동 연결하지 않는다.

```cpp
auto bound = material_widget.set_backdrop_sources(background_widget, content_widget);
if (!bound) return std::unexpected(bound.error());
auto selected = background_widget.snapshot_scene_graph();
if (!selected) return std::unexpected(selected.error());
// selected->scene의 인덱스는 selected->scenes를 가리킨다.
```

자신이나 자신의 장면에 포함된 조상·자식, 다른 창의 소스와 직접·간접 순환은 설정
시 거절하고 기존 두 입력을 보존한다. 숨겨진 위젯도 순환 검사에 포함한다. 위젯
트리가 나중에 교체되면 실제 프레임의 장면 그래프를 다시 검증하며, 잘못된 그래프는
발행하지 않는다. 정적 단일 `snapshot_scene()`은 선택된 장면에 다른 소스가 필요하면
이를 버리지 않고 `unsupported`를 반환한다. 이때 `snapshot_scene_graph()`의 `scene`과
`scenes`를 함께 보관해 `CanvasFrame::scenes`와 동일한 인덱스로 사용한다. 스냅샷은
불변 값이며 이후 위젯 변화에 따라 자동 갱신되지 않는다.

Windows의 Direct3D와 WebGL 장면 합성은 소스에 구현되었으나 실제 플랫폼 GPU 실행
검증은 남아 있다. Direct3D의 Microsoft 셰이더 컴파일도 검증하지 않았다.
`CanvasFrame`의 불투명
창 배경 계약은 그대로이며 투명 배경은 내부 오프스크린 렌더에만 사용한다.
헤드리스 검사는 그래프 순서·공유·직간접 순환·입력 한도·클립·효과 예산과 셰이더
컴파일을 검증하며, 실제 GPU 픽셀·운영체제 창 또는 플랫폼별 성능은 측정하지 않는다.

Vulkan 효과 셰이더는 [Khronos glslang 16.6.0](https://github.com/KhronosGroup/glslang/releases/tag/16.6.0)의
macOS universal release로 Vulkan 1.1 SPIR-V를 생성했다. 사용한 배포 압축 파일의
SHA-256은 `4f8c05f2888a73ed0b7b274bc524af9130e09ae94b43c0f3f9ed83cfae852834`이다.
컴파일러는 앱의 실행 의존성이 아니며 체크인된 SPIR-V와 소스 해시를 검사한다.

`iiGUI.direct3d_shader`는 `IIGUI_GLSLANG_VALIDATOR` 또는 PATH의 `glslangValidator`로
내장 HLSL의 다섯 진입점을 오프라인 컴파일하고 SPIR-V를 검증한다. 도구가 없으면
검사를 건너뛴다. 이는 Windows의 `D3DCompile` 및 실제 장치 검증과 별개다.

`iiGUI.webgl_shader`는 같은 선택적 도구로 WebGL의 네 GLSL ES 프로그램을
오프라인 컴파일·링크한다. `iiGUI.web_canvas_fake`는 실제 창이나 GPU를 만들지 않는
macOS 호스트 테스트로 프레임 순서, 투명도, 크롭, 두 픽셀 포맷의 예산, 스냅샷,
텍스처 공유·해제, 변환·업로드 실패 복구를 검사한다. 이 테스트와 Emscripten 대상
객체 컴파일은 브라우저 셰이더 드라이버나 표시 결과를 증명하지 않는다.

2026-11-02 WebGL 변경 후 iiGUI 전체 개발 빌드와 관련 헤드리스 검사 113건이
통과했다. 실제 GLSL ES 네 프로그램은 glslang 16.6.0으로 오프라인 컴파일·링크했고,
`CanvasWeb.cpp`와 `NativeCanvas.cpp`는 Emscripten 3.1.56으로 WebAssembly 객체를
생성했다. 테스트에서는 RGBA8과 두 확장의 RGBA16F, 필요한 스냅샷만 할당하는 경우 및 할당
실패 후 즉시 재시도도 확인했다. 전체 Web 앱 링크나 브라우저·GPU 실행은 하지 않았다.

2026-11-02 Direct3D 장면 변경 후 iiGUI 개발 빌드와 관련 헤드리스 검사 111건이
통과했다. 이때 위 HLSL 검사는 glslang 16.6.0 macOS universal release로 실행했다.
Windows x86-64 및 ARM64 대상의 `CanvasWindows.cpp` 객체는
[llvm-mingw 20260908](https://github.com/mstorsjo/llvm-mingw/releases/tag/20260908)로
교차 컴파일했다. 도구 압축 파일의 SHA-256은
`d1dc5d1ecf3a3ced5ed5544c72f1acd0c8e84eb3024d520ecc6b143eec62a149`이다.
Windows 전체 앱 링크, Microsoft의 셰이더 컴파일, GPU 픽셀 및 Windows 실행을 뜻하지 않는다.

2026-11-02에 iiGUI 전체 개발 빌드와 관련 헤드리스 검사 48건이 통과했다. 설치
스테이징의 공개 헤더와 소스 트리의 `WidgetState.cpp`, `NativeWidgets.cpp`는 각각
iPadOS 대상으로 구문 컴파일했다. 이 검사는 iPadOS 링크·기기 실행이나 Windows·Linux 실행을
뜻하지 않는다. 이후 장면 그래프와 Metal 소스 변경의 검증 결과는 전환 조사 기록을 따른다.

장면 그래프와 Metal 소스 변경 뒤 iiGUI 전체 개발 빌드와 관련 헤드리스 검사 107건이
통과했다. 이 검사에는 macOS·iPadOS Metal 셰이더의 오프라인 컴파일, 장면 그래프와
메모리 예산, 미지원 대체 렌더러 거절이 포함된다. Metal 구현 및 공통 그래픽·위젯
소스는 iPadOS 대상 구문 컴파일도 통과했다. 실제 GPU 합성 픽셀, OS 창, iPad 기기
링크·실행이나 Windows·Linux 실행을 검증한 것은 아니다.

Vulkan 장면 패스와 자동 렌더러 선택 변경 뒤 iiGUI 전체 개발 빌드와 관련
헤드리스 검사 108건이 통과했다. Vulkan 1.1 SPIR-V는 컴파일러에 포함된
SPIRV-Tools로 오프라인 검증했으며, Vulkan 렌더러와 네이티브 캔버스 소스도
iPadOS 대상으로 구문 컴파일했다. 기존 빌드 캐시를 사용했으며 GPU 실행이나
픽셀 출력, 창 실행과 iPadOS 링크·기기 실행은 검증하지 않았다.

위젯 소스의 자동 장면 그래프 연결 뒤 iiGUI 전체 개발 빌드와 헤드리스 검사
110건이 통과했다. 새 검사는 두 입력의 공유·중첩, 원본 변경, 숨김, 세대 교체,
팝업 포털, 준비 중인 콘텐츠, 순환 거절과 이전 세대 보존을 확인한다.
`WidgetState.cpp`와 `NativeWidgets.cpp`는 iOS 18 대상 구문 컴파일도 통과했다.
기존 빌드 캐시를 사용했으며 실제 창, GPU 출력이나 iPadOS 링크·기기 실행은 아니다.
