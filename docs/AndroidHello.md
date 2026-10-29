# Android 최소 네이티브 LVUI 앱

호스트의 클립보드·외부 URL·AChoreographer 확장과 기존 최소 레이블 범위의 구분은
[HostPlatforms.md](HostPlatforms.md)를 따른다. 아래의 이전 장치 실행 기록과 새
호스트 확장의 검증 기록은 구분한다.

`examples/HelloAndroid`는 Android 10/API 29 이상에서 실행하는 최소 Hello World이다.
LVUI의 `ApplicationWindow`, `VStack`, `Label`을 빌드 호스트의 `lvui-compile`로
C++23 코드에 변환하고 Android NDK로 컴파일한다. 앱 런타임은 iiSDKCore의
`Application`·이벤트 루프·타이머이며, iiGUI의 C++ 위젯을 Vulkan으로 표시한다.

## 네이티브 진입과 표시

Android OS에 내장된 `android.app.NativeActivity`가 NDK `native_app_glue`의
`android_main`을 부른다. `run_android_application`은 이 네이티브 Looper 스레드에
Android 문맥을 빌리고 Core 앱을 구성한다. Core와 GUI 첨부가 정리된 뒤 Activity
종료를 마친다. Core 자체에는 Android GUI나 iiGUI 의존성을 추가하지 않는다.

APK의 `hasCode=false`이며 Java·Kotlin 앱 클래스나 DEX를 생성하지 않는다.
OS의 NativeActivity 연결을 제외한 앱·위젯·렌더링·폰트 처리는 네이티브 코드이다.
Qt·WebView·LVUI 소스·컴파일러·인터프리터를 APK에 포함하지 않는다.

Core의 Observation은 모듈 사이에서 같은 관찰 문맥을 사용하도록 공유 라이브러리로
제공된다. APK 생성기는 앱의 ELF `DT_NEEDED`를 재귀 탐색하여 이 SDK 런타임과
NDK의 `libc++_shared.so`를 함께 담는다. Android의 공개 시스템 라이브러리만 기기에
위임하고 나머지 의존성이 없으면 설치 전에 실패한다. 여러 SDK 공유 라이브러리가
같은 C++ 런타임을 사용하며 Core의 Observation 소유·연결 계약을 바꾸지 않는다.

`HostAndroid.cpp`가 Looper 깨우기, Core 마감 시간, Activity 및 ANativeWindow 수명을
연결한다. 창이 사라지면 GPU 표면을 해제하고 다시 생기면 재구성한다. 일시정지 시
화면 제출을 멈춘다. 크기와 dp 배율은 Android가 관리한다. 위젯은 `NativeWidgetsAndroid.cpp`,
Vulkan surface는 `VulkanWindowAndroid.cpp`가 제공한다. 네이티브 뒤로 가기는 Core
앱과 위젯 수명을 정리한다.

초기 창이 준비되기 전에는 Core의 대기 작업 때문에 native Looper를 0 ms로 반복하지
않는다. 최초 시작 후에는 창이 일시적으로 없어져도 Core의 만료된 작업을 처리하며,
GPU와 모션 cadence는 별도로 중단한다. 따라서 백그라운드 타이머가 만료된 채 남아
반복 폴링하는 상태와 화면 밖에서 모션 타이머가 계속 도는 상태를 방지한다.

## 의도적으로 제한한 범위

이번 경로는 정적 스택·출력 가능한 ASCII 레이블과 Core 타이머를 실제 기기에 표시하기 위한
최소 구현이다. 폰트는 APK의 Pretendard Regular를 메모리에서 읽고, 기존 PlutoVG에
고정된 stb_truetype 소스를 독립 C++ 글리프 어댑터로 사용한다. 폰트 바이트와 비트맵은
소유 값이며 텍스트 호출은 C++ mutex로 직렬화한다. `iiGUI.android_label_font`는
운영체제 창 없이 실제 폰트의 폭·2배 배율·잉크·잘못된 크기 거부를 검사한다.
글자 크기·색상·기본 정렬·명시적 줄바꿈을 처리하지만
복잡한 스크립트 조합, 자동 줄 감기, 말줄임·밑줄·weight별 폰트 선택과 텍스트 편집기는
완전한 Android 텍스트 구현이 아니다. 복잡한 문자와 편집기 요청은 명시적으로 거부한다.
IME, TalkBack 접근성 연결, 클립보드, 외부 URL, 파일 선택기, 다중 창은 미구현이다.
모션의 선택적 Core cadence는 16 ms이며 디스플레이 vsync 지원으로 보고하지 않는다.

## 빌드와 기기 실행

빌드 도구에는 CMake, Ninja, iiSDKBuilder, 빌드 호스트용 LVUI 컴파일러, Android NDK,
Android SDK의 API 35/build-tools/platform-tools, JDK가 필요하다. 모든 결과물과 개발용
서명 키는 iiGUI의 `build/` 안에 둔다. 다른 앱·사용자 데이터는 변경하지 않는다.

```sh
python3 tools/android_hello.py \
  --sdk /absolute/path/to/android-sdk \
  --ndk /absolute/path/to/android-ndk \
  --jdk /absolute/path/to/jdk \
  --builder "$HOME/.local/SDK/iiSDKBuilder/bin/iisdk-builder" \
  --host-compiler "$HOME/.local/SDK/LVUI/bin/lvui-compile" \
  --install --serial DEVICE_SERIAL
```

`--install`을 생략하면 빌드·APK 검사만 수행한다. 승인된 실기기를 명시하며 여러
기기가 있을 때 임의의 첫 기기에 설치하지 않는다. 기본 ABI는 `arm64-v8a`이다.
기본 `--device-kind physical`은 에뮬레이터를 선택하지 않는다. 에뮬레이터 실행에는
`--device-kind emulator --serial emulator-5580`처럼 종류와 serial을 명시한다.
설치 직전 `ro.kernel.qemu`도 확인하여 요청한 종류와 다르면 설치를 거부한다.
64비트 x86 Android 대상도 빌드할 수 있으나 에뮬레이터를 실기기 검증으로 대체하지 않는다.

도구는 iiSDKBuilder로 같은 대상의 iiSDKCore·iiColorManager·LVUI·iiGUI와 앱을
빌드·스테이징한다. ELF ABI, NativeActivity 진입 심벌, Core와 자체 위젯 링크,
Qt·인터프리터·LVUI 파서의 부재를 검사하고 폰트와 라이선스를 패키징한다. 개발용 키로
서명한 `iiSDK-LVUI-HelloWorld.apk`는 배포용 릴리스 서명을 대신하지 않는다.

설치 후 한 번 실행하여 `HELLO_ANDROID_READY`, 2회 Core 타이머의
`HELLO_ANDROID_TIMER_OK`, 살아 있는 앱 PID와 화면을 수집한다.
`verification.json`의 빌드·설치·실기기·에뮬레이터 실행 상태는 별도 필드이다.
에뮬레이터 성공은 `native_emulator_verified`, `emulatorDeviceTested=true`,
`physicalDeviceTested=false`로 기록한다. 캡처 화면을 실제로
검토한 뒤 화면 표시 성공을 판단하며, 빌드 결과로 미구현 기능의 동작을 보증하지 않는다.
실행 로그의 시작 시각은 Android의 `date`에서 얻는다. 호스트와 에뮬레이터의 시간대가
달라도 시작·타이머 로그를 누락하지 않으며 기존 실행 후 관찰 시간 4초를 늘리지 않는다.

## 에뮬레이터 실행

Apple Silicon 호스트에서는 ARM64 시스템 이미지와 `--abi arm64-v8a`를 사용한다.
가상 기기의 OS가 에뮬레이터에서 동작하는 것이며, 앱은 동일한 LVUI 소스에서 AOT
생성한 네이티브 C++23 ELF이다. 에뮬레이터를 위한 Qt·DEX·LVUI 인터프리터를 앱에
추가하지 않는다. 에뮬레이터 자체의 호스트 UI 구현은 APK 의존성과 별개이다.

SDK 이미지·AVD·사용자 설정·서명 키·실행 증거는 `build/` 아래에서 관리한다.
기존 Android SDK의 build-tools·platform-tools·platforms는 읽기 전용으로 재사용할
수 있다. AVD는 `ANDROID_USER_HOME`, `ANDROID_AVD_HOME`으로 분리하고 앱 설치·실행은
명시한 serial에만 수행한다. 실기기를 연결한 상태에서도 선택을 바꾸지 않는다.

```sh
python3 tools/android_hello.py \
  --sdk "$PWD/build/android-emulator-20261027/sdk" \
  --ndk /absolute/path/to/android-ndk \
  --jdk /absolute/path/to/jdk \
  --builder "$HOME/.local/SDK/iiSDKBuilder/bin/iisdk-builder" \
  --host-compiler "$HOME/.local/SDK/LVUI/bin/lvui-compile" \
  --build-root "$PWD/build/android-emulator-20261027/app" \
  --device-kind emulator --serial emulator-5580 --install
```

### macOS 에뮬레이터 패키지의 서명 메타데이터

2026-10-27의 로컬 SDK Manager 설치에서는 Emulator 37.2.12의 일부 텍스트 리소스에
서명 확장 속성이 없었으며 `codesign --verify --strict`가 `NOTICE.csv`에서 실패했다.
공식 배포 ZIP의 `._NOTICE.csv` 등 AppleDouble 항목에는 원래의 `com.apple.cs.*`
서명 데이터가 있었다. 공식 아카이브의 SHA-256과 다운로드 파일이 일치하는지 확인한 뒤
macOS `ditto -x -k`로 별도 경로에 풀어 원본 메타데이터를 복원했다. 이 패키지는
`valid on disk`, `satisfies its Designated Requirement` 검사를 통과했다.

서명 검증과 에뮬레이터 부팅 성공은 서로 다른 증거이다. 복원한 패키지를 실행하기
전에 추출 완료를 확인하며, Gatekeeper 비활성화나 임의 재서명으로 검사를 대체하지 않는다.
로컬 증거는 `build/android-emulator-20261027/archive-provenance.json`,
`archive-resource-metadata.json`, `emulator-ditto-signature.log`에 있다.

## 2026-10-26 실제 검증

Samsung SM-S918N, Android 16/API 36, arm64-v8a에 USB 디버깅으로 설치·실행하였다.
1440×3088 실기기 화면에서 Hello World, iiSDKCore + LVUI + Vulkan, 증가하는 초 단위
표시를 확인하였다. 로그의 GPU는 `Adreno (TM) 740`이며 `HELLO_ANDROID_READY`와
`HELLO_ANDROID_TIMER_OK ticks=2`가 모두 기록되었다.

최종 APK는 약 2 MB이다. 앱 라이브러리, Core Observation, libc++ 공유 라이브러리와
폰트·라이선스를 포함하고 DEX·LVUI 소스·Qt 라이브러리는 포함하지 않는다. 최초 패키지의
Observation 누락은 ELF 의존성 폐쇄 검사로 수정하였다. 초기 글꼴 캐시 잠금에서 마운트가
정지한 문제는 실제 네이티브 스택을 확인한 뒤 독립 글리프 어댑터로 분리하여 해결하였다.

패키징·기기 선택 회귀 검사 6개, 창 없는 실제 폰트 C++ 검사 1개,
동일 폰트 검사의 ASan·UBSan 구성 1개가 통과하였다. 홈 화면 전환 후 기존 Activity를
복귀시켜 같은 PID와 화면·초 단위 갱신을 확인하였다. 백그라운드 약 2.07초의 측정 구간에서
프로세스 CPU 시간은 0.01초였다. 이는 짧은 동작 확인이며 일반적인 성능 벤치마크 수치로
확대 해석하지 않는다.

증거는 `build/android-hello-20261026/final/verification.json`, `device-log.log`,
`lifecycle.json`, `device-screen-resumed.png`에 있다. SDK 설치 소비 경로는 같은 빌드의
`builder/kits/android/Release/prefix`이며, 빌드 호스트의 macOS SDK를 Android에 링크하지
않았다. 앱을 기기에서 계속 실행한 상태로 두었다.

## 2026-10-27 에뮬레이터 실행 검증

Android Emulator 37.2.12와 AOSP Android 15/API 35 ARM64 이미지를 사용했다.
AVD는 `iiSDK_LVUI_API35_ARM64`, serial은 `emulator-5580`이며 720×1280 화면,
2개 vCPU, 2048 MB 게스트 RAM으로 구성했다. CPU는 macOS HVF를 사용하고 Vulkan은
`SwiftShader Device (LLVM 10.0.0)`의 소프트웨어 장치를 사용했다. 따라서 이 기록은
에뮬레이터에서의 실제 실행 증거이며 실기기나 물리 GPU의 성능 검증으로 해석하지 않는다.

iiSDKBuilder로 Android용 Core·ColorManager·LVUI·iiGUI를 빌드·설치 스테이징하고,
같은 설치 경로를 소비한 HelloAndroid를 개발용 키로 서명하여 설치했다. Qt·DEX·LVUI
소스가 없는 APK의 ELF ABI와 네이티브 의존성 폐쇄를 검사했다. 설치 대상은 명시한
에뮬레이터 하나이며 연결된 실제 휴대전화에는 설치하거나 실행하지 않았다.

실행 로그에서 `HELLO_ANDROID_READY widget=iiGUI.Label renderer=Vulkan`,
`runtime=iiSDKCore qt=false aot=true`, `HELLO_ANDROID_TIMER_OK ticks=2`를 확인했다.
캡처 화면에는 Hello World, iiSDKCore + LVUI + Vulkan, Native C++23 - 3 s가 표시되어
자체 C++ 위젯의 실제 렌더링과 Core 타이머에 따른 텍스트 변경을 확인했다.
기존 실행 후 4초 관찰 조건을 유지했으며 앱을 반복 실행하지 않았다.

호스트의 Android 전달 도구 단위 검사 10개와 관련 CTest 4개가 통과했다. CTest에는
같은 도구 단위 검사가 포함되므로 두 수치를 합산하지 않는다. Markdown·agent JSON·색인도
별도로 검증한다. 에뮬레이터의 최초 로딩·서명 메타데이터 복원·이미지 I/O 준비 과정과
앱 실행 검증 결과는 구분하며, 첫 가상 기기 부팅 시간을 앱의 시작 성능으로 보고하지 않는다.

최종 APK·화면·로그·빌드 및 설치 결과는
`build/android-emulator-20261027/app/verification.json`, `app/device-screen.png`,
`app/device-log.log`에 있고 전체 증거 색인은 같은 상위 폴더의 `verification.json`이다.
재실행용 `StartAndroidEmulator.command`는 준비된 AVD만 시작한다. AVD가 이미 실행
중이면 현재 창을 사용한다. 앱 재설치에는 위의 `--device-kind emulator`와
`--serial emulator-5580 --install` 명령을 사용한다. 검증 종료 시 앱과 가상 기기를
실행 상태로 유지했다.

### 지속 표시 검사

초기 장시간 화면 캡처에서는 고정 문구가 누락된 결과가 관찰되었다. 장면에 정적 레이블이
남는지 확인하는 `iiGUI.retained_labels`를 추가해 0·1·2·9·10·99·100·135·999·1000
갱신과 반복 프레임을 검사했다. 앱의 설명 레이블에는 `description` ID를 명시했다.
`IIGUI_ANDROID_RENDER_SMOKE=ON`은 같은 창에서 자리 수를 바꿔가며 GPU readback의
정적 레이블 픽셀을 비교하는 선택적 검사를 실행하고 `ANDROID_RETAINED_LABELS_OK`를
기록한다. 이 검사는 기본값 OFF이며 일반 최소 앱에 주기적 GPU readback을 추가하지 않는다.

최종 일반 모드에서는 같은 PID를 유지하면서 0·15·45·90초 화면을 비교했다. 네 번의
캡처 모두 제목·설명 영역이 초기 화면과 바이트 단위로 일치했다. 타이머 갱신과 임시
진단 코드·GPU readback fixture의 최종 최소 APK 제외도 별도로 확인했다.
원본 결과는 `build/android-emulator-20261027/final-screen-stability.json`에 있다.
픽셀 비교 검사의 통과만으로 초기 캡처 이상에 대한 보편적인 렌더러 원인을 단정하지 않는다.
