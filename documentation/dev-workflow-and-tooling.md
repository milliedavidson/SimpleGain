# Dev workflow & tooling

CLion, CMake generators, git submodules, and getting a project onto GitHub — the practical
side, as opposed to the conceptual side.

**SimpleGain docs** · [Index](README.md)  
**The plugin:** [Plan](plan.md) · [Hosting & threads](plugin-hosting-and-threads.md) · [Parameters & automation](parameters-and-automation.md) · [Identity & macOS](plugin-identity-and-macos.md)  
**C++ foundations:** [Memory model](cpp-memory-model.md) · [Build pipeline](cpp-build-pipeline.md)  
**Workflow:** **Dev tooling** · [Testing](testing.md) · [CI & GitHub Actions](ci-and-github-actions.md)

---

## In short

- **`--recurse-submodules` is not optional** when cloning — forget it and `libs/JUCE` is empty,
  producing a baffling CMake error rather than an obvious one.
- **CLion is a reasonable choice but not "the" standard** — CMake keeps the project
  IDE-agnostic, and plenty of audio devs use Xcode or Visual Studio.
- **CMake is a generator**, not a build system: it writes the Makefiles/Ninja files that then
  do the actual building.
- **Ninja is just a faster `make`** — worth having, not worth worrying about.
- Build output (`build/`, `cmake-build-*/`) is generated and never committed.

---

## 1. What does "shallow" mean in a git clone?

Git normally downloads **every commit ever made** to a repository. JUCE has been developed
since 2004, so that's a lot of history you will never look at.

A **shallow clone** downloads only the most recent snapshot:

```bash
git clone --depth 1 --branch 8.0.15 https://github.com/juce-framework/JUCE.git
```

`--depth 1` = "just the one commit I asked for, no ancestors."

The result: **110 MB instead of well over 1 GB**, and seconds instead of minutes. The tradeoff
is you can't run `git log` or `git blame` inside JUCE — which is fine, we're consuming it as a
dependency, not developing it.

---

## 2. Is CLion the standard IDE for audio development?

Honestly: **no, but it's a perfectly good choice, and it's arguably the best on Linux.**

Roughly what the industry uses:

| Tool | Where | Notes |
|---|---|---|
| **Xcode** | macOS | The most common for Mac audio work. Best debugger for Apple frameworks, required for AU3/iOS and App Store submission. |
| **Visual Studio** | Windows | Dominant on Windows. Excellent debugger. |
| **CLion** | Cross-platform | Popular with people who want *one* IDE for Mac+Windows+Linux. CMake-native, so no generated project files to babysit. Strong refactoring. |
| **VS Code** | Cross-platform | Lightweight, very common, weaker debugging than the above. |

The key insight: **because we chose CMake, the IDE doesn't matter.** CMake can generate an
Xcode project (`cmake -G Xcode`), a Visual Studio solution, or Makefiles — from the exact same
`CMakeLists.txt`. That's a genuine advantage over JUCE's older Projucer workflow, where the
project file was the source of truth.

So: stay in CLion. If you later hit a gnarly Core Audio bug and want Apple's debugger, you can
generate an Xcode project in one command without changing the project at all.

---

## 3. What is Ninja, and should I use it?

**Ninja** is a *build tool* — the thing that actually runs compiler commands. It's an
alternative to `make`.

Remember the two stages: CMake **generates** build files, then a build tool **executes** them.
CMake can generate for either:

| Generator | Build tool | Speed |
|---|---|---|
| `Unix Makefiles` (default on macOS) | `make` — 1976, universal | Fine |
| `Ninja` | `ninja` — built by Google, designed purely for speed | Noticeably faster |

Ninja is faster mainly at *incremental* builds: it tracks dependencies more precisely and
parallelises better, so changing one file rebuilds less.

**Should you use it?**

- **In CLion: you already are.** CLion bundles Ninja and uses it by default. Nothing to do.
- **In the terminal:** you don't have `ninja` on your `PATH` — that's why my first
  `cmake -G Ninja` failed and I fell back to Makefiles. Not worth worrying about, but if you
  want it: `brew install ninja`, then `cmake -B build -G Ninja`.

Honestly: for a project this size the difference is seconds. Leave it.

---

### 4. One wrinkle for this project: the submodule

JUCE is a **submodule**, so the repo records only *"JUCE lives at this URL, at this commit"* —
not JUCE's 110 MB of code. That's what you want. But it means anyone cloning (including
future-you on another machine) must fetch it:

```bash
git clone --recurse-submodules <url>          # do it at clone time
# or, if already cloned:
git submodule update --init --recursive
```

Forgetting this gives an empty `libs/JUCE` and a baffling CMake error. It's the single most
common way to be confused by a JUCE project.
