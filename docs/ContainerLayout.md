# 네이티브 컨테이너의 자연 크기와 기본 콘텐츠 슬롯

StackOptions의 minimum_width·minimum_height는 자연 크기의 하한이다. 명시적인
extent/frame은 자연 크기보다 우선하며 실제 부모의 가용 폭에 맞춰 제한한다.
intrinsic_width는 세로 스택의 자식 stretch를 유지하면서 컨테이너 자체가 자동으로
부모 폭 전체를 요구하지 않도록 한다. 기본값은 기존 동작을 보존한다.

insets는 실제 콘텐츠 여백, implicit_insets는 자연 크기를 계산할 때 더하는 여백이다.
후자가 없으면 기존 padding/insets를 사용한다. 툴바처럼 기본 implicit 높이에 고정
여백을 더하면서 실제 자식 여백은 compact 상태에 따라 바꾸는 구성에 사용한다.
measure_hidden_children은 숨긴 자식도 자연 크기에 포함한다. Item 같은 본문 슬롯의
크기 계산에 쓰며 숨긴 자식을 그리거나 입력 대상으로 만들지는 않는다.

WidgetBuilder::set_default_content(host, slot)는 이후 host에 추가하는 일반 자식을
같은 세대의 slot으로 구성한다. 실제 트리를 다시 부모 지정하거나 복제하지 않는다.
slot은 host의 엄격한 자손인 일반 컨테이너여야 하며 순환·자기 참조·다른 세대를
거절한다. 중첩 슬롯은 최종 슬롯까지 따른다. 팝업은 원래 host 소유권을 유지한다.
이 계약은 구성 단계에서만 설정할 수 있다. 별도의 소스 인터프리터는 사용하지 않는다.

일반 컨테이너에도 enabled를 적용하고 비활성화할 때 하위 눌림·캡처·포커스를 취소한다.
자식의 요청한 enabled 값은 바꾸지 않아 부모를 다시 켜면 기존 개별 정책을 유지한다.
취소 콜백에서 트리가 종료되면 이후 접근은 closed이다.

ButtonStyle은 선택적 text_role·font_size와 고정 line_height를 받는다. 텍스트는 기존
네이티브 글꼴 셰이핑 경로로 그린다. set_button_options는 일반 버튼의 텍스트·스타일·
크기·아이콘·활성 정책을 교체하면서 action·control events와 위젯 정체성을 보존한다.
색상 모델 버튼에는 이 API를 적용하지 않는다. 유효성 실패는 기존 옵션을 유지한다.
동일한 StackOptions·LabelOptions 교체는 불필요한 측정 무효화를 억제한다.

tests/container_layout.cpp에 자연/명시 크기·실제/implicit 여백·intrinsic 폭·숨김 측정·
슬롯/중첩/검증·버튼 교체·하위 취소와 수명 사례 11개를 등록한다. 현재는 컴파일·링크와
정적 검사만 수행한다. 앱·테스트·self-test 실행은 전체 작업의 최종 일괄 단계로 미룬다.

## 버튼의 비대칭 콘텐츠 배치

ButtonOptions의 left_padding·right_padding·content_spacing·indicator_size는 아이콘과
텍스트·메뉴 표시자의 명시 배치 계약이다. 생략하면 기존 버튼 배치를 유지한다.
음수 간격은 -256까지 허용하며, 푸터 메뉴처럼 18+18-2와 좌우 4/2 여백을 합친
자연 폭 40을 구현한다. width는 선택적인 양수 명시 폭이고 생략하면 자연 폭이다.
옵션 교체는 동일 버튼의 입력 콜백·정체성을 유지한다. tests/button_layout.cpp에
자연 폭·간격·명시 폭·유효성·픽셀 배치 사례 5개를 추가하며 실행은 최종 단계로 미룬다.
