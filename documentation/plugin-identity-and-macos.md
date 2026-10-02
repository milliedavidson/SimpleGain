# Plugin identity & macOS

Company name, bundle ID and manufacturer codes; `auval`; Pro Tools/AAX; and the TCC privacy
crash that killed the standalone app on first launch.

**SimpleGain docs** · [Index](README.md)  
**The plugin:** [Plan](plan.md) · [Hosting & threads](plugin-hosting-and-threads.md) · [Parameters & automation](parameters-and-automation.md) · **Identity & macOS**  
**C++ foundations:** [Memory model](cpp-memory-model.md) · [Build pipeline](cpp-build-pipeline.md)  
**Workflow:** [Dev tooling](dev-workflow-and-tooling.md) · [Testing](testing.md) · [CI & GitHub Actions](ci-and-github-actions.md)

---

## In short

- **`COMPANY_NAME` is the only one customers really see** — it's the "Manufacturer" column in
  every DAW's plugin browser.
- The **4-character codes are permanent identity**. `PLUGIN_CODE` must contain exactly one
  uppercase letter, first letter uppercase — break that and GarageBand silently ignores the
  plugin while everything still builds.
- **`auval` ships with macOS** (`/usr/bin/auval`) — it's the same validation Logic runs before
  it will load an AU at all.
- **Pro Tools/AAX needs an Avid NDA and signed builds**, so it's out of scope here.
- The standalone app crashing on first launch was **macOS TCC privacy**, not a bug in the
  plugin — it needs usage-description keys in `Info.plist`.

---

## 1. COMPANY_NAME, BUNDLE_ID and PLUGIN_MANUFACTURER_CODE — what will customers see?

| Field | Current value | Visible to users? | Where |
|---|---|---|---|
| `COMPANY_NAME` | `Maxximum Displacement` | **Yes, prominently** | The "Manufacturer" column in every DAW's plugin browser. Users browse by it. |
| `PLUGIN_MANUFACTURER_CODE` | `Mxdp` | Rarely | Technical AU identifier. Shows up in `auval`, crash logs, some plugin managers. |
| `PLUGIN_CODE` | `Sgai` | Rarely | Same — technical AU identifier for this specific plugin. |
| `BUNDLE_ID` | `com.maxximumdisplacement.simplegain` | Almost never | macOS internal identity. Appears in crash reports and permission dialogs. |

**How to choose:**

- **`COMPANY_NAME`** — this is your brand. Pick whatever you'd want on a product page. It can
  be your name, a made-up label, anything. Easy to change later while it's just us.
- **`BUNDLE_ID`** — reverse-DNS convention: `com.<yourdomain>.<product>`. You don't need to own
  the domain, but it should be globally unique so macOS doesn't confuse your plugin with
  someone else's. If you own a domain, use it.
- **`PLUGIN_MANUFACTURER_CODE`** — 4 characters, **first uppercase, rest lowercase**. Should be
  unique to you across all audio software. Apple used to run a registry; now it's informal —
  just pick something unlikely to collide and reuse it for every plugin you ever make.
- **`PLUGIN_CODE`** — 4 characters, **exactly one uppercase, and it must be the first**. Unique
  *per plugin*, not per company.

> ✅ **Settled 2025-09-23.** `COMPANY_NAME` is **Maxximum Displacement** (double-x, so it reads
> as a deliberate brand rather than the plain concept), manufacturer code **`Mxdp`**, bundle ID
> **`com.maxximumdisplacement.simplegain`**. `PLUGIN_CODE` stays **`Sgai`**. Re-validated with
> `auval -v aufx Sgai Mxdp` — passes.
>
> Changing the bundle ID makes macOS treat it as a brand-new app, so the microphone permission
> is requested again on first launch. That's expected, not a regression.

> ⚠️ **Change these before you ever share a build.** Once someone has your plugin installed, the
> codes are its identity — changing them later makes the DAW treat it as a completely different
> plugin, and existing projects lose their settings.

---

## 2. Where do I find `auval`?

**It's already on your Mac** — it ships with macOS itself, at `/usr/bin/auval`. Nothing to
install. It's Apple's official Audio Unit validator and Logic uses the same checks internally
before it will load a plugin.

```bash
# List every AU installed on this machine
auval -a

# Validate our plugin specifically:  auval -v <type> <subtype> <manufacturer>
auval -v aufx Sgai Mxdp
```

The three codes:
- `aufx` = "audio effect" (a synth would be `aumu`, a MIDI effect `aumi`)
- `Sgai` = our `PLUGIN_CODE`
- `Mxdp` = our `PLUGIN_MANUFACTURER_CODE`

It rendered audio at sample rates from 11 kHz to 192 kHz, at block sizes from 64 to 4096
frames (including deliberately awkward ones like 137), checked mono and stereo, and checked
that we correctly *fail* when handed an illegal buffer size. Ours passed on the first run.

**Worth knowing:** if `auval` fails, Logic will refuse to load the plugin and won't tell you
why. Always run `auval` before blaming Logic.

The cross-format equivalent is **pluginval** (free, from Tracktion) which tests VST3 too, and
is what most people wire into CI.

---

## 3. Can we build this for Pro Tools?

**Short answer: not realistically and it's Avid's fault rather than ours.**

Why AAX is different from every other format:

| | VST3 / AU | AAX |
|---|---|---|
| SDK | Free, public download | Requires an Avid developer account and a signed **NDA** |
| Can you build one? | Yes, today | Only after Avid approves you |
| Will Pro Tools load your build? | n/a | **No** — AAX plugins must be **digitally signed with a PACE key** |
| Cost of the signing key | n/a | A paid PACE/iLok developer account |

That last row is the real blocker. Even with the SDK, an unsigned AAX plugin won't load in
normal Pro Tools. You'd need either a paid PACE developer account, or a special
*Pro Tools Developer* build which is also restricted.

JUCE fully supports AAX — `FORMATS AAX` is one word in our `CMakeLists.txt` — so **if you ever
get the SDK, adding Pro Tools support is genuinely a one-line change.** The obstacle is
commercial, not technical.

**Practical suggestion:** ignore Pro Tools for now. If you later want to release commercially
and Pro Tools matters, apply to Avid's developer programme then. Nothing we're building will
need to change.

---

## 4. The microphone doesn't respond and the app crashed — same bug ✅ FIXED

Both symptoms had one cause: **our `Info.plist` had no privacy usage descriptions.**

macOS gates access to private things (microphone, camera, Bluetooth, contacts…) behind a system
called **TCC** — *Transparency, Consent and Control*. The rule is:

> If an app wants to use the microphone, its `Info.plist` **must** contain a
> `NSMicrophoneUsageDescription` string explaining why. That string is what you see in the
> permission popup.

And if the key is **missing**, macOS does not simply deny access — **it kills the process.**
That's precisely what your crash report says:

```
Termination Reason:  Namespace TCC, Code 0,
This app has crashed because it attempted to access privacy-sensitive data without a
usage description. The app's Info.plist must contain an NSBluetoothAlwaysUsageDescription key
```

So:
- **No mic movement** → missing `NSMicrophoneUsageDescription`; macOS never even asked you, so
  permission was never granted, so the input was silence.
- **The "random" crash** → the Audio/MIDI settings window has a **Bluetooth MIDI** button;
  something touched Bluetooth, there was no `NSBluetoothAlwaysUsageDescription`, TCC killed us.
  It wasn't random — it was that window being open.

### The fix

JUCE exposes these as `juce_add_plugin` options, so it's four lines in `CMakeLists.txt`:

```cmake
MICROPHONE_PERMISSION_ENABLED   TRUE
MICROPHONE_PERMISSION_TEXT      "SimpleGain needs audio input so you can hear the plugin processing live sound."
BLUETOOTH_PERMISSION_ENABLED    TRUE
BLUETOOTH_PERMISSION_TEXT       "SimpleGain uses Bluetooth to discover wireless MIDI devices."
```

Verify it worked:

```bash
plutil -p build/SimpleGain_artefacts/Debug/Standalone/SimpleGain.app/Contents/Info.plist | grep NS
```

**Only the Standalone needs this.** Inside a DAW, the *host* owns the microphone and has
already obtained permission on its own behalf.

> **If macOS still doesn't prompt:** it caches a "denied" decision per bundle ID. Reset it with
> `tccutil reset Microphone com.maxximumdisplacement.simplegain`, then relaunch.

---

## 5. Where did that Audio/MIDI Settings window come from?

**JUCE gave you that for free — neither of us wrote it.**

It's part of the **standalone wrapper**. A plugin has no way to talk to a sound card (the DAW
normally does that), so to run one as an app JUCE provides a small host around it:
`juce_audio_plugin_client_Standalone.cpp`. That wrapper is what supplies:

- the application window and menu bar
- the **Options → Audio/MIDI Settings** dialog (a stock `juce::AudioDeviceSelectorComponent`)
- the connection to CoreAudio, and the code that calls our `processBlock`
- saving your device choice between launches

Everything in that screenshot — device dropdowns, sample rate, buffer size, Bluetooth MIDI
button — is stock JUCE UI. Our own editor is the separate window that just says "SimpleGain".

