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
- **Point at the doc** when there's more to say, on its own line:
  ```cpp
  // Cached so processBlock never does a string lookup, and atomic because the
  // GUI thread writes it while the audio thread reads it.
  // → documentation/parameters-and-automation.md §3
  ```
- **The docs carry the full reasoning** — C# comparisons, diagrams, the history of why a
  decision was made.

The reason for the split: at one point `PluginProcessor.cpp` was 55% comments, and the code
became hard to read *because* of the explanations. Prose and code have different jobs.

## Structure

```
CMakeLists.txt              the build script
libs/JUCE/                  JUCE 8, as a git submodule — not committed source
Source/                     the plugin: PluginProcessor.{h,cpp}, PluginEditor.{h,cpp}
.github/workflows/build.yml CI — builds all 3 formats, validates the AU with auval
documentation/              the reasoning behind the code — see documentation/README.md
docs/banner.svg             README hero image
```

## Verifying changes

- Rebuild and actually run before calling something done:
  `cmake --build build --target SimpleGain_Standalone -j 8`, then open the `.app`.
- Any change to plugin identity (`COMPANY_NAME`, `PLUGIN_CODE`, `PLUGIN_MANUFACTURER_CODE`,
  `BUNDLE_ID`) needs re-validating: `auval -v aufx <code> <manufacturer>`.
- Check facts against the installed source rather than memory — `libs/JUCE/docs/` and the
  module headers under `libs/JUCE/modules/` are the ground truth for JUCE's API in this exact
  version, which can differ from older tutorials.

## Related

All documentation lives in **[`documentation/`](documentation/README.md)** in this repo, and is
meant to be a **one-stop shop** for someone learning audio plugin development — not just the
plugin-specific material (hosting, parameters, `auval`, CI) but the general C++ foundations too
(stack vs heap, RAII, the compile/link pipeline).

That's deliberate: someone arriving from a managed language needs the language fundamentals as
much as the JUCE specifics, and sending them to another repo to find half of it defeats the
point. When adding docs, keep them here rather than splitting by "is this general knowledge?"
