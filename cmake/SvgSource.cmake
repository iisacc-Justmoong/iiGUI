# Reuse the project-owned dependency source without replacing local changes.
function(iigui_svg_source output root)
    set(${output} "" PARENT_SCOPE)
    if(DEFINED FETCHCONTENT_SOURCE_DIR_LUNASVG AND NOT "${FETCHCONTENT_SOURCE_DIR_LUNASVG}" STREQUAL "")
        set(${output} "${FETCHCONTENT_SOURCE_DIR_LUNASVG}" PARENT_SCOPE)
        return()
    endif()
    set(path "${root}/resources/dependencies/lunasvg")
    if(EXISTS "${path}")
        if(NOT EXISTS "${path}/CMakeLists.txt" OR
           NOT EXISTS "${path}/plutovg/CMakeLists.txt" OR
           NOT EXISTS "${path}/plutovg/source/plutovg-stb-image.h")
            message(FATAL_ERROR "Incomplete LunaSVG source at ${path}; preserve or repair it before configuring")
        endif()
        set(${output} "${path}" PARENT_SCOPE)
    endif()
endfunction()
