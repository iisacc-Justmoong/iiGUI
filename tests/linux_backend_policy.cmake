include("${SOURCE_ROOT}/cmake/iiGUILinuxBackend.cmake")

if(DEFINED REQUEST)
    iigui_select_linux_backend("${REQUEST}" "${AVAILABLE}" result)
    return()
endif()

foreach(request AUTO WAYLAND wayland)
    iigui_select_linux_backend("${request}" TRUE result)
    if(NOT result STREQUAL "WAYLAND")
        message(FATAL_ERROR "${request} did not select Wayland")
    endif()
endforeach()
iigui_select_linux_backend("X11" FALSE result)
if(NOT result STREQUAL "X11")
    message(FATAL_ERROR "Explicit legacy X11 selection was not preserved")
endif()
foreach(request AUTO WAYLAND)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DREQUEST=${request}" -DAVAILABLE=FALSE -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE result ERROR_VARIABLE error)
    if(result EQUAL 0 OR NOT error MATCHES "Wayland")
        message(FATAL_ERROR "${request} silently accepted missing Wayland: ${error}")
    endif()
endforeach()
execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
    -DREQUEST=invalid -DAVAILABLE=TRUE -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE result ERROR_VARIABLE error)
if(result EQUAL 0 OR NOT error MATCHES "IIGUI_LINUX_BACKEND")
    message(FATAL_ERROR "Invalid backend was accepted: ${error}")
endif()
