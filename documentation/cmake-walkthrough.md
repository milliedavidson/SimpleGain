# CMake walkthrough

A line-by-line tour of `CMakeLists.txt` — what each block does, why it's there, and which parts
are permanent. The file itself keeps its comments short and points here.

**SimpleGain docs** · [Index](README.md)  
**The plugin:** [Plan](plan.md) · [Hosting & threads](plugin-hosting-and-threads.md) · [Parameters & automation](parameters-and-automation.md) · [Identity & macOS](plugin-identity-and-macos.md)  
**C++ foundations:** [Memory model](cpp-memory-model.md) · [Build pipeline](cpp-build-pipeline.md) · **CMake walkthrough**  
**Workflow:** [Dev tooling](dev-workflow-and-tooling.md) · [Testing](testing.md) · [CI & GitHub Actions](ci-and-github-actions.md)

---

## In short

- **CMake is a generator, not a build system.** It reads `CMakeLists.txt` and writes the real
  build files (Makefiles, Ninja files or an Xcode project), which then do the building.
- **It's your `.csproj` and `.sln` in one**, with two differences: you list source files
  explicitly, and there's no NuGet — JUCE is source that gets compiled alongside yours.
- **One `juce_add_plugin` call creates many targets**: a shared-code static library holding our
  code, plus one target per format (Standalone, AU, VST3).
- **The identity block is permanent once released.** So is `JUCE_VST3_CAN_REPLACE_VST2`.
- **Compile definitions are build-time switches** that remove JUCE features from the binary
  entirely. Some are platform-specific — `JUCE_USE_CURL` is Linux-only and does nothing here.
- **`PRIVATE` / `PUBLIC` control what flows on** to the format wrappers. JUCE's own docs say to
  link its three flag targets `PUBLIC`, so every wrapper gets the same flags.
- **Debug builds are unoptimised** (`-O0`, no LTO). Judge audio performance on a Release build.

---

## 1. CMake is a generator, not a build system

In C#, `dotnet build` reads your `.csproj` and builds. C++ has an extra stage in the middle:

```
CMakeLists.txt  →  [cmake]  →  Makefiles / build.ninja  →  [make / ninja]  →  binary
   you write it      generate       real build files           actually build      output
```

`cmake -B build` is the first arrow: it **configures** the project and writes the build files
into `build/`. `cmake --build build` runs the second. CLion does both for you, using its own
bundled Ninja.

The first configure on this project took about 90 seconds. That isn't CMake being slow — it
builds JUCE's small helper tool, `juceaide`, which JUCE needs in order to generate things like
`Info.plist` files. Later configures are quick.

**Two differences from a `.csproj` that matter in practice:**

| | C# (`.csproj`) | C++ (CMake) |
|---|---|---|
| Source files | SDK-style projects glob `**/*.cs` automatically | Listed **explicitly** — an unlisted `.cpp` is silently ignored |
| Dependencies | NuGet restores a compiled package | **Source you compile yourself**, here as a git submodule |

→ *More on why unlisted files vanish: [Build pipeline §5](cpp-build-pipeline.md#5-what-is-a-o-file-and-is-a-cpp-file-just-a-file).*

---

## 2. The top of the file: version, project, language standard

```cmake
cmake_minimum_required(VERSION 3.22)
project(SimpleGain VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

**`cmake_minimum_required(VERSION 3.22)`** — this file needs at least that CMake. It matches
JUCE's own `CMakeLists.txt`, which asks for the same, so there's no point asking for less.

**`project(... VERSION 0.1.0 ...)`** — names the project and gives it a version. JUCE uses that
version for the plugin; it's the `0.1.0` that showed up in the standalone app's crash report.

**`LANGUAGES C CXX`** — C++ *and* C. JUCE bundles 178 C source files (libpng, Ogg Vorbis and
others), so C has to be enabled or those wouldn't compile.

**The C++ standard.** C++ is revised every few years — C++11, 14, 17, 20, 23 — and each adds
language features. You choose which one you're writing against; the closest C# idea is
`<LangVersion>`. JUCE's own CMake support files request C++17, so that's our floor.

`CMAKE_CXX_STANDARD_REQUIRED ON` means **fail if the compiler can't do C++17**, rather than
quietly falling back to an older standard and producing confusing errors later.

---

## 3. Bringing in JUCE

```cmake
add_subdirectory(libs/JUCE)
```

This runs `libs/JUCE/CMakeLists.txt`, which defines the `juce_add_plugin` function used below
and the `juce::...` targets we link against.

JUCE is a **git submodule**, so if you clone without `--recurse-submodules` the folder is empty
and this is the line that fails — with a CMake error that doesn't say "you forgot the
submodule". The fix is in the *Get started* section of the main README.

---

## 4. `juce_add_plugin` — one call, many targets

`juce_add_plugin(SimpleGain ...)` is the heart of the file. According to JUCE's CMake docs it
adds a **shared-code static library** named `SimpleGain`, plus an extra target for each format
you ask for. You can see all of them:

| Target | What it is |
|---|---|
| `SimpleGain` | the shared code — builds `libSimpleGain_SharedCode.a` |
| `SimpleGain_Standalone` | a normal `.app` |
| `SimpleGain_AU` | the Audio Unit, for Logic and GarageBand |
| `SimpleGain_VST3` | the VST3, for Reaper, Ableton, Bitwig and the rest |

Our own code is compiled **once**, into the shared library, and each format target links it
against a different thin JUCE wrapper — see
[Build pipeline §9](cpp-build-pipeline.md#9-no-portable-binary--does-that-mean-separate-c-code-per-format-and-per-os).

The arguments come in groups.

### Identity

```cmake
COMPANY_NAME                "Maxximum Displacement"
BUNDLE_ID                   "com.maxximumdisplacement.simplegain"
PLUGIN_MANUFACTURER_CODE    Mxdp
PLUGIN_CODE                 Sgai
```

**These are permanent once anyone has a saved project.** The 4-character codes have strict
rules that fail *silently* (GarageBand just ignores a plugin whose code is wrong). All of it is
covered in [Identity & macOS §1](plugin-identity-and-macos.md#1-company_name-bundle_id-and-plugin_manufacturer_code--what-will-customers-see).

### What kind of plugin is this?

```cmake
IS_SYNTH                    FALSE
NEEDS_MIDI_INPUT            FALSE
NEEDS_MIDI_OUTPUT           FALSE
IS_MIDI_EFFECT              FALSE
EDITOR_WANTS_KEYBOARD_FOCUS FALSE
VST3_CATEGORIES             "Fx" "Tools"
```

Every one of the `FALSE` lines is **JUCE's default** (checked against its docs), so they change
nothing. They're written out so the file states plainly what kind of plugin this is.

They do have consequences, though. JUCE picks the Audio Unit **type code** from them: a plugin
that isn't a synth and takes no MIDI becomes an `aufx` (a plain effect) — the `aufx` in
`auval -v aufx Sgai Mxdp`. Add MIDI input and JUCE switches it to a "music effect" instead.

`VST3_CATEGORIES` decides which folder hosts file the plugin under. `Fx` is already the default
for a non-synth; `Tools` is the one we added, since a gain utility is more tool than effect.

`EDITOR_WANTS_KEYBOARD_FOCUS` says whether the plugin window wants keystrokes itself or leaves
them to the host. Hosts generally prefer the latter.

### macOS privacy permissions

```cmake
MICROPHONE_PERMISSION_ENABLED   TRUE
MICROPHONE_PERMISSION_TEXT      "SimpleGain needs audio input so you can hear..."
BLUETOOTH_PERMISSION_ENABLED    TRUE
BLUETOOTH_PERMISSION_TEXT       "SimpleGain uses Bluetooth to discover wireless MIDI devices."
```

Without these, macOS **kills the process** the moment it touches the microphone or Bluetooth —
that's what crashed the standalone app. Only the Standalone needs them, because inside a DAW the
host owns the microphone. Full story: [Identity & macOS §4](plugin-identity-and-macos.md#4-the-microphone-doesnt-respond-and-the-app-crashed--same-bug--fixed).

### Formats, install, and names

```cmake
FORMATS                     Standalone AU VST3
COPY_PLUGIN_AFTER_BUILD     TRUE
PRODUCT_NAME                "SimpleGain"
```

**`FORMATS`** lists what to build. `AU` only exists on macOS — JUCE's docs say AU targets are
enabled only when building there — which is why CI has to run on a macOS runner.

**`COPY_PLUGIN_AFTER_BUILD`** installs the AU and VST3 into `~/Library/Audio/Plug-Ins/` after
every build, so your DAW finds them with no manual step. The catch is that **every build
overwrites whatever your DAW loads**, including Debug builds (see §8).

**`PRODUCT_NAME`** is the filename of the built plugin. The name the DAW displays is
`PLUGIN_NAME`, which defaults to `PRODUCT_NAME`.

---

## 5. `target_sources` — listing the files

```cmake
target_sources(SimpleGain
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
```

Only the `.cpp` files are listed. Headers don't need to be: a `.cpp` `#include`s them, so the
compiler finds them there.

**Add a new `.cpp` and forget this list, and it silently isn't built** — the classic first
stumble for anyone arriving from a globbing `.csproj`.

`PRIVATE` is explained in §7.

---

## 6. `target_compile_definitions` — switching JUCE features off

```cmake
target_compile_definitions(SimpleGain
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)
```

Each of these becomes a `#define` visible to every file. JUCE wraps whole features in
`#if JUCE_WEB_BROWSER ... #endif`, so setting one to `0` removes that code from the binary
entirely — nothing is disabled at runtime. The mechanism is the preprocessor:
[Build pipeline §2](cpp-build-pipeline.md#2-what-is-the-preprocessor).

I checked each one against JUCE's source, because they don't all do what you'd assume:

| Definition | JUCE's default | What `0` does here |
|---|---|---|
| `JUCE_WEB_BROWSER` | `1` | Drops the embedded-browser component, which we never use. |
| `JUCE_USE_CURL` | `1` | **Nothing on macOS.** JUCE documents this one as *Linux only*. It's harmless, and keeps the same file sensible if the project is ever built on Linux. |
| `JUCE_VST3_CAN_REPLACE_VST2` | `1` | The VST3 won't load or save VST2-compatible state. See below. |

### Why `JUCE_VST3_CAN_REPLACE_VST2=0` is a permanent choice

With it on, a VST3 can stand in for an older VST2 version of the same plugin, so hosts can swap
one for the other. We never released a VST2, so there's nothing to replace — and JUCE's own
source comment says exactly that: if you've released neither a VST2 nor a VST3, consider
releasing only a VST3 and disabling this.

But JUCE also warns that **changing it later** makes the VST3 *"incompatible with previous
versions."* Like the identity codes, it's a decision to make before release and then leave
alone.

`PUBLIC` means these definitions also reach the format wrappers, which include JUCE too — see
the next section.

---

## 7. `target_link_libraries` — PRIVATE, PUBLIC and the flag targets

```cmake
target_link_libraries(SimpleGain
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
```

### PRIVATE vs PUBLIC

Every setting in CMake can be marked with how far it spreads:

| Keyword | Used to build this target? | Passed on to anything that links against it? |
|---|---|---|
| `PRIVATE` | yes | **no** |
| `PUBLIC` | yes | **yes** |
| `INTERFACE` | no | yes |

The nearest C# idea is `PrivateAssets="all"` on a `PackageReference`: it says "I use this, but
don't let it flow to whoever references me." Plain `PUBLIC` is the default transitive flow.

Here, `SimpleGain` is the shared-code library and the three format targets link against it —
so `PUBLIC` is how a setting reaches the AU, VST3 and Standalone builds.

### `juce_audio_utils`

An **umbrella module**. Its dependencies, from its own header, are `juce_audio_processors`,
`juce_audio_formats` and `juce_audio_devices` — and those pull in the rest. The one we `#include`
is `juce_audio_processors`; `juce_audio_devices` is what lets the Standalone app talk to your
sound card.

One thing worth knowing for Phase 4: `juce::SmoothedValue` lives in `juce_audio_basics`, which
is already reached through that chain. **Smoothing needs no new library.**

### The three flag targets

`juce_recommended_config_flags`, `..._lto_flags` and `..._warning_flags` aren't libraries. They
carry nothing but compiler and linker flags. JUCE's docs recommend linking all three with
**`PUBLIC`** so that "all the plugin wrappers inherit the same compile/link flags" — which is
exactly what we do.

With AppleClang, from `JUCEHelperTargets.cmake`:

| Target | Debug | Release |
|---|---|---|
| `config_flags` | `-g -O0` (debug info, no optimisation) | `-O3` |
| `lto_flags` | nothing | `-flto` at compile **and** link |
| `warning_flags` | the same long warning list in both | |

**`-flto`** is link-time optimisation. Normally each `.cpp` is compiled in isolation (see
[Build pipeline §4](cpp-build-pipeline.md#4-the-compile--link--binary-pipeline)), so the
optimiser can't see across files. LTO postpones some optimisation until link time, when the
whole program is visible.

**The warning flags are warnings, not errors** — there's no `-Werror` in the list. They *are*
strict, though: `-Wall`, `-Wshadow-all`, `-Wconversion`, `-Wsign-conversion`, `-Wfloat-equal`,
`-Wunused-parameter` and many more. Two consequences you've already met or will:

- `-Wunused-parameter` is why the code calls `juce::ignoreUnused (...)` on parameters it doesn't
  use yet.
- `-Wconversion` and `-Wsign-conversion` will flag implicit `int`/`float`/`size_t` mixes — expect
  them once the sample loops arrive in Phase 4.

---

## 8. Debug vs Release — and why it matters for audio

The table above hides something practical: **a Debug build is unoptimised.** `-O0` means the
compiler does no optimisation at all, and LTO is off.

Our local builds use `-DCMAKE_BUILD_TYPE=Debug`, while CI uses `Release`. And because of
`COPY_PLUGIN_AFTER_BUILD`, **the plugin your DAW loads is whichever you built last** — so while
developing, you've probably been listening to a Debug build.

For a one-multiply gain plugin the difference is unlikely to be audible. But it's a habit worth
forming now, because once the DSP gets heavier a Debug plugin can crackle in a DAW purely
because it's slow, with nothing wrong in the code. When you're judging glitches or CPU use,
build Release:

```bash
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target SimpleGain_AU -j 8
```

With the Makefile generator, one build folder is one build type, hence the separate directory.

---

## 9. What isn't in the file yet

| Missing | Why | When |
|---|---|---|
| `CMAKE_OSX_ARCHITECTURES "x86_64;arm64"` | Builds currently match the machine's own architecture, so an Intel Mac builds Intel-only. | Phase 8 — universal binary |
| `CMAKE_OSX_DEPLOYMENT_TARGET` | Unset, so it follows the SDK you build with, which can make the plugin refuse to load on older macOS versions. | Decide before release |
| `juce_generate_juce_header` | Deliberately omitted — we `#include` module headers directly, which compiles faster and keeps dependencies explicit. | Never — it's a design choice |
