# Windows 네이티브 회귀 검증

D3D11은 일반 장면의 색상 attachment만 먼저 만들고 backdrop·shadow가 처음 필요한
프레임에서 두 snapshot을 할당한다. 일반 장면은 픽셀당 4바이트, 효과 snapshot을
보유한 장면은 추가 8바이트를 실제 `cached_texture_bytes`에 반영한다. resize는
기존 snapshot을 폐기하며, 새 snapshot과 sampler가 모두 생성된 뒤 교체한다.
일반 graphics 검사와 효과 검사를 모두 실행하여 무효 자원을 참조하지 않는지 확인한다.

상호작용 관찰 fixture의 입력 가로채기는 공개 계약에 따라 조상 stack에 연결한다.
콘텐츠 호스트의 Enter 검사는 key_down에서 아직 활성화되지 않고 key_up에서 한 번
활성화되는 전체 입력 수명을 검사한다. 실제 Win32 입력 구현의 계약은 유지한다.

글리프 crop 검사는 실제 호스트 폰트의 ink 시작 행을 먼저 확인하여 그 일부를
잘라낸다. 플랫폼마다 다른 native font leading을 고정 픽셀로 가정하지 않으며,
잘린 결과가 원본의 글리프 모양·원점·남은 픽셀을 유지하고 지정 clip 밖에 그리지
않는지 확인한다. native와 Vulkan 출력에서 같은 검사를 실행한다.

`Widget::measure(available_width)`의 결과 너비는 유효한 제안 너비를 넘지 않게 한다.
고정된 실제 frame을 수정하지 않고 측정 결과만 제한한다. 회귀 검사는 두 제안 너비와
무효 값 거절, 원래 160px frame 보존을 함께 확인한다.
# LVRS 소비자 계약

절대 스택은 비어 있는 0 크기를 생성·변경할 수 있다. 음수·비정상 값 검사는 유지한다.
텍스트 입력 제공자를 연결한 사용자 정의 편집기는 접근성 이름을 설정할 수 있고,
일반 스택에 같은 속성을 설정하는 것은 계속 거절한다. composition과 text input
provider 검사는 이 경계와 실제 snapshot 메타데이터를 확인한다.

## GPU 작업 계수

D3D11은 blur를 effect fragment shader 안에서 실행하므로 `backdrop_blurs`와
`shadow_blurs`는 실제 효과 draw 횟수이다. Metal의 보관된 별도 blur pass 계수와
구분한다. 정적 프레임은 재실행하지 않으며 바뀐 프레임은 inline blur를 다시 실행한다.
활성 backdrop snapshot 저장소와 복사 횟수를 기록하며, 없는 shadow 중간 텍스처를
할당된 것처럼 보고하지 않는다. Metal의 효과 보관 성능을 Windows 성공으로 주장하지 않는다.
