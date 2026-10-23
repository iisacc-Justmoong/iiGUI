# 가상 viewport와 보관형 그리기

대용량 표의 논리 행 좌표를 실제 위젯 위치에 그대로 넣지 않도록 세 가지 C++23 계약을
추가했다. Qt나 OS 스크롤 컨트롤을 만들지 않으며 기존 iiGUI 표면·GPU 그리기·입력 경로를
사용한다. 새 테스트는 작성·빌드 단계이며 실행 검증은 최종 일괄 단계까지 유보한다.

## 논리 스크롤 범위

`ScrollOptions::virtual_content_height`와 `virtual_content_width`는 각 축의 논리
범위를 지정한다. 유한한 0~1e15를 허용하며 실제 viewport 폭·높이는 기존 최대
8192이다. 지정한 축의 자식은 viewport 원점과 크기에 머문다. 수백만/수조 논리
픽셀을 스크롤해도 같은 크기의 실제 위젯이나 비트맵을 만들지 않는다. 소유자가
현재 두 축의 offset을 받아 화면과 교차하는 데이터만 그리는 구조이다.

세로 범위를 생략하면 기존 자연 높이 배치를 사용한다. 가로 범위를 생략한 기본값은
기존처럼 자식 폭을 viewport에 맞춘다. `horizontal_enabled=true`는 자식의 자연
폭을 보존하여 일반 콘텐츠도 가로로 이동하게 한다. 가상 축과 자연 축을 섞을 수 있다.
focus reveal은 자연 축만 자동 이동하며 가상 셀의 드러내기는 모델 소유자가 담당한다.
`enabled=false`는 두 offset을 0으로 제한한다. 콘텐츠 축소도 두 축을 다시 제한한다.

`set_scroll_position(x,y)`는 두 값을 검증한 뒤 한 번에 적용하고 한 상태를 통지한다.
기존 `set_scroll_offset(y)`는 가로 위치를 유지하며 `set_horizontal_scroll_offset(x)`는
세로 위치를 유지한다. 스냅샷에는 `horizontal_scroll_offset`과 `content_width`를 더했다.
콜백은 `ScrollViewport{bounds,offset,content_height,horizontal_offset,content_width}`를
받으며 기존 첫 세 필드의 순서는 유지한다.

가로·세로 휠을 함께 처리한다. Shift+세로 휠은 수평 이동이며 OS가 이미 scroll_x를
제공하면 다시 축을 바꾸지 않는다. 중첩 viewport는 이동한 축만 소비하고 막힌 축은
조상에게 전달한다. 한 축이 경계에 일부만 도달한 경우 해당 이벤트의 그 축은 소비하며
남은 거리까지 쪼개 재전달하는 방식은 아니다. 가로·세로 표시기는 모서리에서 겹치지
않게 그린다. 표시기 직접 드래그와 터치 관성은 아직 포함하지 않는다.

등록 후 첫 유효 조회/입력/표시 경계와 이후 실효 상태 변경에 통지한다. 같은 상태를
반복 통지하지 않으며 layout revision으로 불필요한 전체 노드 재순회를 피한다.
색상·그리기 데이터만 바꾼 경우 layout revision을 증가시키지 않는다.

통지는 입력 처리, 명시적인 snapshot, scroll 옵션/offset 변경, capture/info/present
같은 UI 경계에서 처리한다. GPU frame 생성 함수 안에서는 사용자 코드를 호출하지
않는다. 소유자는 viewport 크기를 직접 설정하는 경우 그 변경 경계에서 갱신할 수 있다.
수신 중인 같은 viewport의 옵션/offset 재변경은 busy로 거절한다. 다른 위젯과 그리기는
갱신할 수 있으며, 연결된 layout 변경은 최대 8회 정착 과정을 거친다. 수신자가 실패했을
때 이미 적용된 offset/옵션을 자동으로 되돌리지 않는다. 통지에서 트리를 해제하면
남은 통지는 중단하고 조회는 closed를 반환한다.

## 위젯 안의 그리기 데이터

스택 위젯의 `set_drawing(WidgetDrawing)`은 지역 좌표의 Panel·TextRun·ImageRun 목록을
보관한다. 기본 Graphics의 선형 sRGB, 폰트, 이미지와 유효성·용량 한도를 그대로 쓴다.
유효하지 않은 입력은 기존 내용을 바꾸지 않는다. 같은 값은 재무효화하지 않는다.

보관한 그리기는 해당 스택의 기하에 맞춰 이동하고 스택 경계와 조상 viewport에 clip한다. `TextRun::clip`은 텍스트 조판 경계를
바꾸지 않는 추가 지역 clip이며 스택 이동과 중첩 팝업의 좌표 이동을 함께 따른다.
그 뒤 실제 자식 위젯이 그려지므로, 표의 많은 읽기 셀은 하나의 그리기 목록으로 표현하고
활성 편집기만 실제 iiGUI 편집기로 둘 수 있다. primitive마다 노드를 만들지 않는다.
모든 그리기 목록은 기존 frame의 64개 레이어·패널/텍스트/이미지·텍스트 바이트 한도를
함께 사용한다. 이 API가 한도를 무제한으로 확장한 것은 아니다.

그리기 데이터는 네이티브 렌더러가 소비하는 값이며 소스 평가기나 UI 인터프리터가 아니다.
`WidgetSnapshot`의 drawing_panels/drawing_texts/drawing_images는 보관한 개수이다.
이 목록만으로 접근성 자식이나 편집 의미를 합성하지 않는다. 구체적인 위젯이 그 의미와
수명·편집기를 제공해야 한다.

## C++ 입력 가로채기

`set_input_handler`는 stack에 소유된 C++ handler를 연결한다. 실제 대상에서 조상으로
올라가며 일반 컨트롤 동작 전에 호출한다. 입력의 target은 실제 대상 ID이며 x/y는 해당
popup 변환을 역으로 적용한 논리 표면 좌표이다. true는 처리 완료, false는 기본 동작을
계속한다. Result 오류는 상위 입력 경계로 전달한다. 콜백 예외는 기존 Host 장벽을 쓴다.

pointer_down을 처리하면 해당 handler가 move/up을 캡처한다. 포인터가 경계 밖으로
나가도 전달하며 up, focus loss, 숨김, handler 교체, popup 열기/닫기와 트리 해제에서
회수한다. 동작 중 취소에는 focus_lost를 전달한다. Host 종료 장벽이 이미 사용자 콜백을
거절하는 단계에서는 논리 캡처만 회수하며 콜백을 새로 실행하지 않는다. 위젯의 소유 자원
정리는 RAII 수명에 두어야 한다. focus_lost는 기본 포커스/IME 정리도 수행한다.
handler가 연결된 stack은 Tab 경로에 참여할 수 있다. 일반 자식 컨트롤의 기본 동작은
handler가 false를 반환하거나 연결이 해제된 경우 유지한다.

처리기가 있는 조상 경로의 hover를 보관하여 포인터가 해당 하위 트리를 벗어나면
`pointer_leave`를 한 번 전달한다. 이 정리 이벤트는 범용 handler가 이동을 소비한 경우에도
유지되며 캡처 중인 드래그는 기존 소유자에게 계속 전달한다. 새 hover 검사 코드도
최종 일괄 실행 단계까지 유보한다.

이 경로는 이미 컴파일된 C++ callback을 실행한다. 표의 선택·편집·키보드 규칙은
구체적인 LVRS Table 구현이 제공한다. 범용 입력 API 자체가 clipboard 명령, resize hit
영역이나 접근성 셀 모델을 자동으로 합성하지 않는다.

## 검사 코드

`iiGUI.virtual_viewport_geometry/drawing/input/lifetime`은 큰 논리 offset, content shrink,
원점 유지와 변경 통지, GPU 픽셀, 잘못된 그리기의 원자성, primitive 수와 독립적인 노드 수,
조상 가로채기, 경계 밖 캡처, 기본 자식 입력 복귀, 숨김/해제 취소를 검사하도록 작성했다.
현재는 컴파일·링크 근거만 수집하며 실행 통과를 주장하지 않는다.
