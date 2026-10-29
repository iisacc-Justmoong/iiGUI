cmake_minimum_required(VERSION 3.31)
project(iiGUIInstalledConsumer LANGUAGES CXX)
include(CTest)
# This application explicitly composes the independently packaged SDKs.
find_package(iiGUI 0.1 CONFIG REQUIRED)
find_package(LVUI 0.1 CONFIG REQUIRED)
add_executable(iiGUIInstalledConsumer WIN32 MACOSX_BUNDLE main.cpp)
target_link_libraries(iiGUIInstalledConsumer PRIVATE iiGUI::iiGUI LVUI::LVUI)
if(MSVC)
    target_link_options(iiGUIInstalledConsumer PRIVATE /ENTRY:mainCRTStartup)
endif()
set_target_properties(iiGUIInstalledConsumer PROPERTIES CXX_EXTENSIONS OFF
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.iisacc.iigui.installed-consumer")
add_test(NAME iiGUI.installed_consumer COMMAND iiGUIInstalledConsumer)
set_tests_properties(iiGUI.installed_consumer PROPERTIES TIMEOUT 15
    PASS_REGULAR_EXPRESSION "INSTALLED_GUI_OK.*background=42.*resize=true.*closed=true")
add_test(NAME iiGUI.installed_vulkan_consumer COMMAND iiGUIInstalledConsumer --vulkan)
set_tests_properties(iiGUI.installed_vulkan_consumer PROPERTIES TIMEOUT 20 RUN_SERIAL TRUE
    PASS_REGULAR_EXPRESSION "INSTALLED_GUI_OK.*lvui=true.*vulkan=true")
