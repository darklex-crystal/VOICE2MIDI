cmake_minimum_required(VERSION 3.22)
project(Vocal2MIDITune VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout (optional)")
option(VOCAL2MIDI_COPY_PLUGIN_AFTER_BUILD "Copy plug-ins to the system plug-in folders after building" OFF)

if(JUCE_PATH)
    if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
        message(FATAL_ERROR "JUCE_PATH does not contain a JUCE CMakeLists.txt: ${JUCE_PATH}")
    endif()
    add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(Vocal2MIDITune
    COMPANY_NAME "Vocal2MIDI"
    BUNDLE_ID "com.vocal2midi.Vocal2MIDITune"
    PLUGIN_MANUFACTURER_CODE V2MI
    PLUGIN_CODE V2MT
    FORMATS VST3 Standalone
    PRODUCT_NAME "Vocal2MIDITune"
    PLUGIN_NAME "Vocal2MIDITune"
    DESCRIPTION "Monophonic voice to MIDI with scale lock, glide and pitch bend"
    IS_SYNTH FALSE
    IS_MIDI_EFFECT FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    VST3_CATEGORIES "Fx" "Analyzer"
    COPY_PLUGIN_AFTER_BUILD ${VOCAL2MIDI_COPY_PLUGIN_AFTER_BUILD}
)

juce_generate_juce_header(Vocal2MIDITune)

target_sources(Vocal2MIDITune PRIVATE
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/PitchDetector.h
    Source/PitchDetector.cpp
    Source/ScaleQuantizer.h
    Source/ScaleQuantizer.cpp
    Source/MidiNoteTracker.h
    Source/MidiNoteTracker.cpp
    Source/MidiUtils.h
    Source/ParameterIDs.h
    Source/LookAndFeel.h
)

target_include_directories(Vocal2MIDITune PRIVATE Source)
target_compile_features(Vocal2MIDITune PRIVATE cxx_std_17)
target_compile_definitions(Vocal2MIDITune PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
)

target_link_libraries(Vocal2MIDITune PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

include(CTest)
if(BUILD_TESTING)
    add_executable(Vocal2MIDITuneDspTests
        tests/DspTests.cpp
        Source/PitchDetector.cpp
        Source/ScaleQuantizer.cpp
    )
    target_include_directories(Vocal2MIDITuneDspTests PRIVATE Source)
    target_compile_features(Vocal2MIDITuneDspTests PRIVATE cxx_std_17)
    add_test(NAME Vocal2MIDITuneDspTests COMMAND Vocal2MIDITuneDspTests)

    add_executable(Vocal2MIDITuneMidiTrackerTests
        tests/MidiTrackerTests.cpp
        Source/MidiNoteTracker.cpp
        Source/ScaleQuantizer.cpp
    )
    target_include_directories(Vocal2MIDITuneMidiTrackerTests PRIVATE Source)
    target_compile_features(Vocal2MIDITuneMidiTrackerTests PRIVATE cxx_std_17)
    target_link_libraries(Vocal2MIDITuneMidiTrackerTests PRIVATE
        juce::juce_audio_basics
        juce::juce_core
    )
    add_test(NAME Vocal2MIDITuneMidiTrackerTests COMMAND Vocal2MIDITuneMidiTrackerTests)
endif()
