cmake_minimum_required(VERSION 3.22)

project(AcidLead VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ACIDLEAD_COPY_AFTER_BUILD "Copiar o plugin para a pasta de plugins do sistema após o build" ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(AcidLead
    COMPANY_NAME "PsyTools"
    PLUGIN_MANUFACTURER_CODE Psyt
    PLUGIN_CODE Acl1
    FORMATS VST3 Standalone
    PRODUCT_NAME "Acid Lead"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${ACIDLEAD_COPY_AFTER_BUILD})

juce_generate_juce_header(AcidLead)

target_sources(AcidLead PRIVATE
    Source/AcidVoice.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)

target_compile_definitions(AcidLead PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(AcidLead
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
