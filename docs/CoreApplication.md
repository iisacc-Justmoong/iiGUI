# Core-owned native widget applications


> Implementation update: the earlier AppKit-control path described below is
> superseded by [iiGUI's self-rendered C++ widgets](SelfRenderedWidgets.md).
> Core ownership and attachment lifetimes remain; OS controls/layout do not.

The native SDK GUI path now uses Core's `Application`, entrypoint and attachment
lifetime. iiGUI supplies an OS driver and native widgets, borrowing the exact
Core-owned EventLoop through HostOptions. Core includes no GUI/DSL dependency.
`attach_application` binds a shared Host to the Core driver; `view_attachment`
adapts a C++ widget object's attach/detach methods. Ordinary C++ widgets and
LVUI-generated C++ widget classes use the same contract.

`iisdkcore_add_application` supplies Core's installed `ApplicationEntry.cpp`,
links native C++ sources/SDK targets and attaches bundle resources. Application
code defines `ii::runtime::configure_application(Application&)` and configures
attachments instead of constructing another worker runtime or writing main().
Core owns arguments, event loop, shared workers, owned work scope, cooperative
stop and exit status. It activates attachments in order. After the native driver
returns, it stops/cancels/joins owned work before reverse deactivation. A closing
window revokes native handles; generated C++ widget instances retain imported
backends until Core detaches them. Unowned external jobs require their own lifetime
coordination. Native Host construction/run/destruction still require the UI thread.

## LVRS appearance with native semantics

`Appearance.h` defines encoded sRGB visual values independent of Qt. The checked-in
`lvrs_appearance()` snapshot applies the current 2% accent window fill, text roles,
button state colors, 22-point button height, 8-point padding/radius, and stack
alignment/spacing. Native buttons remain NSButton instances with AppKit input,
focus, accessibility and target/action; only visual drawing uses the LVRS values.
Labels remain NSTextField and layout remains NSStackView/Auto Layout.

The four unmodified Pretendard font faces and their upstream SIL OFL notice are
in `resources/native/`. `iiGUI_NATIVE_RESOURCES` exposes installed resource files
to Core's final executable helper. The macOS native backend registers the bundle's
fonts with CoreText; a missing face falls back to native system fonts. The actual
Hello World self-test requires the bundled Pretendard family and verifies it.
Design-source/font hashes are recorded in `resources/lvrs-native-appearance.json`
and tested for drift. LVRS/QML source is not a runtime dependency or bundled UI.

This is the implemented appearance for the bounded AOT widget set, not every
LVRS material, grain or animation. Windows/Linux native widget mounting and
general reactive property tracking remain outside this macOS implementation.

## Build and verify

First build/install the updated Core and LVUI into their `build/install` prefixes,
then configure iiGUI using those package directories and the existing Vulkan kit.
Build and run CTest in `iiGUI/build`. The standalone example and installed copy
both use the Core entrypoint.

```sh
cmake -DSOURCE="$PWD" -DBUILD_ROOT="$PWD/build/core-widget-consumer" \
  -DCORE_PREFIX=/path/to/iiSDKCore/build/install \
  -DLVUI_BUILD=/path/to/LVUI/build -DGUI_BUILD="$PWD/build" \
  -P tests/aot_package.cmake
```

The verifier requires a fresh destination. It relocates installed packages,
compiles the installed Core entry source into the consumer executable, checks
bundled font hashes and Qt/DSL-parser absence, then runs with the consumer source
and lvui-compile unavailable. Core Runtime and the existing Vulkan kit remain
native deployment dependencies; this verification does not implement distribution
signing, notarization or a fully self-contained bundle.
