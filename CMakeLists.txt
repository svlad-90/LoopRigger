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
option(LIVELOOPING_BUILD_PYTHON_SCRIPTING "Build embedded Python device scripting support" OFF)
option(LIVELOOPING_BUILD_JUCE_PLUGIN_HOST "Build JUCE plugin scanning support" OFF)
set(LIVELOOPING_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout")
option(LIVELOOPING_FETCH_JUCE "Fetch JUCE with CMake when LIVELOOPING_JUCE_DIR is not set" ON)
set(LIVELOOPING_JUCE_TAG "9.0.0" CACHE STRING "JUCE git tag used by FetchContent")
set(LIVELOOPING_NLOHMANN_JSON_TAG "v3.11.3" CACHE STRING "nlohmann/json git tag used by FetchContent")
option(LIVELOOPING_SKIP_JUCE_PLATFORM_CHECK "Skip platform package checks for JUCE targets" OFF)

add_library(livelooping_core
    components/core/src/LiveLoopingEngine.cpp
)

target_include_directories(livelooping_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/components/core/api
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(livelooping_core PUBLIC cxx_std_17)

add_library(livelooping_control
    components/control/src/ControlMapping.cpp
)

target_include_directories(livelooping_control
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/components/control/api
)

target_link_libraries(livelooping_control
    PUBLIC
        livelooping_core
)

target_compile_features(livelooping_control PUBLIC cxx_std_17)

add_library(livelooping_scripting
    components/scripting/src/ScriptDevice.cpp
)

target_include_directories(livelooping_scripting
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/components/scripting/api
)

target_link_libraries(livelooping_scripting
    PUBLIC
        livelooping_core
        livelooping_control
)

target_compile_features(livelooping_scripting PUBLIC cxx_std_17)

if(LIVELOOPING_BUILD_PYTHON_SCRIPTING)
    find_package(Python3 REQUIRED COMPONENTS Development.Embed)

    if(WIN32)
        get_target_property(LIVELOOPING_PYTHON_RUNTIME Python3::Python IMPORTED_LOCATION_RELEASE)
        if(NOT LIVELOOPING_PYTHON_RUNTIME)
            get_target_property(LIVELOOPING_PYTHON_RUNTIME Python3::Python IMPORTED_LOCATION)
        endif()
        if(LIVELOOPING_PYTHON_RUNTIME)
            get_filename_component(LIVELOOPING_PYTHON_RUNTIME_DIR "${LIVELOOPING_PYTHON_RUNTIME}" DIRECTORY)
        endif()
    endif()

    add_library(livelooping_python_scripting
        components/scripting/src/PythonScriptHost.cpp
    )

    target_include_directories(livelooping_python_scripting
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/components/scripting/api
    )

    target_link_libraries(livelooping_python_scripting
        PUBLIC
            livelooping_scripting
        PRIVATE
            Python3::Python
    )

    target_compile_features(livelooping_python_scripting PUBLIC cxx_std_17)
endif()

add_library(livelooping_virtual_devices
    components/virtual_devices/src/VirtualDeviceRegistry.cpp
)

target_include_directories(livelooping_virtual_devices
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/components/virtual_devices/api
)

target_link_libraries(livelooping_virtual_devices
    PUBLIC
        livelooping_control
)

target_compile_features(livelooping_virtual_devices PUBLIC cxx_std_17)

add_library(livelooping_plugin_host
    components/plugin_host/src/PluginCatalog.cpp
)

target_include_directories(livelooping_plugin_host
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/components/plugin_host/api
)

target_compile_features(livelooping_plugin_host PUBLIC cxx_std_17)

if(LIVELOOPING_BUILD_JUCE_PLUGIN_HOST)
    livelooping_check_linux_juce_plugin_host_dependencies()
    livelooping_add_juce_dependency()

    add_library(livelooping_juce_plugin_host
        components/plugin_host/src/JucePluginProbe.cpp
        components/plugin_host/src/JucePluginScanner.cpp
    )

    target_include_directories(livelooping_juce_plugin_host
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/components/plugin_host/api
    )

    target_link_libraries(livelooping_juce_plugin_host
        PUBLIC
            livelooping_plugin_host
        PRIVATE
            juce::juce_audio_processors
    )

    target_compile_definitions(livelooping_juce_plugin_host
        PRIVATE
            JUCE_PLUGINHOST_VST3=1
            JUCE_PLUGINHOST_AU=1
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
    )

    target_compile_features(livelooping_juce_plugin_host PUBLIC cxx_std_17)

    add_executable(livelooping_plugin_scan_cli
        apps/plugin_scan_cli/main.cpp
    )

    target_link_libraries(livelooping_plugin_scan_cli
        PRIVATE
            livelooping_juce_plugin_host
    )

    add_executable(livelooping_plugin_probe_cli
        apps/plugin_probe_cli/main.cpp
    )

    target_link_libraries(livelooping_plugin_probe_cli
        PRIVATE
            livelooping_juce_plugin_host
    )

    if(BUILD_TESTING)
        set(LIVELOOPING_PLUGIN_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/plugin-fixtures/vst3")

        juce_add_plugin(livelooping_probe_fixture_plugin
            COMPANY_NAME "LoopRigger"
            PRODUCT_NAME "LoopRigger Probe Fixture"
            PLUGIN_MANUFACTURER_CODE LpRg
            PLUGIN_CODE PrbF
            FORMATS VST3
            IS_SYNTH FALSE
            NEEDS_MIDI_INPUT FALSE
            NEEDS_MIDI_OUTPUT FALSE
            COPY_PLUGIN_AFTER_BUILD TRUE
            VST3_COPY_DIR "${LIVELOOPING_PLUGIN_FIXTURE_DIR}"
            NEEDS_WEB_BROWSER FALSE
            NEEDS_CURL FALSE
        )

        target_sources(livelooping_probe_fixture_plugin
            PRIVATE
                components/plugin_host/tests/fixtures/probe_vst3/ProbeFixtureProcessor.cpp
        )

        target_include_directories(livelooping_probe_fixture_plugin
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/components/plugin_host/tests/fixtures/probe_vst3
        )

        target_link_libraries(livelooping_probe_fixture_plugin
            PRIVATE
                juce::juce_audio_processors
        )

        target_compile_definitions(livelooping_probe_fixture_plugin
            PUBLIC
                JUCE_WEB_BROWSER=0
                JUCE_USE_CURL=0
                JUCE_VST3_CAN_REPLACE_VST2=0
        )

        juce_add_gui_app(livelooping_plugin_editor_host
            PRODUCT_NAME "LoopRigger Plugin Editor Host"
            COMPANY_NAME "LoopRigger"
            BUNDLE_ID "com.looprigger.plugin-editor-host"
        )

        target_sources(livelooping_plugin_editor_host
            PRIVATE
                apps/plugin_editor_host/main.cpp
        )

        target_link_libraries(livelooping_plugin_editor_host
            PRIVATE
                juce::juce_audio_processors
                juce::juce_gui_extra
        )

        target_compile_definitions(livelooping_plugin_editor_host
            PRIVATE
                LIVELOOPING_DEFAULT_PLUGIN_DIR="${LIVELOOPING_PLUGIN_FIXTURE_DIR}"
                JUCE_PLUGINHOST_VST3=1
                JUCE_PLUGINHOST_AU=1
                JUCE_WEB_BROWSER=0
                JUCE_USE_CURL=0
        )
    endif()
endif()

if(LIVELOOPING_BUILD_PROFILE_IO)
    livelooping_add_json_dependency()

    add_library(livelooping_profile_io
        components/profile_io/src/ProfileLoader.cpp
    )

    target_include_directories(livelooping_profile_io
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/components/profile_io/api
    )

    target_link_libraries(livelooping_profile_io
        PUBLIC
            livelooping_control
        PRIVATE
            nlohmann_json::nlohmann_json
    )

    target_compile_features(livelooping_profile_io PUBLIC cxx_std_17)
endif()

add_executable(livelooping_sim_cli
    apps/cli/main.cpp
)

target_link_libraries(livelooping_sim_cli PRIVATE livelooping_core)
target_link_libraries(livelooping_sim_cli PRIVATE livelooping_control)
target_compile_definitions(livelooping_sim_cli
    PRIVATE
        LIVELOOPING_PROFILE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/controller_profiles"
)

if(LIVELOOPING_BUILD_PROFILE_IO)
    target_link_libraries(livelooping_sim_cli PRIVATE livelooping_profile_io)
    target_compile_definitions(livelooping_sim_cli PRIVATE LIVELOOPING_HAS_PROFILE_IO=1)
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(livelooping_core_smoke_tests
        components/profile_io/tests/component_tests/core_smoke_tests.cpp
    )

    target_link_libraries(livelooping_core_smoke_tests PRIVATE livelooping_core)
    target_link_libraries(livelooping_core_smoke_tests PRIVATE livelooping_control)
    target_compile_definitions(livelooping_core_smoke_tests
        PRIVATE
            LIVELOOPING_PROFILE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/controller_profiles"
            LIVELOOPING_LAYOUT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/control_surfaces"
            LIVELOOPING_DEVICE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/devices"
    )

    if(LIVELOOPING_BUILD_PROFILE_IO)
        target_link_libraries(livelooping_core_smoke_tests PRIVATE livelooping_profile_io)
        target_compile_definitions(livelooping_core_smoke_tests PRIVATE LIVELOOPING_HAS_PROFILE_IO=1)
    endif()

    add_test(NAME livelooping_core_smoke_tests COMMAND livelooping_core_smoke_tests)

    add_executable(livelooping_scripting_unit_tests
        components/scripting/tests/unit_tests/script_api_tests.cpp
    )

    target_link_libraries(livelooping_scripting_unit_tests PRIVATE livelooping_scripting)

    add_test(NAME livelooping_scripting_unit_tests COMMAND livelooping_scripting_unit_tests)

    if(LIVELOOPING_BUILD_PYTHON_SCRIPTING)
        add_executable(livelooping_python_scripting_unit_tests
            components/scripting/tests/unit_tests/python_script_host_tests.cpp
        )

        target_link_libraries(livelooping_python_scripting_unit_tests PRIVATE livelooping_python_scripting)
        target_compile_definitions(livelooping_python_scripting_unit_tests
            PRIVATE
                LIVELOOPING_SCRIPTING_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/components/scripting/tests/unit_tests"
        )

        add_test(NAME livelooping_python_scripting_unit_tests COMMAND livelooping_python_scripting_unit_tests)
        if(WIN32 AND LIVELOOPING_PYTHON_RUNTIME_DIR)
            set_tests_properties(livelooping_python_scripting_unit_tests
                PROPERTIES
                    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${LIVELOOPING_PYTHON_RUNTIME_DIR}"
            )
        endif()
    endif()

    add_executable(livelooping_virtual_devices_unit_tests
        components/virtual_devices/tests/unit_tests/virtual_device_registry_tests.cpp
    )

    target_link_libraries(livelooping_virtual_devices_unit_tests PRIVATE livelooping_virtual_devices)

    add_test(NAME livelooping_virtual_devices_unit_tests COMMAND livelooping_virtual_devices_unit_tests)

    add_executable(livelooping_plugin_host_unit_tests
        components/plugin_host/tests/unit_tests/plugin_catalog_tests.cpp
    )

    target_link_libraries(livelooping_plugin_host_unit_tests PRIVATE livelooping_plugin_host)

    add_test(NAME livelooping_plugin_host_unit_tests COMMAND livelooping_plugin_host_unit_tests)

    if(LIVELOOPING_BUILD_JUCE_PLUGIN_HOST)
        add_executable(livelooping_juce_plugin_host_unit_tests
            components/plugin_host/tests/unit_tests/juce_plugin_scanner_tests.cpp
        )

        target_link_libraries(livelooping_juce_plugin_host_unit_tests PRIVATE livelooping_juce_plugin_host)

        add_test(NAME livelooping_juce_plugin_host_unit_tests COMMAND livelooping_juce_plugin_host_unit_tests)

        add_executable(livelooping_juce_plugin_probe_unit_tests
            components/plugin_host/tests/unit_tests/juce_plugin_probe_tests.cpp
        )

        target_link_libraries(livelooping_juce_plugin_probe_unit_tests PRIVATE livelooping_juce_plugin_host)

        add_test(NAME livelooping_juce_plugin_probe_unit_tests COMMAND livelooping_juce_plugin_probe_unit_tests)
    endif()
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
            livelooping_control
            juce::juce_gui_extra
    )

    if(LIVELOOPING_BUILD_PROFILE_IO)
        target_link_libraries(livelooping_product PRIVATE livelooping_profile_io)
        target_compile_definitions(livelooping_product
            PRIVATE
                LIVELOOPING_HAS_PROFILE_IO=1
                LIVELOOPING_LAYOUT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/control_surfaces"
        )
    endif()

    target_compile_definitions(livelooping_product
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
    )
endif()
