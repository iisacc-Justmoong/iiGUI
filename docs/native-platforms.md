# 데스크톱 플랫폼 경계

iiGUI의 위젯 트리는 브라우저나 WebView 없이 Cocoa, Win32, X11 창에 부착된다. 공통 WidgetState가 배치, 포커스, 편집, 캡처와 렌더 장면을 소유하고 플랫폼 어댑터가 운영체제 이벤트를 표준 값으로 변환한다.

macOS는 AppKit/Metal, Windows는 Win32/D3D11과 IMM32, Linux는 X11/XInput2/XIM과 Vulkan을 사용한다. 기본 네이티브 가속기가 실패하면 Vulkan으로 전환한다. Windows의 IME·클립보드·파일 드롭과 Linux의 XInput2 압력·기울기, XIM, XDND 및 INCR 클립보드 전송을 소스에 구현했다. Linux 파일 선택은 XDG Desktop Portal, 외부 URL은 GIO, 앱 메뉴는 GMenuModel/GActionGroup의 D-Bus 내보내기를 사용한다. Linux의 직접 Wayland 경로는 포함하지 않는다.

보조창은 부모의 iiSDKCore 이벤트 루프를 공유하며 각 창의 작업 범위만 취소한다. 파일 선택 완료와 메뉴 콜백은 UiExecutor를 통하여 해당 창의 유효한 수명 안에서 전달한다.

현재 검증 호스트는 macOS이다. Windows·Linux 구현은 소스 이식 범위이며 해당 운영체제에서의 빌드·실행·IME·접근성 완료를 주장하지 않는다. Linux 어댑터의 운영체제 접근성 객체 노출은 아직 제공하지 않는다. 공통 위젯 의미와 키보드 조작은 유지한다.

Vulkan 재료 효과의 실제 GPU 검사는 `iiGUI.vulkan_effects`, Cocoa 입력과 보조창 검사는 `iiGUI.desktop_*` 및 `iiGUI.editor_*`에서 수행한다. 펜 장치의 필드 전달 검사와 물리 태블릿 실기기 검증은 별개의 증거이다.

## 설치 ABI 일치 검사

네이티브 위젯·창·그래픽 값의 공개 헤더 해시를 iiGUI 패키지에 기록한다. LVRS는 빌드한 iiGUI의 ABI 식별자를 설치 Config에 보존하며, 같은 버전 번호라도 다른 식별자의 키트와 조합하면 구성 단계에서 거부한다. 공개 Snapshot 크기가 다른 라이브러리를 연결하여 실행 중 스택이 손상되는 문제를 예방한다. `LVRS.native_package_abi`는 일치·오래된 식별자·식별자 누락의 세 구성을 검사한다.
