cmake_minimum_required(VERSION 3.31)
project(HelloGUI VERSION 0.4.0 LANGUAGES CXX)
include(CTest)
find_package(iiGUI 0.4 CONFIG REQUIRED)
add_executable(HelloGUI WIN32 MACOSX_BUNDLE main.cpp)
target_link_libraries(HelloGUI PRIVATE iiGUI::iiGUI)
if(MSVC)
    # Keep ordinary C++ main and CRT initialization in a GUI-subsystem image.
    target_link_options(HelloGUI PRIVATE /ENTRY:mainCRTStartup)
endif()
target_compile_features(HelloGUI PRIVATE cxx_std_23)
set_target_properties(HelloGUI PROPERTIES CXX_EXTENSIONS OFF
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.iisacc.iigui.hello"
    MACOSX_BUNDLE_BUNDLE_NAME "HelloGUI"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "1")
if(BUILD_TESTING)
    add_test(NAME HelloGUI.native_lifecycle COMMAND HelloGUI --self-test)
    set_tests_properties(HelloGUI.native_lifecycle PROPERTIES TIMEOUT 15
        PASS_REGULAR_EXPRESSION "HELLOGUI_OK.*background=42.*resized=true.*closed=true")
endif()
install(TARGETS HelloGUI BUNDLE DESTINATION . RUNTIME DESTINATION bin)
