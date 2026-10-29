cmake_minimum_required(VERSION 3.31)
project(iiGUI VERSION 0.4.1 LANGUAGES C CXX)
set(CMAKE_CXX_EXTENSIONS OFF)
set(IIGUI_LINUX_BACKEND "AUTO" CACHE STRING "Linux window system: AUTO, WAYLAND or X11")
set_property(CACHE IIGUI_LINUX_BACKEND PROPERTY STRINGS AUTO WAYLAND X11)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_language(OBJCXX)
    set(_host_source src/Platform/HostIOS.mm)
    set(_graphics_source src/Graphics/CanvasMetal.mm)
    set(_text_source src/Text/TextMac.mm src/Text/TextLineMac.mm)
    set(_vulkan_window src/Graphics/VulkanWindowIOS.mm)
    set(_widgets_source src/Widgets/NativeWidgetsIOS.mm)
    set(_desktop_source src/Platform/DesktopIOS.mm)
    set(_font_source src/Text/FontNativeMac.mm)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    enable_language(OBJCXX)
    set(_host_source src/Platform/Host.mm)
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
    set(_host_source src/Platform/HostAndroid.cpp)
    set(_graphics_source)
    set(_text_source src/Text/TextAndroid.cpp src/Text/AndroidLabelFont.cpp)
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
    if(NOT IIGUI_LINUX_BACKEND MATCHES "^(AUTO|WAYLAND|X11)$")
        message(FATAL_ERROR "IIGUI_LINUX_BACKEND must be AUTO, WAYLAND or X11")
    endif()
    if(NOT IIGUI_LINUX_BACKEND STREQUAL "X11")
        pkg_check_modules(Wayland QUIET IMPORTED_TARGET wayland-client>=1.20 wayland-cursor>=1.20 xkbcommon>=1.0)
    endif()
    if(IIGUI_LINUX_BACKEND STREQUAL "WAYLAND" AND NOT Wayland_FOUND)
        message(FATAL_ERROR "Wayland host requires wayland-client and xkbcommon development packages")
    endif()
    if(Wayland_FOUND AND NOT IIGUI_LINUX_BACKEND STREQUAL "X11")
        set(IIGUI_LINUX_BACKEND WAYLAND)
        set(_host_source src/Platform/HostWayland.cpp src/Platform/WaylandConnection.cpp)
        set(_widgets_source src/Widgets/NativeWidgetsWayland.cpp)
        set(_vulkan_window src/Graphics/VulkanWindowWayland.cpp)
        set(_desktop_source src/Platform/DesktopWayland.cpp src/Platform/NativeDialogsLinux.cpp)
    else()
        set(IIGUI_LINUX_BACKEND X11)
        find_package(X11 REQUIRED COMPONENTS Xi)
        set(_host_source src/Platform/HostLinux.cpp)
        set(_vulkan_window src/Graphics/VulkanWindowLinux.cpp)
        set(_widgets_source src/Widgets/NativeWidgetsLinux.cpp)
        set(_desktop_source src/Platform/DesktopLinux.cpp src/Platform/NativeDialogsLinux.cpp)
    endif()
    set(_graphics_source)
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
option(BUILD_SHARED_LIBS "Build a shared iiGUI library" OFF)
option(IIGUI_SANITIZE "Enable address/undefined behavior sanitizers" OFF)
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

add_library(iiGUI src/Core/Dispatch.cpp src/Graphics/Graphics.cpp src/Text/Text.cpp src/Graphics/NativeCanvas.cpp src/Platform/HostCapabilities.cpp
    src/Widgets/ColorButton.cpp
    src/Widgets/NativeWidgets.cpp src/Widgets/WidgetState.cpp src/Widgets/ColorPicker.cpp src/Widgets/ColorSurface.cpp src/Graphics/Image.cpp src/Graphics/Png.cpp src/Platform/Foreground.cpp src/Core/Application.cpp src/Presentation/Appearance.cpp ${_widgets_source}
    ${_host_source} ${_graphics_source} ${_text_source} ${_vulkan_window} ${_desktop_source})
add_library(iiGUI::iiGUI ALIAS iiGUI)
target_sources(iiGUI PRIVATE src/Presentation/Motion.cpp src/Presentation/Presentation.cpp)
target_sources(iiGUI PRIVATE src/Widgets/NativeView.cpp)
target_sources(iiGUI PRIVATE src/Text/FontDatabase.cpp src/Text/FontMetadata.cpp ${_font_source})
target_include_directories(iiGUI PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_sources(iiGUI PRIVATE src/Text/TextRasterCache.cpp)
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
target_link_libraries(iiGUI PUBLIC iiSDKCore::Runtime)
if(NOT EMSCRIPTEN)
    target_sources(iiGUI PRIVATE src/Graphics/VulkanCanvas.cpp)
    target_link_libraries(iiGUI PRIVATE Vulkan::Vulkan)
endif()
target_link_libraries(iiGUI PRIVATE lunasvg::lunasvg)
target_include_directories(iiGUI SYSTEM PRIVATE "${lunasvg_SOURCE_DIR}/plutovg/source")
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_link_libraries(iiGUI PRIVATE "-framework UIKit" "-framework IOSurface" "-framework MetalKit" "-framework UniformTypeIdentifiers" "-framework CoreText" "-framework CoreFoundation" "-framework Metal" "-framework MetalPerformanceShaders" "-framework QuartzCore")
    set_source_files_properties(${_host_source} ${_graphics_source} ${_text_source} ${_vulkan_window} ${_widgets_source} ${_desktop_source} ${_font_source} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_link_libraries(iiGUI PRIVATE "-framework AppKit" "-framework CoreText" "-framework CoreFoundation" "-framework Metal" "-framework MetalPerformanceShaders" "-framework QuartzCore")
    set_source_files_properties(src/Platform/Host.mm src/Widgets/NativeWidgetsMac.mm src/Platform/DesktopMac.mm src/Graphics/CanvasMetal.mm src/Text/TextMac.mm src/Text/TextLineMac.mm src/Graphics/VulkanWindowMac.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    set_source_files_properties(${_font_source} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
elseif(WIN32)
    target_link_libraries(iiGUI PRIVATE user32 gdi32 d3d11 dxgi d3dcompiler d2d1 dwrite windowscodecs ole32 imm32 shell32)
    target_compile_definitions(iiGUI PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
elseif(ANDROID)
    target_link_libraries(iiGUI PRIVATE android log dl)
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
endif()
target_include_directories(iiGUI PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
generate_export_header(iiGUI BASE_NAME IIGUI
    EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/iiGUI/Export.h")
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
    VISIBILITY_INLINES_HIDDEN YES VERSION ${PROJECT_VERSION} SOVERSION 0)
if(IIGUI_SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "IIGUI_SANITIZE requires Clang/GCC address and undefined behavior sanitizers")
    endif()
    target_compile_options(iiGUI PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(iiGUI PUBLIC -fsanitize=address,undefined)
    foreach(_codec lunasvg plutovg)
        target_compile_options(${_codec} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    endforeach()
endif()

set(_package "${CMAKE_INSTALL_LIBDIR}/cmake/iiGUI")
install(TARGETS iiGUI EXPORT iiGUITargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES
    src/Core/Bindings.h
    src/Text/FontDatabase.h
    src/Graphics/Image.h
    src/Presentation/Presentation.h
    src/Graphics/Graphics.h src/Text/Text.h src/Text/TextBoundary.h src/Text/TextLine.h src/Text/TextRasterCache.h src/Widgets/WidgetInteraction.h
    src/Platform/Host.h
    src/Widgets/NativeWidgets.h
    src/Widgets/NativeView.h
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
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/iiGUI/Export.h"
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
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/iiGUI)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DATADIR}/iiGUI/docs)
install(FILES src/third_party/utf8proc/LICENSE.md src/third_party/utf8proc/provenance.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/iiGUI/utf8proc)

if(BUILD_TESTING)
    add_test(NAME iiGUI.source_layout COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/source_layout.cmake)
    add_executable(iiGUIWrappedTextTests tests/wrapped_text.cpp)
    target_link_libraries(iiGUIWrappedTextTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.wrapped_text COMMAND iiGUIWrappedTextTests)
    set_tests_properties(iiGUI.wrapped_text PROPERTIES TIMEOUT 30)
    add_executable(iiGUIAccessibilitySemanticsTests tests/accessibility_semantics.cpp)
    target_link_libraries(iiGUIAccessibilitySemanticsTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.accessibility_semantics COMMAND iiGUIAccessibilitySemanticsTests)
    set_tests_properties(iiGUI.accessibility_semantics PROPERTIES TIMEOUT 30)
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
    add_executable(iiGUIWaylandPolicyTests tests/wayland_policy.cpp)
    target_link_libraries(iiGUIWaylandPolicyTests PRIVATE iiGUI::iiGUI)
    add_test(NAME iiGUI.wayland_policy COMMAND iiGUIWaylandPolicyTests)
    set_tests_properties(iiGUI.wayland_policy PROPERTIES TIMEOUT 15)
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
        add_test(NAME iiGUI.font_metadata_${_case} COMMAND iiGUIFontMetadataTests ${_case} "${CMAKE_CURRENT_SOURCE_DIR}/resources/native" "${CMAKE_CURRENT_BINARY_DIR}")
        set_tests_properties(iiGUI.font_metadata_${_case} PROPERTIES TIMEOUT 30)
    endforeach()
    add_executable(iiGUIFontWidgetTests tests/font_widgets.cpp)
    target_link_libraries(iiGUIFontWidgetTests PRIVATE iiGUI::iiGUI)
    foreach(_case tokens updates identity retirement paint_context)
        add_test(NAME iiGUI.font_widgets_${_case} COMMAND iiGUIFontWidgetTests ${_case})
        set_tests_properties(iiGUI.font_widgets_${_case} PROPERTIES TIMEOUT 30)
    endforeach()
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
    add_executable(iiGUIAncestorInputTests tests/ancestor_input.cpp)
    target_link_libraries(iiGUIAncestorInputTests PRIVATE iiGUI::iiGUI)
    foreach(_case ancestor text adjustment)
        add_test(NAME iiGUI.ancestor_input.${_case} COMMAND iiGUIAncestorInputTests ${_case})
        set_tests_properties(iiGUI.ancestor_input.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(iiGUIKeyedContentTests tests/keyed_content.cpp)
    target_link_libraries(iiGUIKeyedContentTests PRIVATE iiGUI::iiGUI)
    foreach(_case identity rollback bounded capacity retirement popup positioned virtual_scroll measurement)
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
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
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
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
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
            "${iiGUI_LVUI_SOURCE_DIR}/src" "${iiGUI_LVUI_SOURCE_DIR}/build/include" "${iiGUI_LVUI_SOURCE_DIR}/tests/fixtures")
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
        find_package(iiColorManager 0.1.1 CONFIG REQUIRED COMPONENTS Values)
        get_property(_lvui_test_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
        foreach(_lvui_target IN LISTS _lvui_test_targets)
            if(_lvui_target MATCHES "^iiGUINative")
                target_link_libraries(${_lvui_target} PRIVATE iiColorManager::Values)
            endif()
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
    foreach(_case initial empty identity replace returned exception empty_candidate invalid independent clear retirement foreign reentry retire_during_build nested drawing frame_data)
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
    add_test(NAME iiGUI.boundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME iiGUI.tooling COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tooling.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME iiGUI.aot_desktop_tooling COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot_desktop.py")
    add_test(NAME iiGUI.android_hello_tooling COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/android_hello.py")
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
option(IIGUI_BUILD_EXAMPLES "Build native host examples" ON)
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
install(DIRECTORY examples/HelloHosts DESTINATION ${CMAKE_INSTALL_DATADIR}/iiGUI/examples
    PATTERN build EXCLUDE)
install(DIRECTORY resources/ DESTINATION ${CMAKE_INSTALL_DATADIR}/iiGUI/resources)

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
