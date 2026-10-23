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
