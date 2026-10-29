# Generated once from the pinned upstream protocol. No target executable or
# target wayland-scanner runs during cross compilation or installed consumption.
set(_wayland_protocol "${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/wayland")
file(READ "${_wayland_protocol}/provenance.json" _wayland_manifest)
foreach(_file xdg-shell.xml xdg-shell-client-protocol.h xdg-shell-protocol.c)
    string(JSON _expected GET "${_wayland_manifest}" files "${_file}")
    file(SHA256 "${_wayland_protocol}/${_file}" _actual)
    if(NOT _actual STREQUAL _expected)
        message(FATAL_ERROR "Pinned Wayland protocol changed: ${_file}")
    endif()
endforeach()
target_sources(iiGUI PRIVATE "${_wayland_protocol}/xdg-shell-protocol.c")
target_include_directories(iiGUI PRIVATE "${_wayland_protocol}")
install(FILES "${_wayland_protocol}/xdg-shell.xml" "${_wayland_protocol}/provenance.json"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/iiGUI/wayland)
