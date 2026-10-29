if(IS_DIRECTORY "${SOURCE_ROOT}/src/iiGUI")
    message(FATAL_ERROR "The source directory repeats the project name")
endif()
foreach(_file Core/Application.h Core/Application.cpp Platform/Host.h Platform/Host.mm
        Graphics/Graphics.h Graphics/Graphics.cpp Text/Text.h Text/Text.cpp
        Widgets/NativeView.h Widgets/NativeView.cpp Presentation/Appearance.h Presentation/Appearance.cpp API/iiGUI.h)
    if(NOT EXISTS "${SOURCE_ROOT}/src/${_file}")
        message(FATAL_ERROR "Missing responsibility-grouped source: ${_file}")
    endif()
endforeach()
