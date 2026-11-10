cmake_minimum_required(VERSION 3.31)
project(iiGUIInstalledPaintConsumer LANGUAGES CXX)
find_package(iiGUI 0.5 CONFIG REQUIRED PATHS "${IIGUI_STAGE}" NO_DEFAULT_PATH)
iigui_compile_lvui(NAME Example SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../lvui_tooling/Hello.lvui"
    COMPILER "${IIGUI_TEST_COMPILER}" OUTPUT_VARIABLE generated)
add_executable(installed_paint_consumer main.cpp "${generated}")
target_include_directories(installed_paint_consumer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(installed_paint_consumer PRIVATE iiGUI::iiGUI)
