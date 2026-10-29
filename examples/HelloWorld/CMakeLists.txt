cmake_minimum_required(VERSION 3.31)
project(HelloWorld LANGUAGES CXX)
include(CTest)
if(NOT TARGET iiGUI::iiGUI)
    find_package(iiGUI 0.4 CONFIG REQUIRED)
endif()
if(NOT TARGET LVUI::Native)
    find_package(LVUI 0.1 CONFIG REQUIRED COMPONENTS Native)
endif()
if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Darwin|Windows|Linux)$" OR ANDROID)
    message(FATAL_ERROR "HelloWorld requires a native macOS, Windows or Linux desktop target")
endif()
lvui_generate(NAME HelloWorld SOURCE HelloWorld.lvui OUTPUT_VARIABLE _view INCLUDES HelloBackend.h)
iisdkcore_add_application(HelloWorld GUI SOURCES main.cpp "${_view}"
    LIBRARIES iiGUI::iiGUI LVUI::Native RESOURCES ${iiGUI_NATIVE_RESOURCES})
target_include_directories(HelloWorld PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(HelloWorld PRIVATE cxx_std_23)
target_compile_definitions(HelloWorld PRIVATE HELLO_WORLD_NATIVE_HOST="${CMAKE_SYSTEM_NAME}")
set_target_properties(HelloWorld PROPERTIES CXX_EXTENSIONS OFF
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.iisacc.lvui.helloworld"
    MACOSX_BUNDLE_BUNDLE_NAME "LVUI Hello World")
if(BUILD_TESTING)
    # Keep the original macOS test name for existing consumers.
    set(_test HelloWorld.aot_native)
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(_test HelloWorld.aot_cocoa)
    endif()
    add_test(NAME ${_test} COMMAND HelloWorld --self-test)
    # Bundle/font loading is outside the five-second callback watchdog. Allow a
    # cold external-volume launch without weakening the runtime lifecycle check.
    set_tests_properties(${_test} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE
        PASS_REGULAR_EXPRESSION "HELLO_WORLD_AOT_OK.*timer=true.*background=42.*resize=true.*closed=true")
endif()
install(TARGETS HelloWorld BUNDLE DESTINATION . RUNTIME DESTINATION bin)
if(NOT APPLE)
    install(FILES ${iiGUI_NATIVE_RESOURCES} DESTINATION bin/resources)
endif()
