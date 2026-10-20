# All destinations are disposable test-owned paths below the caller's build/.
foreach(_required SOURCE BUILD CORE_BUILD LVUI_BUILD GUI_BUILD)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "Missing ${_required}")
    endif()
endforeach()
cmake_path(CONVERT "${BUILD}" TO_CMAKE_PATH_LIST BUILD NORMALIZE)
if(NOT BUILD MATCHES "/build(/|$)")
    message(FATAL_ERROR "Package verification must stay under build/")
endif()
set(_root "${BUILD}/installed-consumer")
set(_stage "${_root}/stage")
set(_relocated "${_root}/relocated")
file(MAKE_DIRECTORY "${_root}")
if(EXISTS "${_relocated}")
    message(FATAL_ERROR "Consumer verification destination already exists; use another build/ location")
endif()
foreach(_project IN ITEMS "${CORE_BUILD}" "${LVUI_BUILD}" "${GUI_BUILD}")
    execute_process(COMMAND "${CMAKE_COMMAND}" --install "${_project}" --prefix "${_stage}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _output ERROR_VARIABLE _error)
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR "Install failed: ${_output}\n${_error}")
    endif()
endforeach()
file(RENAME "${_stage}" "${_relocated}")
file(COPY "${SOURCE}/tests/consumer/" DESTINATION "${_root}/source")
set(_toolchain_args)
set(_clean_env "${CMAKE_COMMAND}" -E env --unset=CPATH --unset=CPLUS_INCLUDE_PATH
    --unset=C_INCLUDE_PATH --unset=LIBRARY_PATH --unset=LD_LIBRARY_PATH
    --unset=DYLD_LIBRARY_PATH --unset=DYLD_FALLBACK_LIBRARY_PATH
    --unset=DYLD_FRAMEWORK_PATH --unset=DYLD_FALLBACK_FRAMEWORK_PATH)
if(DEFINED TOOLCHAIN AND NOT TOOLCHAIN STREQUAL "")
    list(APPEND _toolchain_args "-DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN}")
    list(APPEND _clean_env --unset=PKG_CONFIG_PATH)
endif()
list(APPEND _clean_env --)
set(_vulkan_args)
foreach(_variable Vulkan_INCLUDE_DIR Vulkan_LIBRARY)
    # Native SDK dependencies are external to the relocated Core/LVUI/iiGUI prefix.
    # Reuse this kit's target paths, never discover a different host Vulkan SDK.
    file(STRINGS "${GUI_BUILD}/CMakeCache.txt" _cache_line REGEX "^${_variable}:[^=]*=")
    if(_cache_line)
        string(REGEX REPLACE "^[^=]*=" "" _value "${_cache_line}")
        list(APPEND _vulkan_args "-D${_variable}=${_value}")
    endif()
endforeach()
execute_process(COMMAND ${_clean_env} "${CMAKE_COMMAND}" -S "${_root}/source" -B "${_root}/build" -G Ninja
    ${_toolchain_args} ${_vulkan_args}
    "-DCMAKE_PREFIX_PATH=${_relocated}" -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF
    -DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON
    -DCMAKE_DISABLE_FIND_PACKAGE_iiFileProviderNative=ON
    -DCMAKE_DISABLE_FIND_PACKAGE_iiFileProviderCoreIO=ON
    RESULT_VARIABLE _result OUTPUT_VARIABLE _output ERROR_VARIABLE _error)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "Consumer configure failed: ${_output}\n${_error}")
endif()
file(STRINGS "${_root}/build/CMakeCache.txt" _gui_dir REGEX "^iiGUI_DIR:PATH=")
file(STRINGS "${_root}/build/CMakeCache.txt" _core_dir REGEX "^iiSDKCore_DIR:PATH=")
file(STRINGS "${_root}/build/CMakeCache.txt" _lvui_dir REGEX "^LVUI_DIR:PATH=")
if(NOT _gui_dir STREQUAL "iiGUI_DIR:PATH=${_relocated}/lib/cmake/iiGUI" OR
   NOT _lvui_dir STREQUAL "LVUI_DIR:PATH=${_relocated}/lib/cmake/LVUI" OR
   NOT _core_dir STREQUAL "iiSDKCore_DIR:PATH=${_relocated}/lib/cmake/iiSDKCore")
    message(FATAL_ERROR "Consumer resolved a package outside its relocated prefix: ${_gui_dir}; ${_core_dir}")
endif()
execute_process(COMMAND ${_clean_env} "${CMAKE_COMMAND}" --build "${_root}/build" --parallel 4
    RESULT_VARIABLE _result OUTPUT_VARIABLE _output ERROR_VARIABLE _error)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "Consumer build failed: ${_output}\n${_error}")
endif()
file(STRINGS "${_root}/build/compile_commands.json" _commands)
cmake_path(GET SOURCE PARENT_PATH _sdk_source)
if(_commands MATCHES "${SOURCE}/include" OR _commands MATCHES "${SOURCE}/src" OR _commands MATCHES "${_sdk_source}/LVUI/include")
    message(FATAL_ERROR "Installed consumer accessed iiGUI source-tree headers")
endif()
if(SKIP_EXECUTION)
    message(STATUS "Installed C++23 consumer COMPILED using relocated packages: ${_relocated}; execution explicitly NOT RUN")
else()
    execute_process(COMMAND ${_clean_env} "${CMAKE_CTEST_COMMAND}" --test-dir "${_root}/build" --output-on-failure
        --output-junit "${_root}/tests.xml"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _output ERROR_VARIABLE _error)
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR "Consumer execution failed: ${_output}\n${_error}")
    endif()
    message(STATUS "${_output}")
    message(STATUS "Installed C++23 consumer passed using relocated packages: ${_relocated}")
endif()
