# gameengine2

A genre-agnostic C++20 game engine that is fast, deterministic to the bit, and built for
agents. The engine holds what any game would use, plugins hold what one genre or world
shares, and a game links the engine and the plugins it needs.

The code is written by AI agents, for AI agents. Start at [AGENTS.md](AGENTS.md): it is the
router to every rule, doc and skill in the tree.

## What it is

- **Deterministic.** The simulation produces the same IEEE 754 doubles for the same seed and
  inputs on every compiler and OS. It has its own vendored libm, its own number semantics and
  one PRNG, builds with no FMA contraction and no fast-math, and every simulation module is
  held by a golden table in `tests/golden/`. Replays, ghosts and generated worlds reproduce
  everywhere.
- **Genre-agnostic, with compile-time plugins.** Code lives in one of three homes:
  `engine/` (any game of any genre), `plugins/<name>/` (any game of one genre or world) and
  `games/<game>/` (one game). Dependencies run game → plugin → engine. A plugin is a static
  library declared with one `ge_add_plugin` call, exposing plain functions and data; there is
  no run-time registry and nothing loaded at run time.
- **Measured, not guessed.** Every subsystem, plugin and game has labs on one engine-level
  contract: simulation runs, scenarios, captioned screenshots, sound renders, bot and agent
  drives, and cost benches. Every lab runs headless and doubles as a determinism test through
  its digest.
- **Built for agents.** Every source file carries a `ge:` tag block (summary, keywords,
  owning skill, test), so `make find` and the generated file index route a question to the
  one file that answers it. Every subsystem has a skill under `.agents/skills/`, and the
  skills keep lessons. Every error, assert and log line says what failed, where, and what to
  do next.

## What is in the tree

| Path | What |
| --- | --- |
| `engine/core/` | The deterministic pool: the PRNG, math, the vendored libm, number semantics, noise, heightfields, quaternions, the sun and the moon, the diagnostic output channel |
| `engine/loop/` | The fixed-step run clock between the display's frames and the simulation's steps |
| `engine/platform/` | SDL3: the window, input, the frame clock and the audio device |
| `engine/gpu/` | The GPU device over Dawn: feature tiers, pipelines and their manifest, offscreen targets and readback, GPU timers, WGSL embedded at build time |
| `engine/render/` | GPU-driven rendering: the frame's pass table and named targets, a GPU-culled heightfield clipmap, horizon-map terrain shadows, a physically based sky with aerial perspective, a Hi-Z depth pyramid, instance families with two-phase occlusion culling and LODs, TAA-U and post |
| `engine/audio/` | A deterministic synthesizer: tones, noise and layered voices on a DSP graph (polyBLEP oscillators, state-variable filters, buses, echo, a limiter), sound definitions, racks and a meter |
| `engine/lab/` | The lab contract and runner: metrics, tables, captioned pictures, sounds as WAV, digests, before-and-after comparison, contact sheets |
| `engine/cli/` | The `ge` command line for game repos built on the engine |
| `plugins/` | The genre plugins: `racing` (control tapes for ghosts and replays, the record book, a field's standings), `mountain` (a generated mountain and its ski area, and the rule book that checks it), `skier` (a skier on snow) |
| `games/` | An example game built on the engine and its plugins |
| `consumer-skills/`, `scaffold/` | The skills and the repo layout `ge` gives a new game repo |
| `tests/` | One test executable per topic, the golden tables and the lab digests |

## Rendering

The GPU layer is [Dawn](https://dawn.googlesource.com/dawn), compiled natively through its
`webgpu.h` header, translating to Metal on Apple, Vulkan on Linux and Android, and D3D12 or
Vulkan on Windows. Shaders are WGSL, compiled by Tint to each backend and validated offline
by `make shaders-check`. There is no web build.

Everything that draws also runs headless on a Linux machine with no GPU: Dawn renders on
Mesa's lavapipe, a software Vulkan driver, so tests and screenshot labs are verified by
capture and golden images in a container and on CI. Performance is only ever claimed from
a real GPU. [docs/rendering.md](docs/rendering.md) has the pass table, the techniques and
why each was chosen.

## Platforms

| Platform | Status |
| --- | --- |
| Linux (x64, arm64) | Built and tested on CI; the GPU runs on lavapipe on x64 |
| macOS (Apple silicon) | Built and tested on CI |
| Windows (x64, MSVC) | Built and tested on CI |
| iOS, iPadOS, Mac App Store | Planned (milestone M11) |
| Android | Planned (milestone M11) |

The web is not a target.

## The `ge` command line

`ge` is what a game repo built on the engine uses. Every message it prints is a finding
(what, where, next) and its exit code says what happened.

```sh
ge new <dir>        # scaffold a game repo pinned to this engine: AGENTS.md, CMake, tests, labs, skills
ge skills sync      # copy the engine's consumer skills into .agents/skills/, pinned to this engine
ge doctor           # check the repo against the engine's rules
ge lessons <skill>  # read a skill's lessons back
ge version          # the engine version this ge belongs to
```

In this tree, `make ge ARGS="new ../my-game"` builds and runs it.

## Build and test

Requires CMake 3.25+, Ninja and a C++20 compiler (GCC 13+, Clang 17+, Apple Clang or
MSVC 2022). SDL3 and Dawn are downloaded and pinned by CMake into `.cache/deps/`. On Ubuntu
24.04 or 22.04, [`tools/setup-ubuntu.sh`](tools/setup-ubuntu.sh) installs everything else from a
minimal server install (the toolchain, clang-format and clang-tidy 18, the X11, Wayland and audio
development packages, Mesa's software Vulkan driver and Xvfb for the GPU tests, Node.js and
Python 3 for the checks); rerun it any time, since it installs only what is missing or too old.

```sh
sudo tools/setup-ubuntu.sh   # once per machine; --check reports, --verify also builds and tests
```

```sh
make test                 # debug build with ASan/UBSan, then ctest
make test PRESET=release  # optimized, no sanitizers
make labs                 # list every lab
make lab LAB=<id>         # run one lab; ARGS="--tag=before" / ARGS="--compare=before" to compare runs
make labs-smoke           # every lab at its smoke flags, twice, digests checked
make fmt                  # clang-format in place
make lint                 # clang-tidy
make agents-check         # the repo's structure: symlinks, skills, ge: tags, plugins, legacy scan
make find Q="word word"   # files, skills and lessons matching any word
```

`-DGE_GPU=OFF` and `-DGE_PLATFORM=OFF` build without the GPU and the platform layer; the
simulation, the plugins and their tests never need either.

## Using the engine in a game

A release is a tag plus CMake packages for Linux, macOS and Windows (headers, static
libraries and `ge`) and a source archive, on GitHub Releases and GitHub Packages. A game
takes it with `FetchContent` or `find_package(gameengine2)` and links `ge::core`,
`ge::audio`, `ge::lab`, and a `ge::<plugin>` per plugin. [docs/publishing.md](docs/publishing.md)
has the details.

## Docs

- [docs/goals.md](docs/goals.md): the goals, the non-goals and the milestone roadmap
- [docs/architecture.md](docs/architecture.md): the layers, plugins, the frame, diagnostics, the determinism contract and the build
- [docs/rendering.md](docs/rendering.md): the GPU backend and every rendering technique
- [docs/labs.md](docs/labs.md): the lab contract and how to run labs
- [docs/publishing.md](docs/publishing.md): releases and how a game consumes one

## License

[PolyForm Noncommercial 1.0.0](LICENSE): the source is open to read, use, change and share for
any noncommercial purpose; commercial use is reserved to the copyright holder. Vendored code keeps
its own licence beside it (`engine/core/libm/LICENSE.md`), and data baked into the engine is
listed with its source and terms in [docs/rendering.md](docs/rendering.md) § Dependencies.
