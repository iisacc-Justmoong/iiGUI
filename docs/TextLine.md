# 네이티브 단락 배치

`TextLine`은 줄 바꿈 없는 한 단락을 CoreText(macOS), DirectWrite(Windows), Pango(Linux)에 배치한다. OS 편집 컨트롤은 생성하지 않는다. 글꼴 배치·커서 위치·시각 선택 구간·잘린 글리프 이미지를 자체 C++ 위젯이 사용한다.

입력은 불변 `TextIndex`, 위치는 UTF-16 단위이다. surrogate 중간 위치는 거절하며 마우스 hit test는 확장 자소 경계로 맞춘다. 양방향 텍스트 선택은 시각 run별 구간을 반환한다. 글꼴·크기·굵기·고정 행간·탭 간격을 명시할 수 있다. 제어 문자는 같은 UTF-16 길이의 control picture로 표시하며 원본 인덱스는 보존한다.

논리 너비와 커서 좌표는 double이다. `render(scrollX, viewportWidth, color, scale)`는 전체 줄 너비가 아니라 요청한 가로 뷰포트만 할당한다. 결과는 sRGB straight-alpha RGBA8이며 기존 `rgba_image`에 전달할 수 있다. 생성·배치·래스터 작업은 생성 스레드에서 수행한다. 복사된 핸들도 이 제한을 따른다. 폰트 fallback과 글리프 결과는 OS마다 다를 수 있다.

한 단락의 입력 상한은 1 MiB이다. 문서 전체 크기와 독립적인 shaping admission 한계이며 초과 시 `full`을 반환한다. Linux Pango의 고정 정수 좌표 한계 때문에 문자 수·글꼴 크기·탭 간격을 합한 보수적인 advance 예산도 적용한다. 이 플랫폼 한계는 바이트 상한보다 먼저 도달할 수 있다. 내용을 조용히 잘라 표시하지 않는다. 이미지의 한 변은 8192px, 전체는 기존 16 Mi pixel 한계를 따른다. 긴 run의 내부 글리프 순회 비용까지 일정하다고 보장하지 않는다. 논리 좌표의 저장은 double이지만 OS 글꼴 API 고유의 정밀도 한계는 남아 있다. 여러 단락의 줄 바꿈·리치 텍스트 span·조판·페이지 분할·글리프 atlas는 이 API의 책임이 아니다.

`tests/text_line.cpp`는 자소·UTF-16·탭·양방향 선택·긴 줄의 잘린 이미지·스레드·한계를 검사한다. 현재 사용자 지시에 따라 테스트와 GUI 실행은 전체 이식 작업 마지막에 일괄 수행한다. 중간 빌드는 실행 성공 증거가 아니다.
