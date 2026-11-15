# 조합 컨트롤의 편집기와 셀 경계

`InputOptions::style`은 자체 C++ 입력 위젯의 외형을 명시적으로 교체한다.
생략하면 기존 InputField의 높이 30, 가로 여백 8, hover 배경과 포커스 링이 유지된다.
`InputStyle`은 높이·글자 크기·행 높이·가로 여백·모서리·배경·hover 배경·글자색과
포커스 링을 지정한다. 투명 배경, 여백 0과 링 끄기로 표 셀 안의 편집기를 구성한다.
운영체제 입력 컨트롤은 생성하지 않는다.

`Widget::set_input_options`는 텍스트·폭·스타일·활성 상태를 검증한 뒤 교체한다.
텍스트가 같으면 커서와 선택을 유지하고, 다르면 새 텍스트 끝으로 옮긴다.
프로그램 변경은 `edited`를 발생시키지 않는다. 비활성화 시 기존
`commit_on_focus_loss`가 켜져 있으면 기존 포커스 해제 계약에 따라 submit은 가능하다.
UTF-8·길이·유한한 기하·RGBA 검증 실패는 기존 옵션을 보존한다.

`set_label_options`는 같은 Label의 텍스트·타이포그래피·색상·정렬을 교체한다.
`LabelOptions::elide_right`와 하위 `TextRun::elide_right`는 한 행의 오른쪽을
네이티브 텍스트 엔진의 말줄임표로 처리한다. macOS는 AppKit 조판,
Windows는 DirectWrite trimming, Linux는 Pango ellipsize를 사용한다.
실제 실행·GPU 검사는 macOS에서 수행하며 다른 OS 실행 증거와 구분한다.

`set_clip_content`는 일반 스택의 자손을 같은 사각형으로 클리핑한다. 기존 scroll 및
popup clip과 교차하며 GPU scissor·포인터 대상·가상 접근성 노출이 같은 경계를 사용한다.
별도로 소유된 popup은 기존 portal 계약에 따라 조상 clip에서 벗어날 수 있다.
운영체제 클리핑 위젯이나 스크롤 컨트롤은 추가하지 않는다.

`tests/cell_editor.cpp`는 Unicode 편집, 옵션 변경의 무편집 이벤트, 잘못된 대상·기하 거절,
실제 GPU clip과 말줄임표의 픽셀 변화를 검사한다. 기존 입력기의 전체 IME
replacement-range·클립보드·선택 표시 완성을 뜻하지 않는다.
