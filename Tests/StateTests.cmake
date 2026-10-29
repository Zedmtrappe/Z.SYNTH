# JUCE-side tests (parameters, presets, state, layout, editor) and the preset
# audition tool. Compiles the same app sources as the plug-in.

juce_add_console_app(ZSynthStateTests PRODUCT_NAME "ZSynthStateTests")
target_sources(ZSynthStateTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/StateTests.cpp ${ZSYNTH_APP_SOURCES})
target_include_directories(ZSynthStateTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/Source)
target_compile_definitions(ZSynthStateTests PRIVATE
    ${ZSYNTH_APP_DEFINITIONS}
    JucePlugin_VersionString="${PROJECT_VERSION}"
    JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(ZSynthStateTests PRIVATE
    zsynth_dsp
    ZSynthData
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_gui_basics
    juce::juce_recommended_config_flags)
if(NOT MSVC)
    target_compile_options(ZSynthStateTests PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

add_test(NAME state COMMAND ZSynthStateTests)

# Loads the built VST3 through JUCE's VST3 hosting code, like a DAW would.
juce_add_console_app(ZSynthVst3HostTest PRODUCT_NAME "ZSynthVst3HostTest")
target_sources(ZSynthVst3HostTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Vst3HostTest.cpp)
target_compile_definitions(ZSynthVst3HostTest PRIVATE
    JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(ZSynthVst3HostTest PRIVATE
    juce::juce_audio_processors juce::juce_gui_basics juce::juce_recommended_config_flags)
add_dependencies(ZSynthVst3HostTest ZSynth_VST3)
add_test(NAME vst3_host COMMAND ZSynthVst3HostTest $<TARGET_FILE_DIR:ZSynth_VST3>/../../../Z.SYNTH.vst3)
