# Parameters & automation

How a knob the user turns becomes a number the audio thread can safely read — and how the host
learns what that number means.

**SimpleGain docs** · [Index](README.md)  
**The plugin:** [Plan](plan.md) · [Hosting & threads](plugin-hosting-and-threads.md) · **Parameters & automation** · [Identity & macOS](plugin-identity-and-macos.md)  
**C++ foundations:** [Memory model](cpp-memory-model.md) · [Build pipeline](cpp-build-pipeline.md)  
**Workflow:** [Dev tooling](dev-workflow-and-tooling.md) · [Testing](testing.md) · [CI & GitHub Actions](ci-and-github-actions.md)

---

## In short

Everything below in five bullets, for when you just need reminding:

- **`AudioProcessorValueTreeState` (APVTS)** is one object doing three jobs: exposing parameters
  to the host for automation, saving/loading them as XML, and binding them to GUI controls.
- **Parameters are stored in dB**, not as a raw multiplier, because dB is already a logarithmic
  scale and therefore already matches how hearing works.
- **No skew needed** on a dB parameter — skew exists to bend *linear* units into a perceptually
  even curve, and dB has already done that.
- **`std::atomic<float>*`, cached once** — the GUI thread writes the value, the audio thread
  reads it, and `processBlock` can't afford a string lookup or a data race.
- **A string-formatting lambda** turns the bare float into `"-6.0 dB"` so the host displays
  something meaningful instead of `0.833333`.
- **Range is −60 to +30 dB** — +12 wasn't enough headroom for real use, and +30 matches Logic's
  own Gain plugin.

---

## 1. What APVTS actually does

`juce::AudioProcessorValueTreeState` is a single member that replaces three separate pieces of
plumbing you'd otherwise write by hand:

| Job | What it means | Without APVTS you'd write |
|---|---|---|
| **Host automation** | Logic can record and play back fader moves | Manual `AudioProcessorParameter` subclasses and index bookkeeping |
| **State save/load** | Settings survive closing and reopening a project | Your own serialisation in `get/setStateInformation` |
| **GUI binding** | A slider and the parameter stay in sync, both ways | Listener callbacks in both directions, plus feedback-loop guards |

Constructed in the member initializer list:

```cpp
apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
```

- `*this` — the processor these parameters belong to
- `nullptr` — no `UndoManager` (we don't need undo)
- `"PARAMETERS"` — the XML tag name used when state is written out
- `createParameterLayout()` — the parameters themselves

`createParameterLayout()` has to be **`static`** because it's called *during* construction of
`apvts`, at which point the object is still half-built — calling a normal member function there
would be reading a partially-constructed `this`.

---

## 2. Skew — what it is and why we don't want it here

### The problem skew solves

A knob has a physical position (0% to 100% of its travel). A parameter has a value range. Skew
is **how you bend the relationship between the two**.

With no skew, the mapping is linear: knob at 50% → value at 50% of the range. That sounds
obviously correct and for many parameters it's obviously wrong.

**The classic example — a filter cutoff, 20 Hz to 20,000 Hz:**

```
Linear mapping:
  knob   0% ──────────────── 50% ──────────────── 100%
  value 20 Hz             10,010 Hz            20,000 Hz
```

Musically, almost everything interesting happens below about 2 kHz. With a linear mapping, that
entire useful range — all the bass, all the body, all the mids — is crammed into the **first
10%** of the knob. The remaining 90% sweeps through "hiss" at a rate of ~200 Hz per pixel. The
knob is effectively unusable.

Skew fixes that by compressing one end and expanding the other:

```
Skewed mapping:
  knob   0% ──────────────── 50% ──────────────── 100%
  value 20 Hz               ~630 Hz             20,000 Hz
```

Now the midpoint lands somewhere musically useful and the whole travel feels even to the ear.

### Why our gain parameter needs none

Because **dB is already a logarithmic scale.** That's the entire point of it.

A decibel is *defined* as `20 × log₁₀(ratio)`. So equal steps in dB already correspond to equal
*perceived* changes in loudness:

| dB change | Multiplier | Sounds like |
|---|---|---|
| −6 dB | ×0.50 | one step quieter |
| 0 dB | ×1.00 | unchanged |
| +6 dB | ×2.00 | one step louder |
| +12 dB | ×4.00 | two steps louder |
| +30 dB | ×31.6 | our maximum boost |

Going −6 → 0 and 0 → +6 feel like the same size of change, even though one adds 0.5 to the
multiplier and the other adds 1.0. The log has already been applied — by choosing dB as the
unit.

Mapping −60…+30 dB linearly across the fader therefore already feels even. **Adding skew would
apply the logarithm a second time**, bunching everything up at one end and re-creating exactly
the problem skew is meant to solve.

### The general rule

> **Skew compensates for a parameter whose units are linear but whose perception is
> logarithmic. If the unit is already logarithmic, skip it.**

| Parameter | Unit | Perception | Skew? |
|---|---|---|---|
| Filter cutoff | Hz — linear | logarithmic | **Yes** |
| Raw gain multiplier | 0.0–2.0 — linear | logarithmic | **Yes** |
| Gain in dB | dB — already log | logarithmic | **No** |
| Pitch in semitones | already log | logarithmic | **No** |
| Pan | −1 to +1 — linear | linear | **No** |
| Dry/wet mix | 0–100% — linear | roughly linear | **No** |

In JUCE, skew is set on the `NormalisableRange` — either directly via its skew argument, or
more readably with `range.setSkewForCentre (630.0f)`, which means "make this value land at the
middle of the knob." Ours simply omits it:

```cpp
juce::NormalisableRange<float> gainRange { -60.0f, 30.0f, 0.1f };
//                                          min    max   step — linear, deliberately
```

---

## 3. Choosing the range: why −60 to +30 dB

The first version used **−60 to +12 dB**, and +12 turned out to be nowhere near enough
headroom — you regularly need to push a quiet source much harder than that.

The current range is **−60 dB to +30 dB**, matching Logic's own Gain plugin. For reference:

| Plugin | Range |
|---|---|
| Logic Gain | ±30 dB |
| Ableton Utility | −∞ to +35 dB |
| Typical "trim" utility | ±24 dB |
| **SimpleGain** | **−60 to +30 dB** |

+30 dB is a multiplier of **31.6×**, which is a lot — enough to rescue a quietly-recorded take
or a low-output ribbon mic.

### A side benefit: unity lands nearer the middle

Where 0 dB sits on the fader is a function of the range:

| Range | Span | 0 dB sits at |
|---|---|---|
| −60 … +12 | 72 dB | **83%** of travel |
| −60 … +30 | 90 dB | **67%** of travel |

Unity gain is the most important position on a gain control and having it two-thirds along is
more comfortable than having it jammed near the top.

### ⚠️ This plugin will happily destroy your signal

A gain plugin does **not** protect you. At +30 dB, anything already near full scale clips hard,
and nothing here prevents that — no limiter, no soft clipper, no warning. That's normal for a
trim/gain utility and it's the user's job to watch their meters.

### One thing to get right *before* releasing

Changing a parameter's range after people have saved projects is a genuine
backward-compatibility problem, and the two halves of "saved state" behave differently:

| What | Stores | Effect of widening the range |
|---|---|---|
| **The plugin's own state** (`getStateInformation`) | the **real dB value** — verified in JUCE's `juce_AudioProcessorValueTreeState.cpp`, which does `tree.setProperty (key, unnormalisedValue.load(), um)` | **Safe.** `-6.0` reloads as `-6.0` |
| **Host automation lanes** (AU/VST3) | a **normalised 0–1 value** | **Shifts.** 0.5 meant −24 dB on the old range; it means −15 dB on the new one |

So a saved fader position survives, but any recorded automation would move. Changing it now,
before release and before anyone has automation recorded, costs nothing. Doing it after would
silently alter people's mixes — which is exactly what the `ParameterID` version hint
(`juce::ParameterID { "gain", 1 }`) exists to help hosts manage.

---

## 4. Why the value is a cached `std::atomic<float>*`

```cpp
std::atomic<float>* gainDbParameter = nullptr;   // in the header

gainDbParameter = apvts.getRawParameterValue ("gain");   // once, in the constructor
```

Two separate decisions here, each solving a different problem.

### Why cached

`getRawParameterValue("gain")` does a **string lookup**. `processBlock` runs ~94 times a second
with a hard deadline (see [Plugin hosting & threads](plugin-hosting-and-threads.md#3-what-is-a-thread-and-what-does-must-never-block-mean)), so the lookup happens **once**, in the constructor, where
slowness is free. After that it's a plain pointer dereference.

### Why atomic

This value is **written by the message thread** (the user dragging a slider) and **read by the
audio thread** (`processBlock`). Two threads, one variable.

With a plain `float` that's a **data race** — which in C++ is not "a bit risky", it's formally
*undefined behaviour*. The compiler is entitled to assume nothing else touches that variable,
so it may hoist the read out of the loop and cache it in a register permanently. The classic
symptom: **your slider works in Debug and silently stops working in Release.**

`std::atomic<float>` tells the compiler "hands off", and guarantees you read a whole value
rather than one caught half-written. On x86 an atomic float load compiles to the same
instruction as a normal one — the safety is effectively free.

> The nearest C# equivalents are `volatile` and `Interlocked`. The difference is consequence: in
> C# a torn read is a wrong number, here it's an audible click in someone's recording.

`.load()` is how you read it — deliberately explicit, so you can't do a non-atomic read by
accident.

---

## 5. The string-formatting lambda

```cpp
.withStringFromValueFunction ([] (float valueInDb, int)
{
    return juce::String (valueInDb, 1) + " dB";
})
```

### Why it's needed

The parameter's actual stored value is a bare `float`. The host has no idea what that number
*means* — dB? Hz? a percentage? Without this function, a DAW displays the raw or normalised
number, so the user sees `0.833333` or `-6.000000` in the automation lane.

With it, Logic shows **`-6.0 dB`**. The `auval` output confirms the host picked it up:

```
Values: Minimum = -60.0 dB, Default = 0.0 dB, Maximum = 12.0 dB
Flags: Values Have Strings, ...
```

That `Values Have Strings` flag is the host saying "this plugin supplies its own text" — the dB
suffixes appearing there at all is proof the lambda is working end to end.

### The unnamed `int`

JUCE calls this function with `(value, maximumStringLength)`. Some hosts have narrow displays
and ask for a shortened string. We ignore it, so the parameter is left **unnamed** — a C++ way
of saying "this argument exists and I'm deliberately not using it" without triggering an
unused-parameter warning.

### What `[]` is doing

That's the **capture list**, and it's the biggest lambda difference from C#.

C# closures capture variables implicitly — you just use an outer variable and the compiler and
GC sort it out. C++ makes you state what you're capturing, because **lifetimes are manual**:

| Capture | Meaning |
|---|---|
| `[]` | capture nothing |
| `[x]` | capture `x` **by copy** |
| `[&x]` | capture `x` **by reference** |
| `[=]` / `[&]` | capture everything by copy / by reference |

Empty `[]` is not laziness here, it's correct and important. This lambda is **stored inside the
parameter object**, which outlives the function that created it. Capturing a local variable by
reference would leave a dangling reference pointing at a dead stack frame — a
[use-after-free](cpp-memory-model.md#5-why-is-using-an-object-after-freeing-it-bad-if-its-free-isnt-it-available)
waiting to happen — a dangling reference into a dead stack frame. `[]` guarantees that can't occur.

---

## 6. What "Can Ramp" means in the auval output

```
Flags: Values Have Strings, High Resolution, Can Ramp, Readable, Writable
```

**`Can Ramp`** is an Audio Unit flag (`kAudioUnitParameterFlag_CanRamp`) meaning: *this
parameter can be changed smoothly over time, so the host may schedule a gradual slide from one
value to another rather than only discrete jumps.*

JUCE sets it automatically based on one condition — from
`juce_audio_plugin_client_AU_1.mm`:

```cpp
const bool isParameterDiscrete = param->isDiscrete();

if (! isParameterDiscrete)
    outParameterInfo.flags |= kAudioUnitParameterFlag_CanRamp;
```

So: **continuous parameters get it, discrete ones don't.** That makes sense — you can be
halfway between −6 dB and −5 dB, but you cannot be halfway between "Sine" and "Square" on a
waveform selector, so ramping there would be meaningless.

The other flags in that line:

| Flag | Meaning |
|---|---|
| `Values Have Strings` | the plugin supplies display text (our lambda, §5) |
| `High Resolution` | the value is a float, not a coarse stepped integer |
| `Can Ramp` | safe to change smoothly / automate continuously |
| `Readable` / `Writable` | the host may both query and set it |

### The irony worth noticing

The plugin advertises **Can Ramp** — but as of Phase 3, the implementation *doesn't* ramp. We
read the fader once per block and apply that single value to all 512 samples:

```cpp
const auto gainInDecibels = gainDbParameter->load();   // once per BLOCK
buffer.applyGain (juce::Decibels::decibelsToGain (gainInDecibels));
```

So the host is told "you can automate this smoothly," and we then apply it in chunky per-block
steps — roughly 94 updates a second instead of 48,000. Each step is a discontinuity in the
waveform, and ears hear discontinuities as clicks. Drag the fader fast and it sounds gritty:
**zipper noise**.

Phase 4 closes that gap with `juce::SmoothedValue`, which ramps per-sample and makes the
implementation actually honour the flag. See
[Plugin hosting & threads §8](plugin-hosting-and-threads.md#8-samples-vs-blocks) for why
per-block and per-sample differ by a factor of 512.
