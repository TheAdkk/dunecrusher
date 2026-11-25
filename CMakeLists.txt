cmake_minimum_required(VERSION 3.22)
project(DuneCrusher VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS deployment target")

option(DUNECRUSHER_BUILD_TESTS "Build DSP unit tests and VST3 smoke verifier" ON)
set(DUNECRUSHER_JUCE_DIR "" CACHE PATH "Optional path to an existing local JUCE 8 checkout")

if(DUNECRUSHER_JUCE_DIR AND EXISTS "${DUNECRUSHER_JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${DUNECRUSHER_JUCE_DIR}" "${CMAKE_BINARY_DIR}/juce" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.17-security
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

set(DUNECRUSHER_FORMATS VST3)
if(APPLE)
    list(APPEND DUNECRUSHER_FORMATS AU)
endif()

juce_add_plugin(DuneCrusher
    COMPANY_NAME "TheAdkk Audio"
    PRODUCT_NAME "DuneCrusher"
    BUNDLE_ID "com.theadkk.DuneCrusher"
    PLUGIN_MANUFACTURER_CODE Adkk
    PLUGIN_CODE DnCr
    FORMATS ${DUNECRUSHER_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    AU_SANDBOX_SAFE TRUE
    VST3_CATEGORIES Fx Distortion Dynamics Filter
)

juce_add_binary_data(DuneCrusherWebUI
    HEADER_NAME DuneCrusherWebUI.h
    NAMESPACE dunecrusher_webui
    SOURCES
        ui/dist/index.html
)
set_target_properties(DuneCrusherWebUI PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(DuneCrusher
    PRIVATE
        src/plugin/PluginProcessor.cpp
        src/plugin/PluginEditor.cpp
)

target_include_directories(DuneCrusher
    PRIVATE
        src/dsp
        src/plugin
)

target_compile_definitions(DuneCrusher
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_REPORT_APP_USAGE=0
)
if(WIN32)
    target_compile_definitions(DuneCrusher PUBLIC JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)
endif()

target_link_libraries(DuneCrusher
    PRIVATE
        DuneCrusherWebUI
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(DUNECRUSHER_BUILD_TESTS)
    enable_testing()

    add_executable(dunecrusher_dsp_tests tests/dsp_unit_tests.cpp)
    target_include_directories(dunecrusher_dsp_tests PRIVATE src/dsp)
    target_compile_options(dunecrusher_dsp_tests PRIVATE -O2 -Wall -Wextra)
    add_test(NAME dsp_unit_tests COMMAND dunecrusher_dsp_tests)

    juce_add_console_app(dunecrusher_verify PRODUCT_NAME "dunecrusher_verify")
    target_sources(dunecrusher_verify
        PRIVATE
            tests/verify_plugin.cpp
    )
    target_compile_definitions(dunecrusher_verify
        PRIVATE
            JUCE_PLUGINHOST_VST3=1
            JUCE_WEB_BROWSER=1
            JUCE_USE_CURL=0
            JUCE_REPORT_APP_USAGE=0
    )
    target_link_libraries(dunecrusher_verify
        PRIVATE
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_gui_extra
    )
endif()
