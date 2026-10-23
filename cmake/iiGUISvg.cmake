include(FetchContent)
# Pinned native codec for SVG and bounded in-memory PNG, never UI source or scripts.
set(LUNASVG_BUILD_EXAMPLES OFF CACHE BOOL "No asset-codec examples" FORCE)
set(LUNASVG_DISABLE_LOAD_SYSTEM_FONTS ON CACHE BOOL "No implicit SVG font file access" FORCE)
set(LUNASVG_DISABLE_EXTERNAL_RESOURCES ON CACHE BOOL "No SVG external file access" FORCE)
set(PLUTOVG_BUILD_EXAMPLES OFF CACHE BOOL "No asset-codec examples" FORCE)
FetchContent_Declare(lunasvg
    GIT_REPOSITORY https://github.com/sammycage/lunasvg.git
    GIT_TAG 83c58df8103dc7dca423dfd824992af94d49bed6
    GIT_SUBMODULES_RECURSE TRUE
    UPDATE_DISCONNECTED TRUE
    SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/dependencies/lunasvg/source"
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/dependencies/lunasvg/build")
FetchContent_MakeAvailable(lunasvg)
target_compile_features(lunasvg PRIVATE cxx_std_23)
target_compile_definitions(lunasvg PRIVATE LUNASVG_DISABLE_LOAD_SYSTEM_FONTS LUNASVG_DISABLE_EXTERNAL_RESOURCES)
install(FILES "${lunasvg_SOURCE_DIR}/LICENSE" DESTINATION ${CMAKE_INSTALL_DATADIR}/iiGUI/licenses RENAME LunaSVG-LICENSE)
install(FILES "${lunasvg_SOURCE_DIR}/plutovg/LICENSE" DESTINATION ${CMAKE_INSTALL_DATADIR}/iiGUI/licenses RENAME PlutoVG-LICENSE)
file(READ "${lunasvg_SOURCE_DIR}/plutovg/source/plutovg-stb-image.h" _stb_image_source)
string(FIND "${_stb_image_source}" "ALTERNATIVE A - MIT License" _stb_license_start)
string(FIND "${_stb_image_source}" "ALTERNATIVE B - Public Domain" _stb_license_end)
if(_stb_license_start LESS 0 OR _stb_license_end LESS _stb_license_start)
    message(FATAL_ERROR "Pinned PNG codec license section is missing")
endif()
math(EXPR _stb_license_length "${_stb_license_end}-${_stb_license_start}")
string(SUBSTRING "${_stb_image_source}" ${_stb_license_start} ${_stb_license_length} _stb_license)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/STB-Image-LICENSE" "${_stb_license}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/STB-Image-LICENSE" DESTINATION ${CMAKE_INSTALL_DATADIR}/iiGUI/licenses)
