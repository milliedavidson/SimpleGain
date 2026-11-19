# CI & GitHub Actions

What a pipeline actually is, what went wrong with GitHub's default C/C++ template and what
`git status` is telling CLion — the practical side of getting this repo checked automatically
on every push.

**Simply Gain docs** · [Index](README.md)  
**The plugin:** [Plan](plan.md) · [Hosting & threads](plugin-hosting-and-threads.md) · [Parameters & automation](parameters-and-automation.md) · [Smoothing & the audio loop](smoothing-and-the-audio-loop.md) · [State: save and restore](state-save-and-restore.md) · [Identity & macOS](plugin-identity-and-macos.md) · [The editor (GUI)](editor-gui.md)  
**C++ foundations:** [Memory model](cpp-memory-model.md) · [Build pipeline](cpp-build-pipeline.md) · [CMake walkthrough](cmake-walkthrough.md)  
**Workflow:** [Dev tooling](dev-workflow-and-tooling.md) · [Testing](testing.md) · **CI & GitHub Actions**

---

## In short

- CLion's **"Changes" vs "Unversioned Files"** is just `git status`: files git already tracks
  (modified or deleted) vs files it has never seen.
- GitHub's default **"C/C++ CI" template assumes GNU Autotools** (`./configure && make`) and a
  Linux runner — wrong on both counts for a CMake-based macOS plugin.
- The replacement builds all three formats and runs **`auval`**, so CI checks "would Logic load
  this", not just "does it compile".
- **`submodules: recursive` is essential** — a fresh runner has nothing, so a forgotten flag is
  a hard failure rather than a warning.
- A pipeline isn't a simulation — it's **your real build on a borrowed, blank machine**, run
  automatically on every push and PR.
- A second workflow, `release.yml`, **builds the Windows and Linux downloads** when a release is
  published, because there's no Windows or Linux machine to build them on locally.

---

## 1. What went wrong with GitHub's "C/C++ CI" template

Clicking **Actions → C/C++ CI → Configure** on GitHub adds a generic starter workflow. It
assumes a completely different build system than this project uses:

```yaml
runs-on: ubuntu-latest
steps:
  - uses: actions/checkout@v4
  - run: ./configure
  - run: make
  - run: make check
  - run: make distcheck
```

That's **GNU Autotools**, the build system most C/C++ projects used before CMake became
standard. A `./configure` script (generated ahead of time by `autoconf`, checked into the repo)
inspects the local machine and writes out a `Makefile`, which `make` then reads.

Simply Gain has neither `configure` nor `Makefile.am` — it's CMake. So the very first step fails
immediately:

```
./configure: No such file or directory
Error: Process completed with exit code 127.
```

It also ran on `ubuntu-latest`, which is wrong for a second, independent reason: JUCE's Audio
Unit (AU) format **only builds on macOS** — the CMake docs say so explicitly (`AU and AUv3
plugins will only be enabled when building on macOS`, see
[C++ build pipeline](cpp-build-pipeline.md)). Even
if the autotools mismatch weren't there, a Linux runner could never validate the one format this
whole project cares most about proving actually works.

**Not a sign anything about the local setup was wrong** — just the wrong starter template for a
CMake project.

---

## 2. The replacement: `.github/workflows/build.yml`

Deleted `c-cpp.yml`, added a workflow that mirrors exactly the commands already run by hand,
every session, throughout building this plugin:

```yaml
name: Build & Validate

on:
  push:
    branches: [ "main", "develop" ]
  pull_request:
    branches: [ "main", "develop" ]

jobs:
  build:
    runs-on: macos-latest

    steps:
      - name: Check out Simply Gain (with the JUCE submodule)
        uses: actions/checkout@v7
        with:
          submodules: recursive

      - name: Configure
        run: cmake -B build -DCMAKE_BUILD_TYPE=Release

      - name: Build Standalone, AU and VST3
        run: cmake --build build --target SimpleGain_Standalone SimpleGain_AU SimpleGain_VST3 -j 4

      - name: Validate the AU with Apple's auval
        run: auval -v aufx Sgai Mxdp

      - name: Run the offline tests
        run: cmake --build build --target check -j 4
```

Three details worth calling out:

- **`submodules: recursive`** — without it, `actions/checkout` leaves `libs/JUCE` empty, exactly
  the trap covered in [Dev tooling §1](dev-workflow-and-tooling.md#1-what-does-shallow-mean-in-a-git-clone).
  A fresh CI runner has nothing but what the checkout step gives it. So a forgotten flag here isn't a
  warning, it's a hard failure. Locally, a stale `libs/JUCE` from an earlier clone can quietly mask
  the mistake.
- **`auval -v aufx Sgai Mxdp`** as the final step — the exact same command run locally to
  validate the plugin (see [Plugin identity & macOS §2](plugin-identity-and-macos.md#2-where-do-i-find-auval)), so CI is genuinely
  checking "would Logic actually load this," not just "does it compile."
- **`--target check`** as the last step runs the offline tests: it builds the test app (a second
  compile of JUCE, which is why it's excluded from the normal build) and runs every test through
  CTest. So CI now checks the *behaviour* — the gain glides, state survives, corrupt data can't
  crash it — as well as "does it build" and "would Logic load it". See
  [Testing §2](testing.md#2-automated-tests).
- **`branches: [ "main", "develop" ]`** — appears twice and means two different things. Under
  `push` it's the branch being pushed *to*; under `pull_request` it's the branch the PR is
  *aimed at*. Both include `develop`, so work is checked as soon as it lands there, before it's
  merged to `main`. A PR from `develop` into `main` is covered by the `main` entry — its commits
  will already have had a push run, so that PR shows a second, duplicate run. Harmless.
- **`runs-on: macos-latest`** — checked this against GitHub's own `runner-images` repo rather
  than assume: `macos-latest` currently means **Apple Silicon (arm64)**, and `macos-14` (the
  Intel-era default many older examples still show) is already **deprecated**. If an Intel
  runner is ever needed specifically, the current label for that is `macos-15-intel`.
- **`actions/checkout@v7`** — each action is a small program that runs on a Node.js runtime, and
  the `@v7` is its version. The first version of this workflow used `@v4`, which runs on Node 20;
  GitHub deprecated Node 20 and warned *"actions/checkout@v4 … forced to run on Node.js 24"*.
  Nothing broke, but the fix is to move to a version built for Node 24, which `v5` onwards are
  (checked in the action's own `action.yml`: `using: node24`). The `submodules` input we depend on
  is unchanged. Expect to do this again every few years: it's the same chore as bumping a NuGet
  package when its target framework goes out of support.

---

## 3. What a pipeline actually does

The honest one-line answer: it isn't simulating a build. It's a **real execution of the real
build**, on a real (virtual) machine, running the exact same tools as a local Terminal: same
`cmake`, same `clang`, same `auval`. Nothing is emulated. GitHub-hosted runners are literally
Azure VMs (GitHub is owned by Microsoft) that get created, used once, and thrown away.

What makes that actually useful isn't "different hardware". The architecture difference is
just a property of *which runner label was chosen*, not something CI inherently requires.
(See §2 above: `macos-15-intel` would give an x86_64 runner matching the machine this was developed
on.) The real value is three things a local Terminal can't give you:

1. **A blank slate, every time.** Every run starts from an empty machine: nothing left over
   from a previous run, no stray environment variable, nothing installed that isn't explicitly
   in the workflow. This is what makes the `submodules: recursive` flag matter so much. A local
   machine might still have JUCE sitting around from an earlier, correct clone, even after a
   mistake. A fresh runner has genuinely nothing until the workflow fetches it. It's the
   automated version of asking: does this work from scratch, or only because of state that has
   accumulated on my machine?
2. **Runs without anyone remembering to.** Fires automatically on every push and every pull
   request, including PRs from someone else. Given the README's "open to PRs" invitation, this
   is the part that matters most in practice. A contributor's build gets checked before their
   diff is ever opened, so you don't have to pull their branch and build it by hand just to find
   out if it compiles.
3. **A visible, permanent record.** Pass or fail becomes a green check or red ✕ attached to that
   exact commit, forever — not "I tested this, trust me," a receipt anyone can go back and look
   at.

**Why it's called "Continuous Integration":** the "continuous" part is exactly this — running
the build-and-verify step continuously, on every change, rather than manually and occasionally.
It's the same idea as `dotnet test` in a build pipeline, just applied to something with a real
hardware target (`auval`) instead of only unit tests.

---

## 4. The release builds: Windows and Linux

The first workflow only runs on a Mac, because the AU format and `auval` only exist there. But
people on Windows and Linux want a download too, and there's no Windows or Linux machine here to
build one on. A GitHub runner is a free blank machine of either kind, so
`.github/workflows/release.yml` borrows them.

**When it runs.** Two ways:

1. **You publish a release on GitHub.** The workflow builds both zips and attaches them to that
   release a few minutes later.
2. **You start it by hand** from the Actions tab (**Release builds → Run workflow**). With the tag
   box empty it only builds, and keeps the zips as downloadable workflow artefacts. That's the safe
   way to try it before a real release. With a tag, it also attaches them.

**What it does.**

- **Two jobs in parallel**, one per system, from a *matrix*. It's the same job body run twice
  with different values, a bit like a `foreach` over `[windows, linux]`. `fail-fast: false` means
  a Windows failure doesn't cancel the Linux build.
- **Linux installs JUCE's dependencies first.** JUCE draws its window with X11 and plays sound
  through ALSA, so a blank Ubuntu needs their development packages before the build can link.
- **Windows needs `--config Release` on the build line.** Visual Studio is a *multi-config*
  generator: the Debug-or-Release choice is made at build time, not at configure time, so
  `-DCMAKE_BUILD_TYPE` is ignored there.
- **`scripts/package-other.sh` makes the archive.** It gathers the VST3, the standalone app, the
  install note and the licences, then uses `cmake -E tar` so one command makes a zip on Windows
  and a `.tar.gz` on Linux with no extra tools.
- **A second job attaches the files** with `gh release upload`. It's the only one given permission
  to write to the release, and it runs only when there's a release to write to.

**Why it waits for a published release.** A draft release has no tag yet, and `gh release upload`
looks releases up by tag. So the order is: create the release as a draft, check it, publish it, and
the Windows and Linux files appear on it shortly afterwards.

**What it doesn't do.** It doesn't sign anything. Windows shows a SmartScreen warning for the
standalone app, and the install note says how to get past it. It also can't tell you the plugin
*works* in a Windows or Linux DAW, only that it builds. That check is still a person with a DAW.
