cmake_minimum_required(VERSION 3.31)
project(iiGUIInstalledConsumer LANGUAGES CXX)
include(CTest)
# The unchanged 0.1 public API must remain consumable after adding backends.
find_package(iiGUI 0.1 CONFIG REQUIRED)
add_executable(iiGUIInstalledConsumer WIN32 MACOSX_BUNDLE main.cpp)
target_link_libraries(iiGUIInstalledConsumer PRIVATE iiGUI::iiGUI)
if(MSVC)
    target_link_options(iiGUIInstalledConsumer PRIVATE /ENTRY:mainCRTStartup)
endif()
set_target_properties(iiGUIInstalledConsumer PROPERTIES CXX_EXTENSIONS OFF
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.iisacc.iigui.installed-consumer")
add_test(NAME iiGUI.installed_consumer COMMAND iiGUIInstalledConsumer)
set_tests_properties(iiGUI.installed_consumer PROPERTIES TIMEOUT 15
    PASS_REGULAR_EXPRESSION "INSTALLED_GUI_OK.*background=42.*resize=true.*closed=true")
