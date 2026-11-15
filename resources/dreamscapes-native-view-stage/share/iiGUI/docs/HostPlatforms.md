# Core 대상과 GUI 호스트의 연결

## 창이 표시되는 디스플레이 해상도

네이티브 호스트는 열린 창이 속한 디스플레이의 현재 픽셀 너비와 높이를 조회할 수
있어야 한다. 값은 창이나 작업 가능 영역의 크기가 아니며, 디스플레이의 현재 방향을
반영한다. 배율이 적용된 논리 좌표를 픽셀로 오인하지 않는다. 창이 아직 열리지
않았거나 디스플레이가 아직 결정되지 않았다면 추측한 크기를 반환하지 않는다.
여러 출력이 한 창에 겹치고 시스템이 우선 출력을 알려주지 않는 플랫폼은 선택
기준을 문서화한다. 디스플레이 모드가 바뀌면 다음 조회에 현재 값이 반영된다.

macOS는 창의 `NSScreen`에 연결된 현재 디스플레이 모드의 픽셀 크기를 사용한다.
iPadOS는 실제 화면의 고정 방향 픽셀 경계를 화면 방향에 맞춘다. 분할 화면이나
크기가 조절된 장면의 비율이 화면 방향을 바꾸지 않는다. Windows는
창이 속한 모니터의 전체 픽셀 경계를 사용한다. Wayland는 해당 표면이 진입한
출력의 현재 모드를 사용하며, 여러 출력에 걸쳐 있으면 프로토콜이 주 출력을
알려주지 않으므로 등록 순서가 가장 앞선 출력을 선택한다. 출력 진입 전에는
`busy`를 반환한다. X11은 RandR 1.5 모니터와 활성 출력 모드가 필요하고,
창과 가장 많이 겹치는 단일 출력을 사용한다. Android는 현재 디스플레이 모드를
사용한다. 브라우저 환경과 정보를 제공하지 않는 시스템에서는 실제 화면을 추측하지
않고 `unsupported` 또는 오류를 반환한다.

iiSDKCore의 컴파일 대상과 iiGUI의 창 시스템 호스트는 별개의 계약이다. Core의
자료형·작업·I/O를 컴파일할 수 있다는 사실만으로 창·입력·GPU·OS 서비스가 제공되지는
않는다. 임베디드·Arduino·FPGA·QNX 대상에 데스크톱 창을 광고하지 않는다. 제공하지
않는 기능은 `unsupported`, 권한·세션·서비스 부재는 실패 결과로 돌려준다.

Core를 GUI에 의존시키지 않는다. iiGUI가 Core Runtime을 참조하고
`attach_application`이 Core 소유 EventLoop와 Application 수명에 호스트를 연결한다.
기존 C++23 Host/UiExecutor/WidgetBuilder와 LVUI AOT 계약을 공유한다.

## iiGUI 독립 SDK 경계 명세

앱은 iiGUI와 LVUI를 함께 선택할 수 있다. 이 선택을 iiGUI 모듈이 LVUI에
의존한다는 뜻으로 해석하지 않는다. iiSDKBuilder의 SDK 항목은 다른 SDK 항목을
`depends`로 참조할 수 없으며, HelloGUI의 iiGUI 항목은 실제 iiGUI 소스를 가리켜야
한다. 매니페스트의 Qt·QML·LVRS SDK 선택과 중복·잘못된 SDK 참조는 거부한다.
iiGUI의 소스와 공개 패키지도 기존처럼 LVUI·LVRS·Qt 연결을 허용하지 않는다.

독립 소비자 검사는 현재 빌드의 iiGUI를 일회성 패키지로 설치한 뒤 LVUI·LVRS·Qt
패키지 탐색을 비활성화하여 HelloGUI와 별도 C++ 소비자를 구성·컴파일·링크한다.
선택적 LVUI 생성 위젯 조합 검사는 더 이상 모든 검사 대상에 iiColorManager 값
타깃을 일괄 추가하지 않는다. 각 앱의 색상 객체는 LVUI의 값 계약으로 전달한다.
생성된 앱이나 창은 실행하지 않는다. 임시 패키지에 필수 LVUI 의존성을 주입한
부정 검사도 거부되어야 한다. 직접 동적 연결 검사는 전체 간접 의존성 폐쇄나
다른 운영체제 실행의 증거가 아니다.

2026-11-04 macOS arm64에서 LVUI·LVRS 소스 및 설치 경로와 Qt SDK 경로의 읽기를
차단하고 경계·계약·설치 소비자와 호스트 회귀 검사 18건을 통과했다. iiGUI의 비교 문서
`QtGuiComparison` 두 개는 설치 과정의 비실행 자료이므로 읽기를 허용했다.
현재 `build/`의 iiGUI를 임시 설치·재배치한 후 HelloGUI와 독립 소비자를
각각 컴파일·링크했고 직접 동적 연결에서 LVUI·LVRS·Qt를 찾지 못했다. 임시
패키지에 삽입한 필수 LVUI 의존성도 거부했다. iiSDKCore는 작업 공간의 기존
스테이징 설치본을 사용했으며 실제 창·GPU와 전체 간접 의존성은 검증하지 않았다.
기존 HelloGUI 앱 매니페스트는 변경하지 않았다. 앞선 경계 검사 실패 기록은 당시
검사가 앱의 SDK 선택을 iiGUI 모듈의 의존성과 동일시한 결과로 보존한다.
소스와 모든 iiGUI CMake 모듈의 경계를 검사하며, 별도 부정 검사에서는 검사용
문자열에 LVUI·LVRS 및 Qt/QML 참조를 넣어 각각 거부되는지도 확인한다.

## 외부 이벤트 루프 호스트

macOS의 `Host::show_external`은 이미 실행 중인 `NSApplication`의 실행 루프에
별도 iiGUI 호스트를 연결한다. 기존 애플리케이션의 델리게이트·메뉴·실행 루프를
교체하거나 종료하지 않는다. 호스트 자체의 Core 이벤트 루프와 깨움 원본은 독립적으로
소유하므로 외부 `event_loop` 또는 `frame_loop`를 전달할 수 없다. 애플리케이션 활성·
전경 콜백도 외부 애플리케이션의 책임이다. 숨긴 application driver와 그 보조 창을
사용할 수 있다. 다른 플랫폼은 현재 `unsupported`를 반환한다.

`Host::attach_external`은 기존 창에 속한 macOS `NSView`를 빌려 그 안에 iiGUI 위젯
표면을 연결한다. 창의 델리게이트를 교체하지 않으며 호스트의 닫기 명령은 빌린 창이
아닌 위젯 표면만 닫는다. 창이 닫히면 호스트도 닫힌다. 제목·창 크기·창 동작과 앱
메뉴를 바꾸는 요청은 거절한다. 소유자는 닫힘 콜백까지 부모 뷰를 보존해야 한다.
표면이 포커스를 소유한 채 제거되면 앞서 포커스를 가졌던 응답자를 복원한다.
임베딩 표면은 테마의 창 색상을 바꾸지 않고 투명한 배경으로 합성하므로 소유 창이
모달 뒤에 보인다. macOS에서는 투명 합성을 지원하는 Metal 렌더러를 사용하고,
Vulkan을 명시적으로 선택한 경우에는 불투명하게 표시하는 대신 요청을 거절한다.

생성·연결·종료는 메인 스레드에서 수행하며 외부 애플리케이션의 실행 루프가 끝나기
전에 호스트를 닫아야 한다. 닫힘이 호스트의 작업 처리 안에서 발생할 수도 있으므로
정리와 `closed` 콜백은 현재 네이티브 콜백이 돌아간 다음에 전달한다. 소유자는 그
콜백이 올 때까지 호스트를 유지해야 한다. 닫힘 콜백 예외는 등록된 실패 핸들러에
전달한다. 최초 실패 저장소는 이미 닫힌 호스트 밖에 보관하지 않는다.

`iiGUI.host_external_loop_mac`은 화면에 보이는 창 없이 별도 테스트 프로세스의
AppKit 루프에서 델리게이트·메뉴 보존, 작업과 타이머 전달, 닫힘 순서를 확인한다.
`iiGUI.host_embedded_mac`은 숨긴 부모 창의 실제 위젯 표면 연결과 분리, 델리게이트·
포커스 보존 및 Metal 표면의 투명도를 확인한다. `iiGUI.host_embedded_frame`은 테마
색상과 투명 배경이 분리되는지 확인한다. 부모 창 종료도 별도 검사한다. 다른 앱과의 통합이나 화면의
그래픽·입력 품질을 검증한 결과는 아니다.

## 독립 앱 드라이버 전환 명세

데스크톱 다중 창을 위해 창 표시와 앱의 네이티브 이벤트 펌프를 분리한다.
`Host::set_application_driver(true)`로 설정한 최상위 호스트는 사용자에게 보이는
창을 열지 않지만 `opened`를 전달하고 Core 이벤트 루프와 앱 활성 상태를 소유한다.
Cocoa의 전역 메뉴는 이 드라이버가 담당한다. Win32·X11의
네이티브 메뉴는 창에 부착되므로 보이는 창에 설정해야 하며, 드라이버의 메뉴를
모든 창에 자동 복제하는 기능은 없다. 이 드라이버가 여는 각 보조 호스트는 서로 독립된 창이며,
개별 창의 닫힘과 세대 정리는 다른 창이나 드라이버를 정지시키지 않는다.
드라이버 종료는 남은 창을 정리하고 공통 이벤트 루프를 끝낸다. 이 옵션을 지원하지
않는 호스트는 성공으로 가장하지 않고 `unsupported`를 반환한다. 보조 호스트에
드라이버 설정을 적용하는 것도 거부한다. 기능은 `supports_application_driver()`로
질의하며 기존 `HostOptions`와 `HostCapabilities`의 이진 배치는 바꾸지 않는다.
화면에 나타나지 않는 드라이버에는 창의 표시 주기에 묶인 `frame_loop`를 지정할
수 없다. 각 표시 창이 자체 프레임 주기를 소유한다.
Win32·X11·Wayland 보조 창은 네이티브 닫힘 처리 중 소유자가 해제되지 않도록 닫힘 통지를
부모 이벤트 루프에 지연해 전달한다. 닫힌 보조 호스트를 파괴할 때 부모의 창 목록과
네이티브 자원도 정리한다. 통지 접수에 실패하면 부모 호스트 실패로 보존한다.
이 경로는 Cocoa, Win32, X11과 Wayland 소스에 연결되어 있으며 다른 호스트는
거부한다. Windows와 Linux의 현재 변경은 전체 플랫폼 빌드나 실제 실행을
검증했다는 뜻이 아니다.

2026-11-04 macOS arm64에서 iiGUI 전체 빌드와 드라이버·호스트 관련 헤드리스
검사 9건을 통과했다. 새 드라이버 ABI 표식은 이전 스테이징 라이브러리와의 링크를
거부하고 새 라이브러리와는 링크했다. 이전과 새 `HostOptions` 및
`HostCapabilities`의 macOS arm64 크기·정렬도 비교했다. UIKit 호스트 소스는
iPhoneOS 18 이상 arm64로 문법만 검사했다. 실제 운영체제 창이나 GPU를 열지
않았으며, 이 결과는 다른 아키텍처의 ABI나 Windows·Linux 실행을 검증하지 않는다.

## 호스트 실패 통지 명세

`Host::set_failure_handler`는 주 창이나 보조 창을 실행하기 전에 소유 스레드에서
최초 호스트 실패를 수신할 콜백을 설정하거나 교체한다. 호스트가 내부 실패를 처음
기록하는 시점에 원래 오류 코드·메시지·원인을 동기적으로 한 번 전달하며, 이후
오류와 핸들러 예외는 최초 오류를 덮지 않는다. 콜백은 원본 저장소가 교체되거나
창이 닫혀도 실행 중인 복사본을 유지한다. 이 알림은 API 호출에서 즉시 반환하는
일반 오류를 대신하지 않는다. 기존 `HostCallbacks` 구조체의 이진 배치는 유지한다.

2026-11-04 macOS arm64에서 호스트 라이브러리와 관련 헤드리스 검사 7건이 통과했다.
최초 오류와 핸들러의 재진입·자체 해제·예외는 AddressSanitizer와
UndefinedBehaviorSanitizer로도 검사했다. iPadOS와 Web 호스트 소스는 문법만
검사했으며 실제 창·기기·브라우저는 실행하지 않았다.

## 주 호스트 활성 상태 명세

`opened` 콜백은 호출 중 호스트나 부모 창이 닫혀 원래 콜백 저장소가 정리되더라도
호출이 끝날 때까지 자체 캡처를 유지해야 한다. 호스트는 실행할 콜백을 먼저
복사하며, 그 콜백에서 발생한 예외는 기존 호스트 오류 경로로 전달한다.

`Host::set_application_activity_handler`는 주 호스트가 실행되기 전에
소유 스레드에서 교체 가능한 `bool` 콜백을 등록한다. 플랫폼의 활성·비활성 신호는
호스트가 열리고 `opened` 콜백이 성공적으로 반환된 뒤 같은 스레드에서 전달한다.
그 전에 여러 신호가 오면 마지막 상태만 전달하고, 열린 뒤의 상태 전환은 순서대로
전달하며 같은 상태의 반복 신호는 생략한다. 64개를 넘는 대기 전환이나 무한 재진입은
조용히 버리지 않고 호스트 오류로 처리한다.
상태를 모르는 동안 임의의 초기값을 발행하지 않는다. 종료가 먼저 처리되면 대기
상태는 폐기하고 종료 후에는 전달하지 않으며, 콜백 예외는 호스트 실패로 처리한다.
이 계약은 주 호스트에 적용한다.

상태의 원천은 Cocoa 애플리케이션 활성화, Win32 애플리케이션 활성화, UIKit 장면,
Android의 resume/pause, Web 페이지 표시 상태, X11 활성 최상위 창과 Wayland
최상위 창들의 compositor 활성 상태이다. Wayland 앱 드라이버는 알려진 활성 창이
하나라도 있으면 활성 상태로 집계한다. 플랫폼별 개념이 완전히 동등하다는 뜻은 아니다. X11에서 윈도 관리자 상태가 제공되지 않으면 값을 임의로 만들지 않는다.
보조 호스트에 이 핸들러를 설정한 채 `show`하면 오류를 반환한다. 실제 장치와 창
시스템을 실행한 결과는 이 소스 계약과 별개이다.

2026-11-04 macOS arm64에서 호스트 라이브러리와 상태 전달·등록 검사를 빌드하고
헤드리스 검사 세 건을 통과했다. 상태 전달의 콜백 중 종료와 재진입을 포함한 검사는
AddressSanitizer와 UndefinedBehaviorSanitizer를 적용해서도 통과했다. iPadOS 및 Web 호스트 소스는 각각 대상 SDK로 문법
검사했으며 실제 장치·브라우저나 창을 실행하지 않았다. 전체 경계 검사는 기존
`examples/HelloGUI/iiSDKBuilder.json`의 LVUI 의존성 때문에 실패했다. 이 설정은 이번
변경 전에 수정된 사용자 작업이므로 보존했으며, 검사 출력에는 다른 위반이 없었다.

같은 날 창 열림 콜백을 실행 중인 상태에서 원래 콜백 저장소가 비워지는 경우와
콜백 예외를 헤드리스 검사와 AddressSanitizer·UndefinedBehaviorSanitizer로
확인했다. macOS 호스트 라이브러리 빌드와 호스트 관련 검사 다섯 건이 통과했으며,
Web 호스트는 문법 검사했다. 실제 창·브라우저 실행은 포함하지 않는다.

## 앱 전경·백그라운드 전환 명세

`Host::set_application_foreground_handler`는 앱의 전경 상태를 수신할 콜백을
실행 전에 주 호스트의 소유 스레드에서 설정한다. `true`는 플랫폼이 보고한 전경,
`false`는 백그라운드이다. 창 포커스에 따른 `set_application_activity_handler`와
별개의 상태이며 한쪽을 다른 쪽에서 추정하지 않는다. 최초 상태가 알려지지 않으면
임의로 발행하지 않는다. 창 열림 전에는 마지막 상태를 보관하고 열림 후에는 같은
상태의 중복을 생략하며 순서와 재진입·용량 제한을 유지한다. 종료 뒤에는 전달하지
않고 콜백 실패는 호스트 오류로 전달한다.

UIKit은 프로세스 전체 `UIApplication` 전경·백그라운드 알림, Android는 현재
NativeActivity의 start/stop, Web은 문서의 visibility 상태를 사용한다. Cocoa에서는
앱이 숨겨지지 않았고 최소화되지 않은 iiGUI 창이 하나 이상 있을 때 전경이다.
숨은 앱 드라이버는 창으로 세지 않으며 첫 실제 창이 등록되기 전에는 상태를 만들지
않는다. 앱이 계속 실행 중일 때 모든 창이 최소화되거나 닫히거나 앱이 숨겨지면
백그라운드가 된다. 종료가 승인된 뒤의 대기 이벤트는 폐기한다. 창이
다른 창에 완전히 가려졌는지와 키 포커스는 이 판정에 사용하지 않는다. 앱 숨김과
창 최소화는 [앱 숨김](https://developer.apple.com/documentation/appkit/nsapplication/didhidenotification)과
[창 최소화](https://developer.apple.com/documentation/appkit/nswindow/didminiaturizenotification)
AppKit 알림을 사용한다. Win32에서는 표시된 iiGUI 창 가운데 최소화되지 않은 창이
하나 이상이면 전경이다. 숨은 앱 드라이버와 포커스·다른 창에 의한 가림은 판정에서
제외한다. 창 표시·숨김과 위치 변경, 최소화·복구 메시지를 반영하며 첫 실제 창을
등록하기 전에는 상태를 만들지 않는다. UIKit에서는
실행 중인 열린 앱이 백그라운드로 옮겨질 때 운영체제가 중단하기 전에 전달을
시도한다. 강제 종료나 즉시 중단될 때 항상 콜백이 실행된다고 보장하지 않는다.
이 상태는 백그라운드 실행 권한을 뜻하지 않는다. 네이티브 수명 모델의 범위가 서로
다르므로 이 신호의 모든 운영체제에서 완전한 동등성도 보장하지 않는다.

X11은 각 실제 창의 `StructureNotify`에 포함된 `MapNotify`·`UnmapNotify`를
집계한다. 첫 실제 창의 매핑 상태 알림을 받기 전에는 상태를 만들지 않으며, 매핑된
창이 하나라도 있으면 전경이다. 숨은 앱 드라이버는 제외하고 마지막 창이
매핑 해제되거나 닫히면 백그라운드이다. 포커스와 단순 가림은 사용하지 않는다.
이 계약은 [ICCCM 창 상태 규칙](https://www.x.org/releases/X11R7.7/doc/xorg-docs/icccm/icccm.pdf)을
따르는 창 관리자와 그 알림에 의존하며, 창 관리자의 작업 공간 처리에 따라 다른
데스크톱 호스트와 전경의 의미가 다를 수 있다.

Wayland에서는 신뢰할 수 있는 동등한 신호를 아직 제공하지 않는다. 이 호스트에
비어 있지 않은 콜백을 설정하면 `unsupported`를
반환하며, `supports_application_foreground()`로 컴파일된 지원 여부를 확인한다.
보조 호스트는 앱 전역 수신자를 소유할 수 없다.

2026-11-04 macOS arm64에서 호스트 전체 빌드와 관련 헤드리스 검사 10건을
통과했다. UIKit 호스트와 생성 앱은 iPhoneOS 18 이상 arm64 객체로, X11·Wayland
호스트와 생성 앱은 Linux aarch64 객체로 컴파일했다. Web 호스트는 Emscripten
문법 검사만 수행했다. Android 호스트도 API 29를 대상으로 문법 검사했으나
NDK C 헤더와 별도의 libc++ 헤더를 함께 사용한 검사이므로 완전한 NDK 빌드·링크
증거는 아니다. 실제 창·장치·브라우저 실행은 수행하지 않았다.
같은 시점 `iiGUI.boundary` 재검사는 기존 HelloGUI SDK 구성에 LVUI가 포함되어
실패했으며, 해당 기존 사용자 설정은 보존했다.

2026-11-05 Cocoa 전경 집계를 추가한 뒤 macOS arm64의 정적 iiGUI 빌드와 관련
헤드리스 검사 20건, 별도 공유 iiGUI 빌드의 전경 검사 세 건을 통과했다. 앱 숨김·
복귀, 창 최소화·복구, 다중 창 집계, 첫 창 이전의 미확정 상태, 재진입·종료·예외를
검사했다. 실제 AppKit 델리게이트 메서드에 가상 알림을 보내 집계로 연결하는 검사와
집계 단위 검사는 각각 AddressSanitizer·UndefinedBehaviorSanitizer도 통과했다.
이는 창이나 앱을 실제로 열어 운영체제 전환을 확인한 결과가 아니다. 검사는 LVUI·
LVRS 및 Qt SDK 경로를 차단했으며 iiGUI 비교 문서 두 개는 비실행 자료로 허용했다.

같은 날 Win32 전경 집계를 연결한 뒤 macOS arm64의 정적 iiGUI 빌드와 관련
헤드리스 검사 21건, 별도 공유 라이브러리 빌드의 전경 검사 네 건을 통과했다.
Win32 메시지 모의 검사는 표시·숨김, 최소화·복구, 다중 창, 포커스와 가림,
재진입을 확인했고 AddressSanitizer·UndefinedBehaviorSanitizer도 통과했다.
Windows x86-64와 arm64 대상으로 Win32 호스트, LVUI 생성 앱과 관련 API·LVUI
검사 소스를 COFF 객체로 각각 교차 컴파일했다. 이 변경에 대한 Windows 전체
라이브러리·앱 링크와 실제 운영체제 메시지·앱 실행은 검증하지 않았다. 격리
검사에서 LVUI·LVRS·Qt SDK 경로를 차단했으며 iiGUI 비교 문서 두 개는 비실행
자료로 허용했다. 교차 컴파일 도구는 공식 배포본의 해시를 확인해 임시로 사용했다.

같은 날 X11 창 매핑 집계를 연결한 뒤 macOS arm64에서 iiGUI 전체 빌드와
관련 헤드리스 검사 25건을 통과했다. 가상의 X11 구조 알림을 이용한 검사는
여러 창, 알림 전의 미확정 상태, 중복·무관한 알림, 재진입과 창 퇴역을 다뤘으며
AddressSanitizer·UndefinedBehaviorSanitizer도 통과했다. Linux aarch64의 실제
X11 헤더와 Debian sysroot로 X11 호스트, 호스트 기능 조회와 관련 검사 소스를
ELF 객체로 교차 컴파일했다. 해당 변경의 Linux 전체 라이브러리·앱 링크와 실제
X11 창 관리자·앱 실행은 검증하지 않았다. iiGUI 검사에서는 LVUI·LVRS 경로와
알려진 Qt SDK 경로를 차단했으며 비교 문서는 비실행 자료로 허용했다.

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

Win32 창의 고정 주기 프레임 구동은 명시적인 앱 프레임 루프나 하나 이상의 창에
활성 위젯 모션이 있을 때만 유지한다. 보조 창의 모션도 각각의 프레임 루프를
진행하며, 마지막 요청이 끝나면 공통 cadence를 취소한다. 이 경로는 네이티브
디스플레이 동기화로 표시하지 않는다.
해당 호스트와 공통 프레임 수요 검사 소스는 Windows 10 이상 대상의 x86-64와
ARM64 객체로 교차 컴파일했다. Windows 라이브러리 링크나 실제 창 실행은 별도이다.

## 디스플레이 배율과 HiDPI

호스트는 창의 논리 크기와 현재 화면의 배율을 분리한다. `WindowSnapshot`의
`backing_scale`이 현재 배율이며, 입력과 위젯 배치는 논리 좌표를 유지한다.
배율이나 크기가 바뀌면 기존 프레임·글꼴·이미지 캐시를 필요한 범위에서 갱신하고
GPU 표면을 해당 물리 해상도에 맞춘다. 값이 바뀌지 않은 통지는 중복 갱신하지
않으며, 애니메이션이 없는 화면을 위한 상시 DPI 폴링은 만들지 않는다.

- macOS는 창의 backing 속성을 사용한다. 소유 창은 AppKit delegate로, 외부
  앱이 소유한 창은 객체별 backing 변경 알림으로 배율 변화를 받는다. 기존 외부
  delegate를 교체하지 않으며 창이 퇴역할 때 관찰을 해제한다. 배율 또는 색상
  속성 변경은 표면을 다시 표시하고, 논리 크기와 배율이 모두 같으면 크기 변경
  콜백을 반복하지 않는다. 앱 번들의 Info.plist와 사용자의 저해상도 설정이
  실제 backing scale에 영향을 준다. 번들 작성자는 필요할 때 Apple의
  `NSHighResolutionCapable`을 명시해야 하며 iiGUI가 외부 앱의 설정을 덮어쓰지는 않는다.
- Windows는 스레드를 per-monitor DPI aware v2로 설정하고 `WM_DPICHANGED`의
  권장 창 영역을 적용한 뒤 실제 크기와 배율을 다시 읽는다. 논리 크기가 같아
  `WM_SIZE`가 발생하지 않더라도 캔버스를 무효화하고 위젯 표시를 요청한다.
- Wayland는 입력 좌표를 표면 논리 좌표로 유지한다. fractional-scale-v1과
  viewporter가 모두 있고 compositor가 선호 배율을 통보하면 그 배율로 그린다.
  그렇지 않으면 현재 출력의 정수 배율을 사용한다. 전환 및 자원 수명은 아래
  [Wayland 계약](#linux-직접-wayland)을 따른다.
- Android는 구성과 창 크기 이벤트에서 density를 갱신한다. UIKit은 현재 창의
  화면 scale을 사용하고 표시 배율 trait이 바뀌면 크기가 같아도 캔버스와 위젯을
  갱신한다. 바뀌지 않은 레이아웃·trait은 추가 갱신을 일으키지 않는다.
  Web은 브라우저 DPR과 창 resize 이벤트를
  읽고, 모니터 이동 등에서 DPR만 바뀌면 media-query 변경 알림으로 갱신한다.
  CSS 컨테이너의 독립적인 크기 변경은 현재 별도 관찰하지 않는다.

물리 캔버스의 각 축은 최대 8192픽셀이다. 한도를 넘으면 배율을 임의로 낮추거나
성공으로 가장하지 않고 기존 검증 오류를 반환한다. 이 기능은 실제 모니터의 물리
크기나 ICC/HDR 특성을 추정하거나, 모든 GPU에 최소 비용을 보장하지 않는다.

## Wayland 다중 연결 대기 계약

다중 창에서 각 Wayland 연결은 소유 스레드에서만 프로토콜을 처리한다. 한 번의
이벤트 대기는 각 연결의 대기 중 이벤트를 전달하고 읽기를 준비한 뒤,
compositor 소켓·공유 깨움·클립보드 전송을 함께 기다린다. 키 반복과 전송 만료는
가장 이른 제한 시간을 적용한다. 읽기 가능해진 연결은 한 번 읽고 나머지는 준비된
읽기를 취소한다. 준비·전송·대기·읽기 중 어느 단계에서 실패하거나 시스템 호출이
중단되어도 이미 준비된 나머지 읽기는 반드시 취소한다.

콜백은 대기 중 호스트를 파괴하지 않고 입력·구성·닫기 상태를 기록한다. 실제
사용자 콜백과 창 정리는 프로토콜 대기가 반환된 뒤 소유 이벤트 루프에서 수행한다.
독립 앱 드라이버는 화면을 만들지 않고 Core 이벤트 루프와 공유 깨움만 소유한다.
각 보이는 창은 별도 Wayland 연결과 렌더링 표면을 소유하고 같은 Core 루프를
공유한다. 창의 프로토콜 이벤트는 연결별로 수집해 바깥 루프에서 전달한다.
개별 창 종료는 다른 창과 앱 루프를 중지하지 않으며 닫힘 통지는 부모 루프에
지연한다. 앱 종료는 남은 창을 먼저 정리한다. 알려진 활성 창이 있으면 앱 활성
상태로 집계한다. 클립보드는 살아 있는 연결 중 최근 포커스 이력을 우선하며, 이력이
없으면 남은 연결을 사용한다. 이 선택이 compositor의 입력 serial 승인까지 보장하지는
않는다.

Wayland 호스트는 이 계약을 연결해 독립 앱 드라이버와 보조 창 기능을 광고한다.
종료가 승인되면 겹친 네이티브·프로그램 닫기 요청을 합치고, 창이 닫힌 뒤 도착한
파일 선택 응답은 전달하지 않는다. 이는 소스 구현과 모의 호스트 검사이며 실제
compositor, Vulkan 표시나 Linux 장치에서의 실행을 확인했다는 뜻이 아니다.

2026-11-04 macOS arm64에서 iiGUI 전체 빌드와 관련 헤드리스 검사 12건을
통과했다. 실제 `HostWayland.cpp`를 모의 연결·표면과 연결하여 독립 창, 공유 루프,
활성 상태 집계, 종료 거부와 중복 종료, 생성 실패 복구, 앱 종료 중 새 창 차단,
파괴된 창의 파일 선택 응답 및 오류 전달을 검사했다. 연결별 읽기 준비의 취소와
클립보드 포커스 복구도 단위 검사했다. 모의 호스트·대기 계층·연결 선택 검사는
AddressSanitizer와 UndefinedBehaviorSanitizer로 확인했으나 연결된 SDK 라이브러리
전체를 계측하지는 않았다. 실제 `HostWayland.cpp`, `WaylandConnection.cpp`,
`HostCapabilities.cpp`는 기존 Debian sysroot의 헤더를 사용해 Linux aarch64 ELF
객체로 컴파일했다. 전체 Linux SDK 링크, compositor·Vulkan·실제 Linux 실행은
검증하지 않았다. iiGUI 전체 경계 검사는 기존 `examples/HelloGUI/iiSDKBuilder.json`이
LVUI를 포함해 실패했으며 기존 사용자 변경은 보존했다.

## Linux 직접 Wayland

폐기 예정인 X11 경로의 클립보드 선택 응답은 X 서버 소켓의 읽기 준비 또는 해당 응답의 2초 기한까지
대기한다. 무관한 X 이벤트를 받으면 대상 이벤트를 다시 확인하며, 연결 오류와 기한
초과는 오류로 반환한다. 이 API는 여전히 호출한 UI 스레드에서 동기적으로 완료된다.

```sh
cmake -S . -B build -G Ninja \
  -DIIGUI_LINUX_BACKEND=WAYLAND \
  -DCMAKE_PREFIX_PATH="/path/to/target/sdk-prefix"
cmake --build build
cmake --install build --prefix "$PWD/build/wayland-prefix"
```

`IIGUI_LINUX_BACKEND`의 기본값은 WAYLAND이다. 기존 AUTO 설정도 Wayland를 선택하며
wayland-client/wayland-cursor 1.20 이상과 xkbcommon 1.0 이상 개발 패키지가 없으면
구성에서 실패한다. X11은 폐기 예정이며 기존 소비자가 명시적으로 `X11`을 선택한 경우에만
구성 경고와 함께 사용한다. 빌드나 실행 중 실패를 XWayland 전환으로 숨기지 않는다.
한 설치 패키지는 한 창 시스템을 선택한다.
Wayland 빌드는 X11/Xi를 필수로 찾지 않고 X11 호스트·위젯·Vulkan 창 소스를 제외한다.
Pango/Cairo, Fontconfig, GIO는 텍스트·글꼴·포털 서비스에 사용한다. 이 라이브러리의
배포판별 간접 의존성과 창 호스트 선택은 구분한다.

- xdg-shell configure를 ack한 뒤 앱 attachment와 GPU 표시를 시작한다. 0 크기는
  기존 크기를 유지하고 음수·좌표 예산 초과는 GPU 할당에 반영하지 않는다.
- 출력 진입·이탈, 배율, 크기 변경을 논리 좌표와 물리 픽셀로 구분한다.
  fractional-scale-v1과 viewporter를 함께 지원하면 compositor가 알린 120분율을
  사용하고 surface buffer scale은 1로 유지한다. 프레임 픽셀 크기는 논리 크기에
  배율을 곱해 가장 가까운 정수로 정하고, viewport destination은 논리 크기를
  유지한다. 프로토콜 중 하나가 없거나 선호 배율이 오기 전에는 들어간 출력의
  최대 정수 배율을 사용한다. 출력 및 선호 배율의 중복 통지는 다시 그리지 않는다.
  입력 좌표는 논리 좌표를 유지하며, 커서는 배율 이상인 정수 해상도를 사용한다.
- prepare_read/read_events/cancel_read와 poll로 compositor 소켓, Core eventfd,
  데드라인을 함께 기다린다. 막힌 출력에는 POLLOUT을 사용한다.
- 정수 DPI에 맞는 시스템 커서와 seat 포인터·키보드·수정 키·키 반복을 C++ 입력 값으로 변환한다. XKB keymap 크기를
  제한하며 keymap/state를 교체할 때 이전 소유권을 해제한다.
- seat의 `wl_touch` 접점 하나는 논리 좌표의 touch 누름·이동·해제로 전달한다.
  compositor 취소나 터치 장치 제거는 눌린 위젯에 취소를 전달한다. 두 접점이 함께
  있으면 기존 누름을 취소하고 모든 접점이 해제될 때까지 새 누름을 만들지 않는다.
  다중 접점 제스처·펜·압력·접촉 면적은 이 계약에 포함되지 않는다.
- 필요한 동안에만 프레임 callback을 요청한다. compositor 프레임이 멈춰도 Core
  타이머는 처리하며 임의 16ms 타이머를 디스플레이 프레임으로 가장하지 않는다.
- 클립보드는 실제 wl_data_device offer/source/pipe를 사용한다. 쓰기에는 seat 입력
  serial이 필요하다. 읽기는 UTF-8·64 MiB와 진행 없는 시간 2초로 제한한다. 전송 pipe는 nonblocking이고
  수신자가 끊어도 SIGPIPE로 프로세스가 종료되지 않는다.
  전송 쓰기는 해당 pipe의 쓰기 가능 또는 오류 통지가 왔을 때만 시도하며, 대기는
  가장 이른 실제 전송 만료 시각으로 제한한다. 20ms 주기 재검사는 사용하지 않는다.
  다른 창이나 compositor 이벤트만 발생한 경우 전송을 다시 시도하지 않는다.
- 파일 선택은 XDG Desktop Portal을 사용하며 결과를 UiExecutor에 전달한다. 현재
  Wayland parent export가 없어 parent 문자열은 비어 있다. 파일 선택은 파일 생성이
  아니다. 실제 파일 접근·생성·제공은 iiFileProvider가 맡는다.
- 외부 URL은 OS의 기본 처리기로 전달한다.

Wayland와 X11은 기본 AT-SPI 접근성 전달을 컴파일한다. 지원 범위와 실제 Linux
실행 검증의 공백은 [접근성 의미 정보](AccessibilitySemantics.md)에 기록한다.
현재 Wayland에는 다중 접점 제스처·tablet, DnD와 xdg-activation token이 없다. 프로토콜 없는 글로벌 메뉴나 token 없는 raise는 성공으로 가장하지 않는다.

2026-11-07 macOS arm64에서 터치 상태와 대기·pipe 준비 처리의 헤드리스 검사 세 건을
통과했다. 이 검사는 논리 좌표, 접점 취소, 다중 접점 억제, 전송 준비 및 만료를
확인한다. macOS의 C++ 컴파일러로 [Wayland 개발 헤더](https://packages.debian.org/trixie/arm64/libwayland-dev),
[xkbcommon 개발 헤더](https://packages.debian.org/trixie/libxkbcommon-dev)와 Linux 입력
상수 헤더를 사용한 `WaylandConnection.cpp` 문법 검사도 통과했다. 이는 Linux 대상의
전체 컴파일·링크나 compositor·실제 터치 장치 실행을 검증한 결과가 아니다.

Wayland 조합 입력의 구현 계약은 다음과 같다. compositor가 보내는 문자열과
커서·삭제 위치는 UTF-8 바이트이며 iiGUI 편집기 위치는 UTF-16 단위이다.
경계가 코드 포인트를 나누거나 문자열이 유효하지 않으면 편집을 거부한다.
`WaylandImeProtocol`은 버전이 있는 편집기에 한해 한 묶음의 조합, 주변 삭제,
확정 문자열과 다음 조합을 변환한다. 기존 선택을 대체하는 조합은 확정 전에
투영으로 유지해 확정과 실행 취소가 한 편집 단위로 유지되며, 빈 done은 조합과
대체된 선택을 제거한다. identity·focus 세대가 바뀐 편집기에는 이전 묶음을
전달하지 않는다. 비밀번호의 주변 문자열은 반환하지 않으며, 그 외의 주변
문자열은 선택 영역과 UTF-8 경계를 유지하면서 4,000바이트 이하로 제한한다.
잘못된 조합 문자열과 바이트 경계는 묶음을 변경하기 전에 검사한다. 별도
단위 검사는 기본 위젯과 LVUI 입력 필드·코드 편집기에서 이 경계를 확인한다.

`zwp_text_input_v3`가 제공되면 seat에 텍스트 입력 객체를 만들고 preedit·확정·
삭제를 done까지 모은다. 호스트는 일반 입력과 IME 묶음을 같은 유한 큐에 넣어
순서를 유지한다. 창 focus 세대와 편집기 focus 세대, commit 일련번호를 함께
확인한다. 다른 편집기의 오래된 묶음은 버리고 같은 편집기의 오래된 일련번호는
적용하되 새 상태 전송은 일치하는 done까지 보류한다. 비밀번호 전환은 즉시 입력
상태를 재설정하고 이전 묶음을 폐기한다. 주변 문자열 제공 가능 여부가 바뀌면
조합 중인 문자열을 보존한 뒤 재활성화하고 이전 묶음을 폐기한다. 프로토콜을 제공하지 않는 compositor에서는
기존 XKB 입력을 유지한다. 위젯 교체·종료 시 이전 표면은 새 표면의 입력 상태를
해제하지 않으며 마운트 실패로 이전 표면이 복구되면 입력 소유권도 복구한다.

macOS의 헤드리스 검사에서 실제 Wayland 위젯 표면과 모의 연결을 사용해 LVUI
입력 필드·코드 편집기, 조합과 확정, 실행 취소, 이벤트 순서, 포커스 변경,
복구와 비밀번호 전환을 확인했다. 호스트·연결·표면 및 생성된 프로토콜 C 코드는
Linux aarch64 객체로 교차 컴파일했다. Linux 전체 링크나 실제 compositor·IME
실행은 검증하지 않았으며 물리 키보드와 IME의 중복 전달 여부도 실제 환경에서
확인해야 한다.

조합 표시 계약은 숨김 커서를 입력 이벤트로 전달하고, 선택의 두 끝
순서를 UTF-16 편집기 선택 방향으로 보존한다. 숨김 상태에서도 조합 밑줄과
텍스트 기하 조회는 유지하고 조합이 끝나거나 다음 표시 요청이 오면 커서 표시를
복원한다. 기본 위젯과 LVUI 입력 필드·코드 편집기에서 같은 계약을 검사한다.
`WidgetInput`의 공개 구조가 확장되므로 이를 사용하는 앱과 라이브러리는 같은
iiGUI 빌드의 헤더와 바이너리를 사용해야 한다. Linux aarch64 교차 객체와 모의
Wayland 연결의 헤드리스 회귀, AddressSanitizer·UndefinedBehaviorSanitizer
검사를 통과했으나 실제 입력기 동작을 검증한 것은 아니다.

xdg-shell, text-input-v3, fractional-scale-v1과 viewporter는
wayland-protocols 1.44 XML에서 wayland-scanner 1.23.1로 생성했다. XML·private 생성 코드·SHA-256·라이선스 고지를
src/third_party/wayland에 유지한다. 교차 빌드와 설치 소비자는 타겟 scanner를
실행하지 않는다.

Android·Wayland·Web의 단일 OS 표면은 약한 GPU registry를 사용한다. 위젯 세대가
교체 중 잠시 겹쳐도 같은 창에 두 swapchain/context를 만들지 않는다. 실패한 마운트는
이전 세대를 다시 표시하고, 마지막 소유자가 사라지면 GPU 자원을 해제한다.

Android·Wayland·Web의 raw `set_canvas`는 현재 창 전체 viewport만 받는다. 부분
viewport를 억지로 늘려 그리지 않으며 `unsupported`를 반환한다. 부분 화면 구성은
공통 C++ 위젯·레이어·클립을 사용한다.

## Windows 사용자 정의 위젯 접근성

Win32 호스트는 `WM_GETOBJECT`에서 iiGUI 위젯을 MSAA 객체로 제공하며 포커스와
값 알림에 세대별 객체 ID를 사용한다. 역할·텍스트의 노출, 소유자 스레드 요청,
비밀번호와 퇴역 처리 및 미구현 UI Automation 패턴은
[접근성 계약](AccessibilitySemantics.md)에 기록한다. 구현은 모의 Win32·COM
검사와 Windows 교차 빌드로 확인했으며 실제 Windows 보조 기술은 실행하지 않았다.

## Android 호스트와 OS 연결

### 모바일 접촉 취소와 두 손가락 입력 명세

두 손가락 조작이 기존 한 손가락 누르기를 대체하거나 접촉만 취소된 경우에는
`pointer_cancel`을 해당 캡처에 전달한다. 이는 앱·창의 포커스 손실과 구별한다.
누른 컨트롤은 취소되지만 포커스, 입력기의 조합 및 열린 팝업은 보존한다.
앱 비활성화·창 소멸에 따른 `focus_lost`는 종전처럼 포커스를 해제한다.

Android의 같은 장치에서 시작한 두 손가락은 포인터 ID로 추적한다. 실제 이동 전에는
기존 단일 접촉을 유지하며, 일정한 이동이 감지되면 기존 누르기를 한 번 취소한다.
두 접촉 중심의 이동은 논리 좌표의 정밀 스크롤로 전달한다. 간격 변화는 기존 UIKit과
같은 `control` 정밀 스크롤로 전달하고 중심을 입력 위치로 제공한다. 확대 비율은
`pinch_scale_factor(input)`으로 확인하며 핀치가 아닌 입력과 유효하지 않은 비율은
값이 없다. 핀치를 수락하는 리스너가 없으면 그 변화는 일반 스크롤이
되지 않는다. 한 손가락이 들린 후에는 이전 접촉을 클릭이나 새 단일 터치로 승격하지
않는다. 펜·다른 장치·취소·표면 퇴역은 독립적인 기존 소유권 규칙을 보존한다.

이 계약의 모형·위젯 검사는 운영체제 입력이나 실제 멀티터치 기기 검증이 아니다.

진입점은 `run_android_application`이다. Core Application을 native_app_glue 스레드에서
생성·실행·정리하고 Activity 수명과 창 재생성은 ALooper 명령으로 처리한다. 앱 동작과
위젯은 C++23이다. 시스템 Activity/ClipboardManager/Intent와 텍스트 서비스에 C++ JNI를
사용하며 Java/Kotlin 위젯이나 LVUI 인터프리터를 넣지 않는다.

### 선언형 앱의 Android 진입 계약

Android용 iiGUI 설치본은 `iiGUI::AndroidEntry` 정적 타깃을 제공한다. 이 타깃은
NDK `native_app_glue`와 `android_main`을 보유하며, 소비자의
`ii::runtime::configure_application`을 `run_android_application`에 전달한다.
NativeActivity가 찾는 진입 심볼은 공유 라이브러리 링크에서 유지한다. 앱 구성과
실행이 끝나면 호스트가 Core 수명을 정리하고 Activity 종료를 처리한다. 진입
경계의 실패와 예외는 Android 로그로 전달하며 C ABI 밖으로 예외를 내보내지 않는다.
진입 타깃이 직접 사용하는 Android·로그 시스템 라이브러리도 전이 링크하므로
정적 및 공유 iiGUI 구성에서 소비 앱이 이를 별도로 지정하지 않는다.

이 계약은 네이티브 공유 라이브러리 생성에 관한 것이다. Android 매니페스트,
패키지 식별자, 리소스, APK 구성·서명 및 기기 실행은 별도로 관리한다.
설치본의 `share/licenses/iiGUI/`에는 native_app_glue와 NDK의 원본 고지를
각각 보존하며, 앱 배포 시 관련 고지를 포함하는 책임은 앱에 있다.
2026-11-05 Android NDK r30의 arm64 API 24에서 정적·공유 iiGUI와 진입 타깃을
각각 빌드하고, 설치 소비자의 LVUI 선언에서 생성된 NativeActivity 공유
라이브러리에 연결했다.
진입 심볼과 전이 공유 라이브러리를 검사했으며 실제 Activity를 실행하지 않았다.
호스트 모형 검사에서는 구성 전달과 반환 오류·예외의 로그 및 조기 종료를 확인했다.

### Android 프레임 시계 호환 명세

Android API 24–28 대상은 `AChoreographer_postFrameCallback`, API 29 이상 대상은
`AChoreographer_postFrameCallback64`를 사용한다. 32비트 레거시 콜백의 잘린
나노초 값은 같은 `CLOCK_MONOTONIC` 시계의 현재 값과 결합해 현재보다 늦지 않은
가장 가까운 프레임 시각을 복원한다. 따라서 콜백 전달이 2³² ns 이상 지연되면
원래 상위 비트를 확정할 수 없다. 시계 읽기나 유효한 복원이 실패하면 호스트 오류로
전달하고 잘못된 시각으로 프레임 루프를 진행하지 않는다. 64비트 레거시 값과
최신 콜백의 64비트 값은 그대로 사용한다. [Android NDK의 관련 제한](https://developer.android.com/ndk/reference/bug)을 따른다.

read_clipboard/write_clipboard/open_external_url을 실제 Android 서비스로 연결했다.
내부 RAII 객체가 JNI attach/detach, local reference frame, 예외 정리와 Core의
UTF-8↔UTF-16 변환을 맡는다. modified UTF-8을 공개 문자열로 오인하지 않는다.
클립보드 텍스트·URL을 지원하고 이미지·파일 payload는 명시적으로 거부한다. OS의
포커스·권한 정책이나 URL 처리기 부재는 실패가 될 수 있다.

Android API 수준에 맞는 AChoreographer 콜백이 프레임을 공급한다. 앱 FrameLoop나
위젯 모션이 있을 때만 다음 callback을 요청한다. pause/창 해제에서는 GPU와 다음
프레임을 멈추고 resume에서 Core 시계를 재설정한다. 이미 요청된 callback은 소유권
gate를 통해 종료된 Host를 참조하지 않는다. 미리 할당한 callback 슬롯을 재사용하고
Looper 스레드가 끝나면 OS가 전달하지 않은 슬롯도 정리하므로 매 프레임 ticket을
새로 할당하지 않는다.

iiSDKBuilder의 Android 기본 대상은 API 24이며 프레임 콜백은 그 대상에서 API 29
심볼을 요구하지 않는다. 이것만으로 전체 Android SDK와 앱의 API 24 빌드·링크나
실기기 동작이 검증된 것은 아니다. HelloAndroid 예제의 기존 대상은 API 29이다.

2026-11-04 macOS arm64에서 API 24·29 콜백 선택과 32비트 시간 순환·오류
경계를 검사했고 두 검사에 ASan·UBSan을 적용했다. NDK 26.1의 Android C 헤더와
별도의 LLVM libc++ 헤더를 조합한 arm64 API 24·29 호스트 객체는 각각 해당 API의
콜백 심볼만 참조했다. armv7 API 24는 LLVM 23에서 NDK의 `hardware_buffer.h`에
있는 32비트 상수식 오류로 원본 헤더 컴파일이 실패했다. 해당 한 줄을 임시 복사본에서만
수정하고 별도 libc++ 헤더를 사용한 진단 객체에서는 레거시 콜백과 `clock_gettime`
심볼을 확인했다. 이 객체들은 정식 NDK 단독 빌드·링크나 기기 실행의 증거가 아니다.

Android 일반 `TextRun`은 선택한 글꼴과 시스템 대체 글꼴을 사용하여 `StaticLayout`으로
조합·줄 바꿈하고 비트맵을 Vulkan에 전달한다. 말줄임·밑줄·물리적 정렬·고정 줄 높이와
클립의 계약 및 API 수준별 제한은 [Android 텍스트](AndroidHello.md#android-네이티브-텍스트-계약)에 기록한다.
편집기용 `TextLine`은 Android 시스템 문단 배치에 연결되었으며 세부 한계는
[네이티브 단락 배치](TextLine.md#android-구현-계약)에 기록한다. 하드웨어 키보드는
[Android 키보드 계약](AndroidKeyboard.md)에 따라 위젯으로 전달한다. IME, 문서 선택,
접근성, 보조 창은 아직 별도 구현이 필요하며 `text_input` 완전 지원으로 광고하지 않는다.
호스트 모형과 NDK 링크는 실제 Android 문자 표시나 기기 실행의 증거가 아니다. 재현 도구는
tools/android_hello.py이다.

### Android 포인터 입력 계약

Android 입력 변환은 도구 종류를 손가락·마우스·펜·지우개로 구분하고, 활성
포인터의 기기 ID와 포인터 ID를 함께 보존한다. iiGUI 위젯 트리는 한 포인터를
캡처하며 두 손가락 조작이 인식되면 그 캡처를 취소하고 위의 제스처 계약으로 전환한다.
두 번째 손가락이 기존 포인터의 이동·종료를 대신하지 않는다. 손가락이
먼저 닿은 상태에서 펜이나 지우개가 들어오면 손가락의 캡처를 취소하고 펜을 시작한다. 포인터가
사라지거나 취소·창 소멸·비활성화되면 기존 캡처를 취소한다. 마우스와 펜의 hover,
마우스 휠과 보조 버튼은 일반 터치 클릭과 구분한다.

펜의 Android 기울기·방향은 화면 축의 tilt X/Y로 변환하고 정규화된 압력과 버튼을
전달한다. 이동 이벤트에 포함된 과거 샘플은 시간 순서로 먼저 전달한다. 이 계약은
임의 개수의 접촉을 직접 전달하거나 소프트 키보드·IME의 완전한 지원을 뜻하지 않는다. Android 12 이하는
여러 포인터 중 취소된 접촉을 명확히 구분할 수 없으며, Android 13 이상의 취소
플래그는 보조 포인터의 종료를 클릭으로 처리하지 않도록 사용한다.

2026-11-05 호스트의 NDK 모형과 실제 위젯 트리 검사에서 포인터 인덱스 변경,
펜 우선 전환, 취소·재진입, 기울기·압력, hover, 휠과 과거 샘플을 확인했다.
Android SDK의 취소 플래그 값과 NDK r30 arm64 API 24 빌드·LVUI 앱 링크를
검사했다. 그 기록은 이번 두 손가락 제스처 변경의 기기 실행 증거가 아니다.

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
WebGL2 소스는 재질·그림자·콘텐츠 변환과 배경·콘텐츠 장면 입력도 처리한다.
레이어별 clip과 장면 crop을 교차하고, framebuffer에 직접 연결된 텍스처를 동시에
샘플링하지 않도록 입력을 해제한다. 확장별 렌더 가능 형식은
[Khronos 전체 부동소수점 확장](https://registry.khronos.org/webgl/extensions/EXT_color_buffer_float/) 또는
[16비트 부동소수점 확장](https://registry.khronos.org/webgl/extensions/EXT_color_buffer_half_float/)을 따른다.
호스트 모의 그래픽 테스트와 오프라인 셰이더 검사는 실제 브라우저 출력 검증이 아니다.

클립보드와 외부 URL은 HTTPS/localhost 및 사용자 활성화·권한 정책을 따른다.
파일 handle을 가짜 로컬 경로로 돌려주지 않는다. 문서 선택/FileProvider handle,
IME/텍스트 편집, 접근성, 보조 canvas/window는 현재 계약 밖이다.

필요 조건은 Emscripten pthread, SharedArrayBuffer, OffscreenCanvas/WebGL2,
worker requestAnimationFrame, Atomics.waitAsync, Worker FontFace/FontFaceSet이다.
서버에 COOP same-origin, COEP require-corp가 필요하다. tools/serve_web.py는
127.0.0.1에 이 헤더들을 제공하며 공개 서버나 자동 시작 작업을 등록하지 않는다.

## HiDPI 변경 검증

2026-11-08 macOS arm64에서 iiGUI와 관련 테스트를 빌드했다. 외부 Cocoa 창의
배율 알림 관찰·해제와 delegate 전달, Wayland 정수/소수 배율 정책·픽셀 반올림,
모의 Wayland 호스트의 상태 변경과 중복 이벤트 억제, Web DPR 변경 관찰과
해제와 UIKit 배율 전환 정책을 포함한 헤드리스 검사 여섯 건이 통과했다. 추가로
캔버스 손상·장면 구성·모의 WebGL 자원 재사용 검사 세 건이 통과했다. Web 검사는 실제 브라우저가
아닌 모의 객체로 JavaScript 연결을 확인한다.

Wayland 연결 소스·Vulkan 렌더러와 두 프로토콜 C 파일은 Linux aarch64 객체로
교차 컴파일했고 UIKit 호스트·위젯은 iOS arm64 객체로 컴파일했다. Web 호스트는 현재 Emscripten의 실험적 C++ 표준 라이브러리
옵션을 사용해 객체로 컴파일했다. 현재 Core 헤더의 `std::stop_token`은 해당
Emscripten 기본 표준 라이브러리 설정에서 사용할 수 없어 이 옵션 없는 Web 전체
빌드와 링크를 통과했다고 주장하지 않는다. Windows 수정 경로는 이번에 대상
컴파일을 하지 않았다. 실제 화면 이동, compositor·GPU·브라우저·iOS·Android
기기 출력이나 배율별 시각 품질은 검증하지 않았다.

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
문자 클립보드 왕복을 검사·복원한다. 다국어 레이블을 포함한 후보를 만든 뒤 의도적인
구성 오류로 마운트를 취소하고 기존 GPU capture가 동일한지도 검사한다. 정상 Activity 종료는 HELLO_ANDROID_EXIT로
관찰할 수 있으며, 기본 샘플은 OS fixture 없이 빌드된다.

Windows 회귀 빌드에서는 DirectWrite의 파일 객체용 CreateFontFaceReference가
MSVC 헤더의 overload와 MinGW IDL 헤더의 underscore 이름으로 다르게 선언되는
경계를 검사한다. C++23 requires로 해당 시그니처를 선택해 동일 COM 슬롯을 호출하며,
이미 분석한 파일 객체를 경로로 다시 열지 않는다. 모의 양쪽 헤더 계약 검사와 실제
Windows 교차 빌드를 분리해 검증한다.
## Cocoa 파일 열기 요청 접수

Cocoa 앱 델리게이트는 운영체제의 파일 열기 요청을 iiGUI 주 호스트의
`HostCallbacks::files_opened`에 전달한다. 요청은 호스트 실행 중이고 콜백이 있을
때만 접수한다. 경로는 NUL이 없는 비어 있지 않은 UTF-8이며 경로당 최대
131072바이트이다. 대기 중인 경로는 최대 256개와 총 4 MiB이며 묶음 전체가 유효하고
여유가 있을 때만 원자적으로 추가한다. 유효성·용량·수명 조건을 충족하지 못하면
기존 대기 요청은 보존하고 운영체제에 실패를 응답한다. 성공 응답은 큐 접수를 뜻하며
앱이 문서를 실제로 열었다는 보장은 아니다. 소유 스레드에서 대기 경로를 순서대로
하나의 묶음으로 전달하고, 호스트 종료 때 전달되지 않은 경로는 폐기한다.
콜백이 실행되는 동안 새 요청이 들어오면 콜백이 끝난 뒤 다시 깨우므로 중첩된
네이티브 이벤트 루프가 먼저 깨움 신호를 소비해도 대기 요청이 남지 않는다.
현재 콜백은 실행이 끝날 때까지 별도로 보유하므로 그 안에서 호스트를 닫아도
실행 중인 콜백 객체가 먼저 파괴되지 않는다.
경계와 대기열은 창을 띄우지 않는 검사로 검증하며 실제 AppKit 파일 연결은 별도로
검증해야 한다.

2026-11-04 macOS arm64에서 iiGUI 전체 빌드와 관련 헤드리스 검사 8건이 통과했고,
대기열은 AddressSanitizer와 UndefinedBehaviorSanitizer도 통과했다. 별도
`iiGUI.boundary` 검사는 기존 `examples/HelloGUI/iiSDKBuilder.json`의 LVUI SDK
항목 때문에 실패했다. 이 예제 구성은 보존했으며 실패를 호스트 런타임의 링크 결과로
간주하지 않는다. 실제 창이나 운영체제의 파일 열기는 실행하지 않았다.
