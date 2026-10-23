cmake_minimum_required(VERSION 3.31)
project(HelloWorld LANGUAGES CXX)
include(CTest)
if(NOT TARGET iiGUI::iiGUI)
    find_package(iiGUI 0.4 CONFIG REQUIRED)
endif()
if(NOT APPLE OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    message(FATAL_ERROR "HelloWorld native-widget example currently requires macOS/AppKit")
endif()
lvui_generate(NAME HelloWorld SOURCE HelloWorld.lvui OUTPUT_VARIABLE _view INCLUDES HelloBackend.h)
iisdkcore_add_application(HelloWorld GUI SOURCES main.cpp "${_view}"
    LIBRARIES iiGUI::iiGUI RESOURCES ${iiGUI_NATIVE_RESOURCES})
target_include_directories(HelloWorld PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(HelloWorld PRIVATE cxx_std_23)
set_target_properties(HelloWorld PROPERTIES CXX_EXTENSIONS OFF
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.iisacc.lvui.helloworld"
    MACOSX_BUNDLE_BUNDLE_NAME "LVUI Hello World")
if(BUILD_TESTING)
    add_test(NAME HelloWorld.aot_cocoa COMMAND HelloWorld --self-test)
    # Bundle/font loading is outside the five-second callback watchdog. Allow a
    # cold external-volume launch without weakening the runtime lifecycle check.
    set_tests_properties(HelloWorld.aot_cocoa PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
endif()
