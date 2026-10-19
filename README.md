# iiGUI

Package release: **0.4.1** (2026-10-19). This patch packages the current
source snapshot; build, test and installation evidence is recorded in the SDK
patch-release report. Earlier versioned feature notes remain historical.


iiGUI 0.4.1 is a C++23 native desktop GUI host and retained GPU canvas above iiSDKCore 0.16. The same
public API and HelloGUI source build on Windows, macOS and Linux. Each backend
uses its platform window system directly. Qt, SDL, a WebView, JavaScript engine
and an application interpreter are not used. Standard C/C++ runtime libraries
and native system libraries remain normal deployment dependencies.

| Target | Private backend | Requirements |
|---|---|---|
| Windows | Win32, DirectWrite/Direct2D text, Direct3D 11 flip presentation, wake event/deadline timer | Windows 10 1703+; C++23 compiler/Windows SDK; D3D feature level 11.0 device or permitted WARP fallback |
| macOS | AppKit/CoreText text, Metal/CAMetalLayer, CFRunLoop wake/deadline sources | Xcode/Command Line Tools; logged-in WindowServer; Metal GPU for canvas |
| Linux | X11, Pango/Cairo text, EGL/OpenGL ES 3, eventfd/poll | X11/XWayland desktop, fonts, EGL sRGB surface and GLES3 driver for canvas |

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
  iiSDKBuilder manifest. Existing `find_package(iiGUI 0.1)` consumers remain
  source compatible with the existing host API.
- Optional native GPU canvas with instanced rounded gradient panels, analytic
  antialiasing, linear-light alpha blending and sRGB attachments. Updates
  coalesce at the end of a Core dispatch; identical scenes reuse GPU storage.
  Pipelines/devices persist; no GPU allocation occurs for a text-only host.
- Adapter/API, verified hardware availability, device-pixel size, attachment
  bytes and render/presentation counters. A strict hardware policy rejects an
  unverified/software device. Explicit in-memory capture supports pixel tests.
- Native Unicode shaping and font fallback: AppKit/CoreText, DirectWrite, and
  Pango/Cairo. Linux no longer wraps text by repeatedly measuring UTF-8 prefixes.

Mobile/web hosts, direct Wayland, editable text/IME, general input, a widget
framework, custom-control accessibility, arbitrary vector/image drawing,
3D/compute rendering, HDR and ICC-managed content are outside this
release. Unsupported targets fail configuration. No GUI implementation or
reverse iiGUI dependency is added to iiSDKCore.

## Build and test

All hosts require CMake 3.31+, Ninja, a C++23 compiler with std::expected,
iiSDKCore Runtime and Python 3.9+ for build-time checks. Python is not an
application dependency. On Debian-like systems the native development packages
are `libx11-dev`, `libpango1.0-dev`, `libcairo2-dev`, `libegl-dev`, `libgles-dev`
and `pkg-config`, plus fonts such as `fonts-dejavu-core` and suitable CJK,
Arabic, Indic and emoji fonts. No GTK widget toolkit is used. Install CMake 3.31+
separately if the distribution ships an older version.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/path/to/iiSDKCore \
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
explicit toolchains. Core is built/installed first, then iiGUI, then the common
HelloGUI source. Builder uses FileProvider for its own manifests and logs;
HelloGUI links Core, iiGUI and native system libraries only.

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
Core/iiGUI, moves their prefix and builds/runs a copied consumer against only
those packages. Logs, JUnit XML and verification.json are written to a fresh
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
  -P tests/package.cmake
```

For cross consumption add `-DTOOLCHAIN=/path/to/toolchain.cmake` and
`-DSKIP_EXECUTION=ON`. Existing relocated prefixes are preserved. See
[architecture](docs/Architecture.md), [usage](docs/Usage.md) and
[verification](docs/Verification.md), [GPU contracts](docs/Graphics.md) and
[Qt GUI comparison](docs/QtGuiComparison.md) for contracts and evidence boundaries.
