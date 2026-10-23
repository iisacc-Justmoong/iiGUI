# Compiled native widgets and LVUI AOT

Composed transparent text editors, explicit option replacement and native
ellipsis are described in [CellEditors](CellEditors.md).

Retained vertical viewports, nested hardware clips, focus reveal and linear-stack
height allocation are described in [ScrollViews](ScrollViews.md). LVRS Sheet
uses this C++ path for a fixed header and a separately scrolling body.

> Implementation update: the earlier AppKit-control path described below is
> superseded by [iiGUI's self-rendered C++ widgets](SelfRenderedWidgets.md).
> Core ownership and attachment lifetimes remain; OS controls/layout do not.

The current application path is [CoreApplication.md](CoreApplication.md): generated
C++ widget instances, LVRS appearance and Core-owned app/entrypoint/attachment.
The direct factory API described below remains available for standalone hosts.

The working tree adds a C++23 native widget mount API in `iiGUI/NativeWidgets.h`.
The initial backend is macOS/AppKit. It directly creates `NSStackView`,
`NSTextField` and `NSButton` under the existing `NSWindow`. No Qt, source
interpreter, bytecode or GPU-painted imitation of these controls is involved.
The existing cross-platform window and GPU canvas APIs remain available.

`Host::mount_widgets(WidgetFactory)` runs a synchronous construction transaction
on the open host's main thread. A `WidgetBuilder` constructs `vstack`, `hstack`,
`label` and `button` immediately. An empty parent creates the unique root; only
stacks accept children. Every widget has a unique UTF-8 ID. Propagate builder
errors from the factory. Only a successful factory replaces existing content.
Construction exceptions become failed Results and preserve the old tree.

The returned weak `WidgetMount` finds controls by ID and snapshots actual native
classes, attached state and logical sizes. `Widget` updates text/button enabled
state or invokes the native control action. All operations require the owner
thread. Worker code delivers results through `UiExecutor`, never AppKit calls.
Auto Layout follows content width, native font metrics and stack spacing/padding;
root height is the natural content height. This API does not add scrolling.

Unmount, replacement and window close detach action targets, revoke handles and
release scope owners. Already-running callbacks retain their current action until
they return; they can unmount their own tree. Late handles cannot mutate a new
generation. Native callback exceptions pass through the existing host failure
barrier and return from `Host::run`. `WidgetBuilder::retain` keeps ordinary C++
scope owners alive even when no control captures them. User-owned asynchronous
work must still be cancelled/joined by its owner; it is not automatically tracked.

A successful native mount replaces a canvas. A successful `set_canvas` replaces
native widgets; rejected canvas input leaves them intact. `set_text` returns
busy while widgets are mounted; update the intended Widget instead.
`unmount_widgets` restores the host's basic label. Widget-only apps allocate no
Metal/Vulkan canvas, although this iiGUI package still links its renderer libraries.
Windows/Linux return unsupported for the new mount API. Their existing hosts
and renderers are not replaced by an unverified native widget implementation.

## Build the actual LVUI Hello World

Build/install the updated LVUI package into `LVUI/build/install` first, then:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DiiSDKCore_DIR=/path/to/iiSDKCore/lib/cmake/iiSDKCore \
  -DLVUI_DIR=/path/to/LVUI/build/install/lib/cmake/LVUI \
  -DVulkan_INCLUDE_DIR=/path/to/vulkan/include \
  -DVulkan_LIBRARY=/path/to/libMoltenVK.dylib \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
open build/examples/HelloWorld/HelloWorld.app
```

`examples/HelloWorld/HelloWorld.lvui` is compiled by installed `lvui-compile`
at build time into `HelloWorld.generated.h`. C++23 then compiles the generated
native construction calls and scoped backend callbacks. The app binary does
not call or contain the LVUI parser/compiler, and needs neither the source file
nor the build tool to run. The `.app` is also runnable directly with `--self-test`.
Its timer, background result 42, resize, two native button actions and normal
close are verified, including one backend construction/destruction per mount.

The legacy `examples/HelloLVUI` remains an explicitly separate runtime-parsed
GPU canvas demonstration. Embedding that earlier markup is not the AOT route.

## Installed-only consumer

```sh
cmake -DSOURCE="$PWD" -DBUILD_ROOT="$PWD/build/aot-consumer" \
  -DCORE_PREFIX=/path/to/installed/iiSDKCore \
  -DLVUI_BUILD=/path/to/LVUI/build -DGUI_BUILD="$PWD/build" \
  -P tests/aot_package.cmake
```

Use a fresh destination under `build/`. This stages the installed Core plus
updated LVUI/iiGUI, relocates the prefix, copies the installed example and builds
it using exported targets only. It audits Qt linkage and absence of LVUI parser
symbols, removes access to consumer sources and `lvui-compile` during execution,
then restores those test-owned paths. Loader override variables are cleared.
The Vulkan SDK/loader remains the same external native kit used by iiGUI.
The script writes logs, JUnit results and `verification.json` under BUILD_ROOT.
This is package consumption validation, not a fully self-contained distribution
bundle or a replacement of the user's default installed SDKs.

Verified on macOS arm64 / AppleClang 21 on 2026-10-22: LVUI 20/20,
iiGUI regression 35/35, focused ASan/UBSan 7/7 and relocated installed consumer
1/1. The actual app was also opened and its native Say Hello button was clicked
through the macOS accessibility interface; the label changed to Hello World · Cocoa!.
Evidence is in `build/aot-regression-tests.xml`,
`build/aot-sanitizer/build/aot-sanitizer-tests.xml` and `build/aot-consumer/`.
