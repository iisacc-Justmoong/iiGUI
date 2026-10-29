# 네이티브 C++ 메서드 조회의 표시 연결

LVUI의 [객체 메서드 조회](../../LVUI/docs/MethodQueries.md)를 생성 C++와 기존 위젯 트리에
연결한다. getter 내부의 private Core Observable publication으로 표시 속성을 갱신하며,
호출 인자를 넘기거나 레이아웃을 다시 만든다는 이유로 파일 작업을 실행하지 않는다.

`tests/native_method_queries.cpp`는 창 없는 WidgetState에서 직접 백엔드·중첩 객체·별칭·
타입 지정 함수·구성 입력·괄호·인덱스 및 메서드 반환값의 멤버 표시를 검사한다.
조건부 private 원본 전환, 실패 후 정상 표시 보존과 복구, 선택 객체의 재진입 교체,
조건부 재활성화, 입력 문자열·포커스 보존과 문서 revision 비변경도 확인한다.

설치 후 재배치한 소비자의 `Methods.lvui`는 공개 패키지로 생성·컴파일·링크한다.
실제 OS 창·GPU 표면은 열지 않으며, 해당 실행과 LVRS 앱 검증은 전체 작업의 최종
일괄 단계에서 확인한다. 창 없는 검사 결과를 다른 플랫폼의 실행 결과로 대신하지 않는다.

## 공유 라이브러리의 private getter

앱과 모델 DLL은 Core의 `iiSDKCore::Observation` 실행부를 공유한다. 헤더마다 분리된
스레드 지역 수집 슬롯을 두면 숨김 가시성으로 빌드한 모델 내부의 읽기를 앱이 수집하지
못하므로, 같은 실행부의 범위 함수에 연결한다. 모델 라이브러리에는 iiGUI나 LVUI 또는
전체 작업자 Runtime 의존성을 추가하지 않는다.

`tests/native_shared_method.cpp`와 LVUI의 `NativeSharedMethod.lvui`는 실제 숨김 공유
라이브러리를 사용한다. private 값의 갱신이 표시 결과만 바꾸는지, 파일 revision과
합성 publication 없이 동작하는지, 편집 중인 입력·포커스·위젯 수가 유지되는지 검사한다.
제거한 뷰가 이후 모델 publication을 받지 않고 백엔드를 한 번만 해제하는지도 검사한다.
공유 모델은 Core의 설치·재배치 소비자에서도 검사한다. 구체적인 실행 결과는
LVUI의 [공유 관측 단계 검증 기록](../../LVUI/docs/MethodQueries.md#공유-관측-실행부의-검증)에 기록한다.

콘텐츠 호스트가 부여한 ID 범위를 보존하여 위젯을 조회하고, 중복되는 이름은 검사 실패로
처리한다. 설치 소비자 빌드가 실패하면 캡처한 컴파일·링크 출력을 먼저 기록한 뒤 오류를
반환하므로 임시 디렉터리 정리 후에도 실제 실패 근거를 확인할 수 있다.

## 일반 객체 메서드 참조

LVUI의 [일반 객체 메서드 참조](../../LVUI/docs/ObjectMethodReferences.md)를 실제 위젯 이벤트에
연결한다. `tests/native_object_method_references.cpp`는 중첩·선택·별칭·후위 식·구성 입력의
콜백과 반환 함수, 문자열 이벤트, Static·초기 가변 함수 선택을 검사한다. 잘못된 선택의
마지막 정상 동작과 복구, 입력·포커스·위젯 정체성 보존, 조건부 재활성화, 공유 부모와
임시 객체·자기 제거의 수명도 창 없는 WidgetState에서 확인한다.

재배치 소비자의 `MethodReferences.lvui`는 설치된 컴파일러와 공개 패키지로 생성·컴파일·
링크한다. LVRS 팩터리 실행과 실제 창·GPU·다른 플랫폼 검증은 전체 작업의 최종 단계에 둔다.
