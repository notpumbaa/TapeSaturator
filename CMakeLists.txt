cmake_minimum_required(VERSION 3.22)

project(TapeSaturator VERSION 1.1.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS: Universal Binary (Apple Silicon + Intel), ab macOS 11
if(APPLE)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    endif()
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
    endif()
endif()

option(TAPESAT_BUILD_TESTS "Build DSP tests / offline renderer" ON)
option(TAPESAT_COPY_AFTER_BUILD "Copy plugins into the system plugin folders after building" OFF)

# ---------------------------------------------------------------------------
# JUCE
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.9
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
juce_add_plugin(TapeSaturator
    COMPANY_NAME                "notpumbaa"
    BUNDLE_ID                   "com.notpumbaa.tapesaturator"
    PLUGIN_MANUFACTURER_CODE    Ntpb
    PLUGIN_CODE                 Tsat
    FORMATS                     VST3 AU Standalone
    PRODUCT_NAME                "TapeSaturator"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Distortion
    AU_MAIN_TYPE                kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD     ${TAPESAT_COPY_AFTER_BUILD}
)

set(TAPESAT_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/dsp/TapeEngine.cpp
    Source/ui/TapeLookAndFeel.cpp
    Source/ui/VUMeter.cpp
    Source/ui/Fonts.cpp
    Source/ui/TapeMachine.cpp
)

set(TAPESAT_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# Eingebettete Schriften (SIL Open Font License, siehe Resources/fonts/OFL-*.txt)
juce_add_binary_data(TapeSaturatorAssets
    NAMESPACE TapeAssets
    SOURCES
        Resources/fonts/BarlowCondensed-SemiBold.ttf
        Resources/fonts/BarlowCondensed-Medium.ttf
        Resources/fonts/PlayfairDisplaySC-Black.ttf
)
set_target_properties(TapeSaturatorAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(TAPESAT_LIBS
    TapeSaturatorAssets
    juce::juce_audio_utils
    juce::juce_dsp
)

target_sources(TapeSaturator PRIVATE ${TAPESAT_SOURCES})
target_include_directories(TapeSaturator PRIVATE Source)
target_compile_definitions(TapeSaturator PUBLIC ${TAPESAT_DEFINITIONS})
target_link_libraries(TapeSaturator
    PRIVATE
        ${TAPESAT_LIBS}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ---------------------------------------------------------------------------
# Tests / Offline-Renderer (Konsolen-App, braucht keine DAW)
# ---------------------------------------------------------------------------
if(TAPESAT_BUILD_TESTS)
    enable_testing()

    juce_add_console_app(TapeSaturatorTests PRODUCT_NAME "TapeSaturatorTests")
    target_sources(TapeSaturatorTests PRIVATE tests/TestMain.cpp ${TAPESAT_SOURCES})
    target_include_directories(TapeSaturatorTests PRIVATE Source)
    target_compile_definitions(TapeSaturatorTests PRIVATE ${TAPESAT_DEFINITIONS})
    target_link_libraries(TapeSaturatorTests
        PRIVATE
            ${TAPESAT_LIBS}
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    add_test(NAME TapeSaturatorTests COMMAND TapeSaturatorTests)
endif()
