# CLAUDE.md — working notes for agents

## What this repo is

**SimpleGain** — a volume/gain audio plugin (Standalone / AU / VST3) built from scratch with
JUCE 8 and CMake. See [README.md](README.md) for the full overview; this file is for how to
*work* in the repo, not what it *is*.

## Ground rules

### Work on `develop`, never `main`

Both `main` and `develop` exist. All work — edits, commits, pushes — happens on `develop`.
Millie merges to `main` herself, by hand, once she's tested a change there. Never commit
directly to `main`, never push to `main`, never merge `develop` into `main`.

If a session somehow starts on `main`, switch to `develop` before making any change rather than
committing to `main` and asking after.

### Never commit or push without being asked

Millie reviews every change by hand before it's committed. Write and edit freely, then stop and
say what changed. Wait for an explicit go-ahead **each time** — approval for one commit doesn't
carry over to the next.

### Comments stay short; depth goes in `documentation/`

This repo is a deliberate C++/audio-DSP learning project (see README's "An overview"). Millie
is an experienced C# developer new to C++, so the code is explained — but there's a split:

- **Code comments: 1–3 lines.** What it does, plus the single non-obvious *why*. If a comment
  is growing past ~3 lines, that's the signal it belongs in a doc instead.
- **Point at the doc** when there's more to say, on its own line, in exactly this form (it's
  machine-checkable, so don't vary it):
  ```cpp
  // Cached so processBlock never does a string lookup, and atomic because the
  // GUI thread writes it while the audio thread reads it.
  // -> documentation/parameters-and-automation.md section 4
  ```
- **The docs carry the full reasoning** — C# comparisons, diagrams, the history of why a
  decision was made.

The reason for the split: at one point `PluginProcessor.cpp` was 55% comments, and the code
became hard to read *because* of the explanations. Prose and code have different jobs.

### `processBlock` is real-time — keep it that way

`processBlock` runs roughly 100 times a second (94 at a 512-sample block) against a hard
deadline of a few milliseconds, and missing it is an audible click. So in `processBlock`, and in anything it calls, **never**:

- allocate: `new`, growing a `std::vector`, building a `juce::String`
- take a lock: `std::mutex`, `juce::CriticalSection`
- do file or network I/O, or log (`DBG()`, `std::cout`) — a log line can cause the very glitch
  it was added to find
- look a parameter up by string

Instead: allocate in `prepareToPlay`; read parameters through the cached `std::atomic<float>*`;
ramp values with `juce::SmoothedValue`; hand data to the GUI through atomics or a lock-free
structure, never a mutex the audio thread might wait on. If you aren't sure something is
real-time safe, assume it isn't and ask.

Why, in full: [Hosting & threads](documentation/plugin-hosting-and-threads.md) (the sections on
threads and blocking).

### Some things are permanent once released

Hosts and users store these, so changing them later silently breaks saved projects, recorded
automation or plugin recognition. **Don't change any of them — even to tidy — without asking
first:**

- **Identity:** `COMPANY_NAME`, `BUNDLE_ID`, `PLUGIN_MANUFACTURER_CODE`, `PLUGIN_CODE`
- **Parameter IDs:** the `"gain"` string and its version hint in `juce::ParameterID`
- **Parameter ranges:** hosts store automation as a *normalised* 0–1 value, so widening or
  narrowing a range shifts every recorded automation lane. (The plugin's own saved state holds
  the real dB value, so a fader position survives — automation doesn't.)
- **`JUCE_VST3_CAN_REPLACE_VST2`:** JUCE warns that changing it makes the VST3 incompatible with
  previous versions.

Nothing has been released yet, so these *can* still change — which is exactly the moment to ask,
not afterwards. Detail: [Identity & macOS](documentation/plugin-identity-and-macos.md),
[Parameters & automation](documentation/parameters-and-automation.md) (the section on choosing
a range), [CMake walkthrough](documentation/cmake-walkthrough.md).

### Keep the docs in step with the code

When a change alters something a doc states — a range, an identity value, a phase's status, a
section or file name — update the doc, the **Status** list in `README.md`, and the "In short" in
`documentation/plan.md` **in the same change**. These have all gone stale before.

Before calling docs work done, verify, rather than assuming:

- every link and `#anchor` resolves. GitHub's anchors lowercase the heading, strip punctuation
  and turn each space into its own hyphen — so an em dash between two spaced words gives a
  **double** hyphen. Generate anchors from the real heading; never hand-guess them
- every image referenced exists, and none in `documentation/images/` is orphaned
- every `documentation/<file>.md section N` pointer in `Source/` and `CMakeLists.txt` names a
  section that really exists. Inserting or renumbering a doc section breaks these silently
- `documentation/README.md` lists every doc, and each doc has an "In short" block and a nav line
  linking the others

## Structure

```
CMakeLists.txt              the build script
libs/JUCE/                  JUCE 8, as a git submodule — not committed source
Source/                     the plugin: PluginProcessor.{h,cpp}, PluginEditor.{h,cpp}
.github/workflows/build.yml CI — builds all 3 formats, validates the AU with auval
documentation/              the reasoning behind the code — see documentation/README.md
documentation/images/banner.svg   README hero image
```

## Verifying changes

- Rebuild and actually run before calling something done:
  `cmake --build build --target SimpleGain_Standalone -j 8`, then open the `.app`.
- After **any** change to `Source/` or `CMakeLists.txt` — not just identity — rebuild all three
  formats and run `auval -v aufx Sgai Mxdp`. CI runs it too, but check locally first.
- Judging glitches or CPU use? Build **Release**: Debug is `-O0` with no LTO, and
  `COPY_PLUGIN_AFTER_BUILD` installs whichever you built last. See the Debug-vs-Release section
  of the [CMake walkthrough](documentation/cmake-walkthrough.md).
- Check facts against the installed source rather than memory — `libs/JUCE/docs/` and the
  module headers under `libs/JUCE/modules/` are the ground truth for JUCE's API in this exact
  version. Memory has been stale here before: JUCE 8 split `juce_audio_processors` into a
  `_headless` module, and GitHub deprecated the `macos-14` runner.

## Related

All documentation lives in **[`documentation/`](documentation/README.md)** in this repo, and is
meant to be a **one-stop shop** for someone learning audio plugin development — not just the
plugin-specific material (hosting, parameters, `auval`, CI) but the general C++ foundations too
(stack vs heap, RAII, the compile/link pipeline).

That's deliberate: someone arriving from a managed language needs the language fundamentals as
much as the JUCE specifics, and sending them to another repo to find half of it defeats the
point. When adding docs, keep them here rather than splitting by "is this general knowledge?"
