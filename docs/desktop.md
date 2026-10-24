# 네이티브 데스크톱 서비스

`Desktop.h`는 앱 메뉴, 창 상태, 파일 선택, 클립보드와 외부 URL 경계를 제공한다. 이미지 디코딩은 iiImageCodec, 실제 파일 접근과 저장은 iiFileProvider의 책임이다. 이미지 클립보드·드롭은 인코딩된 바이트와 MIME을 전달하며 64 MiB, 파일 목록은 256개로 제한한다. UI는 문자열·로컬 파일 URL·이미지 드롭을 같은 타입 값으로 전달한다.

`Host::choose_file`은 완료 콜백을 받는 비동기 OS 대화상자이다. macOS에서는 부모 창에 붙는 Cocoa sheet이며 별도 중첩 이벤트 루프를 돌리지 않는다. 취소는 `canceled=true`이다. 창마다 한 대화상자만 허용한다. 창이 닫히면 대화상자와 지연 완료 전달을 폐기한다.

`Host::show(parent, callbacks)`는 같은 Core EventLoop를 사용하는 보조창을 연다. 주 창만 OS 앱 실행과 Core pump를 소유한다. 보조창의 창·위젯·UiExecutor 수명은 독립적이며, 보조창을 닫아도 주 창의 루프를 정지하지 않는다. 부모가 닫히면 남은 보조창도 닫힌다. 닫힌 창의 큐·타이머·I/O 콜백은 실행되지 않는다. 보조창의 닫힘 알림은 부모 루프에 전달하므로 부모 종료 중에는 후속 UI 알림을 보장하지 않는다.

`set_app_menu`의 ID·활성·체크·단축키와 콜백은 실제 OS 앱 메뉴로 연결한다. 호출은 Core UI 실행기에 전달한다. `window_action`은 최소화·복원·확대·전체 화면·전면 표시를 제공한다. `open_external_url`은 HTTP(S)·mailto URL을 OS에 넘긴다.

`iiGUI.desktop_*`는 실제 Cocoa 보조창/메뉴/파일 sheet 취소, 클립보드와 한글 IME replacement range를 검사한다. 클립보드 검사는 기존 모든 item/type을 복원한다. 타 OS 구현·실행 결과는 별도로 기록한다.

Windows의 파일 목록 게시에는 Shell의 `DROPFILES` 구조체를 사용하며 해당 선언은
`shlobj.h`에서 명시적으로 가져온다. `iiGUI.windows_desktop_validation`은 실제 Windows
백엔드와 링크하여 빈 파일 이름·잘못된 UTF-8·파일 수 초과·지원하지 않는 이미지 MIME과
URL이 네이티브 게시 전에 `invalid_argument`로 거절되는지 확인한다. 정상 클립보드
게시나 외부 앱 실행을 호출하지 않으므로 기존 사용자 클립보드는 유지한다.
위젯 상호작용과 콘텐츠 호스트 검사는 Result의 원인 예외도 출력하여 Win32 콜백
경계에서 원래 실패가 일반적인 콜백 오류 문자열로 가려지지 않게 한다.
# 운영체제의 파일 열기 요청

macOS Finder의 문서 열기 요청은 `HostCallbacks::files_opened`로 전달한다. 경로는 UTF-8이며 최대 256개를 허용한다. 창의 `opened` 이후 동일 Core 루프에서 배달하고 창이 닫힌 뒤에는 호출하지 않는다. 파일의 형식·문서 선택·저장은 소비자의 책임이다. `iiGUI.desktop_files`가 Cocoa delegate의 실제 요청 경로를 검사한다.
