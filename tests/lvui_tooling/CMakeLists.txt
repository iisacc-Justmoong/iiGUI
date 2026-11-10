cmake_minimum_required(VERSION 3.31)
project(iiGUIExternalLVUITooling LANGUAGES CXX)
include("${IIGUI_SOURCE_ROOT}/cmake/iiGUILvui.cmake")
iigui_compile_lvui(NAME Example SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Hello.lvui"
    COMPILER "${IIGUI_TEST_COMPILER}" OUTPUT_VARIABLE generated)
add_custom_target(generated_view ALL DEPENDS "${generated}")
add_library(generated_view_cpp OBJECT Compile.cpp "${generated}")
target_include_directories(generated_view_cpp PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
    "${IIGUI_TEST_HEADERS}" "${IIGUI_CORE_HEADERS}")
target_compile_features(generated_view_cpp PRIVATE cxx_std_23)
