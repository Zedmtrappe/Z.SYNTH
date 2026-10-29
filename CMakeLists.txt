cmake_minimum_required(VERSION 3.22)

project(ZSynth VERSION 0.9.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(ZSYNTH_BUILD_PLUGIN "Build the JUCE standalone app and plug-ins" ON)
option(ZSYNTH_BUILD_TESTS  "Build the automated tests" ON)
set(ZSYNTH_JUCE_DIR "" CACHE PATH "Path to a JUCE 8 checkout. If empty, JUCE 8.0.9 is fetched with FetchContent.")

# -----------------------------------------------------------------------------
# Core DSP engine: plain C++17, no JUCE, no GUI. Shared by every target.
# -----------------------------------------------------------------------------
add_library(zsynth_dsp STATIC
    Source/Parameters/ParamTable.cpp
    Source/DSP/Wavetable/WavetableBank.cpp
    Source/DSP/Voices/Voice.cpp
    Source/DSP/Voices/VoiceManager.cpp
    Source/DSP/Performance/Arpeggiator.cpp
    Source/DSP/Performance/StepSequencer.cpp
    Source/DSP/Engine/SynthEngine.cpp
)
target_include_directories(zsynth_dsp PUBLIC Source)
set_target_properties(zsynth_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(zsynth_dsp PRIVATE /W4 /fp:fast)
else()
    target_compile_options(zsynth_dsp PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

if(ZSYNTH_BUILD_PLUGIN)
    include(cmake/ZSynthPlugin.cmake OPTIONAL RESULT_VARIABLE ZSYNTH_PLUGIN_CMAKE)
endif()

if(ZSYNTH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
