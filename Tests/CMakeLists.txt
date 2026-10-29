add_executable(ZSynthDspTests DspTests.cpp)
target_link_libraries(ZSynthDspTests PRIVATE zsynth_dsp)
target_include_directories(ZSynthDspTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME dsp COMMAND ZSynthDspTests)

if(TARGET ZSynth)
    include(${CMAKE_CURRENT_SOURCE_DIR}/StateTests.cmake OPTIONAL)
endif()

add_executable(ZSynthBench Bench.cpp)
target_link_libraries(ZSynthBench PRIVATE zsynth_dsp)
target_include_directories(ZSynthBench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
