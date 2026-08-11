cmake_minimum_required(VERSION 3.22)

project(LoopRigger VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LIVELOOPING_BUILD_JUCE_APP "Build the JUCE product shell" OFF)
set(LIVELOOPING_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout")
option(LIVELOOPING_FETCH_JUCE "Fetch JUCE with CMake when LIVELOOPING_JUCE_DIR is not set" ON)
set(LIVELOOPING_JUCE_TAG "9.0.0" CACHE STRING "JUCE git tag used by FetchContent")

add_library(livelooping_core
    src/ControlMapping.cpp
    src/LiveLoopingEngine.cpp
)

target_include_directories(livelooping_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(livelooping_core PUBLIC cxx_std_17)

add_executable(livelooping_sim_cli
    apps/cli/main.cpp
)

target_link_libraries(livelooping_sim_cli PRIVATE livelooping_core)

include(CTest)

if(BUILD_TESTING)
    add_executable(livelooping_core_smoke_tests
        tests/core_smoke_tests.cpp
    )

    target_link_libraries(livelooping_core_smoke_tests PRIVATE livelooping_core)

    add_test(NAME livelooping_core_smoke_tests COMMAND livelooping_core_smoke_tests)
endif()

if(LIVELOOPING_BUILD_JUCE_APP)
    if(LIVELOOPING_JUCE_DIR)
        add_subdirectory(${LIVELOOPING_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
    elseif(LIVELOOPING_FETCH_JUCE)
        include(FetchContent)
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG ${LIVELOOPING_JUCE_TAG}
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(JUCE)
    else()
        message(FATAL_ERROR "Set LIVELOOPING_JUCE_DIR or enable LIVELOOPING_FETCH_JUCE")
    endif()

    juce_add_gui_app(livelooping_product
        PRODUCT_NAME "LiveLooping"
        COMPANY_NAME "Dream Machines"
        BUNDLE_ID "com.dreammachines.livelooping"
    )

    target_sources(livelooping_product
        PRIVATE
            apps/juce/Main.cpp
    )

    target_link_libraries(livelooping_product
        PRIVATE
            livelooping_core
            juce::juce_gui_extra
            juce::juce_audio_utils
            juce::juce_audio_processors
    )

    target_compile_definitions(livelooping_product
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
    )
endif()
