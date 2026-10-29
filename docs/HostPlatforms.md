# Core 대상과 GUI 호스트의 연결

iiSDKCore의 컴파일 대상과 iiGUI의 창 시스템 호스트는 별개의 계약이다. Core의
자료형·작업·I/O를 컴파일할 수 있다는 사실만으로 창·입력·GPU·OS 서비스가 제공되지는
않는다. 임베디드·Arduino·FPGA·QNX 대상에 데스크톱 창을 광고하지 않는다. 제공하지
않는 기능은 `unsupported`, 권한·세션·서비스 부재는 실패 결과로 돌려준다.

Core를 GUI에 의존시키지 않는다. iiGUI가 Core Runtime을 참조하고
`attach_application`이 Core 소유 EventLoop와 Application 수명에 호스트를 연결한다.
기존 C++23 Host/UiExecutor/WidgetBuilder와 LVUI AOT 계약을 공유한다.

## 대상별 계약

| 대상 | 호스트 | 자체 위젯 출력 | 프레임 공급원 |
| --- | --- | --- | --- |
| macOS | Cocoa | Metal, 선택적 Vulkan | 창 display link |
| Windows | Win32 | D3D11, 선택적 Vulkan | 기존 명시적 cadence |
| Linux/Wayland | wl_compositor + xdg-shell | VK_KHR_wayland_surface | wl_surface.frame |
| Linux/X11 | Xlib + XI2 | VK_KHR_xlib_surface | 기존 명시적 cadence |
| Android | NativeActivity/native_app_glue/ALooper | VK_KHR_android_surface | AChoreographer |
| iOS/iPadOS | UIKit 장면 | Metal, 선택적 MoltenVK | 기존 display link |
| Web | WASM application pthread + canvas | WebGL2/OffscreenCanvas | worker requestAnimationFrame |

OS 버튼·레이블·레이아웃 엔진으로 위젯을 교체하지 않는다. 공통 WidgetState가
레이아웃, 입력 대상, 포커스, 상태와 표시 순서를 소유한다. Web도 DOM 위젯 트리를
생성하지 않는다. canvas는 한 개의 입력·출력 표면이며 문자 shaping/raster만 Canvas
2D 글꼴 서비스에 연결한다.

`host_capabilities()`는 **빌드에 구현된 계약**을 반환한다. 실제 권한이나 compositor,
세션 서비스의 존재를 보장하지 않으므로 각 작업의 Result를 계속 확인해야 한다.
`display_frames`는 디스플레이/compositor callback 경로를 뜻한다. 실제 scanout 시각을
측정했다는 뜻이 아니다. 고정 주기 타이머를 `display_synced`로 보고하지 않는다.

## Linux 직접 Wayland

```sh
cmake -S . -B build/wayland -G Ninja \
  -DIIGUI_LINUX_BACKEND=WAYLAND \
  -DCMAKE_PREFIX_PATH="/path/to/target/sdk-prefix"
cmake --build build/wayland
cmake --install build/wayland --prefix "$PWD/build/wayland-prefix"
```

`IIGUI_LINUX_BACKEND`는 AUTO/WAYLAND/X11이다. AUTO는 wayland-client/wayland-cursor 1.20 이상과 xkbcommon 1.0 이상 개발
패키지가 있으면 Wayland, 없으면 X11을 선택한다. WAYLAND를 명시했는데 의존성이
없으면 구성에서 실패한다. 실행 중 실패를 XWayland 전환으로 숨기지 않는다. 한 설치
패키지는 한 창 시스템을 선택하므로 둘을 배포하려면 별도 prefix/kit를 사용한다.
Wayland 빌드는 X11/Xi를 필수로 찾지 않고 X11 호스트·위젯·Vulkan 창 소스를 제외한다.
Pango/Cairo, Fontconfig, GIO는 텍스트·글꼴·포털 서비스에 사용한다. 이 라이브러리의
배포판별 간접 의존성과 창 호스트 선택은 구분한다.

- xdg-shell configure를 ack한 뒤 앱 attachment와 GPU 표시를 시작한다. 0 크기는
  기존 크기를 유지하고 음수·좌표 예산 초과는 GPU 할당에 반영하지 않는다.
- 출력 진입·이탈, 정수 buffer scale, 크기 변경을 논리 좌표와 물리 픽셀로 구분한다.
- prepare_read/read_events/cancel_read와 poll로 compositor 소켓, Core eventfd,
  데드라인을 함께 기다린다. 막힌 출력에는 POLLOUT을 사용한다.
- 정수 DPI에 맞는 시스템 커서와 seat 포인터·키보드·수정 키·키 반복을 C++ 입력 값으로 변환한다. XKB keymap 크기를
  제한하며 keymap/state를 교체할 때 이전 소유권을 해제한다.
- 필요한 동안에만 프레임 callback을 요청한다. compositor 프레임이 멈춰도 Core
  타이머는 처리하며 임의 16ms 타이머를 디스플레이 프레임으로 가장하지 않는다.
- 클립보드는 실제 wl_data_device offer/source/pipe를 사용한다. 쓰기에는 seat 입력
  serial이 필요하다. 읽기는 UTF-8·1MiB·2초로 제한한다. 전송 pipe는 nonblocking이고
  수신자가 끊어도 SIGPIPE로 프로세스가 종료되지 않는다.
- 파일 선택은 XDG Desktop Portal을 사용하며 결과를 UiExecutor에 전달한다. 현재
  Wayland parent export가 없어 parent 문자열은 비어 있다. 파일 선택은 파일 생성이
  아니다. 실제 파일 접근·생성·제공은 iiFileProvider가 맡는다.
- 외부 URL은 OS의 기본 처리기로 전달한다.

현재 Wayland에 text-input-v3/IME preedit, 접근성, tablet/touch, DnD, 보조 창,
fractional-scale, xdg-activation token은 없다. 직접 키보드 UTF-8 입력과 Pango
shaping을 이 기능들의 완성으로 간주하지 않는다. 프로토콜 없는 글로벌 메뉴나
token 없는 raise는 성공으로 가장하지 않는다.

xdg-shell은 wayland-protocols 1.44 XML에서 wayland-scanner 1.23.1로 생성했다.
XML·private 생성 코드·SHA-256·MIT 고지를 src/third_party/wayland에 유지한다.
교차 빌드와 설치 소비자는 타겟 scanner를 실행하지 않는다.

Android·Wayland·Web의 단일 OS 표면은 약한 GPU registry를 사용한다. 위젯 세대가
교체 중 잠시 겹쳐도 같은 창에 두 swapchain/context를 만들지 않는다. 실패한 마운트는
이전 세대를 다시 표시하고, 마지막 소유자가 사라지면 GPU 자원을 해제한다.

Android·Wayland·Web의 raw `set_canvas`는 현재 창 전체 viewport만 받는다. 부분
viewport를 억지로 늘려 그리지 않으며 `unsupported`를 반환한다. 부분 화면 구성은
공통 C++ 위젯·레이어·클립을 사용한다.

## Android 호스트와 OS 연결

진입점은 `run_android_application`이다. Core Application을 native_app_glue 스레드에서
생성·실행·정리하고 Activity 수명과 창 재생성은 ALooper 명령으로 처리한다. 앱 동작과
위젯은 C++23이다. 시스템 Activity/ClipboardManager/Intent 접속에만 C++ JNI를
사용하며 Java/Kotlin 위젯이나 LVUI 인터프리터를 넣지 않는다.

read_clipboard/write_clipboard/open_external_url을 실제 Android 서비스로 연결했다.
내부 RAII 객체가 JNI attach/detach, local reference frame, 예외 정리와 Core의
UTF-8↔UTF-16 변환을 맡는다. modified UTF-8을 공개 문자열로 오인하지 않는다.
클립보드 텍스트·URL을 지원하고 이미지·파일 payload는 명시적으로 거부한다. OS의
포커스·권한 정책이나 URL 처리기 부재는 실패가 될 수 있다.

API 29+ AChoreographer_postFrameCallback64가 프레임을 공급한다. 앱 FrameLoop나
위젯 모션이 있을 때만 다음 callback을 요청한다. pause/창 해제에서는 GPU와 다음
프레임을 멈추고 resume에서 Core 시계를 재설정한다. 이미 요청된 callback은 소유권
gate를 통해 종료된 Host를 참조하지 않는다. 미리 할당한 callback 슬롯을 재사용하고
Looper 스레드가 끝나면 OS가 전달하지 않은 슬롯도 정리하므로 매 프레임 ticket을
새로 할당하지 않는다.

Android 문자 출력은 기존 ASCII 레이블 범위이다. Unicode shaping/IME, 문서 선택,
접근성, 보조 창은 아직 별도 구현이 필요하다. OS 연결로 이 범위까지 완성했다고
간주하지 않는다. NDK 빌드와 실제 휴대폰 실행은 별도로 기록한다. 재현 도구는
tools/android_hello.py이다.

## Web 실행 계약

Web은 OS 실행 파일과 구별되는 **AOT WebAssembly 대상**이다. 브라우저의 JS glue와
API bridge는 존재한다. LVUI 소스 해석, Qt/QML, 공통 바이트코드 VM은 추가하지 않는다.
Web 결과를 OS 머신 코드 바이너리라고 부르지 않는다.

```sh
iisdk-builder build --manifest examples/HelloHosts/iiSDKBuilder.json
python3 -B tools/serve_web.py build/hosts/kits/web/Release/app/build
# http://127.0.0.1:8765/HelloHosts.html
# 자동 종료 검증: HelloHosts.html?self-test
```

교차 빌드에서 LVUI_HOST_COMPILER에 호스트용 lvui-compile을 지정한다. HelloHosts의
뷰는 빌드 시 C++로 생성하며 최종 산출물에 .lvui나 컴파일러를 넣지 않는다. 설치한
예제도 별도 소비자에서 다시 빌드할 수 있다. Emscripten 배포본에 clang-scan-deps가
없는 경우 일반 C++23 소비자에는 CMAKE_CXX_SCAN_FOR_MODULES=OFF를 지정한다.
HelloHosts는 언어 module 단위를 사용하지 않으므로 네이티브 kit에서도 이 값을
기본으로 둬 불필요한 전체 헤더 스캔을 피한다. 소비자가 명시한 설정은 보존한다.
iiSDKBuilder와 설치 소비자 검증 도구는 이 값을 설정하며 C++ 언어/ABI 검사는 유지한다.

Core가 브라우저 UI 스레드에서 worker 생성/join을 거부하는 기존 계약을 유지한다.
설치된 Core Runtime이 공개 CMake 계약에 -pthread를 내보내지 않으면 iiGUI 구성에서
즉시 거부한다. 스레드 없는 Core와 GUI 호스트를 섞은 패키지를 성공으로 보고하지 않는다.
PROXY_TO_PTHREAD로 앱을 pthread에서 실행하고 Core 상시 worker pool을 사용한다.
OffscreenCanvas도 앱 스레드가 소유하며 DOM/클립보드/URL 호출만 메인 스레드로
전달한다. OS I/O readiness waiter는 만들지 않고 HostLoop wake 계약을 사용한다.

Asyncify는 C++ 스택을 보존하면서 브라우저에 제어를 돌려주는 빌드 변환이다. 위젯
소스를 해석하거나 JavaScript로 위젯 로직을 재작성하지 않는다. 외부 입력 callback은
Asyncify 대기 중 사용자 코드를 호출하지 않고 제한된 C++ 큐에 복사한다. 대기 복귀
뒤에만 처리해 비동기 OS 요청의 중첩 unwind를 방지한다. 연속 이동만 병합하고
press/release/key 경계는 보존한다. 포화 시 입력을 조용히 버리지 않고 오류를 남긴다.
Promise에서 C++ 스택을 복귀할 때는 Emscripten의 callUserCallback 경계를 사용한다.
정상 pthread 종료의 ExitStatus/unwind와 실제 오류를 구분하며 전역 오류를 숨기지 않는다.
설치 소비자의 실제 브라우저 검사도 정상 종료 로그 이후 console 오류가 없는지 확인한다.
tools/verify_web_runtime.py는 브라우저 개발 도구에서 수집한 JSON 로그를 검사한다.
READY/OK/CLOSED 순서, 모든 기능 검사와 양수 프레임 수를 요구하며 CLOSED 뒤 오류도
실패로 판정한다. 검증기의 단위 테스트는 실제 브라우저 실행 증거와 구분한다.

유휴 대기는 Atomics.waitAsync와 Core 데드라인으로 구성한다. 모션/FrameLoop가 있을
때만 worker requestAnimationFrame을 요청한다. Core wake가 와도 이미 요청한 프레임을 취소하지
않고, MessageChannel task yield를 통해 연속 wake가 브라우저 프레임을 굶기지 않게 한다.
자체 검사에서는 1ms 간격 background wake 중에도 프레임이 진행되는지 확인한다. 숨겨진 페이지에서도 Core 데드라인을
처리하고 GPU 표시는 멈춘다. context loss에는 표시를 정지하고 restore에서 자원을
재구축한다. readback은 명시적 capture에서만 사용한다.

WebGL2는 선형 target에 premultiplied 합성 후 최종 패스에서 sRGB로 변환한다.
지원되는 경우 RGBA16F, 그 외에는 RGBA8 선형 target을 사용한다. 문자·이미지 texture는
sRGB로 읽고 내용·크기·DPI가 같은 foreground는 재래스터하지 않는다. 문자·이미지의
실제 잉크 영역만 texture로 올리며 overlay마다 창 전체 bitmap을 만들지 않는다. 브라우저 GPU
masking 때문에 GraphicsInfo.hardware를 추측해 참으로 만들지 않는다.
require_hardware는 거부한다. srgb_attachment=false는 shader가 최종 인코딩을
수행한다는 뜻이며 화면에 sRGB 변환이 없다는 뜻이 아니다.

클립보드와 외부 URL은 HTTPS/localhost 및 사용자 활성화·권한 정책을 따른다.
파일 handle을 가짜 로컬 경로로 돌려주지 않는다. 문서 선택/FileProvider handle,
IME/텍스트 편집, 접근성, 보조 canvas/window는 현재 계약 밖이다.

필요 조건은 Emscripten pthread, SharedArrayBuffer, OffscreenCanvas/WebGL2,
worker requestAnimationFrame, Atomics.waitAsync, Worker FontFace/FontFaceSet이다.
서버에 COOP same-origin, COEP require-corp가 필요하다. tools/serve_web.py는
127.0.0.1에 이 헤더들을 제공하며 공개 서버나 자동 시작 작업을 등록하지 않는다.

## 검증 기록

결과는 build/host-expansion-20261027/verification.json과 플랫폼 로그에 기록한다.
교차 빌드·설치 소비자 링크·실제 브라우저/장치 실행을 구분한다. Windows/Linux의
실제 실행을 생략하라는 사용자 지침을 유지하고 실행 보증으로 승격하지 않는다.
macOS 창 실행 검증은 마지막 묶음에서만 진행한다.

2026-10-27 검증에서는 독립 prefix 설치 후 경로를 옮긴 소비자로 다음 결과를 얻었다.
이 prefix들은 build/ 아래의 검증 설치본이며 사용자 ~/.local 설치본을 교체한 것은 아니다.

| 호스트 | 빌드·설치 소비자 | 실제 실행 |
| --- | --- | --- |
| macOS/Cocoa, arm64 | 라이브러리·AOT 앱·설치 소비자 통과 | Metal 창, 타이머, worker 결과 42, resize, 디스플레이 프레임, capture, 정상 종료 통과 |
| Web/Wasm pthread | 라이브러리·AOT 앱·설치 소비자 통과 | Chrome에서 타이머, worker 결과 42, resize, 프레임 60회, capture, 오류 없는 종료와 실제 Close 버튼 입력 통과 |
| Linux/Wayland, aarch64 | 직접 Wayland 호스트·AOT ELF·설치 소비자 통과 | 사용자 지시에 따라 생략 |
| Linux/X11, aarch64 | 회귀 라이브러리·AOT ELF·설치 소비자 통과 | 사용자 지시에 따라 생략 |
| Windows/Win32, x86_64 | 회귀 라이브러리·AOT PE32+·설치 소비자 통과 | 사용자 지시에 따라 생략 |
| Android, arm64-v8a | NDK 빌드·의존성 폐쇄·NativeActivity APK 정렬·서명 검사 통과 | 기존 Hello World 성공에 따라 이번 재실행은 생략 |

창을 열지 않는 CTest 34/34, ASan+UBSan 계약 검사 7/7, Python 검사 25/25가
통과했다. Python 수치는 CTest에서 호출한 일부 검사와 중복되므로 이들을 합산하지 않는다.
pthread 공개 계약을 제거한 Core 패키지를 실제 구성에서 거부하는 부정 구성 검사도 통과했다.
Windows/Linux/macOS 최종 앱의 Qt·LVUI 파서·인터프리터 부재, Core 최종 진입점과
설치 헤더 사용을 확인했다. Wayland 앱에 XOpenDisplay/XCreateWindow 경로가 없음을
확인했으며 배포판 라이브러리의 모든 간접 의존성 부재까지 주장하지 않는다.

브라우저 첫 실행에서 발견한 정상 종료의 unhandled unwind는 수정 후 같은 소비자를
재빌드해 해소했다. 최초 실패 로그와 수정 후 실행 로그를 모두 보존했다. macOS 창은
최종 검증 단계에서만 실행했다. 위 결과는 전체 GUI 테스트나 모든 OS 기능의 동등성을
의미하지 않으며 각 대상의 남은 기능 제한은 앞 절을 따른다.

참고 계약: [Wayland client API](https://wayland.freedesktop.org/docs/html/apb.html),
[Emscripten HTML5 API](https://emscripten.org/docs/api_reference/html5.h.html),
[Android JNI 지침](https://developer.android.com/ndk/guides/jni-tips).

종료 요청은 즉시 작업 접수를 닫고 Core 루프를 정지시키되, 실행 중인 입력/프레임
callback이 반환한 뒤 OS/GPU 자원을 해제한다. Wayland의 protocol callback은 입력,
configure, 프레임, 닫기 통지만 저장하며 사용자 코드는 바깥 Core pump에서 실행한다.
이 규칙은 클립보드 전송 중 들어온 새 프로토콜 이벤트의 재진입에도 적용된다.

물리 Android OS fixture는 빌드 시 IIGUI_ANDROID_OS_SMOKE=ON으로 선택한다.
활성 창과 잠금 해제 상태를 먼저 확인하고 원본 ClipData를 보관한 뒤 한국어/보조 평면
문자 클립보드 왕복을 검사·복원한다. 지원하지 않는 레이블로 마운트를 실패시킨 뒤
기존 GPU capture가 동일한지도 검사한다. 정상 Activity 종료는 HELLO_ANDROID_EXIT로
관찰할 수 있으며, 기본 샘플은 OS fixture 없이 빌드된다.

Windows 회귀 빌드에서는 DirectWrite의 파일 객체용 CreateFontFaceReference가
MSVC 헤더의 overload와 MinGW IDL 헤더의 underscore 이름으로 다르게 선언되는
경계를 검사한다. C++23 requires로 해당 시그니처를 선택해 동일 COM 슬롯을 호출하며,
이미 분석한 파일 객체를 경로로 다시 열지 않는다. 모의 양쪽 헤더 계약 검사와 실제
Windows 교차 빌드를 분리해 검증한다.
