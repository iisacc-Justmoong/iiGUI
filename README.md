# iiGUI

Custom-drawn containers can expose [accessibility semantics](docs/AccessibilitySemantics.md)
including an adjustable slider range without giving iiGUI ownership of the
application's component model.
The host package does not depend on LVUI. Applications can compose it with
LVUI's [document adapter](../LVUI/docs/NativeDocument.md) when needed.

The latest [native authoring verification record](../LVUI/docs/NativeReadinessVerification.md) distinguishes executed checks, installed-package proof, and platform limits from the historical implementation notes below.

[Host platform contracts](docs/HostPlatforms.md) cover a direct
Wayland/xdg-shell/Vulkan host, Android OS clipboard/URL services and Choreographer
frames, and an AOT WebAssembly pthread host with self-rendered WebGL2 widgets.
`host_capabilities()` exposes the implemented host contract. Core build targets
and GUI host availability are distinct; embedded support does not imply a window
system. [HelloHosts](examples/HelloHosts) exercises the common C++23/LVUI path.
Native builds and browser/device execution have separate verification evidence.

The minimal [Android LVUI Hello World](examples/HelloAndroid) uses NDK
NativeActivity, iiSDKCore and self-rendered Vulkan labels. The
[Android guide](docs/AndroidHello.md) states its supported label subset and the
separate build, installation, physical-device and emulator evidence. Emulator
installation requires an explicit `--device-kind emulator --serial emulator-NNNN`.

The AOT [Hello World](examples/HelloWorld) uses one LVUI source and one C++23
application on macOS, Windows and Linux. Its [desktop verification guide](docs/AOTDesktop.md)
covers native cross compilation, binary audits and relocated installed consumers
without launching a GUI or implying device runtime validation.

[Inferred LVUI values](docs/InferredValues.md) connect C++-typed values and functions
to retained widgets, reactive aliases and native replacement without a runtime parser.

[iPadOS hosts](docs/IPADOS.md) use UIKit scenes, a self-rendered C++ widget
surface, shared Metal/CoreText rendering, touch and Pencil input, and an optional
Vulkan surface through MoltenVK. The Congregation device build exercises Metal
and forced Vulkan independently. Full marked-text input and external drag and
drop remain outside this adapter's current support.

Native [container layout](docs/ContainerLayout.md) now supports minimum implicit
metrics, independent implicit/content insets and construction-time default slots.
Ordinary container disabling cancels descendant input, and native button option
replacement preserves callbacks and identity. Runtime verification is deferred.

[Retained stack layout](docs/StackLayout.md) validates and applies options,
extent and flex atomically, with independent overlay axes and owner-domain access.
LVUI-generated dynamic layouts reuse native measurement and preserve local editing
state; constant layouts are folded before the application runs.

[Action surfaces](docs/ActionSurfaces.md) add a transparent native child container
with text fallback, state foreground colors, keyboard actions and link semantics.
Text underline is shaped by the platform font service. New runtime validation is
deferred to the final batch; compilation and static package checks are separate.

Native [motion](docs/NativeMotion.md) shares Core display frames across app and game
updates, with on-demand idle behavior and live reduced-motion preferences. Metal
[shadows](docs/Shadows.md) and cached foreground scale/opacity composition support
the authored LVRS Alert transitions without OS control wrappers.


Native widgets now use iiGUI's own C++ layout, hit testing, focus, input state and
GPU drawing. Cocoa supplies one input/presentation surface and virtual
accessibility peers; OS control wrappers are no longer the widget implementation.
See [Self-rendered C++ widgets](docs/SelfRenderedWidgets.md) for the current
architecture and the LVRS framework migration boundary.

[Popovers](docs/Popovers.md) retain ordinary C++ widget content in the same GPU
overlay, with child-first Escape, modal/nonmodal outside presses, anchor placement
and generation-safe close/reopen behavior.

[Activation-owned content](docs/PopupContent.md) can build a fresh C++ view on
each open and retire it on close while keeping the original native GPU surface.
Generation-specific handles, native focus/IME and bounded accessibility caches
keep repeated activations independent from the outer application's lifetime.

[Generic content hosts](docs/ContentHosts.md) construct and atomically replace
independent page generations in that same surface, with scoped ownership,
viewport layout, input routing and a shared node budget. Runtime verification
of this new path is deferred to the final validation batch.

[Dialog layout](docs/DialogLayout.md) adds centered popup constraints, width-aware
stack alignment and flex sizing, authored button/image geometry, and distinct
close/cancel events for native dialog compositions such as LVRS Modal.

[GPU backdrop materials](docs/BackdropMaterials.md) add real Metal Gaussian
glass from underlying content, retained ROI textures, transparent tint and
rounded inner borders. Authored fixed line heights, asymmetric stack insets and
button state styles support LVRS Alert composition. Metal/Vulkan strokes are
pixel-tested; D3D11 stroke execution and non-Metal backdrop implementations remain
unverified or unfinished as documented.

[Core-owned widget applications](docs/CoreApplication.md) connect native C++ widgets
to iiSDKCore's app runtime, entrypoint and attachment lifecycle. LVUI emits concrete
C++ widget classes; LVRS provides appearance data/fonts; iiGUI supplies the OS driver
and its own C++ widgets using the same Core-owned event loop. Hello World now uses this
path and Core's final-executable/resource helper.

The working tree now also implements [compiled self-rendered widgets and LVUI AOT](docs/SelfRenderedWidgets.md):
`examples/HelloWorld/HelloWorld.lvui` generates C++23 during the build and mounts
iiGUI C++ stacks, labels and buttons on the native desktop GPU surface. Hello World
uses Core timers/workers, follows resize and closes normally. This AOT route does
not embed or parse LVUI source at runtime. The shared desktop sample targets
macOS, Windows and Linux; see the separate build and execution evidence in
[AOT desktop verification](docs/AOTDesktop.md).

[Windows build and execution](docs/WindowsBuildAndRun.md)

Package release: **0.4.1** (2026-10-19). This patch packages the current
source snapshot; build, test and installation evidence is recorded in the SDK
patch-release report. Earlier versioned feature notes remain historical.

Applications can additionally compose [LVUI 0.1](docs/LVUI.md) with iiGUI.
LVUI owns parsing and document layout; iiGUI provides text measurement and a GPU canvas.
The iiGUI package does not require `LVUI::LVUI`; OS hosts use Vulkan 1.1+ where selected and
the Web target uses WebGL2 without a Vulkan SDK. [Native rendering](docs/Renderers.md)
uses Metal/Direct3D first and Vulkan as the default fallback; Linux uses Vulkan.
Released 0.4.1 binaries do not include these additions until rebuilt. The version above is the last package release.

iiGUI 0.4.1 is a C++23 native desktop GUI host and retained GPU canvas above iiSDKCore 0.16. The same
public API and HelloGUI source build on Windows, macOS and Linux. Each backend
uses its platform window system directly. Qt, SDL, a WebView and a JavaScript engine
are not used. Standard C/C++ runtime libraries
and native system libraries remain normal deployment dependencies.

| Target | Private backend | Requirements |
|---|---|---|
| Windows | Win32, DirectWrite/Direct2D text, Direct3D 11 flip presentation, wake event/deadline timer | Windows 10 1703+; C++23 compiler/Windows SDK; D3D feature level 11.0 device; Vulkan loader/driver for fallback |
| macOS | AppKit/CoreText text, Metal/CAMetalLayer, CFRunLoop wake/deadline sources | Xcode/Command Line Tools; logged-in WindowServer; Metal GPU; Vulkan/MoltenVK for fallback |
| Linux | X11, Pango/Cairo text, Vulkan, eventfd/poll | X11/XWayland desktop, fonts, Vulkan graphics/presentation driver for canvas |

Linux currently uses X11. A Wayland desktop needs XWayland; there is no direct
Wayland backend. Linux derives a logical scale from the desktop's Xft.dpi
resource (96 DPI when absent), including live resource changes. This is one
X11 desktop scale, not Wayland per-output fractional scaling.
Windows uses 96-DPI logical units with per-monitor scaling; macOS uses AppKit
points. A cross build or binary inspection does not prove desktop execution.

## Scope

- Core HostLoop binds the native wake gate, pins Core dispatch to the UI owner,
  and delivers native channel readiness to the same thread. Core producer
  endpoints and direct TaskToken cancellation wake the host automatically.
  UiExecutor exposes watch/read/write alongside timers and ordinary tasks.
- Core FrameLoop access through the open Host::event_loop, structural timed
  TaskScope ownership, and Operation::then_on with an owned weak UI endpoint.
  Pending tracked-operation waits reject blocking on the connected host thread.
- One resizable standalone native window with read-only UTF-8 display text.
  AppKit provides selectable text; Windows/Linux paint a read-only text surface.
- Core EventLoop admission, priorities, cancellation, bounded queues and callback
  budgets. UiExecutor wakes an idle host for new work or changed deadlines.
  There is no permanent frame or polling timer.
- Core Runtime workers deliver results through a weak UiExecutor. All callbacks
  and native window mutations execute on the owning UI thread.
- Native close, close veto, exception-to-Result propagation and a close barrier
  rejecting late work. Normal shutdown returns through C++ main.
- Installed target `iiGUI::iiGUI`, a common HelloGUI source and a three-kit
  iiSDKBuilder manifest. LVUI document users include the LVUI adapter and link
  LVUI separately; iiGUI-only consumers need no LVUI package.
- Optional native GPU canvas with instanced rounded gradient panels, analytic
  antialiasing, linear-light alpha blending and sRGB attachments. Updates
  coalesce at the end of a Core dispatch; identical scenes reuse GPU storage.
  Pipelines/devices persist; no GPU allocation occurs for a text-only host.
- Adapter/API, verified hardware availability, device-pixel size, attachment
  bytes and render/presentation counters. A strict hardware policy rejects an
  unverified/software device. Explicit in-memory capture supports pixel tests.
- Native Unicode shaping and font fallback: AppKit/CoreText, DirectWrite, and
  Pango/Cairo. Linux no longer wraps text by repeatedly measuring UTF-8 prefixes.
- An optional application-side LVUI adapter owns document parsing and bounded
  row/column layout. `Host::set_view` accepts an app-supplied view extension;
  `lvui::native::layout_document` exposes logical ID bounds. HelloLVUI embeds
  a real `.lvui` source at build time.

Mobile/web hosts, direct Wayland, editable text/IME, general input, a widget
framework, custom-control accessibility, arbitrary vector/image drawing,
3D/compute rendering, HDR and ICC-managed content are outside this
release. Unsupported targets fail configuration. No GUI implementation or
reverse iiGUI dependency is added to iiSDKCore.

## Build and test

All hosts require CMake 3.31+, Ninja, a C++23 compiler with std::expected,
iiSDKCore Runtime, LVUI 0.1, Vulkan 1.1+ and Python 3.9+ for build-time checks. Python is not an
application dependency. On Debian-like systems the native development packages
are `libx11-dev`, `libpango1.0-dev`, `libcairo2-dev`, `libvulkan-dev`
and `pkg-config`, plus fonts such as `fonts-dejavu-core` and suitable CJK,
Arabic, Indic and emoji fonts. No GTK widget toolkit is used. Install CMake 3.31+
separately if the distribution ships an older version.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/path/to/iiSDKCore \
  -DLVUI_DIR=/path/to/LVUI/build/install/lib/cmake/LVUI \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
cmake --install build --prefix /path/to/iiGUI/build/install
```

Tests open native windows and run serially. They require an interactive target
desktop, including DISPLAY on Linux. Default installation is
`~/.local/SDK/iiGUI`; verification uses isolated prefixes under build/ and does
not replace global SDK packages.

## HelloGUI through iiSDKBuilder

```sh
iisdk-builder build --manifest examples/HelloGUI/iiSDKBuilder.json --platform host
```

The manifest contains Windows, macOS and Linux kits. `--platform host` selects
the native host. Without it all three are requested; foreign desktop kits need
explicit toolchains. Core and iiGUI are built before the common HelloGUI source.
Builder uses FileProvider for its own manifests and logs; HelloGUI links Core,
iiGUI, Vulkan and native system libraries. LVUI-based examples add LVUI explicitly.
See [renderer selection and deployment](docs/Renderers.md) for Vulkan SDK paths.

HelloGUI displays a 250 ms Core timer, a worker's computed result of 42, content
size, resize events, multilingual text and a GPU gradient scene. The C++23
main.cpp uses tracked Operation completion, TaskScope timed ownership and
FrameLoop prepare_present with no platform conditionals. Close
the window to finish; Command-Q on macOS and Alt-F4 on Windows also request the
same close-veto path. `HelloGUI --self-test` resizes to 840×540 logical units
and checks timer, worker, resize, native GPU readback and normal close before returning 0.

## Reproducible installed-consumer verification

From iiGUI, run the verifier with an installed Builder CLI:

```sh
python3 -B tools/verify_desktop.py --builder /path/to/iisdk-builder
```

On Windows use an initialized compiler environment with CMake/Ninja/Python:

```powershell
python -B tools/verify_desktop.py --builder C:/path/to/iisdk-builder.exe
```

The verifier builds through Builder, runs the host and HelloGUI tests, installs
Core/LVUI/iiGUI, moves their prefix and builds/runs a copied consumer against
those installed SDK packages plus the target's Vulkan/native system dependencies.
Logs, JUnit XML and verification.json are written to a fresh
run directory under build/. Repeated runs preserve older reports and prefixes.

`--compile-only` deliberately omits target execution. A foreign target requires
that flag plus an explicit toolchain:

```sh
python3 -B tools/verify_desktop.py --builder /path/to/iisdk-builder \
  --platform linux --toolchain /path/to/linux-toolchain.cmake --compile-only
```

The report then states `built_execution_deferred`; native runtime tests are
never marked passed. A cross toolchain must find target libraries and permit
the installed SDK prefix outside its sysroot, for example with
`CMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH`. Library/include root modes should
still select target libraries. Child builds clear inherited CPATH,
CPLUS_INCLUDE_PATH and LIBRARY_PATH to prevent host SDK contamination. A
toolchain build also clears inherited PKG_CONFIG_PATH; its toolchain must set
target PKG_CONFIG_SYSROOT_DIR/PKG_CONFIG_LIBDIR.
Additional kit cache entries use `--cache KEY=VALUE`.

For a separate package check:

```sh
cmake -DSOURCE=/path/to/iiGUI -DBUILD=/path/to/iiGUI/build/package-check \
  -DCORE_BUILD=/path/to/Core/build -DGUI_BUILD=/path/to/iiGUI/build \
  -DLVUI_BUILD=/path/to/LVUI/build \
  -P tests/package.cmake
```

For cross consumption add `-DTOOLCHAIN=/path/to/toolchain.cmake` and
`-DSKIP_EXECUTION=ON`. Existing relocated prefixes are preserved. See
[architecture](docs/Architecture.md), [usage](docs/Usage.md) and
[verification](docs/Verification.md), [GPU contracts](docs/Graphics.md) and
[Qt GUI comparison](docs/QtGuiComparison.md) for contracts and evidence boundaries.

## License

SPDX-License-Identifier: MIT

The original code and documentation in this repository are licensed under the
[MIT License](LICENSE). Third-party code, libraries, fonts, tools, and model
weights retain their respective licenses.

## Source header layout

Public headers and implementations live together in responsibility directories below `src/`.
There is no separate authored `include/` tree. Existing public `#include` names
and exported CMake targets are unchanged; installation still places only public
headers in the package include directory. Generated export headers belong to
`build/`, not to the authored source tree.

Validate this layout from the SDK workspace with
`python3 -B tests/test_source_header_layout.py`. Reconfigure, build, run CTest,
and verify a staged installed consumer after changing source/header paths.

Composed native buttons support asymmetric content insets, explicit indicator size,
content spacing and optional width. See [container layout](docs/ContainerLayout.md);
new layout test executables are compiled with runtime execution deferred.

Native image composition supports source crops, rounded masks, borders, scrims,
explicit foreground order and tightly allocated foreground textures. Pure retained
paint providers expose native action state; see [image composition](docs/ImageComposition.md).

See [native material inputs](docs/MaterialInputs.md) for immutable effect masks, preblur inputs and owned on-demand widget frame callbacks.

[TextIndex](docs/TextBoundary.md) supplies an immutable, window-free UTF-8/UTF-16 map and extended grapheme/word movement with sparse ASCII storage. Unicode sources and license hashes are pinned in the package; execution checks remain deferred.

[TextLine](docs/TextLine.md) shapes an unwrapped paragraph with native font services,
returns UTF-16 caret/hit positions and visual selection spans, and rasterizes only
the requested horizontal viewport. It constructs no OS text control.

[TextRasterCache](docs/TextRasterCache.md) keeps native RGB and color-font palette
pixels intact, bounds viewport images with an LRU, and reuses the same image when
only opacity changes. Color and opacity checks are compiled; execution is deferred.

[Custom text input providers](docs/TextInputProvider.md) expose bounded document
ranges and caret geometry to native IME services without whole-document snapshot
copies. Widgets share the host's Core event loop for owned timed work.

[Widget interaction observers](docs/WidgetInteraction.md) keep weak, independent
subscriptions to geometry, clipping and hover/press/focus metadata. Explicit
popup frames and input regions support a painted bubble tail while preserving
the origin's focus, click and pointer capture. Runtime checks remain deferred.

Related implementation contracts: [Retained popup effects](docs/RetainedPopupEffects.md).

[Retained Window properties](docs/WindowProperties.md) update native geometry and
base appearance through an owner-thread weak endpoint, stage candidate settings,
preserve user resizing and local edits, and isolate embedded content from window ownership.

Generated readonly C++ projections are covered by `iiGUI.native_readonly.*`,
including selection changes, native references, record state, retirement and preserved input.
Generated typed C++ View inputs are covered by `iiGUI.native_object_inputs.*`,
including shared computations, object replacement, null recovery, failed candidate
preservation, keyed rows, and editing state. See [native input contracts](docs/ObjectInputs.md).

Generated C++ getter dependencies are covered by `iiGUI.native_method_queries.*`,
including private observable state, nested and indexed receivers, conditional
dependency changes, failure recovery and reentrant selection replacement.
See [native method query contracts](docs/MethodQueries.md).

Native C++ visual providers can be composed with [NativeView](docs/NativeView.md).
Source identity retains edits and focus; candidate failures preserve the previous
view, and parent retirement closes weak endpoints. Existing native drawing,
input capture and frame data APIs remain the rendering path.

Source and adjacent headers are organized by responsibility under `src/`; installed `<iiGUI/...>` include paths remain unchanged. See [source layout](docs/SourceLayout.md).

<!-- workspace-docs:readme:start -->
## Documentation entry points

- Human overview: this English `README.md`.
- Agent instructions: [AGENT.md](AGENT.md).
- Human documentation: [documentation index](docs/README.md).
- Structured agent documentation: [agent index](docs/agent-index.json).
<!-- workspace-docs:readme:end -->
