# Plugin identity & macOS

Company name, bundle ID and manufacturer codes; `auval`; Pro Tools/AAX; the TCC privacy
crash that killed the standalone app on first launch; and packaging it to send to someone.

**Simply Gain docs** · [Index](README.md)  
**The plugin:** [Plan](plan.md) · [Hosting & threads](plugin-hosting-and-threads.md) · [Parameters & automation](parameters-and-automation.md) · [Smoothing & the audio loop](smoothing-and-the-audio-loop.md) · [State: save and restore](state-save-and-restore.md) · **Identity & macOS** · [The editor (GUI)](editor-gui.md)  
**C++ foundations:** [Memory model](cpp-memory-model.md) · [Build pipeline](cpp-build-pipeline.md) · [CMake walkthrough](cmake-walkthrough.md)  
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
- **macOS caches plugin names.** After a rename, Logic can show the old name until the cache is
  cleared.
- **Sending it to someone needs three things**: a universal binary, an old enough minimum macOS,
  and a way past Gatekeeper. `scripts/package-mac.sh` does the first two and signs ad hoc; a
  warning-free installer needs an Apple Developer ID.

---

## 1. COMPANY_NAME, BUNDLE_ID and PLUGIN_MANUFACTURER_CODE — what will customers see?

| Field                      | Current value                         | Visible to users?    | Where                                                                           |
|----------------------------|---------------------------------------|----------------------|---------------------------------------------------------------------------------|
| `COMPANY_NAME`             | `Maxximum Displacement`               | **Yes, prominently** | The "Manufacturer" column in every DAW's plugin browser. Users browse by it.    |
| `PLUGIN_MANUFACTURER_CODE` | `Mxdp`                                | Rarely               | Technical AU identifier. Shows up in `auval`, crash logs, some plugin managers. |
| `PLUGIN_CODE`              | `Sgai`                                | Rarely               | Same — technical AU identifier for this specific plugin.                        |
| `BUNDLE_ID`                | `com.maxximumdisplacement.simplegain` | Almost never         | macOS internal identity. Appears in crash reports and permission dialogs.       |

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

> ✅ **Renamed 2026-11-16.** `PRODUCT_NAME` is now **simply gain**: the name in the brand plan,
> in lower case to match the logo-style title in the window. It's the name hosts list, and the
> bundles are called `simply gain.component`, `simply gain.vst3` and `simply gain.app`. The
> lockup at the bottom of the window still shows it in capitals.
>
> The CMake target, the class names and the bundle ID keep `SimpleGain`. Nobody sees them, and
> the bundle ID is one of the permanent things. Hosts recognise the plugin by its codes (`Sgai`,
> `Mxdp`), not its name, so a saved project still finds it.
>
> Side effects:
>
> - The Standalone keeps its settings in a file named after the product, so it started afresh at
>   0 dB, and the old `SimpleGain.settings` can go.
> - Old bundles in `~/Library/Audio/Plug-Ins/` have to be removed, or the host sees two plugins
>   with the same codes.
> - macOS's file names ignore case, so `Simply Gain.component` and `simply gain.component` are the
>   *same* folder. Copying over it keeps the old capitals, so the old bundle was removed first.

**When a host still shows the old name.** macOS keeps a cache of every Audio Unit's name, and Logic
reads its list from it. After a rename, quit Logic, then run:

```bash
killall -9 AudioComponentRegistrar
rm ~/Library/Caches/AudioUnitCache/com.apple.audiounits.cache
```

The first stops the background process that keeps the cache; the second deletes the cache, which
macOS rebuilds. In Logic, *Settings → Plug-in Manager → Reset & Rescan Selection* does the same
for one plugin.

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
frames (including deliberately awkward ones like 137), and checked mono and stereo. It also checked
that we correctly *fail* when handed an illegal buffer size. Ours passed on the first run.

**Worth knowing:** if `auval` fails, Logic will refuse to load the plugin and won't tell you
why. Always run `auval` before blaming Logic.

The cross-format equivalent is **pluginval** (free, from Tracktion) which tests VST3 too, and
is what most people wire into CI.

---

## 3. Can we build this for Pro Tools?

**Short answer: not realistically and it's Avid's fault rather than ours.**

Why AAX is different from every other format:

|                                 | VST3 / AU             | AAX                                                               |
|---------------------------------|-----------------------|-------------------------------------------------------------------|
| SDK                             | Free, public download | Requires an Avid developer account and a signed **NDA**           |
| Can you build one?              | Yes, today            | Only after Avid approves you                                      |
| Will Pro Tools load your build? | n/a                   | **No** — AAX plugins must be **digitally signed with a PACE key** |
| Cost of the signing key         | n/a                   | A paid PACE/iLok developer account                                |

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
juce_add_plugin(SimpleGain
    MICROPHONE_PERMISSION_ENABLED   TRUE
    MICROPHONE_PERMISSION_TEXT      "Simply Gain needs audio input so you can hear the plugin processing live sound."
    BLUETOOTH_PERMISSION_ENABLED    TRUE
    BLUETOOTH_PERMISSION_TEXT       "Simply Gain uses Bluetooth to discover wireless MIDI devices."
    # ... more arguments
    )
```

Verify it worked:

```bash
plutil -p "build/SimpleGain_artefacts/Debug/Standalone/simply gain.app/Contents/Info.plist" | grep NS
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
button — is stock JUCE UI. Our own editor is the separate window that says "simply gain".

---

## 6. Sending it to someone: universal builds, signing and Gatekeeper

The everyday build works on the Mac that built it. Three things stop it working on someone else's:

| Problem         | Why                                                                                         | Fix                                                                      |
|-----------------|---------------------------------------------------------------------------------------------|--------------------------------------------------------------------------|
| The wrong chip  | A build matches the machine's own chip. This Mac is Intel, so its builds are Intel-only     | `CMAKE_OSX_ARCHITECTURES "arm64;x86_64"`: a universal binary             |
| Too new a macOS | Left unset, the minimum macOS is the SDK's own version (15.5 here)                          | `CMAKE_OSX_DEPLOYMENT_TARGET 11.0`, in `CMakeLists.txt`                  |
| Gatekeeper      | A download is marked as quarantined, and macOS blocks quarantined code Apple hasn't checked | For now: sign ad hoc, and remove the mark by hand. Later: a Developer ID |

- **A universal binary** is two programs in one file, one per chip, and macOS picks the right one.
  `lipo -archs <binary>` lists what's inside. It takes twice as long to build, so only the package
  is built that way.
- **macOS 11** was the first version on Apple Silicon, so it's a natural floor. `otool -l <binary>`
  shows it as `minos`. This one *is* in `CMakeLists.txt`, for every build: a Debug build that
  needs the latest macOS would only be found out later.
- **Gatekeeper.** Mail and browsers add an attribute, `com.apple.quarantine`, to everything they
  download. macOS then refuses quarantined code unless it's signed with an Apple **Developer ID**
  *and* notarised. The install note has a one-line `xattr` command that removes the attribute.

**C# comparison.** This is Windows' Mark of the Web and SmartScreen. The quarantine attribute is
the Mark of the Web, Gatekeeper is SmartScreen, and a Developer ID is an Authenticode certificate.

### Ad hoc signing

`codesign --sign -` signs with *no* identity: the `-`. The signature proves the bundle hasn't
changed since it was signed, but says nothing about who made it. Apple Silicon Macs refuse to run
code with no signature at all, so even this much matters. Intel builds come out of the linker
unsigned, which is why the plugins installed on this Mac show "not signed at all".

### Making the package

```bash
scripts/package-mac.sh
```

It writes `dist/simply-gain-0.1.0-mac.zip` (the version comes from `project()`). Inside:

| File                    | What it is                                     |
|-------------------------|------------------------------------------------|
| `simply gain.component` | The Audio Unit, for Logic and GarageBand       |
| `simply gain.vst3`      | The VST3, for most other DAWs                  |
| `How to install.txt`    | Where to put them, and the `xattr` command     |
| `Licences/`             | The plugin's, the fonts', JUCE's and the SDKs' |

- **It builds in `build-release/`**, so the everyday Debug build is left alone.
- **It doesn't install anything.** `-DSIMPLEGAIN_INSTALL_AFTER_BUILD=OFF` turns off
  `COPY_PLUGIN_AFTER_BUILD`, so the Release build doesn't overwrite the plugins on this Mac.
- **It checks itself**: it stops if the result isn't universal, and prints the minimum macOS.
- **`ditto`, not `zip`.** `ditto` is the zip tool the Finder uses. It keeps a bundle's symbolic
  links and permissions, which a plain `zip` can break.
- **The licences travel with it.** The Open Font License asks for its text to go wherever the
  fonts go, and they're compiled into the plugin. JUCE is used under the AGPLv3, which asks that
  anyone given the plugin can get its source, so the note links to this repository.

### Later: a warning-free installer

For a plugin that installs with no warnings and no Terminal, macOS needs it signed by a known
developer and checked by Apple. That needs the paid **Apple Developer Program** (about £79 or $99 a
year), joined in your own name. Then:

1. Sign each bundle with a *Developer ID Application* certificate, with the hardened runtime and
   a timestamp: `codesign --sign "Developer ID Application: …" --options runtime --timestamp`.
2. Build an installer that puts them in `/Library/Audio/Plug-Ins/` (`pkgbuild` and
   `productbuild`), signed with a *Developer ID Installer* certificate.
3. Send it to Apple: `xcrun notarytool submit … --wait`. Apple scans it and records the result.
4. Attach Apple's result to the file: `xcrun stapler staple …`, so it can be checked offline.

The tools are already on this Mac, with the command-line tools. Only the certificates are missing.
