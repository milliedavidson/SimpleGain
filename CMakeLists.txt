# =============================================================================
#  SimpleGain — a volume plugin built with JUCE 8
# =============================================================================
#  This file is the build script. In C# terms it plays the role of your .csproj
#  AND your .sln, but with one important difference: CMake is a *generator*.
#  It doesn't build anything itself — it reads this file and writes out the
#  real build files (Makefiles / Ninja files / an Xcode project), which then do
#  the building. CLion runs CMake for you automatically.
# =============================================================================

# The minimum CMake version this script relies on. JUCE 8 requires 3.22+.
# You have 3.31.3, so we're fine.
cmake_minimum_required(VERSION 3.22)

# Declares the project name and version, and tells CMake we're using C and C++.
# (JUCE has a little C in it, hence both.)
project(SimpleGain VERSION 0.1.0 LANGUAGES C CXX)

# Use the C++17 standard, and require it rather than silently falling back.
# C# just has "the language version"; C++ has a series of standards (11/14/17/20/23)
# and you pick which one you're writing against. JUCE 8 needs at least 17.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
#  Bring in JUCE
# -----------------------------------------------------------------------------
#  There is no NuGet in C++. A dependency is *source code you compile yourself*.
#  add_subdirectory() runs libs/JUCE/CMakeLists.txt, which defines targets like
#  juce::juce_audio_utils that we can then link against below.
add_subdirectory(libs/JUCE)

# -----------------------------------------------------------------------------
#  Define the plugin
# -----------------------------------------------------------------------------
#  juce_add_plugin is a function JUCE provides. From this single call it creates
#  SEVERAL build targets — one per format (Standalone, VST3, AU) — plus a shared
#  "SimpleGain" target holding the code they all have in common.
juce_add_plugin(SimpleGain

    # --- Identity -------------------------------------------------------
    COMPANY_NAME                "Maxximum Displacement"
    BUNDLE_ID                   "com.maxximumdisplacement.simplegain"

    # Four-character codes that uniquely identify you and this plugin.
    # These are an ancient Apple convention and the rules are strict:
    #   MANUFACTURER: first letter uppercase, rest lowercase.
    #   PLUGIN_CODE:  EXACTLY one uppercase letter, first letter uppercase.
    # Break these and the plugin builds fine but GarageBand silently ignores it.
    PLUGIN_MANUFACTURER_CODE    Mxdp
    PLUGIN_CODE                 Sgai

    # --- What kind of plugin is this? -----------------------------------
    IS_SYNTH                    FALSE   # we process audio, we don't generate it
    NEEDS_MIDI_INPUT            FALSE   # no MIDI at all
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             "Fx" "Tools"

    # --- macOS privacy permissions --------------------------------------
    # macOS refuses to let an app touch the microphone or Bluetooth unless its
    # Info.plist explains WHY, in a string shown to the user in the permission
    # dialog. If the key is missing, macOS doesn't just deny access — it KILLS
    # the process (a "TCC violation"). That's what crashed the standalone.
    #
    # Only the Standalone needs these: in a DAW, the HOST owns the microphone
    # and has already asked for permission on its own behalf.
    MICROPHONE_PERMISSION_ENABLED   TRUE
    MICROPHONE_PERMISSION_TEXT      "SimpleGain needs audio input so you can hear the plugin processing live sound."
    BLUETOOTH_PERMISSION_ENABLED    TRUE
    BLUETOOTH_PERMISSION_TEXT       "SimpleGain uses Bluetooth to discover wireless MIDI devices."

    # --- Which formats to build -----------------------------------------
    #   Standalone = a normal .app you can run directly (our fast dev loop)
    #   AU         = Audio Unit, for Logic Pro / GarageBand (macOS only)
    #   VST3       = for Reaper / Ableton / Bitwig / Studio One
    FORMATS                     Standalone AU VST3

    # After each build, copy the AU/VST3 into ~/Library/Audio/Plug-Ins/
    # so your DAW picks them up with no manual steps.
    COPY_PLUGIN_AFTER_BUILD     TRUE

    # The filename of the built plugin, and the name the DAW shows.
    PRODUCT_NAME                "SimpleGain")

# -----------------------------------------------------------------------------
#  Our source files
# -----------------------------------------------------------------------------
#  PRIVATE means "these files are used to build SimpleGain, but anything that
#  links against SimpleGain does not inherit them". Roughly like `internal`.
target_sources(SimpleGain
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

# -----------------------------------------------------------------------------
#  Compile-time configuration
# -----------------------------------------------------------------------------
#  These become #define directives visible to every file. The C++ preprocessor
#  is a text-substitution step that runs *before* compilation — there is no
#  direct C# equivalent (the nearest thing is a #if DEBUG conditional).
#  JUCE uses these flags to compile whole features in or out entirely.
target_compile_definitions(SimpleGain
    PUBLIC
        JUCE_WEB_BROWSER=0          # we don't embed a browser -> don't compile one
        JUCE_USE_CURL=0             # we make no network requests
        JUCE_VST3_CAN_REPLACE_VST2=0)

# -----------------------------------------------------------------------------
#  Link against JUCE
# -----------------------------------------------------------------------------
#  "Linking" is the step where the compiler stitches separately-compiled pieces
#  into one binary. It has no C# analogue because the CLR resolves assembly
#  references at runtime; C++ resolves them at build time, into one file.
target_link_libraries(SimpleGain
    PRIVATE
        # juce_audio_utils is an umbrella module: it pulls in audio_processors,
        # audio_basics, gui_basics, dsp and friends.
        juce::juce_audio_utils
    PUBLIC
        # These three are not real libraries — they're bundles of compiler
        # settings JUCE recommends (optimisation flags, warning levels,
        # link-time optimisation). Attaching them here applies them to us.
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
