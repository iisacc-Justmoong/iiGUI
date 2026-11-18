# 네이티브 단락 배치

`TextLine`은 줄 바꿈 없는 한 단락을 CoreText(macOS·iPadOS), DirectWrite(Windows), Pango(Linux), StaticLayout(Android), 브라우저 DOM과 Canvas(WebAssembly)에 배치한다. OS 편집 컨트롤은 생성하지 않는다. 글꼴 배치·커서 위치·시각 선택 구간·잘린 글리프 이미지를 자체 C++ 위젯이 사용한다.

입력은 불변 `TextIndex`, 위치는 UTF-16 단위이다. surrogate 중간 위치는 거절하며 마우스 hit test는 확장 자소 경계로 맞춘다. 양방향 텍스트 선택은 시각 구간을 반환하며 플랫폼에 따라 인접한 run이 합쳐질 수 있다. 글꼴·크기·굵기·고정 행간·탭 간격을 명시할 수 있다. 제어 문자는 같은 UTF-16 길이의 control picture로 표시하며 원본 인덱스는 보존한다.

논리 너비와 커서 좌표는 double이다. `render(scrollX, viewportWidth, color, scale)`는 전체 줄 너비가 아니라 요청한 가로 뷰포트만 할당한다. 결과는 sRGB straight-alpha RGBA8이며 기존 `rgba_image`에 전달할 수 있다. 생성·배치·래스터 작업은 생성 스레드에서 수행한다. 복사된 핸들도 이 제한을 따른다. 폰트 fallback과 글리프 결과는 OS마다 다를 수 있다.

`wrap(width, maximum_lines)`는 원래 단락을 운영체제의 단어·자소 줄바꿈으로 나눈다. 결과의 각 `TextLineSlice`는 원본의 UTF-16 시작·끝과, 자체 로컬 인덱스로 커서·시각 선택·hit test·래스터를 제공하는 `TextLine`을 보유한다. 줄의 원문은 순서대로 합치면 입력과 같고, 중간에 제어 문자나 공백을 버리지 않는다. 원래 단락의 양방향 서식과 글자 배치 문맥을 유지하므로 줄을 단순히 잘라 새 단락으로 조판하지 않는다. 원본 핸들을 놓아도 결과 줄은 필요한 네이티브 문단을 공유한다. 새 너비에는 원본 `TextLine`을 다시 줄바꿈하며 결과 조각의 재줄바꿈은 거절한다.

너비는 유한한 0 초과 8192 이하의 논리 px이다. 줄 수 기본 한도는 4096이며 호출자는 1–65536으로 지정할 수 있다. 한도 초과는 `full`을 반환하고 부분 결과를 내지 않는다. 빈 단락은 빈 줄 하나이다. 플랫폼의 줄 경계가 원본 Unicode 확장 자소 또는 스칼라 경계를 나누면 잘못된 결과를 내보내지 않고 실패한다. macOS·iPadOS는 [CoreText typesetter](https://developer.apple.com/documentation/coretext/cttypesettersuggestlinebreak%28_%3A_%3A_%3A%29?language=objc), Windows는 [DirectWrite](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/ne-dwrite-dwrite_word_wrapping), Linux는 [Pango 문단 배치](https://docs.gtk.org/Pango/class.Layout.html)를 사용한다. 각 플랫폼의 글꼴과 줄바꿈 규칙은 다를 수 있다.

한 단락의 입력 상한은 1 MiB이다. 문서 전체 크기와 독립적인 shaping admission 한계이며 초과 시 `full`을 반환한다. Linux Pango의 고정 정수 좌표 한계 때문에 문자 수·글꼴 크기·탭 간격을 합한 보수적인 advance 예산도 적용한다. 이 플랫폼 한계는 바이트 상한보다 먼저 도달할 수 있다. 내용을 조용히 잘라 표시하지 않는다. 이미지의 한 변은 8192px, 전체는 기존 16 Mi pixel 한계를 따른다. 긴 run의 내부 글리프 순회 비용까지 일정하다고 보장하지 않는다. 논리 좌표의 저장은 double이지만 OS 글꼴 API 고유의 정밀도 한계는 남아 있다. 하드 줄바꿈으로 나뉜 여러 단락의 조합·리치 텍스트 span·페이지 분할·글리프 atlas는 이 API의 책임이 아니다.

## Android 구현 계약

Android는 `StaticLayout`의 문단 문맥을 보존해 줄 경계, UTF-16 커서와 hit test,
불연속 양방향 선택을 제공한다. 줄 조각은 전체 문단과 배치를 공유하며 별도
부분 문자열로 다시 조합하지 않는다. 고정 줄 높이, 선택 글꼴과 대체 글꼴,
탭 정지점 및 뷰포트 크기의 비트맵 출력을 제공한다. 선택 영역은 1/64 논리 px의
정수 영역으로 변환하며 인접한 시각 영역은 합쳐질 수 있다. Android 탭 정지점은
정수 px로 반올림되고 보수적 상한은 8192개이다. 문단의 네이티브 좌표 상한은
8388608 논리 px이다. 상한을 넘으면 부분 결과 대신 `full`을 반환한다. 운영체제의
줄 수 제한을 사용하되 말줄임 경계를 별도로 넓혀 뷰포트보다 큰 글자도 보존한다.
API 24–27은 대체 글꼴의 줄 높이를 반영하지 못해 일부 글리프가 잘릴 수 있다.
소유 스레드와 JNI 참조의 수명을 분리한다. Android 입력기·기기 화면 실행의 지원
여부는 텍스트 배치와 별도이다.

`tests/text_line.cpp`와 `tests/text_line_wrap.cpp`는 자소·UTF-16·탭·양방향 선택·문단 방향이 이어지는 줄바꿈·긴 줄의 잘린 이미지·스레드·수명·한계를 검사한다. Android는 별도의 모의 운영체제 검사와 NDK 크로스 빌드·링크로 JNI 경계를 확인한다. 이 검사들은 Android OS의 실제 문자 조합, 입력기, 화면 또는 접근성 실행 결과가 아니다. Windows·Linux는 소스 경로를 작성했으며 실제 컴파일·실행을 이번 단계에서 확인하지 않았다.

## 웹 단일 행 입력

브라우저의 임시 비표시 DOM Range가 UTF-16 커서와 선택 영역을 구한다. 포인터로 위치를 찾을 때 확장 자소 경계의 커서 좌표를 준비하고 같은 텍스트가 유지되는 동안 재사용한다. 실제 글자는 기존 Canvas 글꼴 래스터 경로에서 그리며 iiGUI 위젯이 이미지와 선택을 표시한다. 비밀번호 입력은 위젯이 마스킹한 글자만 줄 배치에 전달한다.

이 경로에는 브라우저 문서에 접근하는 UI 스레드가 필요하다. 웹의 단락 줄바꿈은 `TextLine::wrap`에서 지원되지 않는다. `tests/web_text_backend.cpp`는 모의 DOM에서 좌표와 뷰포트 래스터를 확인하고 DOM이 없는 환경에서는 기존의 미지원 결과를 확인한다.

## 화면 배율과 글리프 품질

macOS와 iOS의 자체 렌더러는 CoreText로 한글 자소 조합, 폴백 글꼴 및 영문 혼용을 조판한다. 반투명 배경에 올리는 글자는 RGB 서브픽셀 색상이 없는 회색조 커버리지로 처리한다. 배율에 따라 글자 래스터만 추가 샘플링하고, 8 MiB를 넘기는 추가 작업 버퍼는 만들지 않는다.

Metal과 외부 페인터는 `device_rgba_image`로 만들어진 글자 래스터를 화면 배율에서 정수 픽셀에 배치한다. `device_image_pixels`는 화면과 원본의 픽셀 수가 일치하고 잘라낼 영역이 픽셀 경계에 놓일 때만 좌표를 반환한다. 확대·축소가 있는 글자와 일반 이미지의 필터링은 그대로 유지된다. 이 경로는 장면 전체를 초과 샘플링하거나 새 프레임 타이머를 사용하지 않는다.
