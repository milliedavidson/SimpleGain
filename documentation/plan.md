# The Plan

The phased build plan for SimpleGain, a JUCE 8 gain plugin. Concept explanations and
troubleshooting live in the sibling files below, cross-linked from wherever they came up.

**SimpleGain docs** · [Index](README.md)  
**The plugin:** **Plan** · [Hosting & threads](plugin-hosting-and-threads.md) · [Parameters & automation](parameters-and-automation.md) · [Identity & macOS](plugin-identity-and-macos.md)  
**C++ foundations:** [Memory model](cpp-memory-model.md) · [Build pipeline](cpp-build-pipeline.md)  
**Workflow:** [Dev tooling](dev-workflow-and-tooling.md) · [Testing](testing.md) · [CI & GitHub Actions](ci-and-github-actions.md)

---

## In short

**Where this is up to:** Phases 0–3 are built and verified. The plugin builds as Standalone, AU
and VST3, passes `auval`, and has a working host-automatable gain parameter. Next up is Phase 4
— per-sample smoothing to kill the zipper noise.

- The build is **CMake + JUCE 8 as a submodule** — no Projucer.
- **Standalone is a dev tool**, not a product; AU and VST3 are the real outputs.
- Each phase ends in something runnable, so progress is always audible or visible.
- Nothing is marked done here unless it has actually been built and run.

**Settled plugin identity** — permanent once shared, so these don't change:

```
COMPANY_NAME             "Maxximum Displacement"
BUNDLE_ID                "com.maxximumdisplacement.simplegain"
PLUGIN_MANUFACTURER_CODE Mxdp
PLUGIN_CODE              Sgai
```

> One expected side effect of setting the bundle ID: macOS treats this as a brand-new app, so
> the standalone asks for microphone permission again on next launch. That's not a regression,
> it's TCC doing its job — permissions are granted per bundle ID.

---

## Plan: Build a Gain Plugin from Scratch with JUCE 8 + CMake + CLion

**Verified environment:**

| Thing | Status |
| --- | --- |
| CLion | `/Applications/CLion.app` ✓ |
| CMake | 3.31.3 (JUCE 8 needs ≥ 3.22) ✓ |
| Compiler | Apple clang 17.0.0 ✓ |
| macOS / SDK | macOS 26.6.2, CLT SDK 15.5 ✓ |
| `auval` | `/usr/bin/auval` ✓ |
| AU frameworks | AudioUnit, AudioToolbox, CoreAudioKit, Accelerate all in CLT SDK ✓ |
| Architecture | Intel x86_64 (Core i9-9880H) |
| Disk free | 60 GB ✓ |

**Known risk:** there is no full `Xcode.app` — only Command Line Tools. The AU build is
*expected* to work (all required frameworks confirmed present above), but if the AU target
fails to link, the fix is to install Xcode from the App Store and run
`sudo xcode-select -s /Applications/Xcode.app`. Standalone and VST3 are unaffected either way.
We build Standalone first precisely so this can't block early progress.

## Target layout

plaintext```
/Users/millie/Documents/Programming/REPOS/Audio/SimpleGain/
├── CMakeLists.txt            # the build script — this replaces your .csproj/.sln
├── .gitignore
├── libs/
│   └── JUCE/                 # git submodule, JUCE 8 (not committed, just referenced)
└── Source/
    ├── PluginProcessor.h     # the audio engine — declaration
    ├── PluginProcessor.cpp   # the audio engine — implementation
    ├── PluginEditor.h        # the GUI — declaration
    └── PluginEditor.cpp      # the GUI — implementation

Two classes, four files. The `.h`/`.cpp` split has no C# equivalent and is the first thing
we'll unpack.

## Phase 0 — Orientation (no code)

Before touching the keyboard, cover the mental model:

- **What a plugin actually is**: not an app — a *library* the DAW loads and calls. The DAW owns
  the audio hardware and the clock; your plugin is a callback it invokes. Closest C# analogy:
  writing a class that implements an interface a framework calls into, except the framework
  calls you from a hard-real-time thread.
  → *deep dive: [§1 App vs library](plugin-hosting-and-threads.md#1-whats-the-difference-between-an-app-and-a-library), [§2 Why the DAW owns main()](plugin-hosting-and-threads.md#2-why-does-the-daw-own-main-isnt-a-plugin-just-icing-on-someone-elses-cake)*

- **The two-thread model**: the *audio thread* (`processBlock`, runs every few ms, must never
  block) and the *message thread* (GUI, user input). Every design decision in the plugin comes
  from keeping these two apart safely.
  → *deep dive: [§3 Threads and blocking](plugin-hosting-and-threads.md#3-what-is-a-thread-and-what-does-must-never-block-mean), [§4 Keeping them apart](plugin-hosting-and-threads.md#4-keeping-the-two-threads-apart--wouldnt-you-just-do-that-anyway)*

- **Sample / sample rate / block / channel**: audio is `float` arrays. 48000 samples per second
  per channel; the host hands you them in blocks of typically 64–512.

- **JUCE is planar, not interleaved**: one separate `float*` array per channel, not `LRLRLR`.
  → *deep dive: [§6 Planar vs interleaved](plugin-hosting-and-threads.md#6-planar-vs-interleaved)*

- **Why gain is a multiply**, and why loudness is measured in **decibels** (logarithmic —
  human hearing is ratio-based, so a linear fader feels wrong).

**C++ vs C# concepts introduced:** compiled-and-linked vs JIT/IL; no runtime, no GC;
what a "native library" means.

## Phase 1 — Project scaffold and the build system

**Actions:**
1. `mkdir SimpleGain`, `git init` inside it.
2. `git submodule add https://github.com/juce-framework/JUCE.git libs/JUCE` then
   `git -C libs/JUCE checkout <latest 8.x tag>` (shallow where possible — this is the slow step,
   a few minutes).
3. Write `.gitignore` (`build/`, `.idea/`, `.DS_Store`).
4. Write `CMakeLists.txt`.
5. Open the folder in CLion, let it configure, confirm the targets appear.

**The CMakeLists will contain:**
- `cmake_minimum_required(VERSION 3.22)` / `project(SimpleGain VERSION 0.1.0)`
- `set(CMAKE_CXX_STANDARD 17)`
- `add_subdirectory(libs/JUCE)`
- `juce_add_plugin(SimpleGain ...)` with:
  - `COMPANY_NAME "Maxximum Displacement"`, `BUNDLE_ID com.maxximumdisplacement.simplegain`
  - `PLUGIN_MANUFACTURER_CODE Mxdp` and `PLUGIN_CODE Sgai` — 4-character codes, and the
    manufacturer code **must** contain at least one uppercase letter (an Apple AU requirement
    that silently breaks the build if violated)
  - `FORMATS AU VST3 Standalone`
  - `IS_SYNTH FALSE`, `NEEDS_MIDI_INPUT FALSE`, `COPY_PLUGIN_AFTER_BUILD TRUE`
- `target_sources` listing the four Source files
- `target_compile_definitions`: `JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`,
  `JUCE_VST3_CAN_REPLACE_VST2=0`
- `target_link_libraries` against `juce::juce_audio_utils` plus the
  `juce_recommended_config_flags` / `warning_flags` / `lto_flags` interface targets

**Deliberate choice:** we will **not** use `juce_generate_juce_header` / `JuceHeader.h`.
We include module headers directly (`#include <juce_audio_processors/juce_audio_processors.h>`).
This is current JUCE best practice, compiles faster, and makes dependencies explicit — closer
in spirit to C#'s explicit `using` directives than a catch-all header.

**C++ vs C# concepts introduced:** CMake as the `.csproj`/`.sln` equivalent but *generating* a
build rather than being one; there is no NuGet — dependencies are source you compile yourself;
the compile → link → binary pipeline; what "linking a library" means.
→ *background: [§4 Compile/link](cpp-build-pipeline.md#4-the-compile--link--binary-pipeline), [§2 Preprocessor](cpp-build-pipeline.md#2-what-is-the-preprocessor), [§1 Headers](cpp-build-pipeline.md#1-the-juce-headers--is-it-like-using-directives-in-c)*

### Phase 1 - Deep dive

There's no NuGet in C++. A dependency is source code you compile yourself. That's why libs/JUCE is 110 MB of actual C++ that will get compiled into your plugin.

CMakeLists.txt plays the role of your .csproj and .sln, with one structural difference worth internalising:

CMake is a generator. It doesn't build anything. It reads CMakeLists.txt and writes out real build files (Makefiles, in our case), which then do the building. So there are three stages where C# has one:

target_compile_definitions — these become #defines. The C++ preprocessor is a text-substitution pass that runs before compilation; JUCE_WEB_BROWSER=0 doesn't disable a feature at runtime, it means the browser code is never compiled at all. Nearest C# thing is #if DEBUG, but used far more aggressively.

target_link_libraries — linking is the step that stitches separately-compiled pieces into one binary. C# has no equivalent because the CLR resolves assembly references at runtime; C++ resolves them at build time, into a single file.

One thing I caught while checking the docs rather than trusting memory: PLUGIN_CODE must contain exactly one uppercase letter, and GarageBand additionally wants the first letter uppercase and the rest lowercase. Violate it and the plugin builds fine but GarageBand silently ignores it — a genuinely nasty half-day bug. Hence Sgai, not my originally planned Sgn1.

"Blocking" is the one I'd most encourage you to read properly, because it's the opposite of what most people assume: it isn't your thread being interrupted, it's your code voluntarily going to sleep waiting for something. Nobody stopped you — you stopped yourself. And you can't bound how long it lasts, which is why a mutex.lock() that usually takes nanoseconds can occasionally blow a 10.7 ms budget wide open.

"Would you just keep the threads apart as standard?" — you'd want to, but the trap is that the natural C#-brained way looks fine and works in testing. A plain float shared between the slider and processBlock is a formal data race: the compiler may cache it in a register and your slider silently stops working in Release builds only. [§4 in plugin-hosting-and-threads.md](plugin-hosting-and-threads.md#4-keeping-the-two-threads-apart--wouldnt-you-just-do-that-anyway) has a table of the accidental ones — adding a debug log line to processBlock is the classic, because the log line causes the glitch you're debugging.

## Phase 2 — The processor skeleton (silent pass-through)

Write `PluginProcessor.h`/`.cpp` with a class deriving from `juce::AudioProcessor`, plus a
minimal `PluginEditor`. No DSP yet — audio passes through untouched. Then **build and run the
Standalone target** to prove the toolchain end to end.

**Members we implement and what each is for:**
- `prepareToPlay(sampleRate, samplesPerBlock)` — the host's "here are the conditions, allocate
  now" call. This is where allocation is *allowed*.
- `releaseResources()` — teardown.
- `processBlock(AudioBuffer<float>&, MidiBuffer&)` — the real-time callback. The heart of it.
- `isBusesLayoutSupported()` — declare we accept mono→mono and stereo→stereo.
- `createEditor()` / `hasEditor()` — GUI factory.
- `getStateInformation()` / `setStateInformation()` — save/load (stubbed now, filled in Phase 5).
- The boilerplate: `getName`, `acceptsMidi`, `getTailLengthSeconds`, program methods.
- `createPluginFilter()` — the C-style entry point the host calls to make an instance.

**C++ vs C# concepts introduced** (this is the densest teaching phase):
- **Header vs source**: `.h` = declaration (roughly "the shape"), `.cpp` = implementation.
  C# has no equivalent because the compiler reads whole assemblies; C++ compiles one file at a
  time and needs to be told what exists.

- **`#include` is literal text pasting**, not `using`. Hence `#pragma once` / include guards.

- **`class ... : public juce::AudioProcessor`** vs C# `: AudioProcessor` — and what `public`
  inheritance means (C++ has private inheritance too; C# doesn't).

- **`override`** — same keyword, same idea. **`= 0`** = pure virtual ≈ C# `abstract`.

- **Value semantics**: `juce::AudioBuffer<float> buf;` creates a real object *on the stack*, not
  a null reference. This is the single biggest mental shift from C#.

- **References (`&`) vs pointers (`*`) vs C# `ref`** — and why `processBlock` takes
  `AudioBuffer<float>&`.

- **`const` and `const&`** for parameters — no real C# equivalent; it's a compiler-enforced
  promise.

- **RAII and destructors** — deterministic cleanup, the reason C++ doesn't need a GC.
  Closest C# analogy: `IDisposable`/`using`, but automatic and impossible to forget.

- **Member initializer lists** (`: memberA (x), memberB (y)`) vs C# field initialisers.

- **`auto`** ≈ `var`.

- **`JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR`** — what a macro is, and why C++ needs to
  explicitly forbid copying.

**Verification:** build succeeds; Standalone app launches, shows an empty window, and passes
audio through from input to output.

### Phase 2 - Deep dive

This is the first real C++ shock coming from C#.

In C#, a class lives in one .cs file — declaration and implementation together, and the compiler reads whole assemblies so it always knows what exists. C++ compiles one .cpp at a time, in complete isolation. It knows nothing about any other file.

So we split:

Contains	                                                Analogy
.h header	What exists — class shape, method signatures	The public contract
.cpp source	How it works — the actual bodies	            The secret

When PluginEditor.cpp wants to call a method on the processor, it #includes PluginProcessor.h to be told that method exists.

And critically: #include is literally copy-paste. The preprocessor pastes the entire text of that file in at that line. It is not using. That's why every header starts with #pragma once — without it, including a header twice would declare the class twice and fail to compile.

The concepts embedded in the code

I've commented all four files densely, so they're readable as teaching material. The ones worth flagging now:

Value semantics — the biggest mental shift. In C#, AudioBuffer buf; declares a null reference. In C++, juce::AudioBuffer<float> buf; constructs a real object on the stack, right there. Objects are values by default, not references.

References (&) vs pointers (*). processBlock takes AudioBuffer<float>& — a reference is an alias for the caller's object, so we modify the host's audio in place. Passing it by value would copy the entire block of audio every call. And a reference can never be null, which is stronger than a C# reference.

const appears twice in one signature and means two different things:

```
bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
//                           ^^^^^ won't modify the argument
//                                                      ^^^^^ won't modify this object
```

C# has nothing equivalent — it's a compiler-enforced promise, not a convention.

RAII and destructors — ~SimpleGainAudioProcessor() runs automatically and deterministically the moment the object dies. This is why C++ doesn't need a GC. The nearest C# concept is IDisposable + using, except automatic and impossible to forget.

Member initializer lists — the bit between : and {:

```
SimpleGainAudioProcessorEditor::SimpleGainAudioProcessorEditor (SimpleGainAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p)
```

createPluginFilter() at the bottom of the .cpp is a free function — it lives outside any class. C# has no such thing; everything must live in a type. This is the closest this project has to a Main(): it's what each format wrapper calls to create your processor.

Right now it's a pass-through: audio in, identical audio out. That's the point — it proves the whole chain works before any DSP exists to confuse a "doesn't work" with a "doesn't build".

### Phase 2 - Configuration

COMPANY_NAME is the one customers actually see — it's the "Manufacturer" column in every DAW's plugin browser. The 4-character codes (Mxdp, Sgai) are mostly invisible, but they're the plugin's permanent identity — change them before you ever share a build, because changing them later makes DAWs treat it as a different plugin and existing projects lose their settings.

CLion generates run configurations automatically once CMake finishes loading. Your list was empty because it hadn't configured yet. Do Tools → CMake → Reset Cache and Reload Project, wait ~90 seconds for [Finished], and the dropdown will populate itself with SimpleGain_Standalone and friends.

That Audio/MIDI Settings window on the standalone app — JUCE gave you that free. Neither of us wrote it. It's part of the standalone wrapper, which is a tiny host JUCE wraps around your plugin so it can talk to CoreAudio. Note the buffer size it shows — 512 samples (10.7 ms) — that's exactly the deadline from the threads diagram.

small correction: .ptx is a Pro Tools session file; the plugin format is AAX. And it's effectively closed: the SDK needs an Avid NDA, and even then Pro Tools won't load an AAX plugin unless it's signed with a paid PACE key. JUCE supports it (FORMATS AAX is literally one word), so it's a one-line change if you ever get access. The obstacle is commercial, not technical.  

### Phase 2 - Bugs encountered

No audio input detected and also, the standalone app crashed. This is because the app had no privacy usage descriptions in its Info.plist. macOS gates the mic, Bluetooth, camera etc. behind TCC (Transparency, Consent and Control). If your app touches one of those without a usage-description string explaining why, macOS doesn't deny access — it kills the process. Your crash report said so literally:

Termination Reason: Namespace TCC — attempted to access privacy-sensitive data without a usage description

Four lines in CMakeLists.txt fixed both. Rebuilt and verified the keys are now in the plist. Relaunch and you should get a permission prompt.

## Phase 3 — Parameters (make it actually do something)

We'll use JUCE's auto-generated editor as a throwaway GUI, so you can hear the gain working before writing any interface code.

Add a `juce::AudioProcessorValueTreeState` (APVTS) — the standard JUCE mechanism that makes a
value simultaneously host-automatable, savable, and bindable to a GUI control.

**What we add:**
- A static `createParameterLayout()` returning one `juce::AudioParameterFloat`:
  - ID `"gain"`, name `"Gain"`, range **−60 dB to +30 dB**, step 0.1, default **0 dB**
  - a `NormalisableRange` with a skew factor so the useful range around 0 dB gets more
    fader travel (matching how hearing works)
  - a string-formatting lambda so the host displays `"-6.0 dB"` not `"-6.000000"`
  - createParameterLayout() — a static factory. It has to be static because we call it in the initializer list to build apvts, at which point the object is still half-constructed.
- The APVTS member, constructed in the initializer list
- A cached `std::atomic<float>* gainParameter` — grabbed once in the constructor so
  `processBlock` never does a string lookup
- Apply the gain in `processBlock` (naively for now — we'll fix the artefacts in Phase 4)
- Use `juce::GenericAudioProcessorEditor` as a temporary GUI — JUCE auto-generates a slider
  from the parameter, so we can **hear it working before writing any GUI code**

**C++ vs C# concepts introduced:**
- **Templates vs generics**: `AudioBuffer<float>` looks like `List<float>` but is expanded at
  compile time and is duck-typed — a genuinely different mechanism with different consequences.
- **`std::unique_ptr`** — sole ownership; what `std::make_unique` does; why we rarely write `new`.
- **`std::atomic<float>`** — lock-free cross-thread reads, and *why* the audio thread cannot
  take a lock (priority inversion → dropout). Relates to C# `volatile`/`Interlocked`, but the
  stakes are higher.
- **Lambdas** — `[](float v) { ... }` vs C# `v => ...`, and what the capture list `[ ]` is for
  (C# closures capture implicitly; C++ makes you state it, because lifetimes are manual).
- **`static` member functions** ≈ C# `static`.

**Verification:** run Standalone, move the generated slider, hear the volume change.

JUCE auto-generated it from the parameter list, no GUI code written. Drag it and the volume actually changes. That's the first time this plugin has done anything.

Then try this deliberately: drag the fader fast while audio plays. You should hear a gritty, stepping "zipper" sound. That's a real flaw I left in on purpose — see below.

### Phase 3 - Deep Dive

What the auval tool (Apple's validator) just did is worth noting, because it's the same gate Logic applies before it will load anything:

- Rendered audio at 11 kHz through 192 kHz, and at block sizes from 64 to 4096 frames — deliberately including awkward ones like 137 frames to catch code that assumes a nice round block size

- Verified mono (1-channel) and stereo work

- Checked the plugin correctly fails when handed an illegal buffer size

- Checked parameter, connection and MIDI semantics

So the pass-through processor is already correct across every configuration a host might throw at it. That's not nothing — the isBusesLayoutSupported logic and the "clear unused output channels" loop are exactly what those tests probe.

auval — already on your Mac at /usr/bin/auval, ships with macOS. auval -a lists every AU installed.

---

## Phase 4 — Doing the DSP properly

The Phase 3 version works but is subtly wrong. Fix it, and explain each problem:

1. **`juce::ScopedNoDenormals`** at the top of `processBlock` — denormal floats are
   near-zero values that cause massive CPU stalls on x86. A stack-guard object; pure RAII.
2. **Clear unused output channels** — the host may hand you more outputs than inputs, and
   uninitialised buffers contain garbage (loud garbage).
3. **dB → linear conversion** via `juce::Decibels::decibelsToGain` — the actual maths
   (`10^(dB/20)`) and why it's not a straight multiply.
4. **`juce::SmoothedValue<float, ValueSmoothingTypes::Linear>`** — the important one. Changing
   gain instantly between blocks causes a discontinuity in the waveform, heard as a click
   ("zipper noise"). Smoothing ramps the value per-sample over ~20 ms.
   - `reset(sampleRate, 0.02)` in `prepareToPlay`
   - `setTargetValue()` once per block, `getNextValue()` once per sample
5. **Loop ordering**: samples outer, channels inner — so the smoother advances once per *sample*
   and every channel gets the same gain value. Getting this backwards is a classic bug.
6. Then **optimise** the loop to use `buffer.getWritePointer(channel)` and raw pointer
   arithmetic — a natural, motivated introduction to pointers.

**C++ vs C# concepts introduced:** raw pointers and pointer arithmetic; stack objects with
side-effecting destructors (RAII again, now that it's obviously useful); floating-point
representation; why `float` not `double` in audio.

**Verification:** sweep the fader fast while audio plays — no clicks or zipper noise.

---

## Phase 5 — State save and restore

Implement `getStateInformation` / `setStateInformation` so the plugin remembers its settings
when a DAW project is saved and reopened.

- `apvts.copyState()` → `juce::ValueTree` → XML → `copyXmlToBinary`
- The reverse on load, with a guard that the XML tag name matches
- Explain `juce::ValueTree` as JUCE's universal tree-of-properties type

**C++ vs C# concepts introduced:** serialisation without reflection — C# can inspect types at
runtime, C++ genuinely cannot, so state must be described explicitly. `juce::MemoryBlock` as a
raw byte buffer.

**Verification:** in the Standalone, set a gain, quit, relaunch, confirm the value persisted.

---

## Phase 6 — A real GUI

Replace `GenericAudioProcessorEditor` with a hand-built editor.

- A `juce::Slider` configured as a rotary knob with a text box
- A `juce::Label` for the title
- **`juce::AudioProcessorValueTreeState::SliderAttachment`** — the two-way binding that keeps
  slider and parameter in sync in both directions, including host automation moving the knob.
  Closest C# analogy: WPF data binding, and worth drawing that comparison explicitly.
- `paint(juce::Graphics&)` for the background
- `resized()` for layout, using `juce::Rectangle::removeFromTop` etc. — JUCE's layout idiom
- `setSize()`, and making the window resizable with sensible constraints

**C++ vs C# concepts introduced:** member declaration order matters (destruction is reverse of
construction — the attachment must be declared *after* the slider or it dangles); why the
editor holds a reference to the processor rather than owning it; the message thread again.

**Verification:** knob turns, audio responds, value box is editable, window resizes cleanly.

---

## Phase 7 — Build all formats, validate, and load in a DAW

1. Build the AU and VST3 targets. `COPY_PLUGIN_AFTER_BUILD` installs them to
   `~/Library/Audio/Plug-Ins/Components/` and `.../VST3/`.
2. **Validate the AU**: `auval -v aufx Sgai Mxdp` — Apple's official validator. Logic will
   refuse to load a plugin that fails this, so it's the real gate.
3. Optionally fetch **pluginval** (Tracktion's cross-format validator) and run it at strictness
   level 5+ — this is what the industry actually uses in CI.
4. Load in Logic/GarageBand (AU) and in Reaper/Ableton/Bitwig/Studio One (VST3). Test:
   audio passes, fader works, **host automation** writes and plays back, project save/reload
   preserves state.

**If the AU build fails** (the Xcode-vs-CLT risk noted above): install Xcode, run
`sudo xcode-select -s /Applications/Xcode.app`, reconfigure CMake. This step needs Millie's
password so she runs it, not me.

---

## Phase 8 — Polish (optional, scope-dependent)

Only if she wants to keep going after a working plugin:
- A **bypass** parameter wired to the host's bypass button
- A simple **output level meter** (introduces the correct pattern for getting data *out* of the
  audio thread without locks)
- **Universal binary** (`CMAKE_OSX_ARCHITECTURES "x86_64;arm64"`) so it runs on Apple Silicon
  too — relevant since this is an Intel Mac and most users aren't
- A `Release` build and notes on what code signing would involve for distribution

---

## Working method

- **One phase per exchange.** I write the code, then walk through it, then we build and verify
  before moving on. Nothing advances on an unverified build.
- **Explanations inline and thorough**, per her choice — each new C++ construct gets
  "what it is / the C# equivalent / why C++ differs", at the point we first hit it.
- **Every phase ends in something runnable**, so progress is always audible or visible.

## Licence note

JUCE 8 is free under its Personal licence for individuals below a revenue threshold, or under
GPLv3. Fine for learning and for most indie release — worth a sentence when we clone it, not
a blocker.

## Verification summary

| Phase | How we know it worked |
|---|---|
| 1 | CLion configures; `SimpleGain_Standalone`, `_AU`, `_VST3` targets appear |
| 2 | Standalone launches; audio passes through unmodified |
| 3 | Generated slider audibly changes volume |
| 4 | Fast fader sweeps produce no clicks |
| 5 | Gain value survives quit and relaunch |
| 6 | Custom knob works; window resizes |
| 7 | `auval -v aufx Sgai Mxdp` passes; plugin loads and automates in a real DAW |)

---
