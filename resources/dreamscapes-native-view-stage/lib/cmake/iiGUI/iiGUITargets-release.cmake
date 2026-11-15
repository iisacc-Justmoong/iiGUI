#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "iiGUI::iiGUI" for configuration "Release"
set_property(TARGET iiGUI::iiGUI APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(iiGUI::iiGUI PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C;CXX;OBJCXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libiiGUI.a"
  )

list(APPEND _cmake_import_check_targets iiGUI::iiGUI )
list(APPEND _cmake_import_check_files_for_iiGUI::iiGUI "${_IMPORT_PREFIX}/lib/libiiGUI.a" )

# Import target "iiGUI::HostBridge" for configuration "Release"
set_property(TARGET iiGUI::HostBridge APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(iiGUI::HostBridge PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libiiGUIHostBridge.a"
  )

list(APPEND _cmake_import_check_targets iiGUI::HostBridge )
list(APPEND _cmake_import_check_files_for_iiGUI::HostBridge "${_IMPORT_PREFIX}/lib/libiiGUIHostBridge.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
