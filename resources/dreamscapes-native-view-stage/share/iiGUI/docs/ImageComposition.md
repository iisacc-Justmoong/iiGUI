# 원본 이미지 영역·마스크와 전경 합성

ImageRun은 불변 RasterImage의 정규화 source 사각형, 둥근 radius, 안쪽 border와
세 정점 수직 ImageGradient를 받는다. 자르기는 원본 픽셀을 복제하지 않고 샘플 좌표를
바꾸며, 보간·알파 합성은 선형 sRGB에서 수행한다. ImageOptions의 fit은 stretch·contain·
cover이고 기존 preserve_aspect 입력도 유지한다. circular는 비정사각 뷰 안에 가장 큰
원을 중앙 배치한다. allow_empty를 명시하면 원본 없이 배경만 그릴 수 있다.

CanvasLayer/CanvasFrame의 images_before_text는 이미지 다음에 네이티브 글자를 합성한다.
기본값은 기존 순서를 유지한다. 전경 캐시는 이 순서·원본 영역·마스크·테두리·그라데이션을
키에 포함한다. Metal·Vulkan·Direct3D의 전경 텍스처는 전체 뷰포트 대신 실제 이미지·
텍스트가 차지하는 정수 디바이스 영역을 사용한다. 원점을 셰이더에 전달하며 범위 밖
샘플은 투명이다. Metal의 GPU 변환은 원래 픽셀 캐시를 유지하고 나머지 경로의 CPU
변환은 범위·반경·글자 clip까지 함께 바꾼다. 이미지/글자 래스터는 현재 공통 CPU
전경 경로이고 최종 합성은 네이티브 가속기이다. GPU 이미지 샘플러로 오인하지 않는다.

WidgetPaintHandler는 owner 스레드에서 불변 WidgetDrawing을 반환한다. 로컬 크기와
상호작용 소유자의 활성·호버·눌림·포커스·체크 상태 및 기본 슬롯의 자식 수를 받는다.
상호작용 소유자는 같은 세대의 자신 또는 조상이어야 한다. provider는 앞뒤 순서를
보존하는 별도 레이어이며 이후 자식 컨트롤은 그 위에 그린다. 그리기 중 UI 조회·변경은
busy로 거절하며 데이터 계산만 수행해야 한다. request_repaint는 소유자 스레드에서
기존 위젯의 다시 그리기를 요청한다. 교체·종료된 핸들은 closed이다.

ActionOptions에 checkable·checked와 탭/포인터 포커스 정책을 추가한다. 사용자 활성화는
체크 상태를 먼저 반영한 뒤 action을 호출한다. set_checked는 클릭 콜백을 호출하지
않는다. 비활성 activate는 성공하는 무동작이며 사용자 콜백을 실행하지 않는다.
accessibility_description은 접근성 설명에, snapshot.child_count는 같은 세대의 실제
자식 수에 대응한다. measure_text_layout은 공개된 네이티브 글꼴 측정 계약이다.

새 합성·마스크·영역·캐시·그리기 admission·수명·포커스 사례 17개와 기존 이미지 캐시
검사를 갱신한다. 모든 테스트·앱·self-test 실행은 전체 작업 마지막의 일괄 단계로
보류한다. 컴파일·정적 검사와 실제 픽셀·메모리·플랫폼 실행 검증을 구분한다.

일반 위젯 트리와 팝업의 Tab·Shift+Tab 순회는 모두 `tab_focusable=false`인
ActionSurface를 건너뛴다. 명시적 `Widget::focus()`는 계속 허용하며,
`pointer_focusable=false`는 포인터로 활성화할 때 기존 포커스를 빼앗지 않는다.
이 규칙은 운영체제 입력 어댑터 아래의 공통 위젯 상태에 적용한다.

paint provider는 팩터리 부착 중에도 호출될 수 있다. 캡처 오류 경계 검사는 정상
provider로 부착한 뒤 실패를 주입하고 repaint를 요청한다. 이로써 팩터리 실패와
캡처 `Result`의 오류 전달을 분리해서 검증한다. 검사 콜백의 오류 원인도 보존한다.
