# JUCE plug-in / standalone targets built from the shared engine.

include(FetchContent)
if(ZSYNTH_JUCE_DIR)
    add_subdirectory(${ZSYNTH_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(ZSYNTH_FORMATS Standalone VST3)
if(APPLE)
    list(APPEND ZSYNTH_FORMATS AU)
endif()

juce_add_binary_data(ZSynthData SOURCES ${CMAKE_SOURCE_DIR}/Resources/Presets/FactoryBank.zsbank)
set_target_properties(ZSynthData PROPERTIES POSITION_INDEPENDENT_CODE ON)

juce_add_plugin(ZSynth
    COMPANY_NAME "Z.SYNTH"
    COMPANY_WEBSITE "https://example.invalid/zsynth"
    BUNDLE_ID "com.zsynth.zsynth"
    PRODUCT_NAME "Z.SYNTH"
    VERSION ${PROJECT_VERSION}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    PLUGIN_MANUFACTURER_CODE Zsyn
    PLUGIN_CODE Zsy1
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    MICROPHONE_PERMISSION_ENABLED FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS ${ZSYNTH_FORMATS})

set(ZSYNTH_APP_SOURCES
    ${CMAKE_SOURCE_DIR}/Source/Parameters/ParameterLayout.cpp
    ${CMAKE_SOURCE_DIR}/Source/Presets/PresetFormat.cpp
    ${CMAKE_SOURCE_DIR}/Source/Presets/PresetManager.cpp
    ${CMAKE_SOURCE_DIR}/Source/MIDI/MidiLearn.cpp
    ${CMAKE_SOURCE_DIR}/Source/Plugin/ZSynthProcessor.cpp
    ${CMAKE_SOURCE_DIR}/Source/UI/Components/Theme.cpp
    ${CMAKE_SOURCE_DIR}/Source/UI/Components/ParamControl.cpp
    ${CMAKE_SOURCE_DIR}/Source/UI/Components/Displays.cpp
    ${CMAKE_SOURCE_DIR}/Source/UI/Components/Keyboard.cpp
    ${CMAKE_SOURCE_DIR}/Source/UI/Components/PresetBrowser.cpp
    ${CMAKE_SOURCE_DIR}/Source/UI/Layout/PanelLayout.cpp
    ${CMAKE_SOURCE_DIR}/Source/UI/Layout/ActivityRules.cpp
    ${CMAKE_SOURCE_DIR}/Source/UI/MainPanel.cpp
    ${CMAKE_SOURCE_DIR}/Source/UI/ZSynthEditor.cpp
    CACHE INTERNAL "")

set(ZSYNTH_APP_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    $<$<PLATFORM_ID:Linux>:JUCE_JACK=1>
    $<$<PLATFORM_ID:Linux>:JUCE_ALSA=1>
    CACHE INTERNAL "")

target_sources(ZSynth PRIVATE ${ZSYNTH_APP_SOURCES})
target_compile_definitions(ZSynth PUBLIC ${ZSYNTH_APP_DEFINITIONS})
target_link_libraries(ZSynth
    PRIVATE
        zsynth_dsp
        ZSynthData
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# JUCE's bundled "recommended warning flags" include -Wswitch-enum, which fires
# even on switches that have a default, burying real warnings. Use the same
# -Wall -Wextra baseline as the DSP library instead.
if(MSVC)
    target_compile_options(ZSynth PRIVATE /W4)
else()
    target_compile_options(ZSynth PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
