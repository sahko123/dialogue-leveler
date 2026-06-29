cmake_minimum_required(VERSION 3.22)
project(DialogueLeveler VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static CRT linkage — eliminates any VCRT DLL dependency from the distributed VST3.
# Must be set before juce_add_plugin so all targets inherit it.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
endif()

# --------------------------------------------------------------------------
# Fetch JUCE — no manual SDK install required
# --------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        2cdfca8feb300fb424002ba2c2751569e5bacb64  # JUCE 8.0.14
    # GIT_SHALLOW must be FALSE when GIT_TAG is a raw SHA: GitHub rejects shallow
    # fetches of non-advertised objects. The cache step in CI absorbs the cost.
    # IMPORTANT: when bumping JUCE, update BOTH this SHA and the cache key
    # "juce-8.0.14-..." in .github/workflows/release.yml so they stay in sync.
    GIT_SHALLOW    FALSE
)
FetchContent_MakeAvailable(JUCE)

# --------------------------------------------------------------------------
# Plugin target — VST3 + Standalone only
# --------------------------------------------------------------------------
juce_add_plugin(DialogueLeveler
    VERSION                     ${PROJECT_VERSION}
    COMPANY_NAME                "sahko123"
    PLUGIN_MANUFACTURER_CODE    Sahk
    PLUGIN_CODE                 Dlvl
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "Dialogue Leveler"
    PLUGIN_NAME                 "Dialogue Leveler"
    DESCRIPTION                 "Automatic dialogue loudness leveler"
    VST3_CATEGORIES             "Fx|Dynamics"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
)

juce_generate_juce_header(DialogueLeveler)

# --------------------------------------------------------------------------
# Sources
# --------------------------------------------------------------------------
target_sources(DialogueLeveler PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

# --------------------------------------------------------------------------
# Compile definitions
# --------------------------------------------------------------------------
target_compile_definitions(DialogueLeveler
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# --------------------------------------------------------------------------
# Link JUCE modules
# juce_audio_utils pulls in: audio_basics, audio_devices, audio_formats,
# audio_processors, core, events, graphics, gui_basics, gui_extra
# --------------------------------------------------------------------------
target_link_libraries(DialogueLeveler
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
        # LTO only in Release — enabling it in Debug builds makes incremental compiles very slow
        $<$<CONFIG:Release,RelWithDebInfo>:juce::juce_recommended_lto_flags>
)
