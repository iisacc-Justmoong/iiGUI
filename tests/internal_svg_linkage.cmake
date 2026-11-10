if(NOT LUNA_TYPE STREQUAL "STATIC_LIBRARY" OR NOT PLUTO_TYPE STREQUAL "STATIC_LIBRARY")
    message(FATAL_ERROR "The internal SVG codecs must be embedded in iiGUI")
endif()
