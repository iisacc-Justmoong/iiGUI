cmake_minimum_required(VERSION 3.31)
project(iiGUI VERSION 0.5.0 LANGUAGES C CXX)
set(CMAKE_CXX_EXTENSIONS OFF)
set(IIGUI_LINUX_BACKEND "WAYLAND" CACHE STRING "Linux window system: WAYLAND (default), AUTO, or deprecated X11")
set_property(CACHE IIGUI_LINUX_BACKEND PROPERTY STRINGS WAYLAND AUTO X11)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_language(OBJCXX)
    set(_host_source src/Platform/HostIOS.mm)
    set(_graphics_source src/Graphics/CanvasMetal.mm)
    set(_text_source src/Text/TextMac.mm src/Text/TextLineMac.mm)
    set(_vulkan_window src/Graphics/VulkanWindowIOS.mm)
    set(_widgets_source src/Widgets/NativeWidgetsIOS.mm src/Widgets/UIKitTextInputIOS.mm)
    set(_desktop_source src/Platform/DesktopIOS.mm)
    set(_font_source src/Text/FontNativeMac.mm)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    enable_language(OBJCXX)
    set(_host_source src/Platform/Host.mm src/Platform/CocoaHostDelegate.mm)
    set(_graphics_source src/Graphics/CanvasMetal.mm)
    set(_text_source src/Text/TextMac.mm src/Text/TextLineMac.mm)
    set(_vulkan_window src/Graphics/VulkanWindowMac.mm)
    set(_widgets_source src/Widgets/NativeWidgetsMac.mm)
    set(_desktop_source src/Platform/DesktopMac.mm)
    set(_font_source src/Text/FontNativeMac.mm)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(_host_source src/Platform/HostWindows.cpp)
    set(_graphics_source src/Graphics/CanvasWindows.cpp)
    set(_text_source src/Text/TextWindows.cpp src/Text/TextLineWindows.cpp)
    set(_vulkan_window src/Graphics/VulkanWindowWindows.cpp)
    set(_widgets_source src/Widgets/NativeWidgetsWindows.cpp)
    set(_desktop_source src/Platform/DesktopWindows.cpp src/Platform/NativeDialogsWindows.cpp)
    set(_font_source src/Text/FontNativeWindows.cpp)
elseif(ANDROID)
    set(_host_source src/Platform/HostAndroid.cpp src/Platform/AndroidIme.cpp)
    set(_graphics_source)
    set(_text_source src/Text/TextAndroid.cpp src/Text/TextLineAndroid.cpp src/Text/AndroidLabelFont.cpp)
    set(_vulkan_window src/Graphics/VulkanWindowAndroid.cpp)
    set(_widgets_source src/Widgets/NativeWidgetsAndroid.cpp)
    set(_desktop_source src/Platform/DesktopAndroid.cpp)
    set(_font_source src/Text/FontNativeAndroid.cpp)
elseif(EMSCRIPTEN)
    set(_host_source src/Platform/HostWeb.cpp)
    set(_text_source src/Text/TextWeb.cpp)
    set(_widgets_source src/Widgets/NativeWidgetsWeb.cpp)
    set(_desktop_source src/Platform/DesktopWeb.cpp)
    set(_font_source src/Text/FontNativeWeb.cpp)
    set(_graphics_source src/Graphics/CanvasWeb.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(NativeGraphics REQUIRED IMPORTED_TARGET pangocairo)
    pkg_check_modules(NativeFonts REQUIRED IMPORTED_TARGET fontconfig pangoft2)
    pkg_check_modules(NativeDesktop REQUIRED IMPORTED_TARGET gio-2.0)
    string(TOUPPER "${IIGUI_LINUX_BACKEND}" IIGUI_LINUX_BACKEND)
    if(NOT IIGUI_LINUX_BACKEND STREQUAL "X11")
        pkg_check_modules(Wayland QUIET IMPORTED_TARGET wayland-client>=1.20 wayland-cursor>=1.20 xkbcommon>=1.0)
    endif()
    include(cmake/iiGUILinuxBackend.cmake)
    iigui_select_linux_backend("${IIGUI_LINUX_BACKEND}" "${Wayland_FOUND}" IIGUI_LINUX_BACKEND)
    if(IIGUI_LINUX_BACKEND STREQUAL "WAYLAND")
        set(_host_source src/Platform/HostWayland.cpp src/Platform/WaylandConnection.cpp)
        set(_widgets_source src/Widgets/NativeWidgetsWayland.cpp)
        set(_vulkan_window src/Graphics/VulkanWindowWayland.cpp)
        set(_desktop_source src/Platform/DesktopWayland.cpp src/Platform/NativeDialogsLinux.cpp)
    else()
        find_package(X11 REQUIRED COMPONENTS Xi OPTIONAL_COMPONENTS Xrandr)
        set(_host_source src/Platform/HostLinux.cpp)
        set(_vulkan_window src/Graphics/VulkanWindowLinux.cpp)
        set(_widgets_source src/Widgets/NativeWidgetsLinux.cpp)
        set(_desktop_source src/Platform/DesktopLinux.cpp src/Platform/NativeDialogsLinux.cpp)
    endif()
    set(_graphics_source)
    list(APPEND _widgets_source src/Widgets/AtSpiDbus.cpp src/Widgets/AtSpiSession.cpp)
    set(_text_source src/Text/TextLinux.cpp src/Text/TextLineLinux.cpp)
    set(_font_source src/Text/FontNativeLinux.cpp)
else()
    message(FATAL_ERROR "No iiGUI window-system host for ${CMAKE_SYSTEM_NAME}; Core-only/embedded targets do not imply GUI support")
endif()
include(GNUInstallDirs)
if(EMSCRIPTEN)
    set(iiGUI_HOST_BACKEND web)
elseif(ANDROID)
    set(iiGUI_HOST_BACKEND android)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(iiGUI_HOST_BACKEND cocoa)
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(iiGUI_HOST_BACKEND uikit)
elseif(WIN32)
    set(iiGUI_HOST_BACKEND win32)
elseif(IIGUI_LINUX_BACKEND STREQUAL "WAYLAND")
    set(iiGUI_HOST_BACKEND wayland)
else()
    set(iiGUI_HOST_BACKEND x11)
endif()
include(CMakePackageConfigHelpers)
include(GenerateExportHeader)
include(CTest)
include(cmake/SourceHeaders.cmake)
include(cmake/iiGUIFonts.cmake)
set(_iigui_user_home "$ENV{HOME}")
if(WIN32 AND _iigui_user_home STREQUAL "")
    set(_iigui_user_home "$ENV{USERPROFILE}")
endif()
if(PROJECT_IS_TOP_LEVEL AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND NOT _iigui_user_home STREQUAL "")
    set(CMAKE_INSTALL_PREFIX "${_iigui_user_home}/.local/SDK/iiGUI" CACHE PATH "SDK install prefix" FORCE)
endif()
option(BUILD_SHARED_LIBS "Build a shared iiGUI library" ON)
if(EMSCRIPTEN AND BUILD_SHARED_LIBS)
    set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)
endif()
option(IIGUI_SANITIZE "Enable address/undefined behavior sanitizers" OFF)
set(IIGUI_LVUI_COMPILER "" CACHE FILEPATH "Optional host-native LVUI AOT compiler for iiGUI integration tests")
set(IIGUI_LVUI_TEST_INCLUDE "" CACHE PATH "Optional LVUI generated headers for application integration tests")
set(IIGUI_LVUI_TEST_LIBRARY "" CACHE FILEPATH "Optional LVUI native library for application integration tests")
find_package(iiSDKCore 0.16 CONFIG REQUIRED COMPONENTS Runtime)
if(EMSCRIPTEN AND DEFINED IISDKCORE_WASM_THREADS AND NOT IISDKCORE_WASM_THREADS)
    message(FATAL_ERROR "iiGUI Web requires iiSDKCore built with IISDKCORE_WASM_THREADS=ON")
endif()
if(EMSCRIPTEN)
    get_target_property(_iigui_core_options iiSDKCore::iiSDKCore INTERFACE_COMPILE_OPTIONS)
    if(NOT "-pthread" IN_LIST _iigui_core_options)
        message(FATAL_ERROR "iiGUI Web Core Runtime must export -pthread; install a compatible threaded Core package")
    endif()
endif()
if(NOT EMSCRIPTEN)
    find_package(Vulkan 1.1 REQUIRED)
endif()
include(cmake/iiGUISvg.cmake)

# Scene validation and explicit painting have no operating-system host or GPU
# backend dependency; applications may link this boundary without a window.
add_library(iiGUIPainter src/Graphics/Graphics.cpp src/Graphics/Painter.cpp)
add_library(iiGUI::Painter ALIAS iiGUIPainter)
set_target_properties(iiGUIPainter PROPERTIES EXPORT_NAME Painter CXX_EXTENSIONS OFF
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES VERSION ${PROJECT_VERSION} SOVERSION 1)
target_compile_features(iiGUIPainter PUBLIC cxx_std_23)
target_link_libraries(iiGUIPainter PUBLIC iiSDKCore::Runtime)
target_include_directories(iiGUIPainter PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
generate_export_header(iiGUIPainter BASE_NAME IIGUI_PAINTER
    EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/iiGUI/PainterExport.h")
if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(iiGUIPainter PUBLIC IIGUI_PAINTER_STATIC_DEFINE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set_target_properties(iiGUIPainter PROPERTIES FRAMEWORK TRUE FRAMEWORK_VERSION A
        MACOSX_FRAMEWORK_IDENTIFIER "com.iisacc.iiGUI.Painter")
elseif(EMSCRIPTEN)
    target_compile_options(iiGUIPainter PRIVATE -fPIC -sSIDE_MODULE=1)
    target_link_options(iiGUIPainter PRIVATE -sSIDE_MODULE=1)
elseif(APPLE)
    set_property(TARGET iiGUIPainter APPEND PROPERTY INSTALL_RPATH "@loader_path")
elseif(UNIX)
    set_property(TARGET iiGUIPainter APPEND PROPERTY INSTALL_RPATH "$ORIGIN")
endif()

add_library(iiGUI src/Core/Dispatch.cpp src/Graphics/SvgImageCache.cpp src/Text/Text.cpp src/Graphics/NativeCanvas.cpp src/Platform/HostCapabilities.cpp
    src/Widgets/ColorButton.cpp
    src/Widgets/NativeWidgets.cpp src/Widgets/WidgetState.cpp src/Widgets/ColorPicker.cpp src/Widgets/ColorSurface.cpp src/Graphics/Image.cpp src/Graphics/Png.cpp src/Platform/Foreground.cpp src/Core/Application.cpp src/Presentation/Appearance.cpp src/Presentation/LegacyAppearance.cpp ${_widgets_source}
    ${_host_source} ${_graphics_source} ${_text_source} ${_vulkan_window} ${_desktop_source})
add_library(iiGUI::iiGUI ALIAS iiGUI)
set_source_files_properties(src/Widgets/AtSpiDbus.cpp PROPERTIES COMPILE_DEFINITIONS "IIGUI_VERSION_STRING=\"${PROJECT_VERSION}\"")
target_sources(iiGUI PRIVATE src/Presentation/Motion.cpp src/Presentation/Presentation.cpp)
target_sources(iiGUI PRIVATE src/Widgets/NativeView.cpp)
target_sources(iiGUI PRIVATE src/Text/FontDatabase.cpp src/Text/FontMetadata.cpp ${_font_source})
target_include_directories(iiGUI PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_sources(iiGUI PRIVATE src/Text/TextRasterCache.cpp)
target_sources(iiGUI PRIVATE src/Widgets/PaintView.cpp src/Widgets/WidgetSurfaceNative.cpp)
target_sources(iiGUI PRIVATE src/Widgets/WidgetInteraction.cpp)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/utf8proc/provenance.json" _iigui_unicode_manifest)
foreach(_name utf8proc.c utf8proc.h utf8proc_data.c LICENSE.md)
    set(_iigui_unicode_found FALSE)
    string(JSON _iigui_unicode_count LENGTH "${_iigui_unicode_manifest}" files)
    math(EXPR _iigui_unicode_last "${_iigui_unicode_count}-1")
    foreach(_index RANGE 0 ${_iigui_unicode_last})
        string(JSON _path GET "${_iigui_unicode_manifest}" files ${_index} file)
        if(_path STREQUAL "utf8proc/${_name}")
            set(_iigui_unicode_found TRUE)
            string(JSON _expected GET "${_iigui_unicode_manifest}" files ${_index} sha256)
            file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/utf8proc/${_name}" _actual)
            if(NOT _actual STREQUAL _expected)
                message(FATAL_ERROR "Pinned Unicode source changed: ${_name}")
            endif()
        endif()
    endforeach()
    if(NOT _iigui_unicode_found)
        message(FATAL_ERROR "Pinned Unicode source is absent from its manifest: ${_name}")
    endif()
endforeach()
target_sources(iiGUI PRIVATE src/Text/TextBoundary.cpp src/Text/TextLine.cpp src/third_party/utf8proc/utf8proc.c)
target_compile_definitions(iiGUI PRIVATE UTF8PROC_STATIC)
set_target_properties(iiGUI PROPERTIES C_VISIBILITY_PRESET hidden C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS OFF)
target_compile_features(iiGUI PUBLIC cxx_std_23)
target_link_libraries(iiGUI PUBLIC iiSDKCore::Runtime iiGUI::Painter)
if(NOT EMSCRIPTEN)
    target_sources(iiGUI PRIVATE src/Graphics/VulkanCanvas.cpp src/Graphics/VulkanPaintDevice.cpp)
    # VulkanPaintDevice exposes VkInstance and VkSurfaceKHR in its public API.
    target_link_libraries(iiGUI PUBLIC Vulkan::Vulkan)
endif()
target_link_libraries(iiGUI PRIVATE lunasvg::lunasvg)
target_include_directories(iiGUI SYSTEM PRIVATE "${lunasvg_SOURCE_DIR}/plutovg/source")
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_link_libraries(iiGUI PRIVATE "-framework UIKit" "-framework Foundation" "-framework CoreGraphics" "-framework IOSurface" "-framework MetalKit" "-framework UniformTypeIdentifiers" "-framework CoreText" "-framework CoreFoundation" "-framework Metal" "-framework MetalPerformanceShaders" "-framework QuartzCore")
    set_source_files_properties(${_host_source} ${_graphics_source} ${_text_source} ${_vulkan_window} ${_widgets_source} ${_desktop_source} ${_font_source} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_link_libraries(iiGUI PRIVATE "-framework AppKit" "-framework CoreText" "-framework CoreFoundation" "-framework CoreGraphics" "-framework Metal" "-framework MetalPerformanceShaders" "-framework QuartzCore")
    set_source_files_properties(src/Platform/Host.mm src/Platform/CocoaHostDelegate.mm src/Widgets/NativeWidgetsMac.mm src/Platform/DesktopMac.mm src/Graphics/CanvasMetal.mm src/Text/TextMac.mm src/Text/TextLineMac.mm src/Graphics/VulkanWindowMac.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    set_source_files_properties(${_font_source} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
elseif(WIN32)
    target_sources(iiGUI PRIVATE src/Widgets/WindowsAccessibility.cpp src/Graphics/Direct3DPaintDevice.cpp)
    target_link_libraries(iiGUI PRIVATE user32 gdi32 d3d11 dxgi d3dcompiler d2d1 dwrite windowscodecs ole32 oleacc oleaut32 imm32 shell32)
    target_compile_definitions(iiGUI PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
elseif(ANDROID)
    target_link_libraries(iiGUI PRIVATE android log dl jnigraphics)
    target_include_directories(iiGUI PRIVATE "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue" "${lunasvg_SOURCE_DIR}/plutovg/include")
elseif(EMSCRIPTEN)
    # Core's native worker and shutdown contracts remain on a pthread. The
    # browser main thread only owns DOM/OS bridges; widgets are C++/WebGL2.
    target_compile_options(iiGUI PUBLIC -pthread)
    foreach(_codec lunasvg plutovg)
        # Every object linked into shared WebAssembly memory needs atomics and
        # bulk-memory features, including otherwise platform-neutral codecs.
        target_compile_options(${_codec} PRIVATE -pthread -fexceptions)
    endforeach()
    target_link_options(iiGUI PUBLIC -pthread -sPROXY_TO_PTHREAD=1 -sPTHREAD_POOL_SIZE=5
        -sOFFSCREENCANVAS_SUPPORT=1 "-sOFFSCREENCANVASES_TO_PTHREAD=#canvas"
        -sASYNCIFY=1 -sASYNCIFY_STACK_SIZE=131072
        "-sASYNCIFY_IMPORTS=web_wait,register_font,await_clipboard" -sALLOW_MEMORY_GROWTH=1
        -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sEXIT_RUNTIME=0)
elseif(IIGUI_LINUX_BACKEND STREQUAL "WAYLAND")
    include(cmake/iiGUIWayland.cmake)
    target_compile_definitions(iiGUI PRIVATE IIGUI_WAYLAND=1)
    target_link_libraries(iiGUI PRIVATE PkgConfig::Wayland PkgConfig::NativeGraphics PkgConfig::NativeDesktop PkgConfig::NativeFonts)
else()
    target_link_libraries(iiGUI PRIVATE X11::X11 X11::Xi PkgConfig::NativeGraphics PkgConfig::NativeDesktop PkgConfig::NativeFonts)
    if(TARGET X11::Xrandr)
        set(IIGUI_HAS_XRANDR ON)
        target_link_libraries(iiGUI PRIVATE X11::Xrandr)
        target_compile_definitions(iiGUI PRIVATE IIGUI_XRANDR=1)
    endif()
endif()
if(APPLE)
    target_sources(iiGUI PRIVATE src/Graphics/MetalPaintDevice.mm)
    set_source_files_properties(src/Graphics/MetalPaintDevice.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
endif()
target_include_directories(iiGUI PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
generate_export_header(iiGUI BASE_NAME IIGUI
    EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/iiGUI/Export.h")

# This adapter lives outside the painter boundary and is linked by an app that
# chooses the existing native host and its GPU canvas implementation.
add_library(iiGUIHostBridge src/Platform/HostFramePaintDevice.cpp)
add_library(iiGUI::HostBridge ALIAS iiGUIHostBridge)
set_target_properties(iiGUIHostBridge PROPERTIES EXPORT_NAME HostBridge CXX_EXTENSIONS OFF
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES VERSION ${PROJECT_VERSION} SOVERSION 1)
target_compile_features(iiGUIHostBridge PUBLIC cxx_std_23)
target_include_directories(iiGUIHostBridge PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(iiGUIHostBridge PUBLIC iiGUI::iiGUI)
generate_export_header(iiGUIHostBridge BASE_NAME IIGUI_HOST_BRIDGE
    EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/iiGUI/HostBridgeExport.h")
if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(iiGUIHostBridge PUBLIC IIGUI_HOST_BRIDGE_STATIC_DEFINE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set_target_properties(iiGUIHostBridge PROPERTIES FRAMEWORK TRUE FRAMEWORK_VERSION A
        MACOSX_FRAMEWORK_IDENTIFIER "com.iisacc.iiGUI.HostBridge")
elseif(EMSCRIPTEN)
    target_compile_options(iiGUIHostBridge PRIVATE -fPIC -sSIDE_MODULE=1)
    target_link_options(iiGUIHostBridge PRIVATE -sSIDE_MODULE=1)
elseif(APPLE)
    set_property(TARGET iiGUIHostBridge APPEND PROPERTY INSTALL_RPATH "@loader_path")
elseif(UNIX)
    set_property(TARGET iiGUIHostBridge APPEND PROPERTY INSTALL_RPATH "$ORIGIN")
endif()
if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(iiGUI PUBLIC IIGUI_STATIC_DEFINE)
endif()
if(MSVC)
    target_compile_options(iiGUI PRIVATE /W4 PUBLIC /Zc:__cplusplus /utf-8)
else()
    target_compile_options(iiGUI PRIVATE -Wall -Wextra -Wpedantic)
endif()
set_target_properties(iiGUI PROPERTIES CXX_EXTENSIONS OFF OBJCXX_EXTENSIONS OFF
    OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED YES
    CXX_VISIBILITY_PRESET hidden OBJCXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES VERSION ${PROJECT_VERSION} SOVERSION 1)
if(BUILD_SHARED_LIBS)
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set_target_properties(iiGUI PROPERTIES FRAMEWORK TRUE FRAMEWORK_VERSION A
            MACOSX_FRAMEWORK_IDENTIFIER "com.iisacc.iiGUI")
    elseif(EMSCRIPTEN)
        target_compile_options(iiGUI PRIVATE -fPIC -sSIDE_MODULE=1)
        target_link_options(iiGUI PRIVATE -sSIDE_MODULE=1)
    elseif(APPLE)
        set_property(TARGET iiGUI APPEND PROPERTY INSTALL_RPATH "@loader_path")
    elseif(UNIX)
        set_property(TARGET iiGUI APPEND PROPERTY INSTALL_RPATH "$ORIGIN")
    endif()
    if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
        add_test(NAME iiGUI.shared_type COMMAND ${CMAKE_COMMAND}
            "-DTARGET_TYPE=$<TARGET_PROPERTY:iiGUI,TYPE>" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/shared_target_type.cmake")
        add_test(NAME iiGUI.internal_svg_linkage COMMAND ${CMAKE_COMMAND}
            "-DLUNA_TYPE=$<TARGET_PROPERTY:lunasvg,TYPE>" "-DPLUTO_TYPE=$<TARGET_PROPERTY:plutovg,TYPE>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/internal_svg_linkage.cmake")
    endif()
endif()
if(WIN32 AND BUILD_SHARED_LIBS)
    set_target_properties(iiGUI PROPERTIES OUTPUT_NAME "iiGUI-abi1")
endif()
if(IIGUI_SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "IIGUI_SANITIZE requires Clang/GCC address and undefined behavior sanitizers")
    endif()
    target_compile_options(iiGUI PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(iiGUI PUBLIC -fsanitize=address,undefined)
    target_compile_options(iiGUIPainter PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(iiGUIPainter PUBLIC -fsanitize=address,undefined)
    target_compile_options(iiGUIHostBridge PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(iiGUIHostBridge PUBLIC -fsanitize=address,undefined)
    foreach(_codec lunasvg plutovg)
        target_compile_options(${_codec} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    endforeach()
endif()

if(ANDROID)
    set(_iigui_android_glue "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")
    if(NOT EXISTS "${_iigui_android_glue}/android_native_app_glue.c" OR
       NOT EXISTS "${_iigui_android_glue}/NOTICE" OR NOT EXISTS "${CMAKE_ANDROID_NDK}/NOTICE")
        message(FATAL_ERROR "Android iiGUI requires NDK native_app_glue and its original notices")
    endif()
    add_library(iiGUIAndroidEntry STATIC src/Platform/AndroidApplicationEntry.cpp
        "${_iigui_android_glue}/android_native_app_glue.c")
    add_library(iiGUI::AndroidEntry ALIAS iiGUIAndroidEntry)
    set_target_properties(iiGUIAndroidEntry PROPERTIES EXPORT_NAME AndroidEntry
        POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
    target_compile_features(iiGUIAndroidEntry PUBLIC cxx_std_23)
    target_include_directories(iiGUIAndroidEntry PRIVATE "${_iigui_android_glue}")
    target_link_libraries(iiGUIAndroidEntry PUBLIC iiGUI::iiGUI PRIVATE android log)
    target_link_options(iiGUIAndroidEntry INTERFACE "LINKER:-u,ANativeActivity_onCreate")
endif()

set(_package "${CMAKE_INSTALL_LIBDIR}/cmake/iiGUI")
set(_painter_package "${CMAKE_INSTALL_LIBDIR}/cmake/iiGUIPainter")
install(TARGETS iiGUIPainter EXPORT iiGUIPainterTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(EXPORT iiGUIPainterTargets NAMESPACE iiGUI:: DESTINATION "${_painter_package}")
configure_package_config_file(cmake/iiGUIPainterConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/iiGUIPainterConfig.cmake" INSTALL_DESTINATION "${_painter_package}")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/iiGUIPainterConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/iiGUIPainterConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/iiGUIPainterConfigVersion.cmake" DESTINATION "${_painter_package}")
install(TARGETS iiGUI EXPORT iiGUITargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS iiGUIHostBridge EXPORT iiGUITargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR})
if(ANDROID)
    install(TARGETS iiGUIAndroidEntry EXPORT iiGUITargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES src/Platform/AndroidImeBridge.java src/Platform/AndroidImeBridge.pro
        DESTINATION "${CMAKE_INSTALL_DATADIR}/iiGUI/android")
    install(FILES "${_iigui_android_glue}/NOTICE" DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/iiGUI"
        RENAME AndroidNativeAppGlue-NOTICE)
    install(FILES "${CMAKE_ANDROID_NDK}/NOTICE" DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/iiGUI"
        RENAME AndroidNDK-NOTICE)
endif()
install(FILES
    src/Core/Bindings.h
    src/Text/FontDatabase.h
    src/Graphics/Image.h
    src/Graphics/SvgImageCache.h
    src/Presentation/Presentation.h
    src/Graphics/Graphics.h src/Graphics/Painter.h src/Graphics/MetalPaintDevice.h src/Graphics/Direct3DPaintDevice.h src/Text/Text.h src/Text/TextBoundary.h src/Text/TextLine.h src/Text/TextRasterCache.h src/Widgets/WidgetInteraction.h
    src/Platform/Host.h src/Platform/HostFramePaintDevice.h src/Platform/WindowGeometry.h
    src/Widgets/NativeWidgets.h src/Widgets/PaintView.h
    src/Widgets/PanelPainter.h
    src/Widgets/Divider.h
    src/Widgets/NativeDivider.h
    src/Widgets/ImageModel.h
    src/Widgets/AvatarModel.h
    src/Widgets/SheetModel.h
    src/Widgets/ButtonModel.h
    src/Widgets/DropdownButtonModel.h
    src/Widgets/NativeDropdownButtonView.h
    src/Widgets/StepperModel.h
    src/Widgets/NativeStepperView.h
    src/Widgets/SegmentedControlModel.h
    src/Widgets/NativeSegmentedControlView.h
    src/Widgets/CheckBoxModel.h
    src/Widgets/RadioButtonModel.h
    src/Widgets/RadioGroup.h
    src/Widgets/NativeRadioButtonView.h
    src/Widgets/TableViewGroup.h
    src/Widgets/TableHeaderPresentation.h
    src/Widgets/NativeCheckBoxView.h
    src/Widgets/ToggleSwitchModel.h
    src/Widgets/NativeToggleSwitchView.h
    src/Widgets/ModalModel.h
    src/Widgets/ModalPresentation.h
    src/Widgets/MenuModel.h
    src/Widgets/NativeMenuView.h
    src/Graphics/MaterialGradient.h
    src/Graphics/MaterialModel.h
    src/Widgets/NativeMaterialView.h
    src/Widgets/PopoverModel.h
    src/Widgets/NativePopoverView.h
    src/Widgets/NativePageRouterView.h
    src/Widgets/SliderModel.h
    src/Widgets/NativeSliderView.h
    src/Widgets/NativeSearchFieldView.h
    src/Widgets/NativeTableCellItemView.h
    src/Widgets/TableCellItemModel.h
    src/Widgets/TableRowViewModel.h
    src/Widgets/NativeTableRowView.h
    src/Widgets/NativeColorPickerValuesView.h
    src/Widgets/NativeColorChannelView.h
    src/Widgets/ColorPickerLayout.h
    src/Widgets/NativeColorPickerView.h
    src/Widgets/MobileNavigationModel.h
    src/Widgets/MobileNavigationMotion.h
    src/Widgets/NativeMobileNavigationView.h
    src/Widgets/NativeHierarchyItemView.h
    src/Widgets/HierarchyItemModel.h
    src/Widgets/TableHeaderModel.h
    src/Widgets/NativeTableHeaderView.h
    src/Widgets/NativeTableView.h
    src/Widgets/TableViewport.h
    src/Widgets/HierarchyToolbarModel.h
    src/Widgets/NativeHierarchyToolbarView.h
    src/Widgets/PanelLayout.h
    src/Widgets/NativeHierarchyPanelView.h
    src/Widgets/NativeListPanelView.h
    src/Widgets/NativeListView.h
    src/Widgets/ListViewport.h
    src/Widgets/ListDrag.h
    src/Widgets/NativeListItemCompositeView.h
    src/Widgets/ListItemDetailActionsLayout.h
    src/Widgets/ListItemDetailControlsLayout.h
    src/Widgets/ListItemTaskLayout.h
    src/Widgets/ListItemPreviewRowLayout.h
    src/Widgets/ListItemFormLayout.h
    src/Widgets/ListItemQuantityLayout.h
    src/Widgets/ListItemMenuModel.h
    src/Widgets/CollectionNavigation.h
    src/Widgets/VirtualViewport.h
    src/Widgets/VirtualLineIndex.h
    src/Widgets/VirtualCollectionPlanner.h
    src/Widgets/VirtualMeasurements.h
    src/Widgets/CodeViewport.h
    src/Widgets/FocusRing.h
    src/Widgets/NativeFocusRingView.h
    src/Widgets/MobileTabPainter.h
    src/Widgets/NativeMobileTabView.h
    src/Widgets/WindowNoiseTexture.h
    src/Widgets/NativeWindowNoiseTextureView.h
    src/Widgets/NativeWheelScrollGuard.h
    src/Widgets/NativeCodeEditorView.h
    src/Widgets/TooltipModel.h
    src/Widgets/TooltipController.h
    src/Widgets/TooltipOutline.h
    src/Widgets/NativeTooltipPresentationView.h
    src/Widgets/NativeTooltipTargetView.h
    src/Widgets/TooltipScheduler.h
    src/Widgets/NativeTooltipView.h
    src/Widgets/NativeHierarchyListView.h
    src/Widgets/HierarchyViewport.h
    src/Widgets/HierarchyDrag.h
    src/Widgets/NativeTextEditorView.h
    src/Widgets/NativeInputFieldView.h
    src/Text/TextSurroundingPlan.h
    src/Text/TextInputModel.h
    src/Widgets/ColorChannelSurface.h
    src/Widgets/ListItemSelectorModel.h
    src/Widgets/NativeListItemSelectorView.h
    src/Widgets/ListItemSegmentsModel.h
    src/Widgets/NativeListItemSegmentsView.h
    src/Widgets/CardModel.h
    src/Widgets/NativeCardView.h
    src/Widgets/NativeTabBarView.h
    src/Widgets/TabModel.h
    src/Widgets/NativeTabView.h
    src/Widgets/ColorSurfaceModel.h
    src/Widgets/NativeColorSurfaceView.h
    src/Widgets/CollectionWidgetNavigation.h
    src/Widgets/DocumentLayout.h
    src/Widgets/NativeEditorTimelineView.h
    src/Widgets/NativeEditorCanvasView.h
    src/Widgets/LinkModel.h
    src/Widgets/NativeLinkView.h
    src/Widgets/ComboBoxModel.h
    src/Widgets/NativeComboBoxView.h
    src/Widgets/ColorPickerButtonModel.h
    src/Widgets/NativeColorPickerButtonView.h
    src/Widgets/MenuItemModel.h
    src/Widgets/NativeMenuItemView.h
    src/Widgets/NativeModalView.h
    src/Widgets/ListItemModel.h
    src/Widgets/ListItemDrawing.h
    src/Widgets/NativeListItemView.h
    src/Widgets/AppCardModel.h
    src/Widgets/NativeAppCardView.h
    src/Widgets/AppHeaderModel.h
    src/Widgets/NativeAppHeaderView.h
    src/Widgets/EventListenerModel.h
    src/Widgets/NativeEventListenerView.h
    src/Widgets/AlertButtonModel.h
    src/Widgets/AlertModel.h
    src/Widgets/AlertPresentation.h
    src/Widgets/NativeAlertView.h
    src/Widgets/NativeAlertButtonView.h
    src/Widgets/ToolbarButtonModel.h
    src/Widgets/HierarchyToolbarLayout.h
    src/Widgets/NativeToolbarButtonView.h
    src/Widgets/ListFooterModel.h
    src/Widgets/NativeListFooterView.h
    src/Widgets/ListToolbarModel.h
    src/Widgets/NativeListToolbarView.h
    src/Widgets/NativeIconView.h
    src/Widgets/NativeButtonView.h
    src/Widgets/NativeSheetView.h
    src/Widgets/NativeImageView.h
    src/Widgets/NativeAvatarView.h
    src/Widgets/ProgressDrawing.h
    src/Widgets/ProgressIndicator.h
    src/Widgets/WidgetMotion.h
    src/Widgets/NativeView.h
    src/Widgets/CompiledViewSource.h
    src/Graphics/SrgbColor.h
    src/Widgets/InputClearDrawing.h
    src/Text/NativeTextLayout.h
    src/Widgets/TextFieldMaterial.h
    src/Widgets/EditorViewports.h
    src/Widgets/WidgetLayout.h
    src/Platform/WindowProperties.h
    src/Platform/Desktop.h
    src/Core/Application.h
    src/Platform/Android.h
    src/Presentation/Appearance.h
    src/Widgets/ColorPicker.h
    src/Presentation/Motion.h
    src/API/iiGUI.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiGUI)
if(NOT EMSCRIPTEN)
    install(FILES src/Graphics/VulkanPaintDevice.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiGUI)
endif()
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/iiGUI/Export.h"
    "${CMAKE_CURRENT_BINARY_DIR}/include/iiGUI/PainterExport.h"
    "${CMAKE_CURRENT_BINARY_DIR}/include/iiGUI/HostBridgeExport.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiGUI)
install(EXPORT iiGUITargets NAMESPACE iiGUI:: DESTINATION "${_package}")
set(_iigui_native_abi_input)
foreach(_header NativeWidgets.h Host.h Graphics.h WidgetInteraction.h WidgetLayout.h WindowProperties.h Appearance.h FontDatabase.h Image.h Presentation.h NativeView.h)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_iigui_header_${_header}}")
    file(SHA256 "${_iigui_header_${_header}}" _hash)
    string(APPEND _iigui_native_abi_input "${_header}:${_hash};")
endforeach()
string(SHA256 iiGUI_NATIVE_ABI_ID "${_iigui_native_abi_input}")
set_property(TARGET iiGUI PROPERTY IIGUI_NATIVE_ABI_ID "${iiGUI_NATIVE_ABI_ID}")
configure_package_config_file(cmake/iiGUIConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/iiGUIConfig.cmake" INSTALL_DESTINATION "${_package}")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/iiGUIConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/iiGUIConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/iiGUIConfigVersion.cmake" DESTINATION "${_package}")
install(FILES cmake/iiGUILvui.cmake cmake/iiGUIRunLvui.cmake DESTINATION "${_package}")
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/iiGUI)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DATADIR}/iiGUI/docs)
install(FILES src/third_party/utf8proc/LICENSE.md src/third_party/utf8proc/provenance.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/iiGUI/utf8proc)

if(BUILD_TESTING)
    add_executable(iiGUIDisplayPixelsTests tests/display_pixels.cpp)
    target_link_libraries(iiGUIDisplayPixelsTests PRIVATE iiGUI::iiGUI)
    target_compile_features(iiGUIDisplayPixelsTests PRIVATE cxx_std_23)
    add_test(NAME iiGUI.display_pixels COMMAND iiGUIDisplayPixelsTests)
    add_executable(iiGUIStandalonePainterTests tests/painter_package/main.cpp)
    target_link_libraries(iiGUIStandalonePainterTests PRIVATE iiGUI::Painter)
    add_test(NAME iiGUI.painter_standalone COMMAND iiGUIStandalonePainterTests)
    set_tests_properties(iiGUI.painter_standalone PROPERTIES TIMEOUT 10)
    add_executable(iiGUIFramePainterTests tests/frame_painter.cpp)
    target_link_libraries(iiGUIFramePainterTests PRIVATE iiGUI::Painter)
    add_test(NAME iiGUI.frame_painter COMMAND iiGUIFramePainterTests)
    set_tests_properties(iiGUI.frame_painter PROPERTIES TIMEOUT 10)
    add_executable(iiGUIGpuImagePlanTests tests/gpu_image_plan.cpp)
    target_link_libraries(iiGUIGpuImagePlanTests PRIVATE iiGUI::Painter)
    add_test(NAME iiGUI.gpu_image_plan COMMAND iiGUIGpuImagePlanTests)
    set_tests_properties(iiGUI.gpu_image_plan PROPERTIES TIMEOUT 10)
    add_executable(iiGUIHostFramePainterTests tests/host_frame_painter.cpp)
    target_link_libraries(iiGUIHostFramePainterTests PRIVATE iiGUI::HostBridge)
    add_test(NAME iiGUI.host_frame_painter COMMAND iiGUIHostFramePainterTests)
    set_tests_properties(iiGUI.host_frame_painter PROPERTIES TIMEOUT 10)
    if(NOT EMSCRIPTEN)
        add_executable(iiGUIVulkanExternalSurfaceTests tests/vulkan_external_surface.cpp)
        target_link_libraries(iiGUIVulkanExternalSurfaceTests PRIVATE iiGUI::iiGUI Vulkan::Vulkan)
        add_test(NAME iiGUI.vulkan_external_surface COMMAND iiGUIVulkanExternalSurfaceTests)
        set_tests_properties(iiGUI.vulkan_external_surface PROPERTIES TIMEOUT 10)
        add_executable(iiGUIVulkanHeadlessSurfaceTests tests/vulkan_headless_surface.cpp)
        target_link_libraries(iiGUIVulkanHeadlessSurfaceTests PRIVATE iiGUI::iiGUI Vulkan::Vulkan)
        add_test(NAME iiGUI.vulkan_headless_surface COMMAND iiGUIVulkanHeadlessSurfaceTests)
        set_tests_properties(iiGUI.vulkan_headless_surface PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    endif()
    if(APPLE)
        add_executable(iiGUIMetalPaintDeviceTests tests/metal_paint_device.cpp)
        target_link_libraries(iiGUIMetalPaintDeviceTests PRIVATE iiGUI::iiGUI)
        add_test(NAME iiGUI.metal_paint_device COMMAND iiGUIMetalPaintDeviceTests)
        set_tests_properties(iiGUI.metal_paint_device PROPERTIES TIMEOUT 10)
        if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_CROSSCOMPILING)
            add_executable(iiGUIMetalGpuImageTests tests/metal_gpu_image.mm)
            target_compile_definitions(iiGUIMetalGpuImageTests PRIVATE IIGUI_SOURCE_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}")
            target_compile_options(iiGUIMetalGpuImageTests PRIVATE -fobjc-arc)
            target_link_libraries(iiGUIMetalGpuImageTests PRIVATE iiGUI::iiGUI "-framework Foundation" "-framework Metal")
            add_test(NAME iiGUI.metal_gpu_image COMMAND iiGUIMetalGpuImageTests)
            set_tests_properties(iiGUI.metal_gpu_image PROPERTIES TIMEOUT 30)
            add_executable(iiGUIMetalImagePoolTests tests/metal_image_pool.mm)
            target_compile_options(iiGUIMetalImagePoolTests PRIVATE -fobjc-arc)
            target_link_libraries(iiGUIMetalImagePoolTests PRIVATE iiGUI::iiGUI "-framework Foundation" "-framework Metal")
            add_test(NAME iiGUI.metal_image_pool COMMAND iiGUIMetalImagePoolTests)
            set_tests_properties(iiGUI.metal_image_pool PROPERTIES TIMEOUT 30)
        endif()
    endif()
    if(WIN32)
        add_executable(iiGUIDirect3DExternalSurfaceTests tests/direct3d_external_surface.cpp)
        target_link_libraries(iiGUIDirect3DExternalSurfaceTests PRIVATE iiGUI::iiGUI)
        add_test(NAME iiGUI.direct3d_external_surface COMMAND iiGUIDirect3DExternalSurfaceTests)
        set_tests_properties(iiGUI.direct3d_external_surface PROPERTIES TIMEOUT 10)
    endif()
    add_executable(iiGUIPainterTests tests/painter.cpp)
    target_link_libraries(iiGUIPainterTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.painter COMMAND iiGUIPainterTests)
    set_tests_properties(iiGUI.painter PROPERTIES TIMEOUT 10)
    add_executable(iiGUIPaintViewTests tests/paint_view.cpp)
    target_link_libraries(iiGUIPaintViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.paint_view COMMAND iiGUIPaintViewTests)
    set_tests_properties(iiGUI.paint_view PROPERTIES TIMEOUT 30)
    add_executable(iiGUIPaintViewNoHostTests tests/paint_view_no_host.cpp)
    target_link_libraries(iiGUIPaintViewNoHostTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.paint_view_no_host COMMAND iiGUIPaintViewNoHostTests)
    set_tests_properties(iiGUI.paint_view_no_host PROPERTIES TIMEOUT 10)
    add_executable(iiGUIPaintViewServicesTests tests/paint_view_services.cpp)
    target_link_libraries(iiGUIPaintViewServicesTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.paint_view_services COMMAND iiGUIPaintViewServicesTests)
    set_tests_properties(iiGUI.paint_view_services PROPERTIES TIMEOUT 30)
    add_executable(iiGUIHostSurfaceFactoryTests tests/host_surface_factory.cpp)
    target_link_libraries(iiGUIHostSurfaceFactoryTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.host_surface_factory COMMAND iiGUIHostSurfaceFactoryTests)
    set_tests_properties(iiGUI.host_surface_factory PROPERTIES TIMEOUT 30)
    if(IIGUI_LVUI_COMPILER AND NOT CMAKE_CROSSCOMPILING)
        include(cmake/iiGUILvui.cmake)
        iigui_compile_lvui(NAME Example SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tests/lvui_tooling/Hello.lvui"
            COMPILER "${IIGUI_LVUI_COMPILER}" OUTPUT_VARIABLE _iigui_lvui_example)
        add_executable(iiGUIPaintViewAotTests tests/paint_view_aot.cpp "${_iigui_lvui_example}")
        target_include_directories(iiGUIPaintViewAotTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
        target_link_libraries(iiGUIPaintViewAotTests PRIVATE iiGUI::iiGUI)
        add_test(NAME iiGUI.paint_view_aot COMMAND iiGUIPaintViewAotTests)
        set_tests_properties(iiGUI.paint_view_aot PROPERTIES TIMEOUT 30)
        if(IIGUI_LVUI_TEST_INCLUDE AND IIGUI_LVUI_TEST_LIBRARY)
            iigui_compile_lvui(NAME Application SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tests/lvui_tooling/Application.lvui"
                COMPILER "${IIGUI_LVUI_COMPILER}" OUTPUT_VARIABLE _iigui_lvui_application)
            add_executable(iiGUIPaintViewApplicationAotTests tests/paint_view_application_aot.cpp "${_iigui_lvui_application}")
            target_include_directories(iiGUIPaintViewApplicationAotTests PRIVATE
                "${CMAKE_CURRENT_BINARY_DIR}" "${IIGUI_LVUI_TEST_INCLUDE}"
                "${IIGUI_LVUI_TEST_INCLUDE}/../lvui-generated-icons")
            target_link_libraries(iiGUIPaintViewApplicationAotTests PRIVATE iiGUI::iiGUI "${IIGUI_LVUI_TEST_LIBRARY}")
            add_test(NAME iiGUI.paint_view_application_aot COMMAND iiGUIPaintViewApplicationAotTests)
            set_tests_properties(iiGUI.paint_view_application_aot PROPERTIES TIMEOUT 30)
        endif()
        find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
        get_target_property(_iigui_core_includes iiSDKCore::iiSDKCore INTERFACE_INCLUDE_DIRECTORIES)
        list(GET _iigui_core_includes 0 _iigui_core_include)
        add_test(NAME iiGUI.lvui_aot_tooling COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/lvui_tooling.py" --cmake "${CMAKE_COMMAND}"
            --compiler "${IIGUI_LVUI_COMPILER}" --build-root "${CMAKE_CURRENT_BINARY_DIR}"
            --iigui-include "${CMAKE_CURRENT_BINARY_DIR}/include" --core-include "${_iigui_core_include}")
        set_tests_properties(iiGUI.lvui_aot_tooling PROPERTIES TIMEOUT 60)
        add_test(NAME iiGUI.lvui_package COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/lvui_package.py" --cmake "${CMAKE_COMMAND}"
            --compiler "${IIGUI_LVUI_COMPILER}" --build-root "${CMAKE_CURRENT_BINARY_DIR}"
            --core-cmake "${iiSDKCore_DIR}" --vulkan-include "${Vulkan_INCLUDE_DIR}"
            --vulkan-library "${Vulkan_LIBRARY}")
        set_tests_properties(iiGUI.lvui_package PROPERTIES TIMEOUT 120)
    endif()
    add_executable(iiGUIHostLegacyTextTests tests/host_legacy_text.cpp)
    target_link_libraries(iiGUIHostLegacyTextTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.host_legacy_text COMMAND iiGUIHostLegacyTextTests)
    set_tests_properties(iiGUI.host_legacy_text PROPERTIES TIMEOUT 60)
    add_executable(iiGUIButtonViewTests tests/button_view.cpp)
    target_link_libraries(iiGUIButtonViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.button_view COMMAND iiGUIButtonViewTests)
    set_tests_properties(iiGUI.button_view PROPERTIES TIMEOUT 60)
    add_executable(iiGUIModalViewTests tests/modal_view.cpp)
    target_link_libraries(iiGUIModalViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.modal_view COMMAND iiGUIModalViewTests)
    set_tests_properties(iiGUI.modal_view PROPERTIES TIMEOUT 60)
    add_executable(iiGUIModalModelTests tests/modal_model.cpp)
    target_link_libraries(iiGUIModalModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.modal_model COMMAND iiGUIModalModelTests)
    set_tests_properties(iiGUI.modal_model PROPERTIES TIMEOUT 30)
    add_executable(iiGUIListItemModelTests tests/list_item_model.cpp)
    target_link_libraries(iiGUIListItemModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.list_item_model COMMAND iiGUIListItemModelTests)
    set_tests_properties(iiGUI.list_item_model PROPERTIES TIMEOUT 30)
    add_executable(iiGUIListItemViewTests tests/list_item_view.cpp)
    target_link_libraries(iiGUIListItemViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.list_item_view COMMAND iiGUIListItemViewTests)
    set_tests_properties(iiGUI.list_item_view PROPERTIES TIMEOUT 60)
    add_executable(iiGUIAppCardModelTests tests/app_card_model.cpp)
    target_link_libraries(iiGUIAppCardModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.app_card_model COMMAND iiGUIAppCardModelTests)
    set_tests_properties(iiGUI.app_card_model PROPERTIES TIMEOUT 30)
    add_executable(iiGUIAppCardViewTests tests/app_card_view.cpp)
    target_link_libraries(iiGUIAppCardViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.app_card_view COMMAND iiGUIAppCardViewTests)
    set_tests_properties(iiGUI.app_card_view PROPERTIES TIMEOUT 60)
    add_executable(iiGUIAppHeaderModelTests tests/app_header_model.cpp)
    target_link_libraries(iiGUIAppHeaderModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.app_header_model COMMAND iiGUIAppHeaderModelTests)
    set_tests_properties(iiGUI.app_header_model PROPERTIES TIMEOUT 30)
    add_executable(iiGUIAppHeaderViewTests tests/app_header_view.cpp)
    target_link_libraries(iiGUIAppHeaderViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.app_header_view COMMAND iiGUIAppHeaderViewTests)
    set_tests_properties(iiGUI.app_header_view PROPERTIES TIMEOUT 60)
    add_executable(iiGUIEventListenerModelTests tests/event_listener_model.cpp)
    target_link_libraries(iiGUIEventListenerModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.event_listener_model COMMAND iiGUIEventListenerModelTests)
    set_tests_properties(iiGUI.event_listener_model PROPERTIES TIMEOUT 30)
    add_executable(iiGUIEventListenerViewTests tests/event_listener_view.cpp)
    target_link_libraries(iiGUIEventListenerViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.event_listener_view COMMAND iiGUIEventListenerViewTests)
    set_tests_properties(iiGUI.event_listener_view PROPERTIES TIMEOUT 60)
    add_executable(iiGUIAlertButtonModelTests tests/alert_button_model.cpp)
    target_link_libraries(iiGUIAlertButtonModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.alert_button_model COMMAND iiGUIAlertButtonModelTests)
    set_tests_properties(iiGUI.alert_button_model PROPERTIES TIMEOUT 30)
    add_executable(iiGUIAlertButtonViewTests tests/alert_button_view.cpp)
    target_link_libraries(iiGUIAlertButtonViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.alert_button_view COMMAND iiGUIAlertButtonViewTests)
    set_tests_properties(iiGUI.alert_button_view PROPERTIES TIMEOUT 60)
    add_executable(iiGUIButtonModelTests tests/button_model.cpp)
    target_link_libraries(iiGUIButtonModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.button_model COMMAND iiGUIButtonModelTests)
    set_tests_properties(iiGUI.button_model PROPERTIES TIMEOUT 30)
    add_executable(iiGUIToolbarButtonModelTests tests/toolbar_button_model.cpp)
    target_link_libraries(iiGUIToolbarButtonModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.toolbar_button_model COMMAND iiGUIToolbarButtonModelTests)
    set_tests_properties(iiGUI.toolbar_button_model PROPERTIES TIMEOUT 30)
    add_executable(iiGUIToolbarButtonViewTests tests/toolbar_button_view.cpp)
    target_link_libraries(iiGUIToolbarButtonViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.toolbar_button_view COMMAND iiGUIToolbarButtonViewTests)
    set_tests_properties(iiGUI.toolbar_button_view PROPERTIES TIMEOUT 60)
    add_executable(iiGUIListFooterModelTests tests/list_footer_model.cpp)
    target_link_libraries(iiGUIListFooterModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.list_footer_model COMMAND iiGUIListFooterModelTests)
    set_tests_properties(iiGUI.list_footer_model PROPERTIES TIMEOUT 30)
    add_executable(iiGUIListFooterViewTests tests/list_footer_view.cpp)
    target_link_libraries(iiGUIListFooterViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.list_footer_view COMMAND iiGUIListFooterViewTests)
    set_tests_properties(iiGUI.list_footer_view PROPERTIES TIMEOUT 60)
    add_executable(iiGUIListToolbarModelTests tests/list_toolbar_model.cpp)
    target_link_libraries(iiGUIListToolbarModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.list_toolbar_model COMMAND iiGUIListToolbarModelTests)
    set_tests_properties(iiGUI.list_toolbar_model PROPERTIES TIMEOUT 30)
    add_executable(iiGUIListToolbarViewTests tests/list_toolbar_view.cpp)
    target_link_libraries(iiGUIListToolbarViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.list_toolbar_view COMMAND iiGUIListToolbarViewTests)
    set_tests_properties(iiGUI.list_toolbar_view PROPERTIES TIMEOUT 60)
    add_executable(iiGUIIconViewTests tests/icon_view.cpp)
    target_link_libraries(iiGUIIconViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.icon_view COMMAND iiGUIIconViewTests)
    set_tests_properties(iiGUI.icon_view PROPERTIES TIMEOUT 60)
    add_executable(iiGUIHierarchyToolbarLayoutTests tests/hierarchy_toolbar_layout.cpp)
    target_link_libraries(iiGUIHierarchyToolbarLayoutTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.hierarchy_toolbar_layout COMMAND iiGUIHierarchyToolbarLayoutTests)
    set_tests_properties(iiGUI.hierarchy_toolbar_layout PROPERTIES TIMEOUT 30)
    add_executable(iiGUISvgImageCacheTests tests/svg_image_cache.cpp)
    target_link_libraries(iiGUISvgImageCacheTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.svg_image_cache COMMAND iiGUISvgImageCacheTests)
    set_tests_properties(iiGUI.svg_image_cache PROPERTIES TIMEOUT 60)
    add_executable(iiGUISheetViewTests tests/sheet_view.cpp)
    target_link_libraries(iiGUISheetViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.sheet_view COMMAND iiGUISheetViewTests)
    set_tests_properties(iiGUI.sheet_view PROPERTIES TIMEOUT 60)
    add_executable(iiGUISheetModelTests tests/sheet_model.cpp)
    target_link_libraries(iiGUISheetModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.sheet_model COMMAND iiGUISheetModelTests)
    set_tests_properties(iiGUI.sheet_model PROPERTIES TIMEOUT 30)
    add_executable(iiGUIImageModelTests tests/image_model.cpp)
    target_link_libraries(iiGUIImageModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.image_model COMMAND iiGUIImageModelTests)
    set_tests_properties(iiGUI.image_model PROPERTIES TIMEOUT 30)
    add_executable(iiGUIImageViewTests tests/image_view.cpp)
    target_link_libraries(iiGUIImageViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.image_view COMMAND iiGUIImageViewTests)
    set_tests_properties(iiGUI.image_view PROPERTIES TIMEOUT 30)
    add_executable(iiGUIAlertViewTests tests/alert_view.cpp)
    target_link_libraries(iiGUIAlertViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.alert_view COMMAND iiGUIAlertViewTests)
    add_executable(iiGUIAlertPresentationTests tests/alert_presentation.cpp)
    target_link_libraries(iiGUIAlertPresentationTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.alert_presentation COMMAND iiGUIAlertPresentationTests)
    add_executable(iiGUIMenuItemViewTests tests/menu_item_view.cpp)
    target_link_libraries(iiGUIMenuItemViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.menu_item_view COMMAND iiGUIMenuItemViewTests)
    add_executable(iiGUIPopoverViewTests tests/popover_view.cpp)
    target_link_libraries(iiGUIPopoverViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.popover_view COMMAND iiGUIPopoverViewTests)
    add_executable(iiGUIColorPickerButtonViewTests tests/color_picker_button_view.cpp)
    target_link_libraries(iiGUIColorPickerButtonViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.color_picker_button_view COMMAND iiGUIColorPickerButtonViewTests)
    add_executable(iiGUIHierarchyToolbarViewTests tests/hierarchy_toolbar_view.cpp)
    target_link_libraries(iiGUIHierarchyToolbarViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.hierarchy_toolbar_view COMMAND iiGUIHierarchyToolbarViewTests)
    add_executable(iiGUIListPanelViewTests tests/list_panel_view.cpp)
    target_link_libraries(iiGUIListPanelViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.list_panel_view COMMAND iiGUIListPanelViewTests)
    add_executable(iiGUIHierarchyPanelViewTests tests/hierarchy_panel_view.cpp)
    target_link_libraries(iiGUIHierarchyPanelViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.hierarchy_panel_view COMMAND iiGUIHierarchyPanelViewTests)
    add_executable(iiGUIPanelLayoutTests tests/panel_layout.cpp)
    target_link_libraries(iiGUIPanelLayoutTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.panel_layout COMMAND iiGUIPanelLayoutTests)
    add_executable(iiGUITableHeaderViewTests tests/table_header_view.cpp)
    target_link_libraries(iiGUITableHeaderViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.table_header_view COMMAND iiGUITableHeaderViewTests)
    add_executable(iiGUITableHeaderModelTests tests/table_header_model.cpp)
    target_link_libraries(iiGUITableHeaderModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.table_header_model COMMAND iiGUITableHeaderModelTests)
    add_executable(iiGUIHierarchyToolbarModelTests tests/hierarchy_toolbar_model.cpp)
    target_link_libraries(iiGUIHierarchyToolbarModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.hierarchy_toolbar_model COMMAND iiGUIHierarchyToolbarModelTests)
    add_executable(iiGUIHierarchyItemModelTests tests/hierarchy_item_model.cpp)
    target_link_libraries(iiGUIHierarchyItemModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.hierarchy_item_model COMMAND iiGUIHierarchyItemModelTests)
    add_executable(iiGUIHierarchyItemViewTests tests/hierarchy_item_view.cpp)
    target_link_libraries(iiGUIHierarchyItemViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.hierarchy_item_view COMMAND iiGUIHierarchyItemViewTests)
    add_executable(iiGUIMobileNavigationViewTests tests/mobile_navigation_view.cpp)
    target_link_libraries(iiGUIMobileNavigationViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.mobile_navigation_view COMMAND iiGUIMobileNavigationViewTests)
    add_executable(iiGUIMobileNavigationModelTests tests/mobile_navigation_model.cpp)
    target_link_libraries(iiGUIMobileNavigationModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.mobile_navigation_model COMMAND iiGUIMobileNavigationModelTests)
    add_executable(iiGUIColorPickerViewTests tests/color_picker_view.cpp)
    target_link_libraries(iiGUIColorPickerViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.color_picker_view COMMAND iiGUIColorPickerViewTests)
    add_executable(iiGUIColorPickerLayoutTests tests/color_picker_layout.cpp)
    target_link_libraries(iiGUIColorPickerLayoutTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.color_picker_layout COMMAND iiGUIColorPickerLayoutTests)
    add_executable(iiGUIColorChannelViewTests tests/color_channel_view.cpp)
    target_link_libraries(iiGUIColorChannelViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.color_channel_view COMMAND iiGUIColorChannelViewTests)
    add_executable(iiGUIColorPickerValuesViewTests tests/color_picker_values_view.cpp)
    target_link_libraries(iiGUIColorPickerValuesViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.color_picker_values_view COMMAND iiGUIColorPickerValuesViewTests)
    add_executable(iiGUITableRowViewTests tests/table_row_view.cpp)
    target_link_libraries(iiGUITableRowViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.table_row_view COMMAND iiGUITableRowViewTests)
    add_executable(iiGUITableCellModelsTests tests/table_cell_models.cpp)
    target_link_libraries(iiGUITableCellModelsTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.table_cell_models COMMAND iiGUITableCellModelsTests)
    add_executable(iiGUITableCellItemViewTests tests/table_cell_item_view.cpp)
    target_link_libraries(iiGUITableCellItemViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.table_cell_item_view COMMAND iiGUITableCellItemViewTests)
    add_executable(iiGUISearchFieldViewTests tests/search_field_view.cpp)
    target_link_libraries(iiGUISearchFieldViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.search_field_view COMMAND iiGUISearchFieldViewTests)
    add_executable(iiGUISliderViewTests tests/slider_view.cpp)
    target_link_libraries(iiGUISliderViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.slider_view COMMAND iiGUISliderViewTests)
    add_executable(iiGUISliderModelTests tests/slider_model.cpp)
    target_link_libraries(iiGUISliderModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.slider_model COMMAND iiGUISliderModelTests)
    add_executable(iiGUITextEditorViewTests tests/text_editor_view.cpp)
    target_link_libraries(iiGUITextEditorViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.text_editor_view COMMAND iiGUITextEditorViewTests)
    add_executable(iiGUIInputFieldViewTests tests/input_field_view.cpp)
    target_link_libraries(iiGUIInputFieldViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.input_field_view COMMAND iiGUIInputFieldViewTests)
    add_executable(iiGUITextInputModelTests tests/text_input_model.cpp)
    target_link_libraries(iiGUITextInputModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.text_input_model COMMAND iiGUITextInputModelTests)
    add_executable(iiGUIColorChannelSurfaceTests tests/color_channel_surface.cpp)
    target_link_libraries(iiGUIColorChannelSurfaceTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.color_channel_surface COMMAND iiGUIColorChannelSurfaceTests)
    add_executable(iiGUIListItemSelectorModelTests tests/list_item_selector_model.cpp)
    target_link_libraries(iiGUIListItemSelectorModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.list_item_selector_model COMMAND iiGUIListItemSelectorModelTests)
    add_executable(iiGUIListItemSelectorViewTests tests/list_item_selector_view.cpp)
    target_link_libraries(iiGUIListItemSelectorViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.list_item_selector_view COMMAND iiGUIListItemSelectorViewTests)
    add_executable(iiGUIListItemSegmentsModelTests tests/list_item_segments_model.cpp)
    target_link_libraries(iiGUIListItemSegmentsModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.list_item_segments_model COMMAND iiGUIListItemSegmentsModelTests)
    add_executable(iiGUIListItemSegmentsViewTests tests/list_item_segments_view.cpp)
    target_link_libraries(iiGUIListItemSegmentsViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.list_item_segments_view COMMAND iiGUIListItemSegmentsViewTests)
    add_executable(iiGUICardModelTests tests/card_model.cpp)
    target_link_libraries(iiGUICardModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.card_model COMMAND iiGUICardModelTests)
    add_executable(iiGUICardViewTests tests/card_view.cpp)
    target_link_libraries(iiGUICardViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.card_view COMMAND iiGUICardViewTests)
    add_executable(iiGUITabBarViewTests tests/tab_bar_view.cpp)
    target_link_libraries(iiGUITabBarViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.tab_bar_view COMMAND iiGUITabBarViewTests)
    add_executable(iiGUITabModelTests tests/tab_model.cpp)
    target_link_libraries(iiGUITabModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.tab_model COMMAND iiGUITabModelTests)
    add_executable(iiGUITabViewTests tests/tab_view.cpp)
    target_link_libraries(iiGUITabViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.tab_view COMMAND iiGUITabViewTests)
    add_executable(iiGUIColorSurfaceModelTests tests/color_surface_model.cpp)
    target_link_libraries(iiGUIColorSurfaceModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.color_surface_model COMMAND iiGUIColorSurfaceModelTests)
    add_executable(iiGUIColorSurfaceViewTests tests/color_surface_view.cpp)
    target_link_libraries(iiGUIColorSurfaceViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.color_surface_view COMMAND iiGUIColorSurfaceViewTests)
    add_executable(iiGUICollectionWidgetNavigationTests tests/collection_widget_navigation.cpp)
    target_link_libraries(iiGUICollectionWidgetNavigationTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.collection_widget_navigation COMMAND iiGUICollectionWidgetNavigationTests)
    add_executable(iiGUIDocumentLayoutTests tests/document_layout.cpp)
    target_link_libraries(iiGUIDocumentLayoutTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.document_layout COMMAND iiGUIDocumentLayoutTests)
    add_executable(iiGUIEditorTimelineViewTests tests/editor_timeline_view.cpp)
    target_link_libraries(iiGUIEditorTimelineViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.editor_timeline_view COMMAND iiGUIEditorTimelineViewTests)
    add_executable(iiGUIEditorCanvasViewTests tests/editor_canvas_view.cpp)
    target_link_libraries(iiGUIEditorCanvasViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.editor_canvas_view COMMAND iiGUIEditorCanvasViewTests)
    add_executable(iiGUILinkModelTests tests/link_model.cpp)
    target_link_libraries(iiGUILinkModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.link_model COMMAND iiGUILinkModelTests)
    add_executable(iiGUILinkViewTests tests/link_view.cpp)
    target_link_libraries(iiGUILinkViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.link_view COMMAND iiGUILinkViewTests)
    add_executable(iiGUIComboBoxViewTests tests/combo_box_view.cpp)
    target_link_libraries(iiGUIComboBoxViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.combo_box_view COMMAND iiGUIComboBoxViewTests)
    add_executable(iiGUIComboBoxModelTests tests/combo_box_model.cpp)
    target_link_libraries(iiGUIComboBoxModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.combo_box_model COMMAND iiGUIComboBoxModelTests)
    add_executable(iiGUIColorPickerButtonModelTests tests/color_picker_button_model.cpp)
    target_link_libraries(iiGUIColorPickerButtonModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.color_picker_button_model COMMAND iiGUIColorPickerButtonModelTests)
    add_executable(iiGUIPageRouterViewTests tests/page_router_view.cpp)
    target_link_libraries(iiGUIPageRouterViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.page_router_view COMMAND iiGUIPageRouterViewTests)
    add_executable(iiGUIPopoverModelTests tests/popover_model.cpp)
    target_link_libraries(iiGUIPopoverModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.popover_model COMMAND iiGUIPopoverModelTests)
    add_executable(iiGUIMaterialViewTests tests/material_view.cpp)
    target_link_libraries(iiGUIMaterialViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.material_view COMMAND iiGUIMaterialViewTests)
    add_executable(iiGUIMaterialModelTests tests/material_model.cpp)
    target_link_libraries(iiGUIMaterialModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.material_model COMMAND iiGUIMaterialModelTests)
    add_executable(iiGUIMaterialGradientTests tests/material_gradient.cpp)
    target_link_libraries(iiGUIMaterialGradientTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.material_gradient COMMAND iiGUIMaterialGradientTests)
    add_executable(iiGUIMenuViewTests tests/menu_view.cpp)
    target_link_libraries(iiGUIMenuViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.menu_view COMMAND iiGUIMenuViewTests)
    add_executable(iiGUIMenuModelTests tests/menu_model.cpp)
    target_link_libraries(iiGUIMenuModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.menu_model COMMAND iiGUIMenuModelTests)
    add_executable(iiGUIMenuItemModelTests tests/menu_item_model.cpp)
    target_link_libraries(iiGUIMenuItemModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.menu_item_model COMMAND iiGUIMenuItemModelTests)
    add_executable(iiGUIModalPresentationTests tests/modal_presentation.cpp)
    target_link_libraries(iiGUIModalPresentationTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.modal_presentation COMMAND iiGUIModalPresentationTests)
    add_executable(iiGUIPanelPainterTests tests/panel_painter.cpp)
    target_link_libraries(iiGUIPanelPainterTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.panel_painter COMMAND iiGUIPanelPainterTests)
    add_executable(iiGUIStepperModelTests tests/stepper_model.cpp)
    target_link_libraries(iiGUIStepperModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.stepper_model COMMAND iiGUIStepperModelTests)
    add_executable(iiGUIStepperViewTests tests/stepper_view.cpp)
    target_link_libraries(iiGUIStepperViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.stepper_view COMMAND iiGUIStepperViewTests)
    add_executable(iiGUIDropdownModelTests tests/dropdown_model.cpp)
    target_link_libraries(iiGUIDropdownModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.dropdown_model COMMAND iiGUIDropdownModelTests)
    add_executable(iiGUIDropdownViewTests tests/dropdown_view.cpp)
    target_link_libraries(iiGUIDropdownViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.dropdown_view COMMAND iiGUIDropdownViewTests)
    add_executable(iiGUITableHeaderPresentationTests tests/table_header_presentation.cpp)
    target_link_libraries(iiGUITableHeaderPresentationTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.table_header_presentation COMMAND iiGUITableHeaderPresentationTests)
    add_executable(iiGUITableViewGroupTests tests/table_view_group.cpp)
    target_link_libraries(iiGUITableViewGroupTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.table_view_group COMMAND iiGUITableViewGroupTests)
    add_executable(iiGUISegmentedControlModelTests tests/segmented_control_model.cpp)
    target_link_libraries(iiGUISegmentedControlModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.segmented_control_model COMMAND iiGUISegmentedControlModelTests)
    add_executable(iiGUISegmentedControlViewTests tests/segmented_control_view.cpp)
    target_link_libraries(iiGUISegmentedControlViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.segmented_control_view COMMAND iiGUISegmentedControlViewTests)
    add_executable(iiGUIRadioButtonViewTests tests/radio_button_view.cpp)
    target_link_libraries(iiGUIRadioButtonViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.radio_button_view COMMAND iiGUIRadioButtonViewTests)
    add_executable(iiGUIRadioGroupsTests tests/radio_groups.cpp)
    target_link_libraries(iiGUIRadioGroupsTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.radio_groups COMMAND iiGUIRadioGroupsTests)
    add_executable(iiGUIRadioButtonModelTests tests/radio_button_model.cpp)
    target_link_libraries(iiGUIRadioButtonModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.radio_button_model COMMAND iiGUIRadioButtonModelTests)
    add_executable(iiGUITextFieldMaterialTests tests/text_field_material.cpp)
    target_link_libraries(iiGUITextFieldMaterialTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.text_field_material COMMAND iiGUITextFieldMaterialTests)
    add_executable(iiGUINativeTextLayoutTests tests/native_text_layout.cpp)
    target_link_libraries(iiGUINativeTextLayoutTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.native_text_layout COMMAND iiGUINativeTextLayoutTests)
    set_tests_properties(iiGUI.native_text_layout PROPERTIES TIMEOUT 60)
    add_executable(iiGUIInputClearDrawingTests tests/input_clear_drawing.cpp)
    target_link_libraries(iiGUIInputClearDrawingTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.input_clear_drawing COMMAND iiGUIInputClearDrawingTests)
    add_executable(iiGUISrgbColorTests tests/srgb_color.cpp)
    target_link_libraries(iiGUISrgbColorTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.srgb_color COMMAND iiGUISrgbColorTests)
    add_executable(iiGUICompiledViewSourceTests tests/compiled_view_source.cpp)
    target_link_libraries(iiGUICompiledViewSourceTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.compiled_view_source COMMAND iiGUICompiledViewSourceTests)
    add_executable(iiGUICheckBoxViewTests tests/check_box_view.cpp)
    target_link_libraries(iiGUICheckBoxViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.check_box_view COMMAND iiGUICheckBoxViewTests)
    add_executable(iiGUICheckBoxModelTests tests/check_box_model.cpp)
    target_link_libraries(iiGUICheckBoxModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.check_box_model COMMAND iiGUICheckBoxModelTests)
    add_executable(iiGUIToggleSwitchModelTests tests/toggle_switch_model.cpp)
    target_link_libraries(iiGUIToggleSwitchModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.toggle_switch_model COMMAND iiGUIToggleSwitchModelTests)
    add_executable(iiGUIToggleSwitchViewTests tests/toggle_switch_view.cpp)
    target_link_libraries(iiGUIToggleSwitchViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.toggle_switch_view COMMAND iiGUIToggleSwitchViewTests)
    set_tests_properties(iiGUI.alert_view PROPERTIES TIMEOUT 60)
    add_executable(iiGUIAlertModelTests tests/alert_model.cpp)
    target_link_libraries(iiGUIAlertModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.alert_model COMMAND iiGUIAlertModelTests)
    set_tests_properties(iiGUI.alert_model PROPERTIES TIMEOUT 30)
    add_executable(iiGUIAvatarModelTests tests/avatar_model.cpp)
    target_link_libraries(iiGUIAvatarModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.avatar_model COMMAND iiGUIAvatarModelTests)
    set_tests_properties(iiGUI.avatar_model PROPERTIES TIMEOUT 30)
    add_executable(iiGUIAvatarViewTests tests/avatar_view.cpp)
    target_link_libraries(iiGUIAvatarViewTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.avatar_view COMMAND iiGUIAvatarViewTests)
    set_tests_properties(iiGUI.avatar_view PROPERTIES TIMEOUT 30)
    if(WIN32)
        add_library(iiGUIWindowsImageHeaderMacros OBJECT tests/windows_image_header_macros.cpp)
        target_link_libraries(iiGUIWindowsImageHeaderMacros PRIVATE iiGUI::iiGUI)
    endif()
    add_executable(iiGUIDividerModelTests tests/divider_model.cpp)
    target_link_libraries(iiGUIDividerModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.divider_model COMMAND iiGUIDividerModelTests)
    set_tests_properties(iiGUI.divider_model PROPERTIES TIMEOUT 30)
    add_executable(iiGUIWidgetMotionTests tests/widget_motion.cpp)
    target_link_libraries(iiGUIWidgetMotionTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.widget_motion COMMAND iiGUIWidgetMotionTests)
    set_tests_properties(iiGUI.widget_motion PROPERTIES TIMEOUT 30)
    add_executable(iiGUIProgressIndicatorTests tests/progress_indicator.cpp)
    target_link_libraries(iiGUIProgressIndicatorTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.progress_indicator COMMAND iiGUIProgressIndicatorTests)
    set_tests_properties(iiGUI.progress_indicator PROPERTIES TIMEOUT 30)
    add_executable(iiGUIProgressModelTests tests/progress_model.cpp)
    target_link_libraries(iiGUIProgressModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.progress_model COMMAND iiGUIProgressModelTests)
    set_tests_properties(iiGUI.progress_model PROPERTIES TIMEOUT 30)
    add_executable(iiGUIProgressDrawingTests tests/progress_drawing.cpp)
    target_link_libraries(iiGUIProgressDrawingTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.progress_drawing COMMAND iiGUIProgressDrawingTests)
    set_tests_properties(iiGUI.progress_drawing PROPERTIES TIMEOUT 30)
    add_test(NAME iiGUI.linux_backend_policy COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/linux_backend_policy.cmake")
    add_executable(iiGUIFrameDemandTests tests/frame_demand.cpp)
    target_compile_features(iiGUIFrameDemandTests PRIVATE cxx_std_23)
    add_test(NAME iiGUI.frame_demand COMMAND iiGUIFrameDemandTests)
    add_executable(iiGUIEditorViewportsTests tests/editor_viewports.cpp)
    target_link_libraries(iiGUIEditorViewportsTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.editor_viewports COMMAND iiGUIEditorViewportsTests)
    if(UNIX AND NOT EMSCRIPTEN)
        add_executable(iiGUIReadinessWaitTests tests/readiness_wait.cpp)
        target_compile_features(iiGUIReadinessWaitTests PRIVATE cxx_std_23)
        add_test(NAME iiGUI.readiness_wait COMMAND iiGUIReadinessWaitTests)
    endif()
    if(APPLE)
        find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
        foreach(_metal_sdk macosx iphoneos)
            add_test(NAME iiGUI.metal_shader.${_metal_sdk} COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/metal_shader.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${_metal_sdk}")
            set_tests_properties(iiGUI.metal_shader.${_metal_sdk} PROPERTIES TIMEOUT 30)
        endforeach()
    endif()
    add_executable(iiGUICanvasSceneGraphTests tests/canvas_scene_graph.cpp)
    target_link_libraries(iiGUICanvasSceneGraphTests PRIVATE iiGUI::iiGUI)
    add_executable(iiGUICanvasDamageTests tests/canvas_damage.cpp)
    target_link_libraries(iiGUICanvasDamageTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.canvas_damage COMMAND iiGUICanvasDamageTests)
    add_test(NAME iiGUI.canvas_scene_graph COMMAND iiGUICanvasSceneGraphTests)
    set_tests_properties(iiGUI.canvas_scene_graph PROPERTIES TIMEOUT 30)
    if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        add_executable(iiGUIWebCanvasFakeTests tests/web_canvas_fake.cpp src/Graphics/CanvasWeb.cpp)
        target_include_directories(iiGUIWebCanvasFakeTests BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests/webgl_fake")
        target_link_libraries(iiGUIWebCanvasFakeTests PRIVATE iiGUI::iiGUI)
        add_test(NAME iiGUI.web_canvas_fake COMMAND iiGUIWebCanvasFakeTests)
        set_tests_properties(iiGUI.web_canvas_fake PROPERTIES TIMEOUT 30)
    endif()
    add_executable(iiGUISubtreeSceneTests tests/subtree_scene.cpp)
    target_link_libraries(iiGUISubtreeSceneTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.subtree_scene COMMAND iiGUISubtreeSceneTests)
    set_tests_properties(iiGUI.subtree_scene PROPERTIES TIMEOUT 30)
    add_executable(iiGUIPopupMotionChannelsTests tests/popup_motion_channels.cpp)
    target_link_libraries(iiGUIPopupMotionChannelsTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.popup_motion_channels COMMAND iiGUIPopupMotionChannelsTests)
    set_tests_properties(iiGUI.popup_motion_channels PROPERTIES TIMEOUT 30)
    add_test(NAME iiGUI.source_layout COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/source_layout.cmake)
    add_executable(iiGUIWrappedTextTests tests/wrapped_text.cpp)
    target_link_libraries(iiGUIWrappedTextTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.wrapped_text COMMAND iiGUIWrappedTextTests)
    set_tests_properties(iiGUI.wrapped_text PROPERTIES TIMEOUT 30)
    add_executable(iiGUISegmentedLayoutTests tests/segmented_layout.cpp)
    target_link_libraries(iiGUISegmentedLayoutTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.segmented_layout COMMAND iiGUISegmentedLayoutTests)
    add_executable(iiGUIAccessibilitySemanticsTests tests/accessibility_semantics.cpp)
    target_link_libraries(iiGUIAccessibilitySemanticsTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.accessibility_semantics COMMAND iiGUIAccessibilitySemanticsTests)
    set_tests_properties(iiGUI.accessibility_semantics PROPERTIES TIMEOUT 30)
    add_executable(iiGUIVirtualAccessibilityTests tests/virtual_accessibility.cpp)
    target_link_libraries(iiGUIVirtualAccessibilityTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.virtual_accessibility COMMAND iiGUIVirtualAccessibilityTests)
    set_tests_properties(iiGUI.virtual_accessibility PROPERTIES TIMEOUT 30)
    add_executable(iiGUIIncrementalWidgetUpdatesTests tests/incremental_widget_updates.cpp)
    target_link_libraries(iiGUIIncrementalWidgetUpdatesTests PRIVATE iiGUI::iiGUI)
    foreach(_case options non_action color focused_editor paint_chunks)
        add_test(NAME iiGUI.incremental_widget_updates.${_case} COMMAND iiGUIIncrementalWidgetUpdatesTests ${_case})
        set_tests_properties(iiGUI.incremental_widget_updates.${_case} PROPERTIES TIMEOUT 30)
    endforeach()
    add_executable(iiGUIAccessibilityGroupingTests tests/accessibility_grouping.cpp)
    target_link_libraries(iiGUIAccessibilityGroupingTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.accessibility_grouping COMMAND iiGUIAccessibilityGroupingTests)
    set_tests_properties(iiGUI.accessibility_grouping PROPERTIES TIMEOUT 30)
    add_executable(iiGUIWindowsAccessibilityProjectionTests tests/windows_accessibility_projection.cpp)
    target_link_libraries(iiGUIWindowsAccessibilityProjectionTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.windows_accessibility_projection COMMAND iiGUIWindowsAccessibilityProjectionTests)
    set_tests_properties(iiGUI.windows_accessibility_projection PROPERTIES TIMEOUT 30)
    add_executable(iiGUIAtSpiTreeTests tests/atspi_accessibility_tree.cpp)
    target_link_libraries(iiGUIAtSpiTreeTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.atspi_accessibility_tree COMMAND iiGUIAtSpiTreeTests)
    set_tests_properties(iiGUI.atspi_accessibility_tree PROPERTIES TIMEOUT 30)
    add_executable(iiGUIAtSpiCoordinatorTests tests/atspi_coordinator.cpp)
    target_link_libraries(iiGUIAtSpiCoordinatorTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.atspi_coordinator COMMAND iiGUIAtSpiCoordinatorTests)
    set_tests_properties(iiGUI.atspi_coordinator PROPERTIES TIMEOUT 30)
    add_executable(iiGUIAtSpiTextTests tests/atspi_text.cpp)
    target_link_libraries(iiGUIAtSpiTextTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.atspi_text COMMAND iiGUIAtSpiTextTests)
    set_tests_properties(iiGUI.atspi_text PROPERTIES TIMEOUT 30)
    if((CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID) OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(AtSpiTestGio QUIET IMPORTED_TARGET gio-2.0)
        endif()
        if(AtSpiTestGio_FOUND)
            add_executable(iiGUIAtSpiDbusTests tests/atspi_dbus.cpp src/Widgets/AtSpiDbus.cpp src/Widgets/AtSpiSession.cpp)
            target_link_libraries(iiGUIAtSpiDbusTests PRIVATE iiGUI::iiGUI PkgConfig::AtSpiTestGio)
            target_compile_definitions(iiGUIAtSpiDbusTests PRIVATE IIGUI_EXPECTED_VERSION="${PROJECT_VERSION}")
            add_test(NAME iiGUI.atspi_dbus COMMAND iiGUIAtSpiDbusTests)
            set_tests_properties(iiGUI.atspi_dbus PROPERTIES TIMEOUT 30)
        endif()
    endif()
    if(NOT WIN32)
        add_executable(iiGUIWindowsAccessibilityAdapterTests tests/windows_accessibility_adapter.cpp src/Widgets/WindowsAccessibility.cpp)
        target_include_directories(iiGUIWindowsAccessibilityAdapterTests BEFORE PRIVATE tests/windows_accessibility_fake)
        target_link_libraries(iiGUIWindowsAccessibilityAdapterTests PRIVATE iiGUI::iiGUI)
        add_test(NAME iiGUI.windows_accessibility_adapter COMMAND iiGUIWindowsAccessibilityAdapterTests)
        set_tests_properties(iiGUI.windows_accessibility_adapter PROPERTIES TIMEOUT 30)
    endif()
    add_executable(iiGUIPopupDismissalTests tests/popup_dismissal.cpp)
    target_link_libraries(iiGUIPopupDismissalTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.popup_dismissal COMMAND iiGUIPopupDismissalTests)
    set_tests_properties(iiGUI.popup_dismissal PROPERTIES TIMEOUT 30)
    add_executable(iiGUIAdaptivePopupLayoutTests tests/adaptive_popup_layout.cpp)
    target_link_libraries(iiGUIAdaptivePopupLayoutTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.adaptive_popup_layout COMMAND iiGUIAdaptivePopupLayoutTests)
    set_tests_properties(iiGUI.adaptive_popup_layout PROPERTIES TIMEOUT 30)
    add_executable(iiGUIStagedPopupContentTests tests/staged_popup_content.cpp)
    target_link_libraries(iiGUIStagedPopupContentTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.staged_popup_content COMMAND iiGUIStagedPopupContentTests)
    set_tests_properties(iiGUI.staged_popup_content PROPERTIES TIMEOUT 30)
    add_executable(iiGUIHostContractTests tests/host_contract.cpp)
    target_link_libraries(iiGUIHostContractTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.host_contract COMMAND iiGUIHostContractTests)
    set_tests_properties(iiGUI.host_contract PROPERTIES TIMEOUT 15)
    add_executable(iiGUIClipboardCapacityTests tests/clipboard_capacity.cpp)
    target_link_libraries(iiGUIClipboardCapacityTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.clipboard_capacity COMMAND iiGUIClipboardCapacityTests)
    if(APPLE AND NOT IOS)
        add_executable(iiGUICocoaBackingObserverTests tests/cocoa_backing_observer.mm)
        target_link_libraries(iiGUICocoaBackingObserverTests PRIVATE "-framework AppKit")
        target_compile_options(iiGUICocoaBackingObserverTests PRIVATE -fobjc-arc)
        set_target_properties(iiGUICocoaBackingObserverTests PROPERTIES OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED YES)
        add_test(NAME iiGUI.cocoa_backing_observer COMMAND iiGUICocoaBackingObserverTests)
        set_tests_properties(iiGUI.cocoa_backing_observer PROPERTIES TIMEOUT 15)
        add_executable(iiGUIClipboardCapacityMacTests tests/clipboard_capacity_mac.mm)
        target_link_libraries(iiGUIClipboardCapacityMacTests PRIVATE iiGUI::iiGUI "-framework AppKit")
        target_compile_options(iiGUIClipboardCapacityMacTests PRIVATE -fobjc-arc)
        set_target_properties(iiGUIClipboardCapacityMacTests PROPERTIES OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED YES)
        add_test(NAME iiGUI.clipboard_capacity_mac COMMAND iiGUIClipboardCapacityMacTests)
    endif()
    if(EMSCRIPTEN)
        add_executable(iiGUIClipboardCapacityWebTests tests/clipboard_capacity_web.cpp)
        target_link_libraries(iiGUIClipboardCapacityWebTests PRIVATE iiGUI::iiGUI)
        target_link_options(iiGUIClipboardCapacityWebTests PRIVATE "SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/tests/clipboard_capacity_web_mock.js"
            "-sENVIRONMENT=node,worker" "-sEXIT_RUNTIME=1")
        find_program(_iigui_clipboard_node node)
        if(_iigui_clipboard_node)
            add_test(NAME iiGUI.clipboard_capacity_web COMMAND "${_iigui_clipboard_node}" "$<TARGET_FILE:iiGUIClipboardCapacityWebTests>")
            set_tests_properties(iiGUI.clipboard_capacity_web PROPERTIES TIMEOUT 60)
        endif()
    endif()
    add_executable(iiGUIHostDriverPolicyTests tests/host_driver_policy.cpp)
    target_link_libraries(iiGUIHostDriverPolicyTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.host_driver_policy COMMAND iiGUIHostDriverPolicyTests)
    set_tests_properties(iiGUI.host_driver_policy PROPERTIES TIMEOUT 60)
    add_executable(iiGUIHostEmbeddedFrameTests tests/host_embedded_frame.cpp)
    target_link_libraries(iiGUIHostEmbeddedFrameTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.host_embedded_frame COMMAND iiGUIHostEmbeddedFrameTests)
    if(APPLE AND NOT IOS)
        add_executable(iiGUIHostExternalLoopMacTests tests/host_external_loop_mac.mm)
        target_compile_options(iiGUIHostExternalLoopMacTests PRIVATE -fobjc-arc)
        set_target_properties(iiGUIHostExternalLoopMacTests PROPERTIES OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED YES)
        target_link_libraries(iiGUIHostExternalLoopMacTests PRIVATE iiGUI::iiGUI "-framework AppKit")
        add_test(NAME iiGUI.host_external_loop_mac COMMAND iiGUIHostExternalLoopMacTests)
        set_tests_properties(iiGUI.host_external_loop_mac PROPERTIES TIMEOUT 12)
        add_executable(iiGUIHostEmbeddedMacTests tests/host_embedded_mac.mm)
        target_compile_options(iiGUIHostEmbeddedMacTests PRIVATE -fobjc-arc)
        set_target_properties(iiGUIHostEmbeddedMacTests PROPERTIES OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED YES)
        target_link_libraries(iiGUIHostEmbeddedMacTests PRIVATE iiGUI::iiGUI "-framework AppKit")
        add_test(NAME iiGUI.host_embedded_mac COMMAND iiGUIHostEmbeddedMacTests)
        set_tests_properties(iiGUI.host_embedded_mac PROPERTIES TIMEOUT 12)
        add_test(NAME iiGUI.host_embedded_parent_close_mac COMMAND iiGUIHostEmbeddedMacTests parent-close)
        set_tests_properties(iiGUI.host_embedded_parent_close_mac PROPERTIES TIMEOUT 12)
    endif()
    add_executable(iiGUIHostActivityTests tests/host_activity.cpp)
    target_compile_features(iiGUIHostActivityTests PRIVATE cxx_std_23)
    add_test(NAME iiGUI.host_activity COMMAND iiGUIHostActivityTests)
    set_tests_properties(iiGUI.host_activity PROPERTIES TIMEOUT 15)
    add_executable(iiGUIUIKitSceneBindingTests tests/uikit_scene_binding.cpp)
    target_compile_features(iiGUIUIKitSceneBindingTests PRIVATE cxx_std_23)
    add_test(NAME iiGUI.uikit_scene_binding COMMAND iiGUIUIKitSceneBindingTests)
    set_tests_properties(iiGUI.uikit_scene_binding PROPERTIES TIMEOUT 15)
    add_executable(iiGUIUIKitSceneRouterTests tests/uikit_scene_router.cpp)
    target_compile_features(iiGUIUIKitSceneRouterTests PRIVATE cxx_std_23)
    add_test(NAME iiGUI.uikit_scene_router COMMAND iiGUIUIKitSceneRouterTests)
    set_tests_properties(iiGUI.uikit_scene_router PROPERTIES TIMEOUT 15)
    add_executable(iiGUIUIKitWindowGeometryTests tests/uikit_window_geometry.cpp)
    target_link_libraries(iiGUIUIKitWindowGeometryTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.uikit_window_geometry COMMAND iiGUIUIKitWindowGeometryTests)
    set_tests_properties(iiGUI.uikit_window_geometry PROPERTIES TIMEOUT 15)
    add_executable(iiGUIHostFileRequestsTests tests/host_file_requests.cpp)
    target_link_libraries(iiGUIHostFileRequestsTests PRIVATE iiSDKCore::Contracts)
    add_test(NAME iiGUI.host_file_requests COMMAND iiGUIHostFileRequestsTests)
    set_tests_properties(iiGUI.host_file_requests PROPERTIES TIMEOUT 15)
    if(NOT ANDROID AND NOT EMSCRIPTEN)
        add_executable(iiGUIAndroidGlobalRefTests tests/android_global_ref.cpp)
        target_compile_features(iiGUIAndroidGlobalRefTests PRIVATE cxx_std_23)
        target_include_directories(iiGUIAndroidGlobalRefTests BEFORE PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/android-global" "${CMAKE_CURRENT_BINARY_DIR}/include")
        add_test(NAME iiGUI.android_global_ref COMMAND iiGUIAndroidGlobalRefTests)
        set_tests_properties(iiGUI.android_global_ref PROPERTIES TIMEOUT 15)
        add_executable(iiGUIAndroidApplicationEntryTests tests/android_application_entry.cpp
            src/Platform/AndroidApplicationEntry.cpp)
        target_include_directories(iiGUIAndroidApplicationEntryTests PRIVATE
            "${CMAKE_CURRENT_BINARY_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/android-entry")
        target_compile_definitions(iiGUIAndroidApplicationEntryTests PRIVATE IIGUI_STATIC_DEFINE)
        target_link_libraries(iiGUIAndroidApplicationEntryTests PRIVATE iiSDKCore::Runtime)
        add_test(NAME iiGUI.android_application_entry COMMAND iiGUIAndroidApplicationEntryTests)
        set_tests_properties(iiGUI.android_application_entry PROPERTIES TIMEOUT 15)
    endif()
    add_executable(iiGUIHostCallbackTests tests/host_callback.cpp)
    target_compile_features(iiGUIHostCallbackTests PRIVATE cxx_std_23)
    add_test(NAME iiGUI.host_callback COMMAND iiGUIHostCallbackTests)
    set_tests_properties(iiGUI.host_callback PROPERTIES TIMEOUT 15)
    add_executable(iiGUIApplicationDeferredStopTests tests/application_deferred_stop.cpp)
    target_link_libraries(iiGUIApplicationDeferredStopTests PRIVATE iiSDKCore::Runtime)
    add_test(NAME iiGUI.application_deferred_stop COMMAND iiGUIApplicationDeferredStopTests)
    set_tests_properties(iiGUI.application_deferred_stop PROPERTIES TIMEOUT 30)
    add_executable(iiGUIHostFailureTests tests/host_failure.cpp)
    target_link_libraries(iiGUIHostFailureTests PRIVATE iiSDKCore::Runtime)
    add_test(NAME iiGUI.host_failure COMMAND iiGUIHostFailureTests)
    set_tests_properties(iiGUI.host_failure PROPERTIES TIMEOUT 15)
    if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        execute_process(COMMAND xcrun --sdk iphoneos --show-sdk-path
            RESULT_VARIABLE _iigui_iphoneos_sdk_available OUTPUT_QUIET ERROR_QUIET)
        if(_iigui_iphoneos_sdk_available EQUAL 0)
            add_test(NAME iiGUI.uikit_fullscreen_layout COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/uikit_fullscreen_layout.py")
            set_tests_properties(iiGUI.uikit_fullscreen_layout PROPERTIES TIMEOUT 15)
            add_test(NAME iiGUI.uikit_driver_completion_compile COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/uikit_driver_completion_compile.py"
                --build "${CMAKE_CURRENT_BINARY_DIR}"
                --core-includes "$<JOIN:$<TARGET_PROPERTY:iiSDKCore::Contracts,INTERFACE_INCLUDE_DIRECTORIES>,|>")
            set_tests_properties(iiGUI.uikit_driver_completion_compile PROPERTIES TIMEOUT 120)
            add_test(NAME iiGUI.uikit_package_dependencies COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/uikit_package_dependencies.py" --build "${CMAKE_CURRENT_BINARY_DIR}"
                --vulkan-include "${Vulkan_INCLUDE_DIR}" --vulkan-library "${Vulkan_LIBRARY}")
            set_tests_properties(iiGUI.uikit_package_dependencies PROPERTIES TIMEOUT 120)
        endif()
        add_executable(iiGUIHostActivityApiTests tests/host_activity_api.cpp)
        target_link_libraries(iiGUIHostActivityApiTests PRIVATE iiGUI::iiGUI)
        add_test(NAME iiGUI.host_activity_api COMMAND iiGUIHostActivityApiTests)
        set_tests_properties(iiGUI.host_activity_api PROPERTIES TIMEOUT 15)
        add_executable(iiGUIHostForegroundApiTests tests/host_foreground_api.cpp)
        target_link_libraries(iiGUIHostForegroundApiTests PRIVATE iiGUI::iiGUI)
        add_test(NAME iiGUI.host_foreground_api COMMAND iiGUIHostForegroundApiTests)
        set_tests_properties(iiGUI.host_foreground_api PROPERTIES TIMEOUT 15)
        add_executable(iiGUICocoaForegroundTests tests/cocoa_foreground.cpp)
        target_compile_features(iiGUICocoaForegroundTests PRIVATE cxx_std_23)
        add_test(NAME iiGUI.cocoa_foreground COMMAND iiGUICocoaForegroundTests)
        set_tests_properties(iiGUI.cocoa_foreground PROPERTIES TIMEOUT 15)
        add_executable(iiGUICocoaForegroundDelegateTests tests/cocoa_foreground_delegate.mm src/Platform/CocoaHostDelegate.mm)
        target_link_libraries(iiGUICocoaForegroundDelegateTests PRIVATE iiSDKCore::Contracts "-framework AppKit" "-framework QuartzCore")
        set_target_properties(iiGUICocoaForegroundDelegateTests PROPERTIES OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED YES)
        target_compile_options(iiGUICocoaForegroundDelegateTests PRIVATE -fobjc-arc)
        add_test(NAME iiGUI.cocoa_foreground_delegate COMMAND iiGUICocoaForegroundDelegateTests)
        set_tests_properties(iiGUI.cocoa_foreground_delegate PROPERTIES TIMEOUT 15)
        add_executable(iiGUIHostFailureApiTests tests/host_failure_api.cpp)
        target_link_libraries(iiGUIHostFailureApiTests PRIVATE iiGUI::iiGUI)
        add_test(NAME iiGUI.host_failure_api COMMAND iiGUIHostFailureApiTests)
        set_tests_properties(iiGUI.host_failure_api PROPERTIES TIMEOUT 15)
    endif()
    find_program(_iigui_hidpi_node NAMES node)
    if(_iigui_hidpi_node)
        add_test(NAME iiGUI.web_display_observer COMMAND "${_iigui_hidpi_node}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/web_display_observer.js")
        set_tests_properties(iiGUI.web_display_observer PROPERTIES TIMEOUT 15)
    endif()
    add_executable(iiGUIWaylandPolicyTests tests/wayland_policy.cpp)
    target_link_libraries(iiGUIWaylandPolicyTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.wayland_policy COMMAND iiGUIWaylandPolicyTests)
    set_tests_properties(iiGUI.wayland_policy PROPERTIES TIMEOUT 15)
    add_executable(iiGUIWaylandImeProtocolTests tests/wayland_ime_protocol.cpp)
    target_link_libraries(iiGUIWaylandImeProtocolTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.wayland_ime_protocol COMMAND iiGUIWaylandImeProtocolTests)
    set_tests_properties(iiGUI.wayland_ime_protocol PROPERTIES TIMEOUT 30)
    add_executable(iiGUIWaylandImeSessionTests tests/wayland_ime_session.cpp)
    target_link_libraries(iiGUIWaylandImeSessionTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.wayland_ime_session COMMAND iiGUIWaylandImeSessionTests)
    set_tests_properties(iiGUI.wayland_ime_session PROPERTIES TIMEOUT 30)
    add_executable(iiGUIWaylandWaitGroupTests tests/wayland_wait_group.cpp)
    target_compile_features(iiGUIWaylandWaitGroupTests PRIVATE cxx_std_23)
    add_test(NAME iiGUI.wayland_wait_group COMMAND iiGUIWaylandWaitGroupTests)
    set_tests_properties(iiGUI.wayland_wait_group PROPERTIES TIMEOUT 30)
    add_executable(iiGUIWaylandTouchTests tests/wayland_touch.cpp)
    target_link_libraries(iiGUIWaylandTouchTests PRIVATE iiGUI::iiGUI)
    target_compile_features(iiGUIWaylandTouchTests PRIVATE cxx_std_23)
    add_test(NAME iiGUI.wayland_touch COMMAND iiGUIWaylandTouchTests)
    add_executable(iiGUIWaylandConnectionRegistryTests tests/wayland_connection_registry.cpp)
    target_compile_features(iiGUIWaylandConnectionRegistryTests PRIVATE cxx_std_23)
    add_test(NAME iiGUI.wayland_connection_registry COMMAND iiGUIWaylandConnectionRegistryTests)
    set_tests_properties(iiGUI.wayland_connection_registry PROPERTIES TIMEOUT 30)
    add_executable(iiGUIWaylandChildrenTests tests/wayland_children.cpp)
    target_compile_features(iiGUIWaylandChildrenTests PRIVATE cxx_std_23)
    add_test(NAME iiGUI.wayland_children COMMAND iiGUIWaylandChildrenTests)
    set_tests_properties(iiGUI.wayland_children PROPERTIES TIMEOUT 30)
    add_executable(iiGUIWaylandCapabilitiesTests tests/wayland_capabilities.cpp src/Platform/HostCapabilities.cpp)
    target_include_directories(iiGUIWaylandCapabilitiesTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/include")
    target_link_libraries(iiGUIWaylandCapabilitiesTests PRIVATE iiSDKCore::Contracts)
    target_compile_definitions(iiGUIWaylandCapabilitiesTests PRIVATE IIGUI_STATIC_DEFINE IIGUI_WAYLAND)
    target_compile_features(iiGUIWaylandCapabilitiesTests PRIVATE cxx_std_23)
    add_test(NAME iiGUI.wayland_capabilities COMMAND iiGUIWaylandCapabilitiesTests)
    set_tests_properties(iiGUI.wayland_capabilities PROPERTIES TIMEOUT 30)
    if(UNIX)
        add_executable(iiGUIWaylandPollTests tests/wayland_poll.cpp)
        target_compile_features(iiGUIWaylandPollTests PRIVATE cxx_std_23)
        add_test(NAME iiGUI.wayland_poll COMMAND iiGUIWaylandPollTests)
        set_tests_properties(iiGUI.wayland_poll PROPERTIES TIMEOUT 30)
    endif()
    if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        add_executable(iiGUIWaylandMockHostTests src/Platform/HostWayland.cpp
            tests/wayland_mock_host.cpp tests/wayland_mocks/WaylandConnectionMock.cpp
            tests/wayland_mocks/WidgetSurfaceMock.cpp)
        target_include_directories(iiGUIWaylandMockHostTests BEFORE PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/wayland_mocks" "${CMAKE_CURRENT_BINARY_DIR}/include")
        target_link_libraries(iiGUIWaylandMockHostTests PRIVATE iiGUI::iiGUI)
        target_compile_features(iiGUIWaylandMockHostTests PRIVATE cxx_std_23)
        add_test(NAME iiGUI.wayland_mock_host COMMAND iiGUIWaylandMockHostTests)
        set_tests_properties(iiGUI.wayland_mock_host PROPERTIES TIMEOUT 30)
        add_executable(iiGUIWaylandImeSurfaceTests src/Widgets/NativeWidgetsWayland.cpp
            tests/wayland_ime_surface.cpp tests/wayland_mocks/WaylandConnectionMock.cpp
            tests/wayland_mocks/NativeCanvasMock.cpp)
        target_include_directories(iiGUIWaylandImeSurfaceTests BEFORE PRIVATE
            "${CMAKE_CURRENT_BINARY_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src")
        target_link_libraries(iiGUIWaylandImeSurfaceTests PRIVATE iiGUI::iiGUI)
        target_compile_features(iiGUIWaylandImeSurfaceTests PRIVATE cxx_std_23)
        add_test(NAME iiGUI.wayland_ime_surface COMMAND iiGUIWaylandImeSurfaceTests)
        set_tests_properties(iiGUI.wayland_ime_surface PROPERTIES TIMEOUT 30)
    endif()
    add_executable(iiGUIHostInputQueueTests tests/host_input_queue.cpp)
    target_link_libraries(iiGUIHostInputQueueTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.host_input_queue COMMAND iiGUIHostInputQueueTests)
    set_tests_properties(iiGUI.host_input_queue PROPERTIES TIMEOUT 15)
    add_executable(iiGUISurfaceRegistryTests tests/surface_registry.cpp)
    target_include_directories(iiGUISurfaceRegistryTests PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/include")
    target_compile_features(iiGUISurfaceRegistryTests PRIVATE cxx_std_23)
    add_test(NAME iiGUI.surface_registry COMMAND iiGUISurfaceRegistryTests)
    set_tests_properties(iiGUI.surface_registry PROPERTIES TIMEOUT 15)
    add_executable(iiGUIPendingCallbackTests tests/pending_callbacks.cpp)
    target_include_directories(iiGUIPendingCallbackTests PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/include")
    target_compile_features(iiGUIPendingCallbackTests PRIVATE cxx_std_23)
    add_test(NAME iiGUI.pending_callbacks COMMAND iiGUIPendingCallbackTests)
    set_tests_properties(iiGUI.pending_callbacks PROPERTIES TIMEOUT 15)
    add_executable(iiGUIWin32ForegroundTests tests/win32_foreground.cpp)
    if(NOT WIN32)
        target_include_directories(iiGUIWin32ForegroundTests BEFORE PRIVATE tests/win32_foreground_fake)
    endif()
    target_compile_features(iiGUIWin32ForegroundTests PRIVATE cxx_std_23)
    add_test(NAME iiGUI.win32_foreground COMMAND iiGUIWin32ForegroundTests)
    set_tests_properties(iiGUI.win32_foreground PROPERTIES TIMEOUT 15)
    add_executable(iiGUIX11ForegroundTests tests/x11_foreground.cpp)
    if(iiGUI_HOST_BACKEND STREQUAL "x11")
        target_include_directories(iiGUIX11ForegroundTests PRIVATE "${X11_INCLUDE_DIR}")
    else()
        target_include_directories(iiGUIX11ForegroundTests BEFORE PRIVATE tests/x11_foreground_fake)
    endif()
    target_compile_features(iiGUIX11ForegroundTests PRIVATE cxx_std_23)
    add_test(NAME iiGUI.x11_foreground COMMAND iiGUIX11ForegroundTests)
    set_tests_properties(iiGUI.x11_foreground PROPERTIES TIMEOUT 15)
    if(NOT ANDROID)
        foreach(_android_api IN ITEMS 24 29)
            add_executable(iiGUIAndroidFrameApi${_android_api}Tests tests/android_frame_cadence.cpp)
            target_include_directories(iiGUIAndroidFrameApi${_android_api}Tests BEFORE PRIVATE tests/android_frame_fake src)
            target_compile_features(iiGUIAndroidFrameApi${_android_api}Tests PRIVATE cxx_std_23)
            target_compile_definitions(iiGUIAndroidFrameApi${_android_api}Tests PRIVATE __ANDROID_API__=${_android_api})
            add_test(NAME iiGUI.android_frame_api_${_android_api} COMMAND iiGUIAndroidFrameApi${_android_api}Tests)
            set_tests_properties(iiGUI.android_frame_api_${_android_api} PROPERTIES TIMEOUT 15)
        endforeach()
    endif()
    add_executable(iiGUIFontFaceReferenceTests tests/font_face_reference.cpp)
    target_include_directories(iiGUIFontFaceReferenceTests PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/include")
    target_compile_features(iiGUIFontFaceReferenceTests PRIVATE cxx_std_23)
    add_test(NAME iiGUI.font_face_reference COMMAND iiGUIFontFaceReferenceTests)
    set_tests_properties(iiGUI.font_face_reference PROPERTIES TIMEOUT 15)
    if(UNIX AND NOT EMSCRIPTEN)
        add_executable(iiGUIPipeWriteTests tests/pipe_write.cpp)
        target_link_libraries(iiGUIPipeWriteTests PRIVATE iiGUI::iiGUI)
        add_test(NAME iiGUI.pipe_write COMMAND iiGUIPipeWriteTests)
        set_tests_properties(iiGUI.pipe_write PROPERTIES TIMEOUT 15)
    endif()
    add_executable(iiGUIFontDatabaseTests tests/font_database.cpp)
    target_link_libraries(iiGUIFontDatabaseTests PRIVATE iiGUI::iiGUI)
    foreach(_case defaults load resources missing refresh application absent substitution stack fold validation observation failure owner indexed retirement)
        add_test(NAME iiGUI.fonts_${_case} COMMAND iiGUIFontDatabaseTests ${_case})
        set_tests_properties(iiGUI.fonts_${_case} PROPERTIES TIMEOUT 30)
    endforeach()
    add_test(NAME iiGUI.fonts_native COMMAND iiGUIFontDatabaseTests native "${CMAKE_CURRENT_SOURCE_DIR}/resources/native")
    set_tests_properties(iiGUI.fonts_native PROPERTIES TIMEOUT 30)
    add_executable(iiGUIFontMetadataTests tests/font_metadata.cpp)
    target_link_libraries(iiGUIFontMetadataTests PRIVATE iiGUI::iiGUI)
    foreach(_case pinned missing short unsupported bounds collection standalone)
        add_test(NAME iiGUI.font_metadata_${_case} COMMAND iiGUIFontMetadataTests
            ${_case} "${CMAKE_CURRENT_SOURCE_DIR}/resources/native")
        set_tests_properties(iiGUI.font_metadata_${_case} PROPERTIES TIMEOUT 30)
    endforeach()
    add_executable(iiGUIFontWidgetTests tests/font_widgets.cpp)
    target_link_libraries(iiGUIFontWidgetTests PRIVATE iiGUI::iiGUI)
    foreach(_case tokens updates identity retirement paint_context)
        add_test(NAME iiGUI.font_widgets_${_case} COMMAND iiGUIFontWidgetTests ${_case})
        set_tests_properties(iiGUI.font_widgets_${_case} PROPERTIES TIMEOUT 30)
    endforeach()
    add_executable(iiGUIAppearancePolicyTests tests/appearance_policy.cpp)
    target_link_libraries(iiGUIAppearancePolicyTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.appearance_policy COMMAND iiGUIAppearancePolicyTests)
    set_tests_properties(iiGUI.appearance_policy PROPERTIES TIMEOUT 30)
    if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS" AND NOT BUILD_SHARED_LIBS)
        add_executable(iiGUIAppearanceBoundaryProbe tests/appearance_boundary.cpp)
        target_link_libraries(iiGUIAppearanceBoundaryProbe PRIVATE iiGUI::iiGUI)
        add_test(NAME iiGUI.appearance_boundary COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/appearance_boundary.py" "$<TARGET_FILE:iiGUIAppearanceBoundaryProbe>" "${CMAKE_NM}")
        set_tests_properties(iiGUI.appearance_boundary PROPERTIES TIMEOUT 30)
        add_test(NAME iiGUI.appearance_link_compat COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/appearance_link_compat.py" "${CMAKE_CXX_COMPILER}"
            "${CMAKE_CURRENT_BINARY_DIR}/include" "$<TARGET_FILE:iiGUI>")
        set_tests_properties(iiGUI.appearance_link_compat PROPERTIES TIMEOUT 30)
        add_test(NAME iiGUI.host_driver_link_compat COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/host_driver_link_compat.py" "${CMAKE_CXX_COMPILER}" "$<TARGET_FILE:iiGUI>")
        set_tests_properties(iiGUI.host_driver_link_compat PROPERTIES TIMEOUT 30)
    endif()
    add_executable(iiGUIRetainedLabelsTests tests/retained_labels.cpp)
    target_link_libraries(iiGUIRetainedLabelsTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.retained_labels COMMAND iiGUIRetainedLabelsTests)
    set_tests_properties(iiGUI.retained_labels PROPERTIES TIMEOUT 15)
    add_executable(iiGUIAndroidLabelFontTests tests/android_label_font.cpp src/Text/AndroidLabelFont.cpp)
    target_compile_features(iiGUIAndroidLabelFontTests PRIVATE cxx_std_23)
    target_include_directories(iiGUIAndroidLabelFontTests PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/include")
    target_include_directories(iiGUIAndroidLabelFontTests SYSTEM PRIVATE "${lunasvg_SOURCE_DIR}/plutovg/source")
    add_test(NAME iiGUI.android_label_font COMMAND iiGUIAndroidLabelFontTests "${CMAKE_CURRENT_SOURCE_DIR}/resources/native/Pretendard-Regular.ttf")
    set_tests_properties(iiGUI.android_label_font PROPERTIES TIMEOUT 15)
    add_executable(iiGUIAndroidTextLayoutTests tests/android_text_layout.cpp)
    target_include_directories(iiGUIAndroidTextLayoutTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/include")
    target_compile_features(iiGUIAndroidTextLayoutTests PRIVATE cxx_std_23)
    add_test(NAME iiGUI.android_text_layout COMMAND iiGUIAndroidTextLayoutTests)
    set_tests_properties(iiGUI.android_text_layout PROPERTIES TIMEOUT 15)
    add_executable(iiGUIAndroidTextLineModelTests tests/android_text_line_model.cpp)
    target_link_libraries(iiGUIAndroidTextLineModelTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.android_text_line_model COMMAND iiGUIAndroidTextLineModelTests)
    set_tests_properties(iiGUI.android_text_line_model PROPERTIES TIMEOUT 15)
    add_executable(iiGUIAndroidKeyboardTests tests/android_keyboard.cpp)
    target_link_libraries(iiGUIAndroidKeyboardTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.android_keyboard COMMAND iiGUIAndroidKeyboardTests)
    set_tests_properties(iiGUI.android_keyboard PROPERTIES TIMEOUT 15)
    add_executable(iiGUIAndroidKeyboardWidgetTests tests/android_keyboard_widgets.cpp)
    target_link_libraries(iiGUIAndroidKeyboardWidgetTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.android_keyboard_widgets COMMAND iiGUIAndroidKeyboardWidgetTests)
    set_tests_properties(iiGUI.android_keyboard_widgets PROPERTIES TIMEOUT 15)
    add_executable(iiGUITextSelectionInputTests tests/text_selection_input.cpp)
    target_link_libraries(iiGUITextSelectionInputTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.text_selection_input COMMAND iiGUITextSelectionInputTests)
    set_tests_properties(iiGUI.text_selection_input PROPERTIES TIMEOUT 15)
    add_executable(iiGUITextGeometryTests tests/text_geometry.cpp)
    target_link_libraries(iiGUITextGeometryTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.text_geometry COMMAND iiGUITextGeometryTests)
    set_tests_properties(iiGUI.text_geometry PROPERTIES TIMEOUT 15)
    add_executable(iiGUIUIKitTextInputProtocolTests tests/uikit_text_input_protocol.cpp)
    target_link_libraries(iiGUIUIKitTextInputProtocolTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.uikit_text_input_protocol COMMAND iiGUIUIKitTextInputProtocolTests)
    set_tests_properties(iiGUI.uikit_text_input_protocol PROPERTIES TIMEOUT 15)
    add_executable(iiGUIWin32ImeProtocolTests tests/win32_ime_protocol.cpp)
    target_link_libraries(iiGUIWin32ImeProtocolTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.win32_ime_protocol COMMAND iiGUIWin32ImeProtocolTests)
    set_tests_properties(iiGUI.win32_ime_protocol PROPERTIES TIMEOUT 15)
    add_executable(iiGUIAndroidImeProtocolTests tests/android_ime_protocol.cpp)
    target_link_libraries(iiGUIAndroidImeProtocolTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.android_ime_protocol COMMAND iiGUIAndroidImeProtocolTests)
    set_tests_properties(iiGUI.android_ime_protocol PROPERTIES TIMEOUT 15)
    add_executable(iiGUIAndroidImeDispatchTests tests/android_ime_dispatch.cpp)
    target_link_libraries(iiGUIAndroidImeDispatchTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.android_ime_dispatch COMMAND iiGUIAndroidImeDispatchTests)
    set_tests_properties(iiGUI.android_ime_dispatch PROPERTIES TIMEOUT 15)
    add_executable(iiGUIAndroidKeyboardDispatchTests tests/android_keyboard_dispatch.cpp)
    target_link_libraries(iiGUIAndroidKeyboardDispatchTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.android_keyboard_dispatch COMMAND iiGUIAndroidKeyboardDispatchTests)
    set_tests_properties(iiGUI.android_keyboard_dispatch PROPERTIES TIMEOUT 15)
    add_executable(iiGUIAndroidPointerTests tests/android_pointer.cpp)
    target_compile_features(iiGUIAndroidPointerTests PRIVATE cxx_std_23)
    target_include_directories(iiGUIAndroidPointerTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/include")
    target_link_libraries(iiGUIAndroidPointerTests PRIVATE iiSDKCore::Contracts)
    add_test(NAME iiGUI.android_pointer COMMAND iiGUIAndroidPointerTests)
    set_tests_properties(iiGUI.android_pointer PROPERTIES TIMEOUT 15)
    add_executable(iiGUIAndroidPointerWidgetTests tests/android_pointer_widgets.cpp)
    target_link_libraries(iiGUIAndroidPointerWidgetTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.android_pointer_widgets COMMAND iiGUIAndroidPointerWidgetTests)
    set_tests_properties(iiGUI.android_pointer_widgets PROPERTIES TIMEOUT 15)
    if(NOT ANDROID AND NOT EMSCRIPTEN)
        add_executable(iiGUIAndroidKeyboardMapTests tests/android_keyboard_map.cpp)
        target_compile_features(iiGUIAndroidKeyboardMapTests PRIVATE cxx_std_23)
        target_include_directories(iiGUIAndroidKeyboardMapTests BEFORE PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/android-keyboard" "${CMAKE_CURRENT_BINARY_DIR}/include")
        add_test(NAME iiGUI.android_keyboard_map COMMAND iiGUIAndroidKeyboardMapTests)
        set_tests_properties(iiGUI.android_keyboard_map PROPERTIES TIMEOUT 15)
        add_executable(iiGUIAndroidKeyboardNativeTests tests/android_keyboard_native.cpp)
        target_compile_features(iiGUIAndroidKeyboardNativeTests PRIVATE cxx_std_23)
        target_include_directories(iiGUIAndroidKeyboardNativeTests BEFORE PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/android-keyboard" "${CMAKE_CURRENT_BINARY_DIR}/include")
        target_link_libraries(iiGUIAndroidKeyboardNativeTests PRIVATE iiSDKCore::Contracts)
        add_test(NAME iiGUI.android_keyboard_native COMMAND iiGUIAndroidKeyboardNativeTests)
        set_tests_properties(iiGUI.android_keyboard_native PROPERTIES TIMEOUT 15)
        add_executable(iiGUIAndroidPointerNativeTests tests/android_pointer_native.cpp)
        target_compile_features(iiGUIAndroidPointerNativeTests PRIVATE cxx_std_23)
        target_include_directories(iiGUIAndroidPointerNativeTests BEFORE PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/android-pointer" "${CMAKE_CURRENT_BINARY_DIR}/include")
        target_link_libraries(iiGUIAndroidPointerNativeTests PRIVATE iiSDKCore::Contracts)
        add_test(NAME iiGUI.android_pointer_native COMMAND iiGUIAndroidPointerNativeTests)
        set_tests_properties(iiGUI.android_pointer_native PROPERTIES TIMEOUT 15)
        add_executable(iiGUIAndroidTextBackendTests tests/android_text_backend.cpp src/Text/TextAndroid.cpp)
        target_compile_features(iiGUIAndroidTextBackendTests PRIVATE cxx_std_23)
        target_compile_definitions(iiGUIAndroidTextBackendTests PRIVATE IIGUI_STATIC_DEFINE)
        target_include_directories(iiGUIAndroidTextBackendTests BEFORE PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/android-text" "${CMAKE_CURRENT_BINARY_DIR}/include")
        target_link_libraries(iiGUIAndroidTextBackendTests PRIVATE iiSDKCore::Contracts)
        add_test(NAME iiGUI.android_text_backend COMMAND iiGUIAndroidTextBackendTests)
        set_tests_properties(iiGUI.android_text_backend PROPERTIES TIMEOUT 15)
        add_executable(iiGUIAndroidTextLineBackendTests tests/android_text_line_backend.cpp
            src/Text/TextLine.cpp src/Text/TextLineAndroid.cpp)
        target_compile_features(iiGUIAndroidTextLineBackendTests PRIVATE cxx_std_23)
        target_include_directories(iiGUIAndroidTextLineBackendTests BEFORE PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/android-editor"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/android-text" "${CMAKE_CURRENT_BINARY_DIR}/include")
        target_link_libraries(iiGUIAndroidTextLineBackendTests PRIVATE iiGUI::iiGUI)
        add_test(NAME iiGUI.android_text_line_backend COMMAND iiGUIAndroidTextLineBackendTests)
        set_tests_properties(iiGUI.android_text_line_backend PROPERTIES TIMEOUT 15)
    endif()
    set(IIGUI_TEST_ANDROID_JAR "$ENV{ANDROID_HOME}/platforms/android-35/android.jar" CACHE FILEPATH
        "Android API jar for verifying native text JNI signatures")
    find_program(IIGUI_TEST_JAVAP NAMES javap HINTS "$ENV{JAVA_HOME}/bin")
    find_program(IIGUI_TEST_JAVAC NAMES javac HINTS "$ENV{JAVA_HOME}/bin")
    find_program(IIGUI_TEST_D8 NAMES d8 HINTS "$ENV{ANDROID_HOME}/build-tools/35.0.1" "$ENV{ANDROID_HOME}/build-tools/35.0.0")
    if(IIGUI_TEST_JAVAP AND EXISTS "${IIGUI_TEST_ANDROID_JAR}")
        find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
        add_test(NAME iiGUI.android_text_signatures COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/android_text_signatures.py"
            --jar "${IIGUI_TEST_ANDROID_JAR}" --javap "${IIGUI_TEST_JAVAP}")
        set_tests_properties(iiGUI.android_text_signatures PROPERTIES TIMEOUT 60)
        add_test(NAME iiGUI.android_text_line_signatures COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/android_text_line_signatures.py"
            --jar "${IIGUI_TEST_ANDROID_JAR}" --javap "${IIGUI_TEST_JAVAP}")
        set_tests_properties(iiGUI.android_text_line_signatures PROPERTIES TIMEOUT 60)
        add_test(NAME iiGUI.android_keyboard_signatures COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/android_keyboard_signatures.py"
            --jar "${IIGUI_TEST_ANDROID_JAR}" --javap "${IIGUI_TEST_JAVAP}")
        set_tests_properties(iiGUI.android_keyboard_signatures PROPERTIES TIMEOUT 60)
        add_test(NAME iiGUI.android_pointer_constants COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/android_pointer_constants.py"
            --jar "${IIGUI_TEST_ANDROID_JAR}" --javap "${IIGUI_TEST_JAVAP}")
        set_tests_properties(iiGUI.android_pointer_constants PROPERTIES TIMEOUT 60)
        if(IIGUI_TEST_JAVAC AND IIGUI_TEST_D8)
            add_test(NAME iiGUI.android_ime_signatures COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/android_ime_signatures.py"
                --jar "${IIGUI_TEST_ANDROID_JAR}" --javac "${IIGUI_TEST_JAVAC}"
                --javap "${IIGUI_TEST_JAVAP}" --d8 "${IIGUI_TEST_D8}" --build "${CMAKE_CURRENT_BINARY_DIR}")
            set_tests_properties(iiGUI.android_ime_signatures PROPERTIES TIMEOUT 60)
        endif()
    endif()
    add_executable(iiGUIAncestorInputTests tests/ancestor_input.cpp)
    target_link_libraries(iiGUIAncestorInputTests PRIVATE iiGUI::iiGUI)
    add_executable(iiGUINonfocusActionTests tests/nonfocus_action.cpp)
    target_link_libraries(iiGUINonfocusActionTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.action_surface_nonfocus COMMAND iiGUINonfocusActionTests)
    set_tests_properties(iiGUI.action_surface_nonfocus PROPERTIES TIMEOUT 30)
    foreach(_case ancestor text adjustment passthrough reentrant_release)
        add_test(NAME iiGUI.ancestor_input.${_case} COMMAND iiGUIAncestorInputTests ${_case})
        set_tests_properties(iiGUI.ancestor_input.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(iiGUIUserScrollTests tests/user_scroll.cpp)
    target_link_libraries(iiGUIUserScrollTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.user_scroll COMMAND iiGUIUserScrollTests)
    set_tests_properties(iiGUI.user_scroll PROPERTIES TIMEOUT 15)
    add_executable(iiGUITouchInertiaTests tests/touch_inertia.cpp)
    target_link_libraries(iiGUITouchInertiaTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.touch_inertia COMMAND iiGUITouchInertiaTests)
    set_tests_properties(iiGUI.touch_inertia PROPERTIES TIMEOUT 30)
    add_executable(iiGUIRevealLocalRectTests tests/reveal_local_rect.cpp)
    target_link_libraries(iiGUIRevealLocalRectTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.reveal_local_rect COMMAND iiGUIRevealLocalRectTests)
    add_executable(iiGUIInteractionHandlerTests tests/interaction_handler.cpp)
    target_link_libraries(iiGUIInteractionHandlerTests PRIVATE iiGUI::iiGUI)
    foreach(_case committed reentrant fanout retirement capacity owner programmatic_focus)
        add_test(NAME iiGUI.interaction_handler.${_case} COMMAND iiGUIInteractionHandlerTests ${_case})
        set_tests_properties(iiGUI.interaction_handler.${_case} PROPERTIES TIMEOUT 30)
    endforeach()
    add_executable(iiGUIPaintOnlyInvalidationTests tests/paint_only_invalidation.cpp)
    target_link_libraries(iiGUIPaintOnlyInvalidationTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.paint_only_invalidation COMMAND iiGUIPaintOnlyInvalidationTests)
    set_tests_properties(iiGUI.paint_only_invalidation PROPERTIES TIMEOUT 30)
    add_executable(iiGUIKeyedContentTests tests/keyed_content.cpp)
    target_link_libraries(iiGUIKeyedContentTests PRIVATE iiGUI::iiGUI)
    foreach(_case identity rollback commit collapse nested_layout bounded capacity retirement popup positioned virtual_scroll measurement)
        add_test(NAME iiGUI.keyed_content.${_case} COMMAND iiGUIKeyedContentTests ${_case})
        set_tests_properties(iiGUI.keyed_content.${_case} PROPERTIES TIMEOUT 30)
    endforeach()
    add_executable(iiGUIWidgetRetirementTests tests/widget_retirement.cpp)
    target_link_libraries(iiGUIWidgetRetirementTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.widget_retirement COMMAND iiGUIWidgetRetirementTests)
    add_executable(iiGUIWidgetPublicationTests tests/widget_publication.cpp)
    target_link_libraries(iiGUIWidgetPublicationTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.widget_publication COMMAND iiGUIWidgetPublicationTests)
    set_tests_properties(iiGUI.widget_retirement PROPERTIES TIMEOUT 15)
    option(IIGUI_BUILD_LVUI_INTEGRATION_TESTS "Build optional application-composition tests with an LVUI checkout" OFF)
    set(iiGUI_LVUI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../LVUI" CACHE PATH "Optional LVUI source for headless generated-widget integration")
    if(IIGUI_BUILD_LVUI_INTEGRATION_TESTS AND NOT CMAKE_CROSSCOMPILING AND EXISTS "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}" AND
       EXISTS "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        set(_conditional_generated "${CMAKE_CURRENT_BINARY_DIR}/NativeConditionalRuntime.generated.h")
        set(_conditional_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/NativeConditional.lvui")
        set(_opacity_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/NativeOpacity.lvui")
        set(_opacity_header "${CMAKE_CURRENT_BINARY_DIR}/NativeOpacityRuntime.generated.h")
        add_custom_command(OUTPUT "${_opacity_header}"
            COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                "-DLVUI_AOT_SOURCE=${_opacity_source}" "-DLVUI_AOT_OUTPUT=${_opacity_header}"
                "-DLVUI_AOT_NAME=NativeOpacityRuntime" "-DLVUI_AOT_INCLUDES=NativeOpacityFixture.h"
                -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
            DEPENDS "${_opacity_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}" VERBATIM)
        add_executable(iiGUINativeOpacityTests "${iiGUI_LVUI_SOURCE_DIR}/tests/native_opacity.cpp" "${_opacity_header}")
        target_include_directories(iiGUINativeOpacityTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeOpacityTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case initial updates invalid zero identity conditional retirement nonmutation)
            add_test(NAME iiGUI.native_opacity.${_case} COMMAND iiGUINativeOpacityTests ${_case})
            set_tests_properties(iiGUI.native_opacity.${_case} PROPERTIES TIMEOUT 25)
        endforeach()
        add_custom_command(OUTPUT "${_conditional_generated}"
            COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                "-DLVUI_AOT_SOURCE=${_conditional_source}" "-DLVUI_AOT_OUTPUT=${_conditional_generated}"
                "-DLVUI_AOT_NAME=NativeConditionalRuntime" "-DLVUI_AOT_INCLUDES=NativeFixture.h"
                -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
            DEPENDS "${_conditional_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
            VERBATIM)
        add_executable(iiGUINativeConditionalTests tests/native_conditional.cpp "${_conditional_generated}")
        target_include_directories(iiGUINativeConditionalTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeConditionalTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        add_test(NAME iiGUI.native_conditional COMMAND iiGUINativeConditionalTests)
        set_tests_properties(iiGUI.native_conditional PROPERTIES TIMEOUT 15)
        set(_view_inputs_generated "${CMAKE_CURRENT_BINARY_DIR}/NativeViewInputsRuntime.generated.h")
        set(_view_inputs_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/NativeViewInputs.lvui")
        add_custom_command(OUTPUT "${_view_inputs_generated}"
            COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                "-DLVUI_AOT_SOURCE=${_view_inputs_source}" "-DLVUI_AOT_OUTPUT=${_view_inputs_generated}"
                "-DLVUI_AOT_NAME=NativeViewInputsRuntime" "-DLVUI_AOT_INCLUDES=NativeFixture.h"
                -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
            DEPENDS "${_view_inputs_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
            VERBATIM)
        add_executable(iiGUINativeViewInputTests tests/native_view_inputs.cpp "${_view_inputs_generated}")
        target_include_directories(iiGUINativeViewInputTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeViewInputTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        add_test(NAME iiGUI.native_view_inputs COMMAND iiGUINativeViewInputTests)
        set_tests_properties(iiGUI.native_view_inputs PROPERTIES TIMEOUT 15)
        set(_native_visual_generated)
        foreach(_visual NativeVisual NativeVisualConditional NativeVisualEmpty)
            set(_output "${CMAKE_CURRENT_BINARY_DIR}/${_visual}Runtime.generated.h")
            add_custom_command(OUTPUT "${_output}"
                COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                    "-DLVUI_AOT_SOURCE=${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/${_visual}.lvui"
                    "-DLVUI_AOT_OUTPUT=${_output}" "-DLVUI_AOT_NAME=${_visual}Runtime" "-DLVUI_AOT_INCLUDES=NativeVisualFixture.h"
                    -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
                DEPENDS "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/${_visual}.lvui" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                VERBATIM)
            list(APPEND _native_visual_generated "${_output}")
        endforeach()
        add_executable(iiGUINativeVisualAotTests tests/native_visual_aot.cpp ${_native_visual_generated})
        target_include_directories(iiGUINativeVisualAotTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        if(IIGUI_SANITIZE)
            set(_visual_native_library "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-completion/sanitizers/build/libLVUINative.a")
        else()
            set(_visual_native_library "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        endif()
        target_link_libraries(iiGUINativeVisualAotTests PRIVATE iiGUI::iiGUI "${_visual_native_library}")
        foreach(_case initial identity selection rollback invalid conditional retirement empty)
            add_test(NAME iiGUI.native_visual_aot_${_case} COMMAND iiGUINativeVisualAotTests ${_case})
            set_tests_properties(iiGUI.native_visual_aot_${_case} PROPERTIES TIMEOUT 30)
        endforeach()
        set(_components_generated "${CMAKE_CURRENT_BINARY_DIR}/NativeComponentsRuntime.generated.h")
        set(_components_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/NativeComponents.lvui")
        add_custom_command(OUTPUT "${_components_generated}"
            COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                "-DLVUI_AOT_SOURCE=${_components_source}" "-DLVUI_AOT_OUTPUT=${_components_generated}"
                "-DLVUI_AOT_NAME=NativeComponentsRuntime" "-DLVUI_AOT_INCLUDES=NativeFixture.h"
                -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
            DEPENDS "${_components_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
            VERBATIM)
        set(_component_retirement_generated "${CMAKE_CURRENT_BINARY_DIR}/NativeComponentRetirementRuntime.generated.h")
        set(_component_retirement_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/NativeComponentRetirement.lvui")
        add_custom_command(OUTPUT "${_component_retirement_generated}"
            COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                "-DLVUI_AOT_SOURCE=${_component_retirement_source}" "-DLVUI_AOT_OUTPUT=${_component_retirement_generated}"
                "-DLVUI_AOT_NAME=NativeComponentRetirementRuntime" "-DLVUI_AOT_INCLUDES=NativeFixture.h"
                -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
            DEPENDS "${_component_retirement_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
            VERBATIM)
        add_executable(iiGUINativeComponentTests tests/native_components.cpp "${_components_generated}" "${_component_retirement_generated}")
        target_include_directories(iiGUINativeComponentTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeComponentTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        add_test(NAME iiGUI.native_components COMMAND iiGUINativeComponentTests)
        set_tests_properties(iiGUI.native_components PROPERTIES TIMEOUT 15)
        set(_callable_inputs_generated "${CMAKE_CURRENT_BINARY_DIR}/NativeCallableInputsRuntime.generated.h")
        set(_callable_inputs_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/NativeCallableInputs.lvui")
        add_custom_command(OUTPUT "${_callable_inputs_generated}"
            COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                "-DLVUI_AOT_SOURCE=${_callable_inputs_source}" "-DLVUI_AOT_OUTPUT=${_callable_inputs_generated}"
                "-DLVUI_AOT_NAME=NativeCallableInputsRuntime" "-DLVUI_AOT_INCLUDES=NativeCallableFixture.h"
                -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
            DEPENDS "${_callable_inputs_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
            VERBATIM)
        add_executable(iiGUINativeCallableInputTests tests/native_callable_inputs.cpp "${_callable_inputs_generated}")
        target_include_directories(iiGUINativeCallableInputTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeCallableInputTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case direct text factory retirement root)
            add_test(NAME iiGUI.native_callable_inputs.${_case} COMMAND iiGUINativeCallableInputTests ${_case})
            set_tests_properties(iiGUI.native_callable_inputs.${_case} PROPERTIES TIMEOUT 15)
        endforeach()
        foreach(_callable Shared Independent RootShared Reference)
            set(_shared_generated "${CMAKE_CURRENT_BINARY_DIR}/Native${_callable}CallableRuntime.generated.h")
            set(_shared_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/Native${_callable}Callable.lvui")
            add_custom_command(OUTPUT "${_shared_generated}"
                COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                    "-DLVUI_AOT_SOURCE=${_shared_source}" "-DLVUI_AOT_OUTPUT=${_shared_generated}"
                    "-DLVUI_AOT_NAME=Native${_callable}CallableRuntime" "-DLVUI_AOT_INCLUDES=NativeSharedCallableFixture.h"
                    -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
                DEPENDS "${_shared_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                VERBATIM)
            list(APPEND _shared_callable_generated "${_shared_generated}")
        endforeach()
        add_executable(iiGUINativeSharedCallableTests tests/native_shared_callable.cpp ${_shared_callable_generated})
        target_include_directories(iiGUINativeSharedCallableTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeSharedCallableTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case identity lifecycle rollback self independent root reference)
            add_test(NAME iiGUI.native_shared_callable.${_case} COMMAND iiGUINativeSharedCallableTests ${_case})
            set_tests_properties(iiGUI.native_shared_callable.${_case} PROPERTIES TIMEOUT 15)
        endforeach()
        set(iiGUI_LVUI_RUNTIME_LIBRARY "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUIRuntime.a" CACHE FILEPATH "LVUI activation runtime archive for generated integration tests")
        set(_function_values_generated "${CMAKE_CURRENT_BINARY_DIR}/NativeFunctionValuesRuntime.generated.h")
        set(_function_values_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/NativeFunctionValues.lvui")
        add_custom_command(OUTPUT "${_function_values_generated}"
            COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                "-DLVUI_AOT_SOURCE=${_function_values_source}" "-DLVUI_AOT_OUTPUT=${_function_values_generated}"
                "-DLVUI_AOT_NAME=NativeFunctionValuesRuntime" "-DLVUI_AOT_INCLUDES=NativeFunctionFixture.h"
                -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
            DEPENDS "${_function_values_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
            VERBATIM)
        add_executable(iiGUINativeFunctionValueTests tests/native_function_values.cpp "${_function_values_generated}")
        target_include_directories(iiGUINativeFunctionValueTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeFunctionValueTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case behavior errors lifetime self snapshots snapshot_self)
            add_test(NAME iiGUI.native_function_values.${_case} COMMAND iiGUINativeFunctionValueTests ${_case})
            set_tests_properties(iiGUI.native_function_values.${_case} PROPERTIES TIMEOUT 15)
        endforeach()
        set(_function_variables_generated "${CMAKE_CURRENT_BINARY_DIR}/NativeFunctionVariablesRuntime.generated.h")
        set(_function_variables_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/NativeFunctionVariables.lvui")
        add_custom_command(OUTPUT "${_function_variables_generated}"
            COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                "-DLVUI_AOT_SOURCE=${_function_variables_source}" "-DLVUI_AOT_OUTPUT=${_function_variables_generated}"
                "-DLVUI_AOT_NAME=NativeFunctionVariablesRuntime" "-DLVUI_AOT_INCLUDES=NativeFunctionVariableFixture.h"
                -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
            DEPENDS "${_function_variables_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
            VERBATIM)
        add_executable(iiGUINativeFunctionVariableTests tests/native_function_variables.cpp "${_function_variables_generated}")
        target_include_directories(iiGUINativeFunctionVariableTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeFunctionVariableTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case behavior replacements events errors lifetime active conditional setter_gate)
            add_test(NAME iiGUI.native_function_variables.${_case} COMMAND iiGUINativeFunctionVariableTests ${_case})
            set_tests_properties(iiGUI.native_function_variables.${_case} PROPERTIES TIMEOUT 15)
        endforeach()
        set(_view_sequence_generated)
        foreach(_sequence ViewSequences ViewSequenceRows ViewSequenceRoot ViewSequenceOverride)
            set(_sequence_generated "${CMAKE_CURRENT_BINARY_DIR}/Native${_sequence}Runtime.generated.h")
            set(_sequence_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/Native${_sequence}.lvui")
            add_custom_command(OUTPUT "${_sequence_generated}"
                COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                    "-DLVUI_AOT_SOURCE=${_sequence_source}" "-DLVUI_AOT_OUTPUT=${_sequence_generated}"
                    "-DLVUI_AOT_NAME=Native${_sequence}Runtime" "-DLVUI_AOT_INCLUDES=NativeViewSequenceFixture.h"
                    -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
                DEPENDS "${_sequence_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                VERBATIM)
            list(APPEND _view_sequence_generated "${_sequence_generated}")
        endforeach()
        add_executable(iiGUINativeViewSequenceTests tests/native_view_sequences.cpp ${_view_sequence_generated})
        target_include_directories(iiGUINativeViewSequenceTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeViewSequenceTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case structure updates independent conditional active rollback lifetime root rows named_override)
            add_test(NAME iiGUI.native_view_sequences.${_case} COMMAND iiGUINativeViewSequenceTests ${_case})
            set_tests_properties(iiGUI.native_view_sequences.${_case} PROPERTIES TIMEOUT 15)
        endforeach()
        if(EXISTS "${iiGUI_LVUI_RUNTIME_LIBRARY}" AND iiSDKCore_VERSION VERSION_GREATER_EQUAL "0.20")
            set(_activation_work_generated "${CMAKE_CURRENT_BINARY_DIR}/NativeWorkRuntime.generated.h")
            set(_activation_work_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/NativeWork.lvui")
            add_custom_command(OUTPUT "${_activation_work_generated}"
                COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                    "-DLVUI_AOT_SOURCE=${_activation_work_source}" "-DLVUI_AOT_OUTPUT=${_activation_work_generated}"
                    "-DLVUI_AOT_NAME=NativeWorkRuntime" "-DLVUI_AOT_INCLUDES=NativeWorkFixture.h"
                    -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
                DEPENDS "${_activation_work_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                VERBATIM)
            add_executable(iiGUINativeActivationWorkTests tests/native_activation_work.cpp "${_activation_work_generated}")
            target_include_directories(iiGUINativeActivationWorkTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
            target_link_libraries(iiGUINativeActivationWorkTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_RUNTIME_LIBRARY}" "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
            foreach(_case result removal self rollback errors publishing_close)
                add_test(NAME iiGUI.native_activation_work.${_case} COMMAND iiGUINativeActivationWorkTests ${_case})
                set_tests_properties(iiGUI.native_activation_work.${_case} PROPERTIES TIMEOUT 20)
            endforeach()
        endif()
        set(_backend_construction_generated "${CMAKE_CURRENT_BINARY_DIR}/NativeBackendConstructionRuntime.generated.h")
        set(_backend_construction_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/NativeBackendConstruction.lvui")
        add_custom_command(OUTPUT "${_backend_construction_generated}"
            COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                "-DLVUI_AOT_SOURCE=${_backend_construction_source}" "-DLVUI_AOT_OUTPUT=${_backend_construction_generated}"
                "-DLVUI_AOT_NAME=NativeBackendConstructionRuntime" "-DLVUI_AOT_INCLUDES=NativeConstructionFixture.h"
                -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
            DEPENDS "${_backend_construction_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
            VERBATIM)
        add_executable(iiGUINativeBackendConstructionTests tests/native_backend_construction.cpp "${_backend_construction_generated}")
        target_include_directories(iiGUINativeBackendConstructionTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeBackendConstructionTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case configured snapshots rollback lifetime runtime)
            add_test(NAME iiGUI.native_backend_construction.${_case} COMMAND iiGUINativeBackendConstructionTests ${_case})
            set_tests_properties(iiGUI.native_backend_construction.${_case} PROPERTIES TIMEOUT 15)
        endforeach()
        set(_returned_callbacks_generated "${CMAKE_CURRENT_BINARY_DIR}/NativeReturnedCallbacksRuntime.generated.h")
        set(_returned_callbacks_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/NativeReturnedCallbacks.lvui")
        add_custom_command(OUTPUT "${_returned_callbacks_generated}"
            COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                "-DLVUI_AOT_SOURCE=${_returned_callbacks_source}" "-DLVUI_AOT_OUTPUT=${_returned_callbacks_generated}"
                "-DLVUI_AOT_NAME=NativeReturnedCallbacksRuntime" "-DLVUI_AOT_INCLUDES=NativeReturnedFixture.h"
                -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
            DEPENDS "${_returned_callbacks_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
            VERBATIM)
        add_executable(iiGUINativeReturnedCallbackTests tests/native_returned_callbacks.cpp "${_returned_callbacks_generated}")
        target_include_directories(iiGUINativeReturnedCallbackTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeReturnedCallbackTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case behavior branch lifetime)
            add_test(NAME iiGUI.native_returned_callbacks.${_case} COMMAND iiGUINativeReturnedCallbackTests ${_case})
            set_tests_properties(iiGUI.native_returned_callbacks.${_case} PROPERTIES TIMEOUT 15)
        endforeach()
        set(_repetition_generated "${CMAKE_CURRENT_BINARY_DIR}/NativeRepetitionRuntime.generated.h")
        set(_repetition_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/NativeRepetition.lvui")
        add_custom_command(OUTPUT "${_repetition_generated}"
            COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                "-DLVUI_AOT_SOURCE=${_repetition_source}" "-DLVUI_AOT_OUTPUT=${_repetition_generated}"
                "-DLVUI_AOT_NAME=NativeRepetitionRuntime" "-DLVUI_AOT_INCLUDES=NativeRepetitionFixture.h"
                -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
            DEPENDS "${_repetition_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
            VERBATIM)
        set(_nested_repetition_generated "${CMAKE_CURRENT_BINARY_DIR}/NativeNestedRepetitionRuntime.generated.h")
        set(_nested_repetition_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/NativeNestedRepetition.lvui")
        add_custom_command(OUTPUT "${_nested_repetition_generated}"
            COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                "-DLVUI_AOT_SOURCE=${_nested_repetition_source}" "-DLVUI_AOT_OUTPUT=${_nested_repetition_generated}"
                "-DLVUI_AOT_NAME=NativeNestedRepetitionRuntime" "-DLVUI_AOT_INCLUDES=NativeRepetitionFixture.h"
                -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
            DEPENDS "${_nested_repetition_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
            VERBATIM)
        set(_list_values_generated "${CMAKE_CURRENT_BINARY_DIR}/NativeListValuesRuntime.generated.h")
        set(_list_values_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/NativeLists.lvui")
        add_custom_command(OUTPUT "${_list_values_generated}"
            COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                "-DLVUI_AOT_SOURCE=${_list_values_source}" "-DLVUI_AOT_OUTPUT=${_list_values_generated}"
                "-DLVUI_AOT_NAME=NativeListValuesRuntime" "-DLVUI_AOT_INCLUDES=NativeRepetitionFixture.h"
                -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
            DEPENDS "${_list_values_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
            VERBATIM)
        set(_shared_repetition_generated "${CMAKE_CURRENT_BINARY_DIR}/NativeSharedRepetitionRuntime.generated.h")
        set(_shared_repetition_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/NativeSharedRepetition.lvui")
        add_custom_command(OUTPUT "${_shared_repetition_generated}"
            COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                "-DLVUI_AOT_SOURCE=${_shared_repetition_source}" "-DLVUI_AOT_OUTPUT=${_shared_repetition_generated}"
                "-DLVUI_AOT_NAME=NativeSharedRepetitionRuntime" "-DLVUI_AOT_INCLUDES=NativeRepetitionFixture.h"
                -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
            DEPENDS "${_shared_repetition_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
            VERBATIM)
        add_executable(iiGUINativeRepetitionTests tests/native_repetition.cpp "${_repetition_generated}" "${_nested_repetition_generated}" "${_list_values_generated}" "${_shared_repetition_generated}")
        target_include_directories(iiGUINativeRepetitionTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeRepetitionTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case identity viewport nested values shared)
            add_test(NAME iiGUI.native_repetition.${_case} COMMAND iiGUINativeRepetitionTests ${_case})
            set_tests_properties(iiGUI.native_repetition.${_case} PROPERTIES TIMEOUT 30)
        endforeach()
        set(_virtual_repetition_generated "${CMAKE_CURRENT_BINARY_DIR}/NativeVirtualRepetitionRuntime.generated.h")
        set(_virtual_repetition_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/NativeVirtualRepetition.lvui")
        add_custom_command(OUTPUT "${_virtual_repetition_generated}"
            COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                "-DLVUI_AOT_SOURCE=${_virtual_repetition_source}" "-DLVUI_AOT_OUTPUT=${_virtual_repetition_generated}"
                "-DLVUI_AOT_NAME=NativeVirtualRepetitionRuntime" "-DLVUI_AOT_INCLUDES=NativeRepetitionFixture.h"
                -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
            DEPENDS "${_virtual_repetition_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
            VERBATIM)
        add_executable(iiGUINativeVirtualRepetitionTests tests/native_virtual_repetition.cpp "${_virtual_repetition_generated}")
        target_include_directories(iiGUINativeVirtualRepetitionTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeVirtualRepetitionTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case viewport anchor resize rollback lifetime horizontal empty reentry autoheight)
            add_test(NAME iiGUI.native_virtual_repetition.${_case} COMMAND iiGUINativeVirtualRepetitionTests ${_case})
            set_tests_properties(iiGUI.native_virtual_repetition.${_case} PROPERTIES TIMEOUT 30)
        endforeach()
        set(_measured_repetition_generated "${CMAKE_CURRENT_BINARY_DIR}/NativeMeasuredRepetitionRuntime.generated.h")
        set(_measured_repetition_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/NativeMeasuredRepetition.lvui")
        add_custom_command(OUTPUT "${_measured_repetition_generated}"
            COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                "-DLVUI_AOT_SOURCE=${_measured_repetition_source}" "-DLVUI_AOT_OUTPUT=${_measured_repetition_generated}"
                "-DLVUI_AOT_NAME=NativeMeasuredRepetitionRuntime" "-DLVUI_AOT_INCLUDES=NativeRepetitionFixture.h"
                -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
            DEPENDS "${_measured_repetition_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
            VERBATIM)
        add_executable(iiGUINativeMeasuredRepetitionTests tests/native_measured_repetition.cpp "${_measured_repetition_generated}")
        target_include_directories(iiGUINativeMeasuredRepetitionTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-generated-icons" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeMeasuredRepetitionTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case measured changing anchor resize zero lifetime offscreen navigation)
            add_test(NAME iiGUI.native_measured_repetition.${_case} COMMAND iiGUINativeMeasuredRepetitionTests ${_case})
            set_tests_properties(iiGUI.native_measured_repetition.${_case} PROPERTIES TIMEOUT 30)
        endforeach()
        set(_navigated_repetition_generated "${CMAKE_CURRENT_BINARY_DIR}/NativeNavigatedRepetitionRuntime.generated.h")
        set(_navigated_repetition_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/NativeNavigatedRepetition.lvui")
        add_custom_command(OUTPUT "${_navigated_repetition_generated}"
            COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                "-DLVUI_AOT_SOURCE=${_navigated_repetition_source}" "-DLVUI_AOT_OUTPUT=${_navigated_repetition_generated}"
                "-DLVUI_AOT_NAME=NativeNavigatedRepetitionRuntime" "-DLVUI_AOT_INCLUDES=NativeRepetitionFixture.h"
                -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
            DEPENDS "${_navigated_repetition_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
            VERBATIM)
        add_executable(iiGUINativeNavigatedRepetitionTests tests/native_navigated_repetition.cpp "${_navigated_repetition_generated}")
        target_include_directories(iiGUINativeNavigatedRepetitionTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-generated-icons" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeNavigatedRepetitionTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case navigation selection programmatic controls pointer lifetime source)
            add_test(NAME iiGUI.native_navigated_repetition.${_case} COMMAND iiGUINativeNavigatedRepetitionTests ${_case})
            set_tests_properties(iiGUI.native_navigated_repetition.${_case} PROPERTIES TIMEOUT 30)
        endforeach()
        set(_colors_generated "${CMAKE_CURRENT_BINARY_DIR}/NativeColorsRuntime.generated.h")
        set(_colors_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/NativeColors.lvui")
        add_custom_command(OUTPUT "${_colors_generated}"
            COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                "-DLVUI_AOT_SOURCE=${_colors_source}" "-DLVUI_AOT_OUTPUT=${_colors_generated}"
                "-DLVUI_AOT_NAME=NativeColorsRuntime" "-DLVUI_AOT_INCLUDES=NativeColorFixture.h"
                -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
            DEPENDS "${_colors_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}" VERBATIM)
        add_executable(iiGUINativeColorValueTests tests/native_colors.cpp "${_colors_generated}")
        target_include_directories(iiGUINativeColorValueTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeColorValueTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case structure updates snapshots variables functions errors primitives lifetime rollback drawing)
            add_test(NAME iiGUI.native_colors.${_case} COMMAND iiGUINativeColorValueTests ${_case})
            set_tests_properties(iiGUI.native_colors.${_case} PROPERTIES TIMEOUT 20)
        endforeach()
        set(_scalar_generated)
        foreach(_fixture NativeScalarInputs NativeScalarForward NativeScalarTypes NativeScalarConditional NativeScalarRoot NativeScalarFunctionDefault)
            set(_scalar_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/${_fixture}.lvui")
            set(_scalar_header "${CMAKE_CURRENT_BINARY_DIR}/${_fixture}Runtime.generated.h")
            add_custom_command(OUTPUT "${_scalar_header}"
                COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                    "-DLVUI_AOT_SOURCE=${_scalar_source}" "-DLVUI_AOT_OUTPUT=${_scalar_header}"
                    "-DLVUI_AOT_NAME=${_fixture}Runtime" "-DLVUI_AOT_INCLUDES=NativeScalarInputFixture.h"
                    -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
                DEPENDS "${_scalar_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}" VERBATIM)
            list(APPEND _scalar_generated "${_scalar_header}")
        endforeach()
        add_executable(iiGUINativeScalarInputTests tests/native_scalar_inputs.cpp ${_scalar_generated})
        target_include_directories(iiGUINativeScalarInputTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeScalarInputTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case sharing updates capture types conditional errors lifetime root function_default)
            add_test(NAME iiGUI.native_scalar_inputs.${_case} COMMAND iiGUINativeScalarInputTests ${_case})
            set_tests_properties(iiGUI.native_scalar_inputs.${_case} PROPERTIES TIMEOUT 20)
        endforeach()
        set(_layout_generated)
        foreach(_fixture NativeLayout NativeLayoutAlignment NativeLayoutConditional NativeLayoutMeasured)
            set(_layout_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/${_fixture}.lvui")
            set(_layout_header "${CMAKE_CURRENT_BINARY_DIR}/${_fixture}Runtime.generated.h")
            add_custom_command(OUTPUT "${_layout_header}"
                COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                    "-DLVUI_AOT_SOURCE=${_layout_source}" "-DLVUI_AOT_OUTPUT=${_layout_header}"
                    "-DLVUI_AOT_NAME=${_fixture}Runtime" "-DLVUI_AOT_INCLUDES=NativeLayoutFixture.h"
                    -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
                DEPENDS "${_layout_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}" VERBATIM)
            list(APPEND _layout_generated "${_layout_header}")
        endforeach()
        add_executable(iiGUINativeLayoutTests tests/native_layout.cpp ${_layout_generated})
        target_include_directories(iiGUINativeLayoutTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include"
            "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-generated-icons" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeLayoutTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case values atomic validation owner updates preservation alignment insets minimums flex errors lifetime conditional measured)
            add_test(NAME iiGUI.native_layout.${_case} COMMAND iiGUINativeLayoutTests ${_case})
            set_tests_properties(iiGUI.native_layout.${_case} PROPERTIES TIMEOUT 25)
        endforeach()
        set(_window_generated)
        foreach(_fixture NativeWindow NativeWindowStatic NativeWindowRoot)
            set(_window_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/${_fixture}.lvui")
            set(_window_header "${CMAKE_CURRENT_BINARY_DIR}/${_fixture}Runtime.generated.h")
            add_custom_command(OUTPUT "${_window_header}"
                COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                    "-DLVUI_AOT_SOURCE=${_window_source}" "-DLVUI_AOT_OUTPUT=${_window_header}"
                    "-DLVUI_AOT_NAME=${_fixture}Runtime" "-DLVUI_AOT_INCLUDES=NativeWindowFixture.h"
                    -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
                DEPENDS "${_window_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}" VERBATIM)
            list(APPEND _window_generated "${_window_header}")
        endforeach()
        add_executable(iiGUINativeWindowTests tests/native_window.cpp ${_window_generated})
        target_include_directories(iiGUINativeWindowTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeWindowTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case values staging validation reactive deltas preservation errors owner lifetime embedded snapshots rollback)
            add_test(NAME iiGUI.native_window.${_case} COMMAND iiGUINativeWindowTests ${_case})
            set_tests_properties(iiGUI.native_window.${_case} PROPERTIES TIMEOUT 25)
        endforeach()
        set(_readonly_generated)
        foreach(_fixture NativeReadonly NativeReadonlyPostfix)
            set(_readonly_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/${_fixture}.lvui")
            set(_readonly_header "${CMAKE_CURRENT_BINARY_DIR}/${_fixture}Runtime.generated.h")
            add_custom_command(OUTPUT "${_readonly_header}"
                COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                    "-DLVUI_AOT_SOURCE=${_readonly_source}" "-DLVUI_AOT_OUTPUT=${_readonly_header}"
                    "-DLVUI_AOT_NAME=${_fixture}Runtime" "-DLVUI_AOT_INCLUDES=NativeReadonlyFixture.h"
                    -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
                DEPENDS "${_readonly_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}" VERBATIM)
            list(APPEND _readonly_generated "${_readonly_header}")
        endforeach()
        add_executable(iiGUINativeReadonlyTests tests/native_readonly.cpp ${_readonly_generated})
        target_include_directories(iiGUINativeReadonlyTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeReadonlyTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case fields selection references records mutations nulls retirement conditional postfix)
            add_test(NAME iiGUI.native_readonly.${_case} COMMAND iiGUINativeReadonlyTests ${_case})
            set_tests_properties(iiGUI.native_readonly.${_case} PROPERTIES TIMEOUT 25)
        endforeach()
        set(_object_input_generated)
        foreach(_fixture NativeObjectInputs NativeObjectHandles NativeObjectRoot NativeObjectRows)
            set(_object_input_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/${_fixture}.lvui")
            set(_object_input_header "${CMAKE_CURRENT_BINARY_DIR}/${_fixture}Runtime.generated.h")
            add_custom_command(OUTPUT "${_object_input_header}"
                COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                    "-DLVUI_AOT_SOURCE=${_object_input_source}" "-DLVUI_AOT_OUTPUT=${_object_input_header}"
                    "-DLVUI_AOT_NAME=${_fixture}Runtime" "-DLVUI_AOT_INCLUDES=NativeObjectInputFixture.h"
                    -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
                DEPENDS "${_object_input_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}" VERBATIM)
            list(APPEND _object_input_generated "${_object_input_header}")
        endforeach()
        add_executable(iiGUINativeObjectInputTests tests/native_object_inputs.cpp ${_object_input_generated})
        target_include_directories(iiGUINativeObjectInputTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeObjectInputTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case values records handles selection nulls invalid_initial local_state retirement conditional rows)
            add_test(NAME iiGUI.native_object_inputs.${_case} COMMAND iiGUINativeObjectInputTests ${_case})
            set_tests_properties(iiGUI.native_object_inputs.${_case} PROPERTIES TIMEOUT 25)
        endforeach()
        find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
        set(_method_query_generated)
        foreach(_fixture NativeMethodQueries NativeMethodPostfix)
            set(_method_query_source "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/${_fixture}.lvui")
            set(_method_query_header "${CMAKE_CURRENT_BINARY_DIR}/${_fixture}Runtime.generated.h")
            add_custom_command(OUTPUT "${_method_query_header}"
                COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                    "-DLVUI_AOT_SOURCE=${_method_query_source}" "-DLVUI_AOT_OUTPUT=${_method_query_header}"
                    "-DLVUI_AOT_NAME=${_fixture}Runtime" "-DLVUI_AOT_INCLUDES=NativeMethodQueryFixture.h"
                    -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
                DEPENDS "${_method_query_source}" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}" VERBATIM)
            list(APPEND _method_query_generated "${_method_query_header}")
        endforeach()
        add_executable(iiGUINativeMethodQueryTests tests/native_method_queries.cpp ${_method_query_generated})
        target_include_directories(iiGUINativeMethodQueryTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeMethodQueryTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case initial updates arguments selection branch failures local_state reentry conditional postfix)
            add_test(NAME iiGUI.native_method_queries.${_case} COMMAND iiGUINativeMethodQueryTests ${_case})
            set_tests_properties(iiGUI.native_method_queries.${_case} PROPERTIES TIMEOUT 25)
        endforeach()
        add_test(NAME iiGUI.layout_package COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/layout_install.py"
            --build "${CMAKE_CURRENT_BINARY_DIR}" --lvui-build "${iiGUI_LVUI_SOURCE_DIR}/build" --cmake "${CMAKE_COMMAND}")
        set_tests_properties(iiGUI.layout_package PROPERTIES TIMEOUT 120)
        if(CMAKE_SYSTEM_NAME MATCHES "^(Darwin|Linux|Windows)$")
            set(_shared_model_root "${iiGUI_LVUI_SOURCE_DIR}/../iiSDKCore/tests")
            add_library(iiGUISharedMethodModel SHARED "${_shared_model_root}/fixtures/observable_module/Model.cpp")
            target_compile_definitions(iiGUISharedMethodModel PRIVATE IISDKCORE_TEST_OBSERVABLE_MODULE=1)
            target_link_libraries(iiGUISharedMethodModel PRIVATE iiSDKCore::Observation)
            set_target_properties(iiGUISharedMethodModel PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
            if(IIGUI_SANITIZE)
                target_compile_options(iiGUISharedMethodModel PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
                target_link_options(iiGUISharedMethodModel PRIVATE -fsanitize=address,undefined)
            endif()
            set(_shared_method_header "${CMAKE_CURRENT_BINARY_DIR}/NativeSharedMethodRuntime.generated.h")
            add_custom_command(OUTPUT "${_shared_method_header}"
                COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                    "-DLVUI_AOT_SOURCE=${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/NativeSharedMethod.lvui"
                    "-DLVUI_AOT_OUTPUT=${_shared_method_header}" "-DLVUI_AOT_NAME=NativeSharedMethodRuntime"
                    "-DLVUI_AOT_INCLUDES=NativeSharedMethodFixture.h" -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
                DEPENDS "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/NativeSharedMethod.lvui" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}" VERBATIM)
            add_executable(iiGUINativeSharedMethodTests tests/native_shared_method.cpp "${_shared_method_header}")
            target_include_directories(iiGUINativeSharedMethodTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures" "${_shared_model_root}")
            target_link_libraries(iiGUINativeSharedMethodTests PRIVATE iiGUISharedMethodModel iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
            foreach(_case updates editing retirement)
                add_test(NAME iiGUI.native_shared_method.${_case} COMMAND iiGUINativeSharedMethodTests ${_case})
                set_tests_properties(iiGUI.native_shared_method.${_case} PROPERTIES TIMEOUT 25)
            endforeach()
        endif()
        set(_object_method_headers)
        foreach(_view NativeObjectMethodReferences NativeObjectMethodPostfix)
            set(_header "${CMAKE_CURRENT_BINARY_DIR}/${_view}Runtime.generated.h")
            add_custom_command(OUTPUT "${_header}"
                COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                    "-DLVUI_AOT_SOURCE=${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/${_view}.lvui"
                    "-DLVUI_AOT_OUTPUT=${_header}" "-DLVUI_AOT_NAME=${_view}Runtime"
                    "-DLVUI_AOT_INCLUDES=NativeObjectMethodReferenceFixture.h" -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
                DEPENDS "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/${_view}.lvui" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}" VERBATIM)
            list(APPEND _object_method_headers "${_header}")
        endforeach()
        add_executable(iiGUINativeObjectMethodReferenceTests tests/native_object_method_references.cpp ${_object_method_headers})
        target_include_directories(iiGUINativeObjectMethodReferenceTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeObjectMethodReferenceTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case initial selection payload preserve failures fixed conditional parent temporary retirement owned_parent indexed_value receiver_condition)
            add_test(NAME iiGUI.native_object_method_references.${_case} COMMAND iiGUINativeObjectMethodReferenceTests ${_case})
            set_tests_properties(iiGUI.native_object_method_references.${_case} PROPERTIES TIMEOUT 30)
        endforeach()
        set(_callable_field_headers)
        foreach(_view NativeCallableFields NativeCallableFieldPostfix)
            set(_header "${CMAKE_CURRENT_BINARY_DIR}/${_view}Runtime.generated.h")
            add_custom_command(OUTPUT "${_header}"
                COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                    "-DLVUI_AOT_SOURCE=${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/${_view}.lvui"
                    "-DLVUI_AOT_OUTPUT=${_header}" "-DLVUI_AOT_NAME=${_view}Runtime"
                    "-DLVUI_AOT_INCLUDES=NativeCallableFieldFixture.h" -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
                DEPENDS "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/${_view}.lvui" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}" VERBATIM)
            list(APPEND _callable_field_headers "${_header}")
        endforeach()
        add_executable(iiGUINativeCallableFieldTests tests/native_callable_fields.cpp ${_callable_field_headers})
        target_include_directories(iiGUINativeCallableFieldTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeCallableFieldTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case initial updates payload preserve errors inflight borrowed publication conditional postfix retirement)
            add_test(NAME iiGUI.native_callable_fields.${_case} COMMAND iiGUINativeCallableFieldTests ${_case})
            set_tests_properties(iiGUI.native_callable_fields.${_case} PROPERTIES TIMEOUT 30)
        endforeach()
        set(_factory_capture_headers)
        foreach(_view NativeFactoryCaptures NativeFactoryCapturePostfix)
            set(_header "${CMAKE_CURRENT_BINARY_DIR}/${_view}Runtime.generated.h")
            add_custom_command(OUTPUT "${_header}"
                COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                    "-DLVUI_AOT_SOURCE=${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/${_view}.lvui"
                    "-DLVUI_AOT_OUTPUT=${_header}" "-DLVUI_AOT_NAME=${_view}Runtime"
                    "-DLVUI_AOT_INCLUDES=NativeFactoryCaptureFixture.h" -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
                DEPENDS "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/${_view}.lvui" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}" VERBATIM)
            list(APPEND _factory_capture_headers "${_header}")
        endforeach()
        add_executable(iiGUINativeFactoryCaptureTests tests/native_factory_captures.cpp ${_factory_capture_headers})
        target_include_directories(iiGUINativeFactoryCaptureTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeFactoryCaptureTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case temporary argument adaptive selection inflight preserve failures fixed payload indexed conditional retirement)
            add_test(NAME iiGUI.native_factory_captures.${_case} COMMAND iiGUINativeFactoryCaptureTests ${_case})
            set_tests_properties(iiGUI.native_factory_captures.${_case} PROPERTIES TIMEOUT 30)
        endforeach()
        foreach(_view NativeInferredValues NativeInferredFields)
            set(_header "${CMAKE_CURRENT_BINARY_DIR}/${_view}Runtime.generated.h")
            if(_view STREQUAL NativeInferredValues)
                set(_fixture NativeFactoryCaptureFixture.h)
            else()
                set(_fixture NativeInferredFieldsFixture.h)
            endif()
            add_custom_command(OUTPUT "${_header}"
                COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}"
                    "-DLVUI_AOT_SOURCE=${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/${_view}.lvui"
                    "-DLVUI_AOT_OUTPUT=${_header}" "-DLVUI_AOT_NAME=${_view}Runtime"
                    "-DLVUI_AOT_INCLUDES=${_fixture}" -P "${iiGUI_LVUI_SOURCE_DIR}/cmake/RunLVUICompiler.cmake"
                DEPENDS "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures/${_view}.lvui" "${iiGUI_LVUI_SOURCE_DIR}/build/lvui-compile${CMAKE_EXECUTABLE_SUFFIX}" VERBATIM)
            list(APPEND _inferred_value_headers "${_header}")
        endforeach()
        add_executable(iiGUINativeInferredValueTests tests/native_inferred_values.cpp ${_inferred_value_headers})
        target_include_directories(iiGUINativeInferredValueTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(iiGUINativeInferredValueTests PRIVATE iiGUI::iiGUI "${iiGUI_LVUI_SOURCE_DIR}/build/libLVUINative.a")
        foreach(_case selection temporary argument frozen fields variable preserve failures reentry conditional retirement)
            add_test(NAME iiGUI.native_inferred_values.${_case} COMMAND iiGUINativeInferredValueTests ${_case})
            set_tests_properties(iiGUI.native_inferred_values.${_case} PROPERTIES TIMEOUT 30)
        endforeach()
    endif()
    add_executable(iiGUIExternalUrlTests tests/external_urls.cpp)
    target_link_libraries(iiGUIExternalUrlTests PRIVATE iiGUI::iiGUI)
    set_target_properties(iiGUIExternalUrlTests PROPERTIES MACOSX_BUNDLE OFF)
    add_test(NAME iiGUI.external_urls COMMAND iiGUIExternalUrlTests)
    add_executable(iiGUIMobileInputTests tests/mobile_input.cpp)
    target_link_libraries(iiGUIMobileInputTests PRIVATE iiGUI::iiGUI)
    set_target_properties(iiGUIMobileInputTests PROPERTIES MACOSX_BUNDLE OFF)
    add_test(NAME iiGUI.mobile_input COMMAND iiGUIMobileInputTests)
    add_executable(iiGUITouchInputBridgeTests tests/touch_input_bridge.cpp)
    target_link_libraries(iiGUITouchInputBridgeTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.touch_input_bridge COMMAND iiGUITouchInputBridgeTests)
    add_executable(iiGUIXInputPenStateTests tests/xinput_pen_state.cpp)
    target_link_libraries(iiGUIXInputPenStateTests PRIVATE iiGUI::iiGUI)
    set_target_properties(iiGUIXInputPenStateTests PROPERTIES MACOSX_BUNDLE OFF)
    add_test(NAME iiGUI.xinput_pen_state COMMAND iiGUIXInputPenStateTests)
    add_executable(iiGUIWidgetInteractionTests tests/widget_interaction.cpp)
    target_link_libraries(iiGUIWidgetInteractionTests PRIVATE iiGUI::iiGUI)
    foreach(_case initial hover press duplicate hidden disabled expired retirement measure owner bounds passthrough capture invalid fanout embedded modal)
        add_test(NAME iiGUI.widget_interaction_${_case} COMMAND iiGUIWidgetInteractionTests ${_case})
        set_tests_properties(iiGUI.widget_interaction_${_case} PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
    endforeach()
    add_executable(iiGUITextRasterCacheTests tests/text_raster_cache.cpp)
    target_link_libraries(iiGUITextRasterCacheTests PRIVATE iiGUI::iiGUI)
    foreach(_case native_rgb alpha palette identity region lru entry_limit budget limits clear invalid owner)
        add_test(NAME iiGUI.text_raster_cache_${_case} COMMAND iiGUITextRasterCacheTests ${_case})
    endforeach()
    add_executable(iiGUITextInputProviderTests tests/text_input_provider.cpp)
    target_link_libraries(iiGUITextInputProviderTests PRIVATE iiGUI::iiGUI)
    foreach(_case bounded geometry focus no_autofocus reentry bad_content bad_state owner retirement legacy)
        add_test(NAME iiGUI.text_input_provider_${_case} COMMAND iiGUITextInputProviderTests ${_case})
    endforeach()
    add_executable(iiGUITextLineTests tests/text_line.cpp)
    target_link_libraries(iiGUITextLineTests PRIVATE iiGUI::iiGUI)
    foreach(_case empty ascii utf16 graphemes tabs bidi crop alpha color_font_alpha nul limits owner)
        add_test(NAME iiGUI.text_line_${_case} COMMAND iiGUITextLineTests ${_case})
    endforeach()
    add_executable(iiGUITextLineWrapTests tests/text_line_wrap.cpp)
    target_link_libraries(iiGUITextLineWrapTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.text_line_wrap COMMAND iiGUITextLineWrapTests)
    add_executable(iiGUITextBoundaryTests tests/text_boundary.cpp)
    target_link_libraries(iiGUITextBoundaryTests PRIVATE iiGUI::iiGUI)
    foreach(_case empty ascii utf16 graphemes flags hangul crlf words nul invalid sparse sharing)
        add_test(NAME iiGUI.text_boundary_${_case} COMMAND iiGUITextBoundaryTests ${_case})
    endforeach()
    add_executable(iiGUIVulkanEffectsTests tests/vulkan_effects.cpp)
    target_link_libraries(iiGUIVulkanEffectsTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.vulkan_effects COMMAND iiGUIVulkanEffectsTests)
    set_tests_properties(iiGUI.vulkan_effects PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
    add_executable(iiGUITextRasterTests tests/text_raster.cpp)
    target_link_libraries(iiGUITextRasterTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.text_raster COMMAND iiGUITextRasterTests)
    set_tests_properties(iiGUI.text_raster PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    add_executable(iiGUIVirtualViewportTests tests/virtual_viewport.cpp)
    add_executable(iiGUIFocusRingTests tests/focus_ring.cpp)
    target_link_libraries(iiGUIFocusRingTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.focus_ring COMMAND iiGUIFocusRingTests)
    add_executable(iiGUINativeFocusRingTests tests/native_focus_ring.cpp)
    target_link_libraries(iiGUINativeFocusRingTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.native_focus_ring COMMAND iiGUINativeFocusRingTests)
    add_executable(iiGUINativeFocusRingMotionTests tests/native_focus_ring_motion.cpp)
    target_link_libraries(iiGUINativeFocusRingMotionTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.native_focus_ring_motion COMMAND iiGUINativeFocusRingMotionTests)
    add_executable(iiGUIWindowNoiseTextureTests tests/window_noise_texture.cpp)
    target_link_libraries(iiGUIWindowNoiseTextureTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.window_noise_texture COMMAND iiGUIWindowNoiseTextureTests)
    add_executable(iiGUINativeWindowNoiseTextureTests tests/native_window_noise_texture.cpp)
    target_link_libraries(iiGUINativeWindowNoiseTextureTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.native_window_noise_texture COMMAND iiGUINativeWindowNoiseTextureTests)
    add_executable(iiGUINativeWheelScrollGuardTests tests/native_wheel_scroll_guard.cpp)
    target_link_libraries(iiGUINativeWheelScrollGuardTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.native_wheel_scroll_guard COMMAND iiGUINativeWheelScrollGuardTests)
    add_executable(iiGUIWidgetOpacityTests tests/widget_opacity.cpp)
    target_link_libraries(iiGUIWidgetOpacityTests PRIVATE iiGUI::iiGUI)
    foreach(_case values inheritance content batching drawing interaction popup validation retirement)
        add_test(NAME iiGUI.widget_opacity.${_case} COMMAND iiGUIWidgetOpacityTests ${_case})
        set_tests_properties(iiGUI.widget_opacity.${_case} PROPERTIES TIMEOUT 20)
    endforeach()
    add_executable(iiGUIContentHostTests tests/content_host.cpp)
    add_executable(iiGUIActionSurfaceTests tests/action_surface.cpp)
    target_link_libraries(iiGUIActionSurfaceTests PRIVATE iiGUI::iiGUI)
    foreach(_case geometry fallback children child_input keyboard colors disabled underline validation retirement)
        add_test(NAME iiGUI.action_surface_${_case} COMMAND iiGUIActionSurfaceTests ${_case})
        set_tests_properties(iiGUI.action_surface_${_case} PROPERTIES RUN_SERIAL TRUE TIMEOUT 60)
    endforeach()
    target_link_libraries(iiGUIContentHostTests PRIVATE iiGUI::iiGUI)
    foreach(_case lifetime atomic input geometry retirement nested capture popup quota)
        add_test(NAME iiGUI.content_host_${_case} COMMAND iiGUIContentHostTests ${_case})
        set_tests_properties(iiGUI.content_host_${_case} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    endforeach()
    target_link_libraries(iiGUIVirtualViewportTests PRIVATE iiGUI::iiGUI)
    foreach(_case geometry drawing input lifetime hover horizontal horizontal_natural nested_axes)
        add_test(NAME iiGUI.virtual_viewport_${_case} COMMAND iiGUIVirtualViewportTests ${_case})
        set_tests_properties(iiGUI.virtual_viewport_${_case} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    endforeach()
    add_executable(iiGUIMotionPreferencesTests tests/motion_preferences.cpp)
    add_executable(iiGUIEditorInputTests tests/editor_input.cpp)
    target_link_libraries(iiGUIEditorInputTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.editor_input COMMAND iiGUIEditorInputTests)
    add_test(NAME iiGUI.editor_input_vulkan COMMAND iiGUIEditorInputTests vulkan)
    set_tests_properties(iiGUI.editor_input PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    set_tests_properties(iiGUI.editor_input_vulkan PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    target_link_libraries(iiGUIMotionPreferencesTests PRIVATE iiGUI::iiGUI)
    foreach(_case preferences observers capacity)
        add_test(NAME iiGUI.motion_${_case} COMMAND iiGUIMotionPreferencesTests ${_case})
    endforeach()
    add_executable(iiGUIColorTests tests/colors.cpp)
    target_include_directories(iiGUIColorTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/include/iiGUI")
    target_compile_definitions(iiGUIColorTests PRIVATE COLOR_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/color-rgba.png")
    target_compile_definitions(iiGUIColorTests PRIVATE COLOR_EXPANSION_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/color-expansion.png")
    target_link_libraries(iiGUIColorTests PRIVATE iiGUI::iiGUI)
    foreach(_case model channels hex observers png domain)
        add_test(NAME iiGUI.colors_${_case} COMMAND iiGUIColorTests ${_case})
    endforeach()
    find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
    add_test(NAME iiGUI.presentation_integration COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/presentation_integration.py")
    add_executable(iiGUIPresentationTests tests/presentation.cpp)
    target_link_libraries(iiGUIPresentationTests PRIVATE iiGUI::iiGUI)
    foreach(_case defaults delivery unsubscribe closure owner errors reentrant clock close_in_callback retirement deferred close_pending rebind overload inline_executor foreign_delivery limits boundary fallback unobserved)
        add_test(NAME iiGUI.presentation_${_case} COMMAND iiGUIPresentationTests ${_case})
        set_tests_properties(iiGUI.presentation_${_case} PROPERTIES TIMEOUT 30)
    endforeach()
    add_executable(iiGUINativeViewTests tests/native_view.cpp)
    target_link_libraries(iiGUINativeViewTests PRIVATE iiGUI::iiGUI)
    foreach(_case initial empty identity replace returned exception empty_candidate invalid independent clear retirement foreign reentry retire_during_build nested drawing touch_scroll_probe frame_data)
        add_test(NAME iiGUI.native_view_${_case} COMMAND iiGUINativeViewTests ${_case})
        set_tests_properties(iiGUI.native_view_${_case} PROPERTIES TIMEOUT 30)
    endforeach()
    add_executable(iiGUIImageBudgetTests tests/image_budget.cpp)
    target_link_libraries(iiGUIImageBudgetTests PRIVATE iiGUI::iiGUI)
    foreach(_case limits svg)
        add_test(NAME iiGUI.image_budget_${_case} COMMAND iiGUIImageBudgetTests ${_case})
        set_tests_properties(iiGUI.image_budget_${_case} PROPERTIES TIMEOUT 30)
    endforeach()
    add_executable(iiGUIImageTests tests/images.cpp)
    target_include_directories(iiGUIImageTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/include/iiGUI")
    target_link_libraries(iiGUIImageTests PRIVATE iiGUI::iiGUI)
    foreach(_case contracts svg composition native vulkan)
        add_test(NAME iiGUI.images_${_case} COMMAND iiGUIImageTests ${_case})
        set_tests_properties(iiGUI.images_${_case} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    endforeach()
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        add_executable(iiGUIMetalDamageTests tests/metal_damage.mm)
        target_link_libraries(iiGUIMetalDamageTests PRIVATE iiGUI::iiGUI "-framework AppKit")
        set_target_properties(iiGUIMetalDamageTests PROPERTIES OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED YES OBJCXX_EXTENSIONS OFF)
        target_compile_options(iiGUIMetalDamageTests PRIVATE -fobjc-arc)
        add_test(NAME iiGUI.metal_damage COMMAND iiGUIMetalDamageTests)
        set_tests_properties(iiGUI.metal_damage PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
        add_executable(iiGUIDesktopTests tests/desktop.mm)
        target_link_libraries(iiGUIDesktopTests PRIVATE iiGUI::iiGUI "-framework AppKit")
        target_compile_options(iiGUIDesktopTests PRIVATE -fobjc-arc)
        set_target_properties(iiGUIDesktopTests PROPERTIES OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED YES OBJCXX_EXTENSIONS OFF)
        foreach(_case windows dialogs clipboard ime files)
            add_test(NAME iiGUI.desktop_${_case} COMMAND iiGUIDesktopTests ${_case})
            set_tests_properties(iiGUI.desktop_${_case} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
        endforeach()
        add_executable(iiGUINativeColorTests tests/colors_native.mm)
        target_link_libraries(iiGUINativeColorTests PRIVATE iiGUI::iiGUI "-framework AppKit")
        set_target_properties(iiGUINativeColorTests PROPERTIES OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED YES OBJCXX_EXTENSIONS OFF)
        target_compile_options(iiGUINativeColorTests PRIVATE -fobjc-arc)
        foreach(_case input lifetime rendering native_events)
            add_test(NAME iiGUI.colors_${_case} COMMAND iiGUINativeColorTests ${_case})
            set_tests_properties(iiGUI.colors_${_case} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
        endforeach()
        add_executable(iiGUIMenuTests tests/menus.mm)
        target_link_libraries(iiGUIMenuTests PRIVATE iiGUI::iiGUI "-framework AppKit")
        set_target_properties(iiGUIMenuTests PROPERTIES OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED YES OBJCXX_EXTENSIONS OFF)
        target_compile_options(iiGUIMenuTests PRIVATE -fobjc-arc)
        foreach(_case lifecycle placement input rendering lifetime native_events scroll nested context)
            add_test(NAME iiGUI.menus_${_case} COMMAND iiGUIMenuTests ${_case})
            set_tests_properties(iiGUI.menus_${_case} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
        endforeach()
        add_executable(iiGUISelfWidgetTests tests/self_widgets.mm)
        target_link_libraries(iiGUISelfWidgetTests PRIVATE iiGUI::iiGUI "-framework AppKit")
        set_target_properties(iiGUISelfWidgetTests PROPERTIES OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED YES OBJCXX_EXTENSIONS OFF)
        target_compile_options(iiGUISelfWidgetTests PRIVATE -fobjc-arc)
        foreach(_case pointer keyboard controls native_events cache tabs compound press_events)
            add_test(NAME iiGUI.self_widgets_${_case} COMMAND iiGUISelfWidgetTests ${_case})
            set_tests_properties(iiGUI.self_widgets_${_case} PROPERTIES TIMEOUT 20 RUN_SERIAL TRUE)
        endforeach()
        add_executable(iiGUIWidgetTests tests/widgets.mm)
        target_link_libraries(iiGUIWidgetTests PRIVATE iiGUI::iiGUI "-framework AppKit")
        set_target_properties(iiGUIWidgetTests PROPERTIES OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED YES OBJCXX_EXTENSIONS OFF)
        target_compile_options(iiGUIWidgetTests PRIVATE -fobjc-arc)
        foreach(_case lifecycle rollback ownership reentrant failure content_switch lvrs_appearance)
            add_test(NAME iiGUI.widgets_${_case} COMMAND iiGUIWidgetTests ${_case})
            set_tests_properties(iiGUI.widgets_${_case} PROPERTIES TIMEOUT 15 RUN_SERIAL TRUE)
        endforeach()
        add_executable(iiGUIApplicationTests tests/application.cpp)
        target_link_libraries(iiGUIApplicationTests PRIVATE iiGUI::iiGUI)
        foreach(_case shared_loop failed_attachment callback_failure)
            add_test(NAME iiGUI.application_${_case} COMMAND iiGUIApplicationTests ${_case})
            set_tests_properties(iiGUI.application_${_case} PROPERTIES TIMEOUT 15 RUN_SERIAL TRUE)
        endforeach()
        add_executable(iiGUIPopoverTests tests/popovers.cpp)
        target_link_libraries(iiGUIPopoverTests PRIVATE iiGUI::iiGUI)
        foreach(_case lifecycle policies placement nesting rendering lifetime reentrant)
            add_test(NAME iiGUI.popovers_${_case} COMMAND iiGUIPopoverTests ${_case})
            set_tests_properties(iiGUI.popovers_${_case} PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
        endforeach()
        add_executable(iiGUIColorButtonTests tests/color_buttons.cpp)
        add_executable(iiGUIPopupContentTests tests/popup_content.cpp)
        add_executable(iiGUIAnchoredContentTests tests/anchored_content.cpp)
        add_executable(iiGUICellEditorTests tests/cell_editor.cpp)
        target_link_libraries(iiGUICellEditorTests PRIVATE iiGUI::iiGUI)
        foreach(_case editing validation clipping ellipsis)
            add_test(NAME iiGUI.cell_editor_${_case} COMMAND iiGUICellEditorTests ${_case})
            set_tests_properties(iiGUI.cell_editor_${_case} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
        endforeach()
        add_executable(iiGUIScrollViewTests tests/scroll_view.cpp)
        add_executable(iiGUIMaterialInputTests tests/material_inputs.cpp)
        target_link_libraries(iiGUIMaterialInputTests PRIVATE iiGUI::iiGUI)
        foreach(_case validation parameters large_image provider frame_stop frame_rearm frame_retirement mask shadow underlay highlight cache vulkan_mask vulkan_shadow vulkan_underlay vulkan_cache)
            add_test(NAME iiGUI.material_input_${_case} COMMAND iiGUIMaterialInputTests ${_case})
            set_tests_properties(iiGUI.material_input_${_case} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
        endforeach()
        add_executable(iiGUIImageCompositionTests tests/image_composition.cpp)
        target_link_libraries(iiGUIImageCompositionTests PRIVATE iiGUI::iiGUI)
        foreach(_case crop mask border gradient region transform cache validation order metrics paint checked mutation slot focus retirement paint_error)
            add_test(NAME iiGUI.image_composition_${_case} COMMAND iiGUIImageCompositionTests ${_case})
            set_tests_properties(iiGUI.image_composition_${_case} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
        endforeach()
        add_executable(iiGUIButtonLayoutTests tests/button_layout.cpp)
        target_link_libraries(iiGUIButtonLayoutTests PRIVATE iiGUI::iiGUI)
        foreach(_case asymmetry spacing retained validation pixels)
            add_test(NAME iiGUI.button_layout_${_case} COMMAND iiGUIButtonLayoutTests ${_case})
            set_tests_properties(iiGUI.button_layout_${_case} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
        endforeach()
        add_executable(iiGUIContainerLayoutTests tests/container_layout.cpp)
        target_link_libraries(iiGUIContainerLayoutTests PRIVATE iiGUI::iiGUI)
        foreach(_case minimum implicit intrinsic hidden slots nested typography button_options cancellation validation retirement)
            add_test(NAME iiGUI.container_layout_${_case} COMMAND iiGUIContainerLayoutTests ${_case})
            set_tests_properties(iiGUI.container_layout_${_case} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
        endforeach()
        target_link_libraries(iiGUIScrollViewTests PRIVATE iiGUI::iiGUI)
        foreach(_case geometry input rendering nested popup z_order)
            add_test(NAME iiGUI.scroll_view_${_case} COMMAND iiGUIScrollViewTests ${_case})
            set_tests_properties(iiGUI.scroll_view_${_case} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
        endforeach()
        target_link_libraries(iiGUIAnchoredContentTests PRIVATE iiGUI::iiGUI)
        foreach(_case lifetime layout motion input focus rendering teardown contracts routing exit_label nesting reentrant_focus label_focus no_focus)
            add_test(NAME iiGUI.anchored_content_${_case} COMMAND iiGUIAnchoredContentTests ${_case})
            set_tests_properties(iiGUI.anchored_content_${_case} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
        endforeach()
        add_executable(iiGUIPopupContentNativeTests tests/popup_content_native.mm)
        target_link_libraries(iiGUIPopupContentNativeTests PRIVATE iiGUI::iiGUI)
        target_compile_options(iiGUIPopupContentNativeTests PRIVATE -fobjc-arc)
        set_target_properties(iiGUIPopupContentNativeTests PROPERTIES OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED YES OBJCXX_EXTENSIONS OFF)
        add_test(NAME iiGUI.popup_content_native COMMAND iiGUIPopupContentNativeTests)
        set_tests_properties(iiGUI.popup_content_native PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
        target_link_libraries(iiGUIPopupContentTests PRIVATE iiGUI::iiGUI)
        foreach(_case lifecycle input rendering failure reuse nesting limits reentrant teardown active_use)
            add_test(NAME iiGUI.popup_content_${_case} COMMAND iiGUIPopupContentTests ${_case})
            set_tests_properties(iiGUI.popup_content_${_case} PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
        endforeach()
        target_link_libraries(iiGUIColorButtonTests PRIVATE iiGUI::iiGUI)
        foreach(_case contracts input rendering cache lifetime)
            add_test(NAME iiGUI.color_buttons_${_case} COMMAND iiGUIColorButtonTests ${_case})
            set_tests_properties(iiGUI.color_buttons_${_case} PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
        endforeach()
        add_executable(iiGUICompositionTests tests/composition.cpp)
        target_link_libraries(iiGUICompositionTests PRIVATE iiGUI::iiGUI)
        foreach(_case layout input images)
            add_test(NAME iiGUI.composition_${_case} COMMAND iiGUICompositionTests ${_case})
            set_tests_properties(iiGUI.composition_${_case} PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
        endforeach()
        add_executable(iiGUIBackdropTests tests/backdrop.cpp)
        add_executable(iiGUIShadowTests tests/shadows.cpp)
        add_executable(iiGUIContentTransformTests tests/content_transform.cpp)
        target_link_libraries(iiGUIContentTransformTests PRIVATE iiGUI::iiGUI)
        foreach(_case validation pixels edges reuse vulkan translation)
            add_test(NAME iiGUI.content_transform_${_case} COMMAND iiGUIContentTransformTests ${_case})
            set_tests_properties(iiGUI.content_transform_${_case} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
        endforeach()
        target_link_libraries(iiGUIShadowTests PRIVATE iiGUI::iiGUI)
        foreach(_case validation pixels budget vulkan)
            add_test(NAME iiGUI.shadows_${_case} COMMAND iiGUIShadowTests ${_case})
            set_tests_properties(iiGUI.shadows_${_case} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
        endforeach()
        add_executable(iiGUIAlertPrimitiveTests tests/alert_primitives.cpp)
        target_link_libraries(iiGUIAlertPrimitiveTests PRIVATE iiGUI::iiGUI)
        foreach(_case layout style events focus stroke_native stroke_vulkan)
            add_test(NAME iiGUI.alert_primitives_${_case} COMMAND iiGUIAlertPrimitiveTests ${_case})
            set_tests_properties(iiGUI.alert_primitives_${_case} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
        endforeach()
        target_link_libraries(iiGUIBackdropTests PRIVATE iiGUI::iiGUI)
        foreach(_case validation pixels vulkan)
            add_test(NAME iiGUI.backdrop_${_case} COMMAND iiGUIBackdropTests ${_case})
            set_tests_properties(iiGUI.backdrop_${_case} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
        endforeach()
        add_executable(iiGUIDialogLayoutTests tests/dialog_layout.cpp)
        target_link_libraries(iiGUIDialogLayoutTests PRIVATE iiGUI::iiGUI)
        foreach(_case geometry cancellation stacks)
            add_test(NAME iiGUI.dialog_layout_${_case} COMMAND iiGUIDialogLayoutTests ${_case})
            set_tests_properties(iiGUI.dialog_layout_${_case} PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
        endforeach()
        add_executable(iiGUIFrameHostTests tests/frame_host.cpp)
        add_executable(iiGUIPopupMotionTests tests/popup_motion.cpp)
        target_link_libraries(iiGUIPopupMotionTests PRIVATE iiGUI::iiGUI)
        foreach(_case lifecycle reverse preferences input paused teardown hidden limits)
            add_test(NAME iiGUI.popup_motion_${_case} COMMAND iiGUIPopupMotionTests ${_case})
            set_tests_properties(iiGUI.popup_motion_${_case} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
        endforeach()
        target_link_libraries(iiGUIFrameHostTests PRIVATE iiGUI::iiGUI)
        foreach(_case display failure contracts widgets)
            add_test(NAME iiGUI.frame_host_${_case} COMMAND iiGUIFrameHostTests ${_case})
            set_tests_properties(iiGUI.frame_host_${_case} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
        endforeach()
    endif()
    add_executable(iiGUIRendererPolicyTests tests/renderer.cpp)
    target_include_directories(iiGUIRendererPolicyTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/include/iiGUI")
    target_link_libraries(iiGUIRendererPolicyTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.renderer_policy COMMAND iiGUIRendererPolicyTests)
    add_executable(iiGUIIOFixture tests/io_fixture.cpp)
    target_link_libraries(iiGUIIOFixture PRIVATE iiSDKCore::Runtime)
    add_executable(iiGUIHostTests tests/host.cpp)
    target_link_libraries(iiGUIHostTests PRIVATE iiGUI::iiGUI)
    foreach(_case contracts lifecycle wakeup earlier_deadline close_veto failure close_failure shutdown_race producer_race full_queue wrong_thread graphics_contracts graphics graphics_vulkan core_io core_operation core_frame core_stop)
        add_test(NAME iiGUI.${_case} COMMAND iiGUIHostTests ${_case} $<TARGET_FILE:iiGUIIOFixture>)
        set_tests_properties(iiGUI.${_case} PROPERTIES TIMEOUT 15 RUN_SERIAL TRUE)
    endforeach()
    set_tests_properties(iiGUI.graphics_vulkan PROPERTIES TIMEOUT 30)
    find_package(Python3 3.9 COMPONENTS Interpreter REQUIRED)
    add_test(NAME iiGUI.lvrs_appearance_snapshot COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/appearance.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME iiGUI.vulkan_shaders COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME iiGUI.direct3d_shader COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/direct3d_shader.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    set_tests_properties(iiGUI.direct3d_shader PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)
    add_test(NAME iiGUI.webgl_shader COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/webgl_shader.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    set_tests_properties(iiGUI.webgl_shader PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)
    add_test(NAME iiGUI.boundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME iiGUI.boundary_contract COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_manifest_boundary.py")
    set_tests_properties(iiGUI.boundary_contract PROPERTIES TIMEOUT 30)
    add_test(NAME iiGUI.source_boundary_contract COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_source_boundary.py")
    set_tests_properties(iiGUI.source_boundary_contract PROPERTIES TIMEOUT 30)
    if(NOT MSVC)
        add_test(NAME iiGUI.event_listener_compat COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/event_listener_compat.py" "${CMAKE_CXX_COMPILER}")
        add_test(NAME iiGUI.popover_compat COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/popover_compat.py" "${CMAKE_CXX_COMPILER}")
    endif()
    add_test(NAME iiGUI.independent_consumer_contract COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_independent_consumer.py")
    set_tests_properties(iiGUI.independent_consumer_contract PROPERTIES TIMEOUT 30)
    add_test(NAME iiGUI.independent_consumer COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/independent_consumer.py" "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_COMMAND}" "$<CONFIG>")
    set_tests_properties(iiGUI.independent_consumer PROPERTIES TIMEOUT 600)
    add_test(NAME iiGUI.tooling COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tooling.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME iiGUI.svg_source COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_svg_source.py" "${CMAKE_COMMAND}")
    add_test(NAME iiGUI.resource_install COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/resource_install.py" "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_COMMAND}" "${CMAKE_INSTALL_DATADIR}")
    add_test(NAME iiGUI.aot_desktop_tooling COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot_desktop.py")
    add_test(NAME iiGUI.android_hello_tooling COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/android_hello.py")
    add_test(NAME iiGUI.layout_install_tooling COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/layout_install_tooling.py")
    add_test(NAME iiGUI.host_consumer_tooling COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/host_consumer_tooling.py")
    add_test(NAME iiGUI.web_server COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/web_server.py")
    add_test(NAME iiGUI.web_runtime_evidence COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/web_runtime.py")
    if(WIN32)
        add_executable(iiGUIWindowsDesktopTests tests/desktop_windows.cpp)
        target_link_libraries(iiGUIWindowsDesktopTests PRIVATE iiGUI::iiGUI)
        add_test(NAME iiGUI.windows_desktop_validation COMMAND iiGUIWindowsDesktopTests)
        set_tests_properties(iiGUI.windows_desktop_validation PROPERTIES TIMEOUT 10)
    endif()
endif()
option(IIGUI_BUILD_EXAMPLES "Build native host examples" OFF)
option(IIGUI_BUILD_LVUI_EXAMPLES "Build optional apps that compose LVUI and iiGUI" OFF)
set(iiGUI_NATIVE_RESOURCES)
foreach(_asset IN LISTS iiGUI_NATIVE_RESOURCE_FILES)
    list(APPEND iiGUI_NATIVE_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/resources/native/${_asset}")
endforeach()
if(IIGUI_BUILD_EXAMPLES)
    if(IIGUI_BUILD_LVUI_EXAMPLES AND NOT EMSCRIPTEN AND NOT ANDROID AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        add_subdirectory(examples/HelloLVUI)
    endif()
    if(IIGUI_BUILD_LVUI_EXAMPLES AND CMAKE_SYSTEM_NAME MATCHES "^(Darwin|Windows|Linux)$" AND NOT ANDROID)
        add_subdirectory(examples/HelloWorld)
    endif()
    if(IIGUI_BUILD_LVUI_EXAMPLES AND EMSCRIPTEN)
        add_subdirectory(examples/HelloHosts)
    endif()
endif()
install(DIRECTORY examples/HelloWorld DESTINATION ${CMAKE_INSTALL_DATADIR}/iiGUI/examples
    PATTERN build EXCLUDE)
install(DIRECTORY examples/HelloAndroid DESTINATION ${CMAKE_INSTALL_DATADIR}/iiGUI/examples
    PATTERN build EXCLUDE)
install(FILES tests/android_os.cpp tests/android_retained_labels.cpp DESTINATION ${CMAKE_INSTALL_DATADIR}/iiGUI/tests)
install(DIRECTORY examples/HelloHosts DESTINATION ${CMAKE_INSTALL_DATADIR}/iiGUI/examples
    PATTERN build EXCLUDE)
install(DIRECTORY resources/ DESTINATION ${CMAKE_INSTALL_DATADIR}/iiGUI/resources
    PATTERN ".*" EXCLUDE PATTERN "dependencies" EXCLUDE PATTERN "font-metadata" EXCLUDE
    PATTERN "android-entry" EXCLUDE PATTERN "android-text" EXCLUDE PATTERN "android-pointer" EXCLUDE
    PATTERN "android-global" EXCLUDE PATTERN "android-editor" EXCLUDE PATTERN "android-keyboard" EXCLUDE)

# Instrumented native process startup is not a release-latency measurement.
# Keep inner behavioral assertions; allow cold OS/GPU initialization to finish.
if(BUILD_TESTING AND IIGUI_SANITIZE)
    function(_iigui_sanitizer_test_budget directory)
        get_property(_tests DIRECTORY "${directory}" PROPERTY TESTS)
        foreach(_test IN LISTS _tests)
            get_property(_timeout TEST "${_test}" DIRECTORY "${directory}" PROPERTY TIMEOUT)
            if(NOT _timeout OR _timeout LESS 120)
                set_tests_properties("${_test}" DIRECTORY "${directory}" PROPERTIES TIMEOUT 120)
            endif()
        endforeach()
        get_property(_children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
        foreach(_child IN LISTS _children)
            _iigui_sanitizer_test_budget("${_child}")
        endforeach()
    endfunction()
    _iigui_sanitizer_test_budget("${CMAKE_CURRENT_SOURCE_DIR}")
endif()
