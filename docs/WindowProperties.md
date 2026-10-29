# 루트 창의 유지된 설정

`Host::set_window_geometry()`는 제목·내용 크기·최소 크기의 명시한 필드를 갱신한다.
`WindowGeometry`의 없는 필드는 실제 현재 값을 유지한다. 논리 크기는 유한한
(0, 32767] 값이며 제목은 NUL 없는 UTF-8, 최대 64 KiB이다. 최소 크기를 올리면 필요한
내용 크기도 늘리고, 명시한 크기가 최소보다 작으면 변경 전에 거부한다.

`WindowProperties`는 geometry와 선택적 `WidgetAppearance`의 값 묶음이다.
`WindowPropertiesTarget`은 루트 위젯 상태의 약한 접점이며 `WidgetBuilder::window_properties()`로
얻는다. 다른 스레드·종료된 상태는 거부한다. 명시하지 않은 필드를 되돌리거나 파일 상태를
발행하는 API가 아니다. `WindowPropertiesTarget::snapshot()`은 마지막 정상 선언의
기록이며 실제 창의 현재 크기를 뜻하지 않는다. 실제 크기는 `Host::snapshot()`으로 읽는다.
이미 승인한 폭 선언이 바뀌지 않은 경우 새 최소 폭의 적용을 그 과거 폭으로 제약하지 않는다.

후보 루트의 `WidgetState::construct()`는 기존 Host와 창 없는 검사에서 같은 빌더 경로를
사용한다. 후보의 설정은 준비 단계에만 머물고, 자식 구성과 표면 연결 뒤에
`commit_window_properties()`가 승인한다. 구성 실패는 후보를 닫으며 창에는 적용하지
않는다. 승인 전 테마 변경은 후보의 자식 구성에만 반영한다.

승인 후에는 마지막 정상 선언과 새 선언의 차이만 Host에 보낸다. 제목 변경으로 사용자가
조절한 크기를 복원하지 않는다. 모든 입력 검증과 네이티브 geometry 적용이 성공한 뒤
기본 테마와 정상 선언을 갱신한다. 자식 위젯·입력·포커스·이벤트를 재생성하지 않는다.
기본 테마는 내부 콘텐츠와 팝업에도 반영하며 폰트·기본 버튼 크기는 측정을 무효화한다.
명시적인 위젯별 색상·스타일은 해당 위젯의 입력이 소유한다.

내부 콘텐츠·팝업은 표면과 기본 테마를 차용하지만 루트 창의 설정 권한은 차용하지 않는다.
그 빌더의 `window_properties()`는 `unsupported`를 반환한다. Host의 최소 크기는
macOS·Windows·X11의 기존 네이티브 경계에 적용한다. X11 크기 통지는 ConfigureNotify를
유지한다. iPadOS 기본 scene의 동적 크기·최소 크기는 OS 소유이므로 전체 요청을
`unsupported`로 거부한다. 보조 창은 기존 UIKit 크기 경로를 사용한다.

`iiGUI.native_window.*`는 OS 창 없이 실제 위젯 상태·의존성 생성 코드의 갱신과 수명을
검사한다. `iiGUI.layout_package`는 재배치한 설치 패키지의 Window AOT 팩터리와 공개
API를 컴파일·링크하고 아직 열리지 않은 Host의 호출 거부를 실행한다. 실제 네이티브 창의
화면 출력·이벤트 및 플랫폼 실행 여부는 별도 증거가 필요하다.
