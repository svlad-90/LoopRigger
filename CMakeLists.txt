cmake_minimum_required(VERSION 3.22)

project(LoopRigger VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(LiveLoopingDependencies)

option(LIVELOOPING_BUILD_JUCE_APP "Build the JUCE product shell" OFF)
option(LIVELOOPING_BUILD_PROFILE_IO "Build JSON controller profile loading support" ON)
set(LIVELOOPING_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout")
option(LIVELOOPING_FETCH_JUCE "Fetch JUCE with CMake when LIVELOOPING_JUCE_DIR is not set" ON)
set(LIVELOOPING_JUCE_TAG "9.0.0" CACHE STRING "JUCE git tag used by FetchContent")
set(LIVELOOPING_NLOHMANN_JSON_TAG "v3.11.3" CACHE STRING "nlohmann/json git tag used by FetchContent")
option(LIVELOOPING_SKIP_JUCE_PLATFORM_CHECK "Skip platform package checks for JUCE targets" OFF)

add_library(livelooping_core
    src/ControlMapping.cpp
    src/LiveLoopingEngine.cpp
)

target_include_directories(livelooping_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(livelooping_core PUBLIC cxx_std_17)

if(LIVELOOPING_BUILD_PROFILE_IO)
    livelooping_add_json_dependency()

    add_library(livelooping_profile_io
        src/ProfileLoader.cpp
    )

    target_include_directories(livelooping_profile_io
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(livelooping_profile_io
        PUBLIC
            livelooping_core
        PRIVATE
            nlohmann_json::nlohmann_json
    )

    target_compile_features(livelooping_profile_io PUBLIC cxx_std_17)
endif()

add_executable(livelooping_sim_cli
    apps/cli/main.cpp
)

target_link_libraries(livelooping_sim_cli PRIVATE livelooping_core)
target_compile_definitions(livelooping_sim_cli
    PRIVATE
        LIVELOOPING_PROFILE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/profiles"
)

if(LIVELOOPING_BUILD_PROFILE_IO)
    target_link_libraries(livelooping_sim_cli PRIVATE livelooping_profile_io)
    target_compile_definitions(livelooping_sim_cli PRIVATE LIVELOOPING_HAS_PROFILE_IO=1)
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(livelooping_core_smoke_tests
        tests/core_smoke_tests.cpp
    )

    target_link_libraries(livelooping_core_smoke_tests PRIVATE livelooping_core)
    target_compile_definitions(livelooping_core_smoke_tests
        PRIVATE
            LIVELOOPING_PROFILE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/profiles"
            LIVELOOPING_LAYOUT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/layouts"
    )

    if(LIVELOOPING_BUILD_PROFILE_IO)
        target_link_libraries(livelooping_core_smoke_tests PRIVATE livelooping_profile_io)
        target_compile_definitions(livelooping_core_smoke_tests PRIVATE LIVELOOPING_HAS_PROFILE_IO=1)
    endif()

    add_test(NAME livelooping_core_smoke_tests COMMAND livelooping_core_smoke_tests)
endif()

if(LIVELOOPING_BUILD_JUCE_APP)
    livelooping_check_linux_juce_gui_dependencies()
    livelooping_add_juce_dependency()

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
    )

    if(LIVELOOPING_BUILD_PROFILE_IO)
        target_link_libraries(livelooping_product PRIVATE livelooping_profile_io)
        target_compile_definitions(livelooping_product
            PRIVATE
                LIVELOOPING_HAS_PROFILE_IO=1
                LIVELOOPING_LAYOUT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/layouts"
        )
    endif()

    target_compile_definitions(livelooping_product
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
    )
endif()
