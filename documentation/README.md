# SimpleGain documentation

Everything behind the code: how an audio plugin is structured, the C++ you need to read and
write it, and the workflow around building and shipping one.

The aim is a **one-stop shop** — if you're coming from a managed language like C# and want to
write a plugin, you shouldn't need to go anywhere else to understand what's going on here.

Code comments in `Source/` stay short on purpose and point into these docs when there's more
to say.

---

## The plugin

How a plugin and a DAW actually relate, and what that forces on your design.

| Doc | What's in it |
|---|---|
| **[The plan](plan.md)** | The phased build, start to finish — what's done and what's next |
| **[Hosting & threads](plugin-hosting-and-threads.md)** | Why the DAW owns `main()`, the audio thread's deadline, samples vs blocks, planar buffers |
| **[Parameters & automation](parameters-and-automation.md)** | APVTS, dB vs linear gain, skew, choosing a range, thread-safe parameter reads |
| **[Identity & macOS](plugin-identity-and-macos.md)** | Bundle IDs, the 4-character codes, `auval`, Pro Tools/AAX, privacy permissions |

## C++ foundations

The language itself, aimed at someone arriving from C#. Not audio-specific, but you can't get
far in a plugin without it.

| Doc | What's in it |
|---|---|
| **[Memory model](cpp-memory-model.md)** | Stack vs heap, references vs pointers, `const`, RAII vs garbage collection, use-after-free |
| **[Build pipeline](cpp-build-pipeline.md)** | Headers, the preprocessor, compiling, linking, `.o` files, and how it all differs from Roslyn and the CLR |

## Workflow

Getting it built, tested and checked.

| Doc | What's in it |
|---|---|
| **[Dev tooling](dev-workflow-and-tooling.md)** | CLion, CMake generators, Ninja, git submodules |
| **[Testing](testing.md)** | How to actually get audio through the plugin, standalone and in a DAW |
| **[CI & GitHub Actions](ci-and-github-actions.md)** | What the pipeline does, and why the default template didn't work |

---

## How these are structured

Each doc opens with an **"In short"** block — a handful of bullets for when you just need
reminding — followed by numbered deep-dive sections for when you want the full reasoning.

Read the bullets; drop into a section only when you need it.
