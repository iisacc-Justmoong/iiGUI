# iPadOS 네이티브 호스트

`CMAKE_SYSTEM_NAME=iOS`는 AppKit 대신 UIKit 창·장면 수명과 자체 C++ 위젯 표면을 선택한다. 최종 진입점과 작업·프레임 루프는 iiSDKCore가 소유하며, 주 창의 드라이버가 `UIApplicationMain`에 진입한다. Qt, QML, WebKit은 사용하지 않는다.

## 플랫폼 경계

- `HostIOS.mm`: UIWindowScene, 주 UIWindow, 같은 장면의 비모달 보조 UIWindow, 안전 영역·회전, CADisplayLink, Core 작업 깨우기, 문서 선택기와 보안 범위 URL을 연결한다. 배경 전환 시 표시 링크와 GPU 제출을 중단하고 복귀 시 프레임 시계를 초기화한다.
- `NativeWidgetsIOS.mm`: 하나의 UIView 위에 iiGUI 위젯의 그리기 목록을 표시한다. UIKeyInput의 확정 문자와 삭제, 하드웨어 키보드, 접근성 요소, 손가락·Pencil 샘플, 두 손가락 이동과 핀치를 C++ 입력으로 전달한다.
- `CanvasMetal.mm`, `TextMac.mm`, `TextLineMac.mm`: Apple 공통 GPU·CoreText 구현을 UIKit/AppKit 경계에서 나눈다. iPadOS는 좌상단 좌표와 UIFont를 사용한다.
- `VulkanWindowIOS.mm`: Vulkan Metal surface를 UIView의 CAMetalLayer에 연결한다. `Renderer::automatic`은 Metal을 우선하고, `Renderer::vulkan`은 별도 Vulkan 경로를 선택한다. iPadOS에서 Vulkan은 MoltenVK를 통해 Metal 장치를 사용한다.
- `DesktopIOS.mm`: UIKit 클립보드와 외부 URL을 제공한다.

## 지원과 검증의 구분

iPadOS 장면의 주 창 크기는 운영체제가 관리하므로 `set_content_size`와 데스크톱 최소화·최대화 요청은 `unsupported`이다. 보조 창은 요청한 크기를 사용 가능한 장면에 맞춰 제한한다. 저장 위치 선택은 문서 선택기로 폴더를 고른 후 요청 파일명을 붙이는 방식이다. 보안 범위 접근은 호스트가 닫힐 때 해제한다.

UIKeyInput 경로는 확정 문자열 입력을 제공한다. 전체 UITextInput의 marked-text 편집기 계약과 외부 드래그 앤드 드롭은 이 구현의 검증 범위에 포함하지 않는다. Pencil 압력·기울기 변환의 자동 테스트 통과와 실제 Pencil 하드웨어 필기 검증을 구분해야 한다.

UIKit은 앱 종료 대신 장면 수명을 관리한다. Core 종료 요청은 창과 작업을 정리하며, 진단 실행의 결과는 파일·콘솔에서 확인한다. 기기 진단 후 일반 실행으로 돌아갈 때는 개발 도구가 해당 앱 프로세스를 종료하고 다시 실행한다.

`opened` 알림과 Core의 초기 UI 작업은 UIKit 컨트롤러의 `viewDidAppear`가 끝난 뒤 시작한다. 보조 창을 생성하자마자 닫는 진단에서도 UIKit의 appearance 전환을 중첩하지 않기 위한 경계이다. 기기 검사에서는 생성·취소·재생성·종료를 실제로 반복하고 appearance 불균형 경고의 유무를 함께 확인한다.

## 빌드와 검사

arm64, iPhoneOS SDK, iOS 18.0 이상과 동일한 대상의 iiSDKCore·LVUI·Vulkan 패키지를 사용한다. macOS 라이브러리를 iOS 링크에 섞지 않는다. 교차 빌드에서 LVUI AOT 생성기를 실행할 때는 macOS용 `LVUI_HOST_COMPILER`를 명시한다. `tests/mobile_input.cpp`는 좌표·접촉 식별자, 압력 정규화, 기울기, 해제와 비유한 값 처리를 검사한다. CMake의 `iiGUI.mobile_input`으로 등록되며, 실제 창·GPU·문서 실행은 소비 앱의 연결 기기 검사로 별도 검증한다.
