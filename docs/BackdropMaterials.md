# Native GPU backdrop materials and authored widget metrics

`CanvasLayer::backdrop` applies a `BackdropMaterial` before the layer's panels,
text and images. Its rounded bounds, tint and optional inner border are logical
canvas coordinates; `Color` components remain linear-light sRGB. It uses the
same native surface as ordinary C++ widgets and creates no OS popup or control.

`BackdropSource::content` captures the base canvas before any overlay. This lets
a dialog blur application content without feeding its own dim layer, text or
previous popup pixels back into the blur. `previous_layers` captures the image
immediately before that layer. A zero blur radius retains a sharp image with
translucent tint; it is not an opaque replacement color. Radius values are
finite 0..64 logical units and correspond to a Gaussian sigma of radius/3.

## Metal implementation and reuse

The native Metal backend copies only the material region expanded by the blur
radius and clipped to the attachment. BGRA8 sRGB input is sampled into an
RGBA16Float linear blur destination by the system Metal Performance Shaders
Gaussian kernel. Rounded masking, tint and border are then composed on the GPU.
The normal rendering path has no CPU backdrop pixel readback.

Textures and kernels persist. A content material reuses its source and blur when
only overlay text, buttons or colors change. Changes to the base background,
panels, text, images, viewport or capture region invalidate the source; a sigma
change invalidates the blur. `previous_layers` recopies when a changed scene is
rendered because the accumulated image can have changed. Identical scenes still
skip rendering through the ordinary retained-canvas contract.

In the Metal implementation, the sum of backdrop and [box-shadow](Shadows.md) working textures is bounded to
64 MiB. A sharp backdrop crop uses
4 bytes/pixel; a blurred crop uses 4-byte source plus 8-byte destination pixels.
Removed materials release these textures. This budget excludes the ordinary
attachment, swapchain, native driver and foreground caches.
`GraphicsInfo::backdrop_supported` reports support. `backdrop_copies`,
`backdrop_blurs` and `backdrop_texture_bytes` expose encoded pass counts and
retained working storage, not a GPU timing or physical display measurement.

The Vulkan and Direct3D11 source backends also declare backdrop support and
implement capture and sampling in GPU effect shaders. They use full-viewport
snapshot textures, so the Metal-specific region allocation and reuse details
above must not be assumed for them. These source paths include Vulkan visual
tests; their presence does not by itself establish current device results or
pixel equivalence with Metal. Windows and Linux execution has not been verified
in this work. `set_canvas` admits a value snapshot; deferred flush or capture
reports a renderer failure rather than silently discarding the material.

## Stroke, text and layout contracts

`Panel::stroke_width == 0` fills its rounded rectangle. Positive values draw
only the inner ring with analytic antialiasing and linear-light alpha blending.
Metal, D3D11 and Vulkan shaders carry the same field in the existing 64-byte
panel record. The current macOS tests execute both Metal and Vulkan strokes;
the D3D11 implementation has not been run on this host.
Focused C++ buttons use the same resolved corner radius and a 1.5-unit inner
stroke, matching the authored LVRS FocusRing geometry without square corner
pixels. Full input-modality visibility, inherited focus customization and
focus motion remain part of the LVRS migration.

`TextRun::line_height` and `LabelOptions::line_height` select a fixed logical line
height when positive; zero preserves native font metrics. Label alignment,
optional color and 400/500/600/700 weight are explicit values. macOS measures
native layout fragments at the chosen width; Windows uses DirectWrite uniform
line spacing; Linux uses Pango absolute line height and requires Pango 1.50+ for
this option. Windows/Linux execution remains unverified here. The limit is
8192 logical units, and ordinary text-size and UTF-8 limits still apply.

`StackOptions::insets` optionally supplies distinct left/top/right/bottom edges;
otherwise `padding` keeps its original meaning. `ButtonStyle` supplies optional
state fills, text colors, border and line height. `PopoverOptions::fit_to_viewport`
allows a centered dialog to shrink below its minimum width, while the default
keeps the earlier minimum-width behavior. `opaque_base`, optional `blur_radius`,
border and corner limit let an owner select a material without changing popup
input, focus or lifetime rules. `ImageOptions::preserve_aspect` centers an image inside its padded area without
stretching; its default false preserves earlier image-view behavior.
Runtime setters invalidate the affected widget
layout or drawing; they do not rebuild its owning view.

흐림을 사용하지 않고 불투명 바탕을 선택한 팝업도 지정된 테두리를 바탕과 틴트
위에 그린다. 두께는 팝업의 짧은 변 절반으로 제한하며 두께나 색상 불투명도가
0이면 테두리를 생략한다. 헤드리스 `iiGUI.popup_motion_channels` 검사는 이
폴백과 테두리 해제를 확인한다. 실제 GPU 픽셀 검사는 포함하지 않는다.

팝오버는 재질의 안쪽 광택 불투명도와 세로 오프셋을 별도로 전달할 수 있다.
불투명도는 0–1, 오프셋은 유한한 논리 좌표여야 하며 기본 불투명도 0은 기존
출력을 보존한다. 반투명 재질 경로의 광택은 팝오버 이동 효과와 별개인 배경
재질에 그린다. 흐림이 꺼진 불투명 팝오버에서도 광택을 지정하면 이전 화면을
캡처하지 않는 불투명 재질을 사용한다. 잘못된 설정은 거절하고 이전 유효한
설정을 유지한다.
이 설정이 `PopoverOptions`에 추가되어 팝오버의 ABI 표식을 갱신했으므로 이전
헤더로 빌드한 호출자는 새 iiGUI 라이브러리와 함께 다시 빌드해야 한다.

팝오버는 선택적인 재질 밑그림 공급자를 받을 수 있다. 공급자는 실제 논리 크기와
화면 배율을 받아 불변 래스터를 반환하며, 재질은 이를 팝오버 경계 안에 같은 크기로
합성하고 지정된 색상을 적용한다. 공급 도중 위젯을 변경하는 재진입은 거절하고,
유효하지 않은 색상은 이전 설정을 유지한다. 공급 실패는 그리기 오류로 전달한다.
`set_popover_underlay_with_bounds`의 공급자는 팝오버의 로컬 논리 좌표로 밑그림
경계를 함께 반환해 팝오버 바깥의 블러 입력도 제공할 수 있다. 기존 공급자는
팝오버와 같은 경계를 유지한다. 유한하지 않거나 유효 범위를 벗어난 경계는
그리기 오류로 거절한다. 새 헤더는 `IIGUI_POPOVER_UNDERLAY_BOUNDS_API`를
공개하며 새 메서드를 사용하면 해당 심볼을 제공하는 라이브러리가 필요하다.
흐림이 꺼진 불투명 팝오버에도 밑그림 또는 광택이 지정되면 불투명 기본색 위에
재질을 합성하고 이전 화면은 캡처하지 않는다. 전환 중에는 카드와 함께 이동하고
축소되며 닫히는 동안에도 유지된다. 두 효과가 모두 없는 기존 불투명 팝오버의
단순 패널 경로는 그대로 둔다. 이 재질은 GPU 재질 합성 경로를 사용하며 소프트웨어
래스터러 지원을 뜻하지 않는다. 헤드리스 검사는 GPU 픽셀 동등성을 증명하지 않는다.

## 팝오버 재질 전환 계약

`Widget::set_popover_material_follows_content(true)`는 팝오버의 재질·그림자를
콘텐츠와 같은 변환 및 불투명도로 그린다. 이 방식은 흐림·틴트·테두리·안쪽 광택·경계 밖 밑그림을 하나의
재질로 유지하며, 열림과 퇴장 중에도 크기와 불투명도가 콘텐츠를 따른다. 선택하지
않은 기존 팝오버의 배경과 카드 분리 방식은 그대로 유지한다. 전환 도중 설정을
바꾸어도 콘텐츠나 입력 수명은 교체하지 않으며 다른 위젯 종류에 대한 설정은
거부한다. 헤더의 `IIGUI_POPOVER_COMBINED_MATERIAL_MOTION_API`로 지원 여부를
확인할 수 있고 새 메서드는 해당 심볼이 있는 라이브러리가 필요하다. 헤드리스
장면 검사와 GPU 인수 계산 검사는 실제 GPU 픽셀 동등성의 증거가 아니다.

## 채색된 팝오버 밑그림 계약

`Widget::set_popover_untinted_underlay_with_bounds`는 이미 채색된 sRGB RGBA
밑그림과 팝오버 로컬 경계를 전달한다. 기존 단색 틴트 공급자는 그대로 유지한다.
채색된 밑그림은 호스트가 RGB를
단색으로 바꾸지 않고 흐림 입력에 합성한다. 공급 실패·잘못된 경계·그리기 중
재진입은 기존 공급자와 같은 방식으로 처리하며, 틴트 방식과 채색된 방식 사이의
교체나 해제는 팝오버 콘텐츠와 수명을 바꾸지 않는다. 지원 여부는
`IIGUI_POPOVER_UNTINTED_UNDERLAY_API`로 확인할 수 있으며 새 메서드의 심볼을
제공하는 라이브러리가 필요하다. 공급자 전환·해제 및 재진입은 헤드리스 장면
검사로 확인하며 실제 GPU 픽셀 동등성은 별도 검증이 필요하다.

`tests/backdrop.cpp` contains validation, native blurred-pixel, overlay,
masking, tint, reuse, release and Vulkan rendering cases. The rendering cases
open native windows and were not run as part of this source and headless audit.
`tests/alert_primitives.cpp` verifies fixed line metrics, asymmetric layout,
transparent outlined button states, event order and Metal/Vulkan stroke pixels.
