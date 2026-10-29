# 유지되는 스택의 배치 변경

`WidgetLayout.h`는 `Insets`, `StackAlignment`, `StackAlignmentAxes`, `StackOptions`,
`StackLayoutOptions`와 값 검증을 제공한다. 기존 NativeWidgets 소비자는 같은 타입을
전이적으로 받는다. 수직 스택은 가로 정렬, 수평 스택은 세로 정렬, 겹침 스택은 두 축을
각각 사용한다. 축별 값이 없으면 기존 `alignment` 값을 사용한다. 겹침 스택의 trailing도
실제 오른쪽·아래쪽 위치에 반영한다.

```cpp
auto options = widget.stack_layout();
if (!options) return std::unexpected(options.error());
options->stack.spacing = 8;
options->stack.insets = ii::gui::Insets{4, 6, 4, 6};
options->extent = {240, 160};
options->flex = 1;
return widget.set_stack_layout(*options);
```

조회와 변경은 소유 실행 영역의 활성 ordinary stack에만 허용한다. 수직·수평·겹침
스택이 대상이며 절대 좌표 스택·스크롤·일반 컨트롤에는 기존 전용 API를 사용한다.
옵션·크기·flex를 전부 검증한 뒤 동일 노드에 적용한다. 실패한 입력은 일부 필드도
변경하지 않으며 같은 값은 배치를 무효화하지 않는다. 자식·이벤트·그리기 제공자·편집
상태·포커스를 교체하지 않는다. 닫힌 세대는 `closed`, 다른 실행 영역은
`wrong_executor`를 반환한다.

간격·공통 패딩은 0~32767, 변별·암묵적 여백과 최소 크기는 0~8192, extent는
0~32767, flex는 0~4096 범위의 유한 값이다. extent의 0인 축은 기존 자연 크기 계약을
유지한다. 명시적 extent·부모 가용 크기·최소 자연 크기의 우선순위도 기존 계약을 따른다.
잘못된 축 enum을 허용하지 않는다. 기존 개별 배치 setter도 유지한다.

변경한 노드와 조상의 측정 캐시를 무효화하고 기존 배치·측정 통지를 사용한다. LVUI는
한 바인딩에서 전체 배치 값을 계산하며 정적 입력은 AOT에서 확정한다. 뷰가 파일 상태를
소유하거나 원본 목록을 재발행하는 경로를 추가하지 않는다.

공개 헤더 ABI 입력에 `WidgetLayout.h`를 포함한다. 이 헤더들과 기존 ABI 대상 헤더가
바뀌면 CMake를 다시 구성하도록 추적하여 설치 패키지의 식별자를 현재 소스와 맞춘다.

`tests/native_layout.cpp`는 값·잘못된 전체 입력의 롤백·실행 영역·실제 배치·측정·입력
보존·실패 복구·퇴역과 LVUI 생성 위젯을 검사한다. `tests/layout_install.py`는
`build/` 안에 LVUI·iiGUI를 설치하고 경로를 옮겨 공개 헤더·API·동적 AOT 팩터리를
컴파일·링크한다. 실행은 값과 종료된 위젯의 접수 거부만 수행한다. OS 창·GPU 표면을
만들지 않는다. 원본 LVRS의 자식별 정렬·앵커·motion·전체 플랫폼과 접근성은 별도 검증이다.
