# The source layout is grouped by responsibility; public include paths stay stable.
foreach(_area IN ITEMS API Core Graphics Platform Presentation Text Widgets)
    file(GLOB _headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/${_area}/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/src/${_area}/*.hpp")
    foreach(_header IN LISTS _headers)
        get_filename_component(_name "${_header}" NAME)
        if(DEFINED _iigui_header_${_name})
            message(FATAL_ERROR "Duplicate iiGUI header basename: ${_name}")
        endif()
        set("_iigui_header_${_name}" "${_header}")
        file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/include/iiGUI/${_name}"
            CONTENT "#pragma once\n#include \"${_header}\"\n")
    endforeach()
endforeach()
