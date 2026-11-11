# =============================================================================
#  SimpleGain — a volume plugin built with JUCE 8
#
#  Comments are short on purpose: the line-by-line tour is in
#  documentation/cmake-walkthrough.md. CMake is a generator — it reads this
#  file and writes the real build files (Makefiles / Ninja / Xcode).
# =============================================================================

cmake_minimum_required(VERSION 3.22)                # same minimum JUCE asks for
project(SimpleGain VERSION 0.1.0 LANGUAGES C CXX)   # JUCE bundles C libraries too

# JUCE needs C++17. REQUIRED = fail rather than quietly fall back to older.
# -> documentation/cmake-walkthrough.md section 2
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is a git submodule: source compiled alongside ours, no package manager.
# -> documentation/cmake-walkthrough.md section 3
add_subdirectory(libs/JUCE)

# One call creates a shared-code static library plus a target per format.
# -> documentation/cmake-walkthrough.md section 4
juce_add_plugin(SimpleGain

    # Identity — permanent once anyone has a saved project. The 4-character
    # codes have strict rules that fail silently.
    # -> documentation/plugin-identity-and-macos.md section 1
    COMPANY_NAME                "Maxximum Displacement"
    BUNDLE_ID                   "com.maxximumdisplacement.simplegain"
    PLUGIN_MANUFACTURER_CODE    Mxdp
    PLUGIN_CODE                 Sgai

    # Plugin kind. These FALSE values are JUCE's defaults, written out so the
    # file says what the plugin is. They also pick the AU type code (aufx).
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             "Fx" "Tools"

    # Standalone only: macOS kills an app that touches the mic or Bluetooth
    # without an Info.plist usage string.
    # -> documentation/plugin-identity-and-macos.md section 4
    MICROPHONE_PERMISSION_ENABLED   TRUE
    MICROPHONE_PERMISSION_TEXT      "SimpleGain needs audio input so you can hear the plugin processing live sound."
    BLUETOOTH_PERMISSION_ENABLED    TRUE
    BLUETOOTH_PERMISSION_TEXT       "SimpleGain uses Bluetooth to discover wireless MIDI devices."

    FORMATS                     Standalone AU VST3   # AU is macOS-only
    COPY_PLUGIN_AFTER_BUILD     TRUE                 # installs into ~/Library/Audio/Plug-Ins
    PRODUCT_NAME                "SimpleGain")

# Source files are listed explicitly — an unlisted .cpp is silently not built.
target_sources(SimpleGain
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

# Build-time switches that remove JUCE features from the binary entirely.
# JUCE_USE_CURL is Linux-only (a no-op on macOS); JUCE_VST3_CAN_REPLACE_VST2
# must not change after release.
# -> documentation/cmake-walkthrough.md section 6
target_compile_definitions(SimpleGain
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

# PUBLIC = also applied to the format targets that link this library, which is
# how JUCE's docs say to use the three flag targets. Debug builds are -O0.
# -> documentation/cmake-walkthrough.md sections 7 and 8
target_link_libraries(SimpleGain
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline tests: a console app that drives the real processor with no DAW and no
# sound card. Excluded from the default build (it compiles JUCE a second time).
#   cmake --build build --target check
# -> documentation/testing.md section 2, documentation/cmake-walkthrough.md section 10
enable_testing()

juce_add_console_app(SimpleGain_Tests PRODUCT_NAME "SimpleGain_Tests")
set_target_properties(SimpleGain_Tests PROPERTIES EXCLUDE_FROM_ALL TRUE)

target_sources(SimpleGain_Tests
    PRIVATE
        tests/main.cpp
        tests/GainSmoothingTests.cpp
        tests/ParameterTests.cpp
        tests/StateTests.cpp
        tests/CorruptStateTests.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_include_directories(SimpleGain_Tests PRIVATE Source tests)

target_compile_definitions(SimpleGain_Tests
    PRIVATE
        "JucePlugin_Name=\"SimpleGain\""   # normally supplied by juce_add_plugin
        JUCE_LOG_ASSERTIONS=1                # lets the tests filter JUCE's own assertion messages
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

target_link_libraries(SimpleGain_Tests
    PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

add_test(NAME SimpleGain_Tests COMMAND SimpleGain_Tests)

# `cmake --build build --target check` = build the tests, then run them.
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIG> --output-on-failure
    DEPENDS SimpleGain_Tests
    USES_TERMINAL)
