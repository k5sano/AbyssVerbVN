cmake_minimum_required(VERSION 3.22)
project(AbyssVerbVN VERSION 1.0.0 LANGUAGES C CXX)

# Use JUCE from system
set(JUCE_MODULES_DIR /Applications/JUCE/modules)
add_subdirectory(/Applications/JUCE JUCE)

# Create VST3 plugin
juce_add_plugin(AbyssVerbVN
    COMPANY_NAME "K5SANO"
    PLUGIN_MANUFACTURER_CODE K5sn
    PLUGIN_CODE Abyv
    FORMATS VST3 Standalone
    PRODUCT_NAME "AbyssVerbVN"
    NEEDS_WEB_BROWSER FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_DESCRIPTION "Violin-optimized reverb with piezo conditioning and envelope following"
)

target_include_directories(AbyssVerbVN
    PRIVATE
        Source
)

target_link_libraries(AbyssVerbVN
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_sources(AbyssVerbVN
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

juce_generate_juce_header(AbyssVerbVN)

target_compile_definitions(AbyssVerbVN
    PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_LOG_ASSERTIONS=1
)

# Universal Binary for macOS
if(APPLE)
    set_target_properties(AbyssVerbVN PROPERTIES
        OSX_ARCHITECTURES "arm64;x86_64"
    )
endif()
