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

### Explain, don't just implement

This repo is a deliberate C++/audio-DSP learning project (see README's "An overview"). Millie
is an experienced C# developer new to C++. Code changes should stay heavily commented in the
same style already in `Source/` — each non-obvious construct gets a short "what/why" note, with
a C# comparison where one clarifies things. Deeper conceptual write-ups belong in the companion
notes repo, not inline here — see Related, below.

## Structure

```
CMakeLists.txt              the build script
libs/JUCE/                  JUCE 8, as a git submodule — not committed source
Source/                     the plugin: PluginProcessor.{h,cpp}, PluginEditor.{h,cpp}
.github/workflows/build.yml CI — builds all 3 formats, validates the AU with auval
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

Deeper C++ and audio-programming explanations — the concepts behind why the code looks the way
it does — live in a separate personal notes repo, `docs/simple-gain/`, not duplicated here.
