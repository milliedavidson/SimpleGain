# CI & GitHub Actions

What a pipeline actually is, what went wrong with GitHub's default C/C++ template and what
`git status` is telling CLion — the practical side of getting this repo checked automatically
on every push.

**SimpleGain docs** · [Index](README.md)  
**The plugin:** [Plan](plan.md) · [Hosting & threads](plugin-hosting-and-threads.md) · [Parameters & automation](parameters-and-automation.md) · [Identity & macOS](plugin-identity-and-macos.md)  
**C++ foundations:** [Memory model](cpp-memory-model.md) · [Build pipeline](cpp-build-pipeline.md)  
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

That's **GNU Autotools** — the build system most C/C++ projects used before CMake became
standard: a `./configure` script (generated ahead of time by `autoconf`, checked into the repo)
inspects the local machine and writes out a `Makefile`, which `make` then reads.

SimpleGain has neither `configure` nor `Makefile.am` — it's CMake. So the very first step fails
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
    branches: [ "main" ]
  pull_request:
    branches: [ "main" ]

jobs:
  build:
    runs-on: macos-latest

    steps:
      - name: Check out SimpleGain (with the JUCE submodule)
        uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Configure
        run: cmake -B build -DCMAKE_BUILD_TYPE=Release

      - name: Build Standalone, AU and VST3
        run: cmake --build build --target SimpleGain_Standalone SimpleGain_AU SimpleGain_VST3 -j 4

      - name: Validate the AU with Apple's auval
        run: auval -v aufx Sgai Mxdp
```

Three details worth calling out:

- **`submodules: recursive`** — without it, `actions/checkout` leaves `libs/JUCE` empty, exactly
  the trap covered in [Dev tooling §1](dev-workflow-and-tooling.md#1-what-does-shallow-mean-in-a-git-clone).
  A fresh CI runner has nothing but what the checkout step
  gives it, so a forgotten flag here isn't a warning, it's a hard failure — unlike locally,
  where a stale `libs/JUCE` from an earlier clone can quietly mask the mistake.
- **`auval -v aufx Sgai Mxdp`** as the final step — the exact same command run locally to
  validate the plugin (see [Plugin identity & macOS §2](plugin-identity-and-macos.md#2-where-do-i-find-auval)), so CI is genuinely
  checking "would Logic actually load this," not just "does it compile."
- **`runs-on: macos-latest`** — checked this against GitHub's own `runner-images` repo rather
  than assume: `macos-latest` currently means **Apple Silicon (arm64)**, and `macos-14` (the
  Intel-era default many older examples still show) is already **deprecated**. If an Intel
  runner is ever needed specifically, the current label for that is `macos-15-intel`.

---

## 3. What a pipeline actually does

The honest one-line answer: it isn't simulating a build — it's a **real execution of the real
build**, on a real (virtual) machine, running the exact same tools as a local Terminal — same
`cmake`, same `clang`, same `auval`. Nothing is emulated. GitHub-hosted runners are literally
Azure VMs (GitHub is owned by Microsoft) that get created, used once, and thrown away.

What makes that actually useful isn't "different hardware" — the architecture difference is
just a property of *which runner label was chosen*, not something CI inherently requires (see
§3 above — `macos-15-intel` would give an x86_64 runner matching the machine this was developed
on). The real value is three things a local Terminal can't give you:

1. **A blank slate, every time.** Every run starts from an empty machine — nothing left over
   from a previous run, no stray environment variable, nothing installed that isn't explicitly
   in the workflow. This is what makes the `submodules: recursive` flag matter so much: a local
   machine might still have JUCE sitting around from an earlier, correct clone even after a
   mistake; a fresh runner has genuinely nothing until the workflow fetches it. It's the
   automated version of asking "does this actually work from scratch, or does it only work
   because of some state that's accumulated on my machine and nowhere else?"
2. **Runs without anyone remembering to.** Fires automatically on every push and every pull
   request — including PRs from someone else. Given the README's "open to PRs" invitation, this
   is the part that matters most in practice: a contributor's build gets checked before their
   diff is ever opened, with no need to pull their branch down and build it by hand first just
   to find out if it even compiles.
3. **A visible, permanent record.** Pass or fail becomes a green check or red ✕ attached to that
   exact commit, forever — not "I tested this, trust me," a receipt anyone can go back and look
   at.

**Why it's called "Continuous Integration":** the "continuous" part is exactly this — running
the build-and-verify step continuously, on every change, rather than manually and occasionally.
It's the same idea as `dotnet test` in a build pipeline, just applied to something with a real
hardware target (`auval`) instead of only unit tests.
