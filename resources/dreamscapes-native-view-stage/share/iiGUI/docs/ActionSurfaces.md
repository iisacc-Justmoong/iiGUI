# 자식 콘텐츠를 가진 네이티브 동작 표면

`WidgetBuilder::action_surface`는 자체 C++ 레이아웃·입력·그리기를 사용하는 투명한
동작 컨테이너이다. 일반 버튼 별칭이나 OS 컨트롤을 만들지 않는다. 자식 슬롯이
비어 있으면 텍스트를 그리고, 일반 자식이 있으면 해당 뷰들을 그린다. 텍스트의
자연 크기는 사용자 정의 콘텐츠가 있어도 implicit 크기 계산에 참여한다.

```cpp
auto action = widgets.action_surface(parent, "action", {
    .text = "Open report", .role = ii::gui::ActionRole::link,
    .padding = {14, 10, 14, 10}, .minimum_height = 36
}, [] { /* native callback */ });
if (!action) return std::unexpected(action.error());
auto content = widgets.hstack(*action, "content");
```

ActionOptions는 텍스트·접근성 이름·활성 여부·버튼/링크 역할·타이포그래피·밑줄·
패딩·최소/명시 크기·모서리·포커스 링과 네 가지 텍스트 색상을 받는다. 배경은
그리지 않는다. 자식들은 같은 위젯 세대에 속하며 각자의 native 입력·포커스·
팝업과 바인딩을 유지한다. 여러 자식의 기본 배치는 같은 콘텐츠 원점이며, 행·열
배치에는 HStack/VStack을 사용한다. 자식의 명시 프레임도 콘텐츠 원점에 더한다.

자식 컨트롤이 포인터 입력을 받으면 부모를 함께 활성화하지 않는다. 장식용 뷰와
빈 패딩 영역은 부모의 동작 영역이다. Enter/Space는 눌림 피드백 후 키를 놓을 때
동작한다. 표면 비활성화는 자식의 눌림·포커스·사용자 캡처도 취소한다. 상위 클립과
모달 차단을 따르며 자체 클립은 set_clip_content로 지정할 수 있다.

`ActionOptions::enter_key_activation`은 Enter 키의 활성화 여부를 선택하며 기본값은
기존 동작과 같은 `true`이다. `false`여도 Space, 포인터, 접근성과 명시적 활성화는
계속 사용할 수 있다. 키보드 누름은 시작한 키의 해제에서만 활성화하며 중복 누름은
반복 호출하지 않는다. Enter를 누르는 도중 이 옵션을 끄면 눌림을 취소하고 이후
키 해제는 클릭하지 않는다. 포커스 상실, 비활성화, 위젯 폐기에도 오래된 입력을
전달하지 않는다.
이 설정은 공개 `ActionOptions` 구조체의 일부이므로 이전 바이너리와 혼합하지 않고
헤더·라이브러리·패키지의 `iiGUI_NATIVE_ABI_ID`를 함께 배포해야 한다. 해당 구조체를
사용하는 기존 소비자는 다시 컴파일한다.

`pointer_focusable=false`인 동작 표면은 포인터 클릭 시 기존 입력 포커스를 보존하면서
눌림·놓임과 활성화를 전달해야 한다. 누른 뒤 포커스가 다른 대상으로 바뀌거나
포인터가 표면 밖에서 놓이면 오래된 클릭을 활성화하지 않는다. 키보드 포커스 가능
여부는 `tab_focusable`이 별도로 결정한다. 이 계약은 검색창 지우기처럼 입력기 조합을
유지해야 하는 동작에도 적용된다.

action_options()와 set_action_options()는 실제 현재 옵션을 조회·교체한다. 기존
set_text/set_enabled/접근성 이름 변경도 이 옵션에 반영되므로, 색상 변경이 최신
바인딩 값을 오래된 옵션으로 덮어쓰지 않게 구성할 수 있다. 모든 접근은 소유
스레드에서 수행하며 종료한 세대의 핸들은 closed이다.

interaction_enabled는 요청한 enabled와 분리된 정책 gate이다. disabled tone처럼
상위 정책이 잠시 입력을 막더라도 요청값은 유지하며 gate 해제 때 복원할 수 있다.
구성 중 치수를 조회한 뒤 자식을 추가해도 부모의 측정 캐시를 무효화하여 새 슬롯
내용이 크기 계산에서 누락되지 않도록 한다.

스냅샷은 ActionSurface 종류, action_role, custom_content, text_underline와 현재
foreground_color를 노출한다. macOS 가상 접근성 피어는 링크 역할과 press를 제공한다.
다른 플랫폼의 전체 접근성 동등성과 상속된 모션·상태 API는 별도 이식 범위이다.

TextRun의 underline은 AppKit attributed text·DirectWrite·Pango의 글꼴 셰이핑
경로에서 처리한다. GPU에는 그 결과를 전달하며 링크 밑줄을 임의의 고정 선으로
대체하지 않는다. 텍스트 동등성/캐시 비교에도 밑줄을 포함한다. Windows/Linux 소스
구현과 현재 macOS 빌드 증거를 실제 플랫폼 실행 검증과 구분한다.

tests/action_surface.cpp에는 크기·텍스트 fallback·자식 합성/입력·키보드·상태 색상·
비활성 하위 트리·밑줄·실패 원자성·소유자 종료를 등록한다. 사용자 지시에 따라 현재는
컴파일·링크만 수행하며 앱·테스트·self-test 실행은 전체 작업의 최종 일괄 단계로 미룬다.

## 사용자 정의 입력과 기본 동작

동작 표면은 자체 자식을 가진 컨테이너이므로 `set_input_handler`를 받을 수 있다.
처리한 입력은 기본 동작으로 다시 전달하지 않으며 false를 반환한 입력은 Enter/Space 등
기존 동작으로 이어진다. 일반 버튼·텍스트 입력 등 다른 컨트롤의 허용 범위를 넓히지는 않는다.
포인터·스크롤과 같은 위치 기반 입력에서 `WidgetInput::local_position`은 해당 핸들러가
설치된 표면을 기준으로 한 좌표이다. 중첩 콘텐츠 호스트와 팝업 변환을 반영하며,
기존 `x`·`y` 값은 이전 좌표 계약을 유지한다. 전달된 위치는 유한해야 하며,
키보드 입력은 이 위치를 요구하지 않는다.
HierarchyItem의 C++ 포인터 드래그도 이 경로를 사용한다. 실제 창의 keyboard 회귀는
사용자 입력 소비·기본 활성화·중복 없는 호출과 처리기 제거를 함께 확인한다.
