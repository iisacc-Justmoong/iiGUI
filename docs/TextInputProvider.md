# 자체 텍스트 위젯과 OS 입력기

`Widget::set_text_input_provider`는 자체 편집 위젯을 OS 텍스트 서비스에 연결한다. `TextInputProvider`는 UTF-16 길이·선택·marked range·뷰포트로 제한한 로컬 caret 사각형, 요청 범위 읽기, 좌표 hit test를 제공한다. `WidgetSnapshot::text_input`, `text_length`, `multiline`, `caret_bounds`에는 메타데이터만 기록한다. 대용량 문서 전체 문자열을 스냅샷에 복제하지 않는다.

read 요청은 호출마다 65,536 UTF-16 단위로 제한한다. 반환된 `TextContent::range`가 실제 범위이며 텍스트의 UTF-16 길이와 일치해야 한다. EOF를 넘는 길이는 줄이고 surrogate를 가르는 범위는 공급자가 거절할 수 있다. OS는 실제 반환 범위를 확인해야 한다. 재귀 위젯 접근은 `busy`, 다른 스레드 접근은 `wrong_executor`, 종료된 generation은 `closed`이다. state/read/position 콜백 안에서는 Widget API를 호출하지 않는다. 저장 모델의 읽기와 불변 텍스트 배치는 가능하다.

`focus_changed`는 별도의 일반 변경 콜백이다. 공급자가 포커스를 얻거나 잃을 때 실행된다. `auto_focus_on_press=false`이면 pointer capture는 유지하되 자동 키보드 포커스를 부여하지 않는다. 공급자 자신의 키·텍스트 handler가 편집과 클립보드를 처리한다. 일반 단일 행 `input_field`에는 기존 동작을 보존하는 내장 공급 경로가 있다.

사용자 위젯이 텍스트·선택·조합을 갱신한 뒤 `notify_text_input_changed`를 호출하면
화면을 다시 그리고 접근성 메타데이터를 갱신한다. `value_changed=false`는 선택 등
값 자체가 바뀌지 않은 갱신에 사용한다. 이 알림은 공급자의 읽기 콜백 밖에서만
호출하며, 공급자가 없거나 해당 세대가 종료되면 실패한다.
선택적으로 `set_text` 변경 콜백을 제공하면 범용 `Widget::set_text`도 같은 소유
편집기에 전달된다. UTF-8과 크기는 전달 전에 검증하고 콜백의 오류와 예외를
호출자에게 전파한다. 이 콜백은 읽기 제공자와 달리 위젯 변경을 수행할 수 있다.

공급자는 `TextInputState::password`로 비밀 입력을 표시한다. snapshot에는 플래그와
메타데이터만 전달하며, 텍스트 서비스에 필요한 범위 읽기는 소유자가 계속 제공한다.
Cocoa 접근성의 값과 범위별 문자열은 비밀 입력에서 반환하지 않으며 iOS 보안 키보드
여부에도 같은 플래그를 사용한다. 마스킹과 사용자 정의 컨트롤의 복사·잘라내기
차단은 해당 컨트롤의 책임이다. 플랫폼 접근성 실행 검증은 별개로 남아 있다.

Cocoa의 NSTextInputClient는 선택·marked range·주변 문자열·화면 caret·문자 hit test를 조회한다. `unmarkText`는 `commit_composition`, 취소용 빈 preedit는 `composition`으로 구분한다. 조합 확정은 일반 텍스트 입력과 같이 중첩 콘텐츠와 팝업의 포커스된 편집기로 전달한다. Windows IMM32와 Linux XIM은 실제 caret 위치에 후보/조합 창을 배치하며 사용자 편집 위젯을 텍스트 대상으로 인식한다. 사용자 위젯의 복사·잘라내기는 문서 전체를 OS 어댑터로 복사하지 않고 위젯 명령으로 전달한다.

Cocoa 가상 접근성 peer에는 텍스트 영역 역할·길이·선택·제한된 범위 읽기가 추가된다. 이는 완전한 접근성 편집 프로토콜이나 Windows UIA/Linux AT-SPI 구현 완료를 의미하지 않는다. Linux의 현재 XIM 조합 창 경로는 유지하며 inline preedit callback, 전체 bidi caret affinity와 모든 플랫폼 IME 교정은 최종 실행 단계의 확인 대상이다.

`WidgetBuilder::event_loop()`는 호스트의 iiSDKCore 루프를 반환한다. 앱이 외부 루프를 제공했다면 같은 객체이다. 위젯은 이를 빌드 시 가져와 취소 가능한 작업 수명에 연결할 수 있다. 새로운 스레드나 OS 타이머를 만들 필요가 없다.

관련 테스트 소스는 `tests/text_input_provider.cpp`이다. 현재 테스트와 GUI 실행은 사용자의 요청에 따라 전체 이식 마지막으로 보류한다.
