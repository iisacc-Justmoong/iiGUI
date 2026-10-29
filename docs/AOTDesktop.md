# LVUI Hello World의 네이티브 데스크톱 빌드 계약

`examples/HelloWorld`는 한 개의 `HelloWorld.lvui`와 C++23 앱을 macOS, Windows,
Linux에서 함께 사용한다. `lvui-compile`은 빌드 호스트에서 LVUI를 C++ 위젯 생성
코드로 바꾼다. 대상 컴파일러가 생성 코드, C++ 백엔드, iiSDKCore 진입점을 최종
실행 파일에 링크한다. 앱은 LVUI 소스나 컴파일러를 실행 시 읽지 않는다.

| 대상 | 창·입력 호스트 | 자체 위젯 표시 | 실행 파일 |
|---|---|---|---|
| macOS | AppKit | Metal, 자동 선택 시 Vulkan 대체 가능 | Mach-O / `.app` |
| Windows | Win32 | Direct3D 11, 자동 선택 시 Vulkan 대체 가능 | PE32+ GUI `.exe` |
| Linux | X11 / XWayland | Vulkan | ELF64 |

레이블·버튼·스택은 iiGUI의 C++ 위젯이다. OS 컨트롤이나 웹뷰로 교체하지 않는다.
Linux의 직접 Wayland 호스트는 제공하지 않는다. GPU 드라이버, 창 서버와 시스템
텍스트 라이브러리는 대상 운영체제가 제공해야 한다.

## 앱 계약과 대상 검사

iiSDKCore가 `Application`, 이벤트 루프, 타이머, 백그라운드 작업, 종료를 소유한다.
`iiGUI::attach_application`은 같은 Core 루프를 창의 OS 펌프에 연결한다. 앱은 250 ms
타이머, 작업 결과 42의 UI 전달, 백엔드 한 번 생성과 해제, 두 번의 버튼 동작,
크기 변경, 종료 후 핸들 무효화를 검사하는 `--self-test`를 제공한다. 기존 5초
콜백 감시 시간과 macOS의 기존 CTest 이름 `HelloWorld.aot_cocoa`를 유지한다.
Windows·Linux의 등록 이름은 `HelloWorld.aot_native`이다.

실행 검사를 요청할 때만 대상 OS의 로그인된 데스크톱에서 다음을 실행한다.

```sh
ctest --test-dir build --output-on-failure -R '^HelloWorld\.'
```

교차 빌드 성공은 OS 창 생성, GPU 드라이버, 입력 또는 실제 타이머 동작의 실증이
아니다. 다음 도구는 실행 파일을 만들고 검사하지만 GUI나 대상 실행 파일을 실행하지 않는다.

## 재현 가능한 빌드와 설치 소비자 검사

```sh
python3 tools/verify_aot_desktop.py \
  --platform linux \
  --host-compiler "$HOME/.local/SDK/LVUI/bin/lvui-compile" \
  --builder "$HOME/.local/SDK/iiSDKBuilder/bin/iisdk-builder" \
  --toolchain /absolute/path/to/linux-toolchain.cmake \
  --llvm-readobj /absolute/path/to/llvm-readobj \
  --llvm-nm /absolute/path/to/llvm-nm \
  --cache Vulkan_INCLUDE_DIR=/target/vulkan/include \
  --cache Vulkan_LIBRARY=/target/lib/libvulkan.so
```

Windows는 `--platform windows`와 Windows C/C++23 툴체인, Vulkan 헤더 및 import
library를 전달한다. 대상용 LVUI 컴파일러를 빌드 호스트에서 실행하지 않도록
`--host-compiler`는 항상 빌드 호스트의 실행 파일을 지정한다. 네이티브 빌드에서는
`--toolchain`을 생략할 수 있다. `--build-root`는 iiGUI의 `build/` 안이어야 한다.

도구는 다음을 검증한다.

1. iiSDKBuilder가 iiSDKCore와 iiColorManager, 독립된 LVUI·iiGUI를 빌드·스테이징한 뒤 HelloWorld를 빌드한다.
   LVUI의 명시적 `iiColorManager::Values` 의존성도 같은 대상 키트에 포함한다.
2. 설치 키트를 별도 경로로 복사하고, 설치된 예제만으로 새 소비자를 구성·빌드한다.
   소비자가 authoring 소스 헤더 대신 설치된 SDK 헤더·CMake 패키지를 사용해야 한다.
3. 대상의 `HostWindows.cpp` 또는 `HostLinux.cpp`가 실제 컴파일 명령에 포함되고,
   설치된 Core의 `ApplicationEntry.cpp`가 최종 소비자에 컴파일되어야 한다.
4. 실행 파일 형식·아키텍처와 Windows GUI subsystem, Core 실행과 위젯 마운트
   심벌, Qt·인터프리터·LVUI 파서/컴파일러의 부재를 검사한다.
5. Pretendard·Inter 폰트와 라이선스 일곱 개의 원본 해시와 앱 리소스를 대조한다.
   Windows·Linux의 설치 앱은 `bin/resources/`에 같은 리소스를 동반한다.

결과는 `build/verify-aot-desktop/<platform>/runs/<UTC>/verification.json`에 기록한다.
`installedConsumerBuilt`, `installedConsumerExecuted`, `guiLaunched`,
`physicalDeviceTested`를 분리하며 빌드만으로 실행 성공을 표시하지 않는다.
빌드 호스트 검사 `tests/aot_desktop.py`는 두 대상의 실행 파일 판별, 외부 런타임
거부, 의존성 그래프와 리소스 변조 거부를 회귀 검사한다.

## iiVirtualizer 검토와 실행 범위

2026-10-26에 확인한 iiVirtualizer의 기본 QEMU 어댑터는 `-nodefaults -display none`으로
Linux + iiSDK 서비스 게스트를 부팅한다. 제공 이미지에는 데스크톱·X11·GUI GPU 경로가
없으며 Windows 게스트 이미지도 제공하지 않는다. 이 상태의 게스트 부팅은 이 예제의
Windows·Linux GUI 실행 증거로 사용할 수 없다. 이번 작업은 사용자 지시에 따라
VM·실기기의 GUI 실행을 하지 않고 소스·컴파일·링크·설치 소비자 경계를 검증한다.
모바일 호스트와 모바일 앱 설치·실행은 이 데스크톱 검사의 범위 밖이다.

## 2026-10-26 빌드 검증 결과

동일 LVUI 소스의 SHA-256은
`d2317b0b4f9a0328048423347a60689eb07ece5aced8f6d6b986c588b9fda108`이다.
세 대상에서 SDK 빌드, 설치 키트 이동 후 새 소비자 구성·컴파일·링크·설치,
Core 진입점과 폰트 일치, Qt·인터프리터·LVUI 파서 부재 검사가 통과하였다.

| 대상 | 실제 생성 형식 | 실행 검증 |
|---|---|---|
| Windows | x86_64 PE32+, Windows GUI subsystem | 사용자 지시로 수행하지 않음 |
| Linux | aarch64 ELF64 | 사용자 지시로 수행하지 않음 |
| macOS 회귀 빌드 | aarch64 Mach-O64 `.app` | 창을 실행하지 않음 |

Windows 교차 컴파일에서 `MSG.time`에 `GetMessageTime()`의 signed `LONG`을 직접
넣던 축소 변환을 `DWORD` 명시 변환으로 수정하였다. 테스트 제한 시간이나 기능 조건을
변경하지 않았다. 빌드 호스트의 AOT 도구 검사 8개와 기존 도구 검사 5개가 통과하였다.

집계는 `build/aot-desktop-20261026/summary.json`과 대상별 `runs/*/verification.json`이다.
이 기록은 앞의 운영체제·GPU 실행 조건을 대체하지 않는다. 별도로 요청받은 Android
실기기 검증은 [Android 최소 앱](AndroidHello.md)에 기록하였다.
