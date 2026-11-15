# 네이티브 글리프 RGB를 보존하는 캐시

`TextRasterCache`는 TextLine의 네이티브 배치 결과를 요청한 가로 범위에만 래스터화한다. 캐시 키는 배치 객체의 세대, RGB, x·너비, 배율이다. Color의 RGB는 네이티브 폰트 서비스에 전달한다. 이모지 등의 고정 팔레트 픽셀을 흰색 마스크나 RGB tint로 덮어쓰지 않는다.

`paint`가 반환하는 TextPaint의 image는 straight-alpha sRGB RasterImage이며 opacity는 요청한 투명도이다. 그릴 때 ImageRun의 image·opacity·source crop을 지정하고 tint는 비워 둔다. 같은 RGB에서 투명도만 달라지면 같은 이미지를 재사용한다. 본문과 선택 문자의 RGB가 다르면 별도의 캐시 항목을 사용하며 동일한 색상을 다시 선택하면 재사용한다. RGB 변경은 정확한 네이티브 결과를 얻기 위해 새로운 래스터가 필요할 수 있다.

기본 한계는 32 MiB·1,024개 항목이다. 픽셀 수·한 변·배율과 메모리 예산을 할당 전에 검사한다. LRU 초과 항목만 회수하며 매 paint마다 전체 목록을 갱신하지 않는다. 약한 배치 객체 식별자는 폰트와 배치 객체의 수명을 늘리지 않고, 같은 메모리 주소에 나중에 만들어진 줄과 충돌하지 않는다. 캐시가 보유한 이미지 바이트를 통계로 제공한다. 이미 반환한 이미지와 이전 프레임·GPU 리소스의 수명은 소비자에게 있으므로 프로세스 전체 메모리 한계와 동일하지 않다. 새 이미지의 작성 중에는 캐시 외에 한 장의 임시 이미지가 존재할 수 있다.

set_limits는 변경한 한계까지 LRU를 회수한다. clear는 이미지를 해제하되 누적 hit/miss·래스터화·회수 횟수를 보존한다. 모든 API는 생성 스레드에서 호출한다. 다른 스레드는 wrong_executor, 유효하지 않은 요청은 invalid_argument, 예산 초과는 full을 반환한다.

TextLine::render도 투명도를 네이티브 래스터화 후 적용하여 일반 글자와 컬러 글리프에 같은 alpha 계약을 보장한다. Windows TextLine의 Direct2D 경로는 [공식 컬러 글꼴 옵션](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/ne-d2d1-d2d1_draw_text_options)을 명시적으로 사용한다. 일반 TextRun의 일괄 래스터 경로를 같은 투명도 계약으로 변경한 것은 아니다. 실제 컬러 글꼴·형식·fallback 지원은 운영체제와 설치된 폰트 서비스의 범위에 따른다. 이 캐시는 OS에 없는 컬러 글리프를 합성하지 않는다.

관련 소스 검사는 tests/text_raster_cache.cpp와 tests/text_line.cpp에 있다. 현재는 컴파일만 수행하고 실제 테스트·GUI 실행은 전체 LVRS 이식의 마지막 일괄 단계로 보류한다.
