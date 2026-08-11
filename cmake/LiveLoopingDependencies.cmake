include_guard(GLOBAL)

function(livelooping_check_linux_juce_gui_dependencies)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        return()
    endif()

    if(LIVELOOPING_SKIP_JUCE_PLATFORM_CHECK)
        message(WARNING "Skipping Linux JUCE platform dependency checks")
        return()
    endif()

    find_package(PkgConfig QUIET)
    if(NOT PkgConfig_FOUND)
        message(FATAL_ERROR
            "pkg-config is required to configure the JUCE GUI target on Linux.\n"
            "Ubuntu/Debian package: pkg-config\n"
            "Pass -DLIVELOOPING_SKIP_JUCE_PLATFORM_CHECK=ON to bypass this check.")
    endif()

    set(required_modules
        freetype2
        fontconfig
        x11
        xcomposite
        xcursor
        xext
        xinerama
        xrandr
        xrender
        xi
    )

    set(missing_modules "")
    foreach(module IN LISTS required_modules)
        string(MAKE_C_IDENTIFIER "${module}" module_var)
        pkg_check_modules("LIVELOOPING_DEP_${module_var}" QUIET "${module}")
        if(NOT LIVELOOPING_DEP_${module_var}_FOUND)
            list(APPEND missing_modules "${module}")
        endif()
    endforeach()

    if(missing_modules)
        string(JOIN ", " missing_text ${missing_modules})
        message(FATAL_ERROR
            "Missing Linux development packages for the JUCE GUI target: ${missing_text}\n"
            "Ubuntu/Debian packages:\n"
            "  sudo apt install pkg-config libfreetype-dev libfontconfig1-dev "
            "libx11-dev libxcomposite-dev libxcursor-dev libxext-dev "
            "libxinerama-dev libxrandr-dev libxrender-dev libxi-dev\n"
            "Pass -DLIVELOOPING_SKIP_JUCE_PLATFORM_CHECK=ON to bypass this check.")
    endif()
endfunction()

function(livelooping_add_juce_dependency)
    if(LIVELOOPING_JUCE_DIR)
        add_subdirectory("${LIVELOOPING_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
        return()
    endif()

    if(NOT LIVELOOPING_FETCH_JUCE)
        message(FATAL_ERROR
            "Set LIVELOOPING_JUCE_DIR or enable LIVELOOPING_FETCH_JUCE")
    endif()

    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ${LIVELOOPING_JUCE_TAG}
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endfunction()
