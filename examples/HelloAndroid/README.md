# Native LVUI Hello World for Android

This minimal APK compiles LVUI into C++23 widgets, runs an iiSDKCore Application
on the NDK NativeActivity thread and renders iiGUI labels with Vulkan. It contains
no Java/Kotlin application classes, DEX, Qt, WebView or runtime LVUI compiler.

Build with `python3 tools/android_hello.py` from the iiGUI repository. Supply
`--sdk`, `--ndk`, `--jdk`, `--builder` and `--host-compiler` for the local tools.
Add `--install --serial <physical-device-serial>` to install and launch it.
The build tool defaults to building an APK without changing a connected device.

See the [Korean implementation and validation guide](../../docs/AndroidHello.md)
for the deliberately limited label backend and device evidence.
