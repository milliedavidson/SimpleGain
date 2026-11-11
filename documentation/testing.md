# Testing

How to feed audio through the plugin and hear what it's doing — standalone and in a DAW —
and how the automated tests check the numbers behind it.

**SimpleGain docs** · [Index](README.md)  
**The plugin:** [Plan](plan.md) · [Hosting & threads](plugin-hosting-and-threads.md) · [Parameters & automation](parameters-and-automation.md) · [Smoothing & the audio loop](smoothing-and-the-audio-loop.md) · [State: save and restore](state-save-and-restore.md) · [Identity & macOS](plugin-identity-and-macos.md) · [The editor (GUI)](editor-gui.md)  
**C++ foundations:** [Memory model](cpp-memory-model.md) · [Build pipeline](cpp-build-pipeline.md) · [CMake walkthrough](cmake-walkthrough.md)  
**Workflow:** [Dev tooling](dev-workflow-and-tooling.md) · **Testing** · [CI & GitHub Actions](ci-and-github-actions.md)


## In short

- **Automated tests run the real processor with no DAW and no sound card**:
  `cmake --build build --target check`. They check the *numbers* — does the gain step or glide,
  does state survive, does corrupt data crash it — not how anything sounds.
- **Listening still matters.** Tests can prove the gain curve is smooth; only ears can say it
  sounds right.
- **You wouldn't ship a standalone gain app** — it isn't a product. The standalone build is a
  *development tool* for fast build-and-hear iteration.
- To test with real audio, route system sound through **BlackHole** (a virtual audio device)
  into the plugin's input.
- For hearing **zipper noise**, use a DAW with a sustained looping signal. Speech is the worst
  possible test signal for it.

---

## 1. How do you actually test a gain plugin standalone?

### First, the honest answer: you wouldn't ship this

**Nobody uses a standalone gain app.** It isn't a product, and the use case doesn't really
exist — gain is something you apply *to a track, inside a mix*, which means inside a DAW.

The Standalone build isn't a deliverable, it's a **development tool**. Its value is the
iteration loop:

| | Standalone | DAW |
|---|---|---|
| Build → hear it | ~5 seconds | rebuild, rescan plugins, reopen session |
| Stale copy held in memory | no | yes — DAWs cache plugins and need restarting |
| Crashes take down | just your app | your whole DAW session |
| Debugger attach | trivial | awkward |

So it exists to answer "does this build and run at all" fast, which is exactly what it did in
Phase 2. It is *not* the right tool for evaluating how something sounds.

### Feeding it real audio instead: BlackHole

[BlackHole](https://github.com/ExistentialAudio/BlackHole) is a free virtual audio device — it
looks like a soundcard to macOS, but instead of going to speakers, whatever is sent to it comes
straight back as an *input*. That lets you pipe Spotify, YouTube or anything else into the
plugin.

1. **System Settings → Sound → Output → `BlackHole 16ch`**
   (all system audio now goes into BlackHole instead of your speakers — you'll hear nothing yet,
   that's expected)
2. **In SimpleGain → Options → Audio/MIDI Settings:**
   - Input: `BlackHole 16ch`
   - Output: `MacBook Pro Speakers` (or your headphones)
3. **Play music.** It now flows Spotify → BlackHole → SimpleGain → speakers, and the gain fader
   is controlling real audio with no mic and no feedback.
4. **Set the system output back** to your speakers when you're done, or you'll wonder why
   nothing makes sound later.

> ⚠️ Don't set the plugin's *output* to BlackHole as well — that routes its output back into its
> own input and builds a digital feedback loop, which is louder and nastier than the acoustic
> kind.

### For Phase 4, prefer a DAW

To hear **zipper noise** you want a *sustained, steady* signal — a held synth pad, a sine wave,
or any looping clip. Mic input is one of the worst possible test signals for it, because speech
is constantly changing anyway and masks the artefact you're listening for.

Drop SimpleGain on a track in Logic or Reaper, loop eight bars of something sustained and drag
the fader fast. That's the test.

---

## 2. Automated tests

### What they are

`tests/` holds a console app, `SimpleGain_Tests`, built from the tests plus the plugin's own
`Source/` files. It uses JUCE's built-in `UnitTest` framework, so there's no extra dependency,
and it's registered with CTest. It drives the real `SimpleGainAudioProcessor` directly — no
DAW, no sound card, no window.

The core trick, in `tests/TestSupport.h`: **feed a constant `1.0` through the processor.** Every
output sample then *is* the gain that was applied to it, which turns "does this click?" into
plain arithmetic on a list of numbers.

### Running them

```bash
cmake --build build --target check                  # build the tests, then run them via CTest
build/SimpleGain_Tests_artefacts/Debug/SimpleGain_Tests                          # run directly, full output
build/SimpleGain_Tests_artefacts/Debug/SimpleGain_Tests "State save and restore" # just one group
```

The exit code is 0 only if every check passed, and naming a group that doesn't exist fails
rather than silently passing. CLion also lists `SimpleGain_Tests` as a target you can run.

The first build compiles JUCE a second time, which takes a few minutes. That's why the target is
excluded from the normal build: `cmake --build build` never touches it unless you ask. CI runs
it on every push.

### What's covered

34 tests in five groups, about 1,460 individual checks:

| File | Group | What it protects |
|---|---|---|
| `GainSmoothingTests.cpp` | Gain smoothing | The glide: no big steps, about 20 ms at any sample rate or block size, both channels identical, no fade-in at the start. See [Smoothing & the audio loop](smoothing-and-the-audio-loop.md). |
| `ParameterTests.cpp` | Parameters | The permanent things: the `"gain"` id and its version hint, the −60…+30 dB range, the default, the display text, the accepted channel layouts. |
| `StateTests.cpp` | State save and restore | The round trip, applying from the first sample, loading during playback, wrong or unexpected documents, the saved format. See [State: save and restore](state-save-and-restore.md). |
| `CorruptStateTests.cpp` | Corrupt state data | Empty, random, truncated and corrupted data fed to the loader: it must never crash. Its own group so it can be run alone, for example under a memory checker. |
| `EditorTests.cpp` | Editor | The GUI, with no screen: the knob follows the parameter *and* the parameter follows the knob, layout at every window size, it really draws, opening and closing 50 times. See [The editor (GUI) §10](editor-gui.md#10-testing-a-gui-with-no-screen). |

**If a test in `ParameterTests` or the format tests in `StateTests` fails, a permanent thing has
changed** — an id, a range, the saved format. Stop and think before "fixing" the test: it's
there to stop exactly that.

### How they were written

**Test-first.** The Phase 5 tests were written before the implementation and run against the
empty stubs: 60 checks passed and 10 failed, every failure in the state tests, with messages like
*"gain −12.3 dB: expected −12.3, actual 0"*. Then the two functions were written and the tests
went green. Writing them first is what proves a test can fail.

**Deterministic.** The corrupt-data tests use fixed random seeds, so every run, and CI, corrupts
the same bytes the same way. A failure is reproducible, not a one-in-a-hundred flake.

**They found things.** The hostile-input tests exposed one wrong assumption of mine (what happens
to a parameter missing from the saved state), one real looseness in the plugin (a document whose
tag differed only by case was accepted in a Release build), and three families of JUCE's own debug
assertions. The story is in
[State: save and restore §6](state-save-and-restore.md#6-loading-never-trust-the-bytes).

### How I know they can fail

A test that can't fail is worthless, and a test that merely passes proves little. So I built
deliberately broken copies of the plugin and ran these same tests against each one:

| Broken on purpose | What I changed | Result | Caught by |
|---|---|---|---|
| per-block gain (the Phase 3 bug) | apply one gain per block again, no smoothing | **caught** | Gain smoothing ×3, State save and restore ×1 |
| loops the wrong way round | channels outer, samples inner, one shared smoother | **caught** | Gain smoothing ×4, State save and restore ×1 |
| smoother not started at the fader | drop setCurrentAndTargetValue in prepareToPlay | **caught** | Gain smoothing ×2, State save and restore ×1 |
| 5 ms glide instead of 20 ms | gainRampSeconds = 0.005 | **caught** | Gain smoothing ×2, State save and restore ×1 |
| steady gain forced to unity | steady-state branch applies 1.0 instead of the gain | **caught** | Gain smoothing ×4, State save and restore ×2 |
| range narrowed to +12 dB | max gain back to +12 | **caught** | Gain smoothing ×1, Parameters ×3, State save and restore ×3 |
| skew added to the range | NormalisableRange with a skew factor | **caught** | Parameters ×1 |
| default gain changed to +3 dB | default 0 -> 3 | **caught** | Gain smoothing ×1, Parameters ×1, State save and restore ×3 |
| parameter id renamed | "gain" -> "level" everywhere | **caught** | the test app crashed |
| parameter version hint bumped | ParameterID version 1 -> 2 | **caught** | Parameters ×1 |
| display text loses its unit | "-6.0 dB" -> "-6.0" | **caught** | Parameters ×1 |
| any channel layout accepted | isBusesLayoutSupported returns true for mismatched in/out | **caught** | Parameters ×1 |
| state functions are empty again | back to the Phase 4 stubs | **caught** | State save and restore ×7, Corrupt state data ×1 |
| no check that the document is ours | tag guard removed | **caught** | State save and restore ×2 |
| tag compared case-insensitively | hasTagName instead of an exact compare | **caught** | State save and restore ×1 |
| unreadable data not checked for | dereference without the null check | **caught** | the test app crashed (after 2 failing) |
| saves without flushing (state.createXml) | skip copyState(), read the tree directly | **caught** | State save and restore ×4 |
| attachment declared before the slider | swap two member lines in the editor | **caught** | the test app crashed |
| no attachment | knob and parameter not connected | **caught** | Editor ×6 |
| generic editor again | createEditor returns JUCE's GenericAudioProcessorEditor | **caught** | Editor ×1 |
| no resize limits | setResizeLimits removed | **caught** | Editor ×1 |
| no fixed aspect ratio | setFixedAspectRatio removed | **caught** | Editor ×1 |
| knob bigger than the window | knob bounds expanded | **caught** | Editor ×2 |
| knob far too small | knob bounds shrunk | **caught** | Editor ×1 |
| flat background | no gradient | **caught** | Editor ×1 |
| knob added but hidden | addChildComponent instead of addAndMakeVisible | **caught** | Editor ×1 |
| knob loses its ID | no setComponentID | **caught** | Editor ×8 |
| denormal guard removed | no ScopedNoDenormals | survived | — |
| unused output channels not cleared | drop the clear() loop | survived | — |

**27 of 29 were caught.** Alongside them I ran the unmodified plugin through the same tests: it
passes every one, which is the control. Without a control, a failure could just mean the harness is
broken. (Mine briefly was: an early run reported the *unmodified* plugin failing, because the build
was reusing a stale binary from the previous variant. The harness now stops if its control fails.)

The ten editor rows were added in Phase 6, and the first round found a real gap: with the knob
added to the window but *hidden*, the "it draws" test still passed, because the background gradient
alone makes the middle of the window differ from a corner. The test now compares the picture with
the knob shown against the same window with it hidden, and that mutant is caught. (One of my first
mutants didn't even compile, so it proved nothing; I fixed the edit and ran it again.)

This was a one-off check, with the harness kept outside the repo: it copies the plugin source,
applies one deliberate break, rebuilds just those files, and runs the real tests against the result.

The survivors are real gaps, and worth knowing about:

- **The denormal guard** can be removed and nothing notices. Its only effect is CPU time when
  denormal numbers arrive, which a test on output values can't see.
- **Clearing unused output channels** can be removed too. With the layouts the plugin accepts,
  inputs always match outputs, so that loop never has anything to clear: it's defensive code the
  tests can't reach.

### Adding a test

1. Add a class that derives from `juce::UnitTest`. Call `beginTest ("what is true")` for each
   behaviour, and `expect...` for each check.
2. Create one instance at namespace scope. Constructing it registers it with JUCE's runner, so
   there's no list to maintain:

   ```cpp
   struct MyTests final : public juce::UnitTest
   {
       MyTests() : juce::UnitTest ("My group", "SimpleGain") {}
       void runTest() override { beginTest ("..."); expect (true); }
   };

   MyTests myTests;
   ```

3. If it's a new file, add it to `target_sources(SimpleGain_Tests ...)` in `CMakeLists.txt`.
4. Write it **before** the code it tests, and watch it fail.

Floats need care: the build's strict warning flags include `-Wfloat-equal`, so compare with
`expectWithinAbsoluteError`, not `==`.

### What they can't tell you

They test the GUI's *behaviour* and layout, not whether it looks or feels good (there's a snapshot mode for looking: see [The editor (GUI) §10](editor-gui.md#10-testing-a-gui-with-no-screen)). They don't test how a particular DAW behaves, changes of sample rate in the middle of
playback, or what anything sounds like. `auval` and a listen in a real DAW still matter, and the
[checklist in §1](#1-how-do-you-actually-test-a-gain-plugin-standalone) is still the way to hear
it.

