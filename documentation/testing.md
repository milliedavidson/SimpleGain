# Testing

How to actually feed audio through the plugin and hear what it's doing — standalone and
in a DAW.

**SimpleGain docs** · [Index](README.md)  
**The plugin:** [Plan](plan.md) · [Hosting & threads](plugin-hosting-and-threads.md) · [Parameters & automation](parameters-and-automation.md) · [Identity & macOS](plugin-identity-and-macos.md)  
**C++ foundations:** [Memory model](cpp-memory-model.md) · [Build pipeline](cpp-build-pipeline.md)  
**Workflow:** [Dev tooling](dev-workflow-and-tooling.md) · **Testing** · [CI & GitHub Actions](ci-and-github-actions.md)


## In short

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
