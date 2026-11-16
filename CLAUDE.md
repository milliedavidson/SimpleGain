# CLAUDE.md — working notes for agents

## What this repo is

**Simply Gain** — a volume/gain audio plugin (Standalone / AU / VST3) built from scratch with
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

### Writing style: British English, short sentences, aligned tables

These apply to every doc, to the README, and to code comments. `scripts/check-docs.py` enforces
all three, and CLion's own checks flag the same things in the editor.

- **British spelling:** colour, behaviour, initialise, serialisation, analyser, centre, artefact.
  Leave code identifiers and tool names alone (`juce::Colour` is already British, and
  AddressSanitizer keeps its name). Don't rename an API to fit.
- **Sentences of 35 words or fewer.** Split a long one at a colon, a dash or a "which", or turn a
  chain of semicolons into separate sentences. A list of items is better as bullets than as one
  sentence with five commas.
- **Tables are aligned**: every cell padded so the pipes line up, and the separator row's dashes
  filling the column, exactly as CLion's *Reformat Code* lays them out. Don't align by hand. Write
  the table roughly, then run `python3 scripts/check-docs.py --fix`.

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

These are locked by tests (`tests/ParameterTests.cpp` and the saved-format tests in
`tests/StateTests.cpp`). **If one of those fails, a permanent thing has changed — don't "fix" the
test to match; stop and ask.**

Nothing has been released yet, so these *can* still change — which is exactly the moment to ask,
not afterwards. Detail: [Identity & macOS](documentation/plugin-identity-and-macos.md),
[Parameters & automation](documentation/parameters-and-automation.md) (the section on choosing
a range), [CMake walkthrough](documentation/cmake-walkthrough.md).

### Keep the docs in step with the code

When a change alters something a doc states, update the doc **in the same change**. That covers a
range, an identity value, a phase's status, or a section or file name. Update the **Status** list in
`README.md` and the "In short" in `documentation/plan.md` too. These have all gone stale before.

Before calling docs work done, run the checker — it must exit 0:

```bash
python3 scripts/check-docs.py
```

It checks, among other things, that:

- every link and `#anchor` resolves, and every referenced image exists (and none is orphaned)
- every `documentation/<file>.md section N` pointer in `Source/` and `CMakeLists.txt` names a
  section that really exists
- `documentation/README.md` lists every doc, and every doc has an "In short" block and a nav line
  linking the others
- code fences are well-formed, and every `cmake` code block is valid CMake (an argument fragment on
  its own isn't, so wrap it in its command, because CLion flags those as errors)
- tables are aligned the way CLion formats them (`python3 scripts/check-docs.py --fix` re-aligns them)
- spelling is British, in prose and in code comments (see "Writing style" below)
- no sentence is longer than 35 words

Never hand-guess an anchor. GitHub turns each space into its own hyphen, so an em dash between two
spaced words gives a **double** hyphen. If you find a new way docs can break, add a check to the
script rather than a sentence here.

## Structure

```
CMakeLists.txt              the build script
libs/JUCE/                  JUCE 8, as a git submodule — not committed source
Source/                     the plugin: PluginProcessor.{h,cpp}, PluginEditor.{h,cpp}
tests/                      the offline tests — `cmake --build build --target check`
.github/workflows/build.yml CI — builds all 3 formats, validates the AU with auval
scripts/check-docs.py         verifies the docs hang together — see "Keep the docs in step"
documentation/              the reasoning behind the code — see documentation/README.md
documentation/images/banner.svg   README hero image
```

## Verifying changes

- **Run the tests after any change to `Source/`:** `cmake --build build --target check`. Every
  test must pass. A change in behaviour needs a test — write it *first*, watch it fail, then make
  it pass. The first build compiles JUCE a second time (a few minutes); after that it's quick.
  The run also fails if JUCE raises any debug assertion (the summary line says "0 JUCE
  assertions"), so don't silence one — it means a rule was broken. How the tests work, and how
  to add one: `documentation/testing.md`.
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

All documentation lives in **[`documentation/`](documentation/README.md)** in this repo. It's meant
to be a **one-stop shop** for someone learning audio plugin development. That means the
plugin-specific material (hosting, parameters, `auval`, CI) and the general C++ foundations too
(stack vs heap, RAII, the compile/link pipeline).

That's deliberate: someone arriving from a managed language needs the language fundamentals as
much as the JUCE specifics, and sending them to another repo to find half of it defeats the
point. When adding docs, keep them here rather than splitting by "is this general knowledge?"
