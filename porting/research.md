# Research: what the engine needs, and how established engines do it

This page records the research behind `docs/architecture.md`. It covers what game5
actually asks of an engine, how well-known engines and libraries handle each part, the
measurements that settled the open questions, and the decision for each part. Dated
2026-10-21. Re-check the library versions before you rely on them.

## 1. What game5 is, in engine terms

game5 (Fall Line) is about 137k lines of engine TypeScript (715 files) and 310k lines of
app (1191 files). It is built in three layers with one direction of dependency:

| game5 layer | What it does | What the C++ engine needs for it |
| --- | --- | --- |
| `engine/` (headless, deterministic) | `generateLevel(seed)` builds a mountain: a heightfield, the piste, the gates, the forest, lifts, the village and the weather. `createGame` / `step(state, input)` advance a run at a fixed 120 Hz. `sim/` holds a bot and a headless simulator, and `analysis/` and `rating/` score maps. It draws randomness only from a seeded mulberry32 and reads no clock. | A deterministic math pool (the PRNG, a libm with fixed bits, noise, heightfields, quaternions, solar), plain-data state, a fixed-step loop and a headless build for tests and the simulator. |
| `pwa/` (browser shell) | Preact UI (cards, HUD, settings) and a three.js renderer (r186, partway through a move from WebGL to WebGPU/TSL). The renderer draws a terrain clipmap, an instanced procedural forest (ten variants per kind at three LODs), a snow shader with glitter and corduroy, trail maps stamped from the engine's contacts, skinned figures dressed in code, glTF skis, particles (spray, snow cloud, snowfall), a physical sky table, haze, a shadow box and a colour grade. Sound is 100% synthesized on one WebAudio instrument; there are no audio files. Saved data lives in localStorage and IndexedDB. | A window, input (keyboard, touch and pointer, gamepad), a GPU API with compute and instancing, skinning, a 2D UI layer with text, a synth DSP graph, small key-value storage and a screenshot path. |
| `tests/` + `scripts/` (Node) | Vitest suites, determinism digests and about 150 labs (level maps, ride labs, screenshots, the profiler). | A test runner, golden files, a headless simulator CLI and image output. |
| `tauri/`, `native/` | Desktop and store shells around the website. | Nothing at first: a native engine is the desktop app. |

The oss-game-framework supplies the generic half: `core/` (prng, math/hypot, noise,
heightfield, polyline, quat, solar, clock, output), `racing/` (tape, records, standings),
`audio/` (the synth), `loop/run-clock`, `hud/format`, `input/*`, `shots/*` and the Node
`tooling/`. All of it is about 6.8k lines, and the C++ port mirrors it module for module.

**What game5 does NOT need:** a general rigid-body physics engine (the skier, the
ragdoll and the machines are game5's own models), a scene editor, a scripting VM, an
asset store, networking, or music.

## 2. The determinism question, measured

game5's contract is **bits**. A pinned map stores the digest of the map it builds, a
ghost replays a recorded control tape, and the CI simulation compares digests. For the
C++ port to be correct, it has to reproduce IEEE 754 doubles bit for bit. Basic
arithmetic (`+ − × ÷ √`) is correctly rounded everywhere, so it ports exactly as long as
the compiler does not contract `a*b+c` into an FMA or reassociate. The transcendental
functions are the risk: game5's engine calls `Math.sin`/`cos` about 1,200 times in its
source, `atan2` 282 times, `exp` 62, `pow` 19 and `tanh` 9.

**Experiment (2026-10-21, this container).** We drew 200,000 random doubles in [−10, 10]
off mulberry32. We evaluated them in Node 22.22 (V8 12.4) and in C++ with glibc 2.39's
libm, built `-O2 -ffp-contract=off`, and compared the bits:

| function | glibc ≠ V8 | V8 ≠ correctly rounded (quad-precision reference) |
| --- | --- | --- |
| sin | 6,477 (3.2%) | 6,565 |
| cos | 6,494 (3.2%) | 6,572 |
| tan | 7,226 (3.6%) | — |
| atan2 | 35,620 (17.8%) | 35,616 |
| exp | 19,465 (9.7%) | 19,482 |
| pow | 19,236 (9.6%) | — |
| asin | 11,922 (6.0%) | — |
| log | 13,339 (6.7%) | — |

Conclusions:

1. **`<cmath>` cannot be used in the simulation.** A platform libm differs from V8 in
   3–18% of calls, and it also differs between glibc, Apple's libm and MSVC's CRT.
2. **Node 22's V8 is not correctly rounded.** glibc is nearly correctly rounded, and V8
   differs from the correct result about as often as it differs from glibc. V8 12.x
   uses its fdlibm port (`src/base/ieee754.cc`).
3. **V8 changed.** On current V8 `main`, `ieee754.cc` forwards acos, asin, atan, atan2,
   cos, exp, expm1, log, log1p, log2, log10, cbrt, sin, tan and pow to **LLVM libc**,
   and `tanh` to `std::tanh` (the platform's). So the bits game5's digests were cut
   under depend on which Node/V8 its CI ran (game5's `.nvmrc` says 24). The C++
   `core/libm` must match **that** runtime: an fdlibm port if the oracle's V8 is fdlibm,
   or LLVM libc's correctly rounded routines if it is LLVM libc. Correctly rounded
   results are identical in every correct implementation, which is the better place to
   end up. `tanh` needs its own answer either way. This is milestone M1, and it is
   settled by golden tables generated under game5's pinned Node, not by reasoning.
4. **Settled by M1 (2026-10-21), measured under game5's pinned Node 24.22.1 (V8
   13.6.233.17) on Linux x86-64, glibc 2.39, one million inputs per function** (game5's
   ranges, the reductions' awkward points and arbitrary bit patterns; the interpreter
   and the optimizing tiers give identical tables):

   | function | V8 13.6 fdlibm ≠ Node 24 | glibc ≠ Node 24 | correctly rounded ≠ Node 24 |
   | --- | --- | --- | --- |
   | sin, cos, tan | 0 | 2.3–2.8% | 2.3–2.9% |
   | asin, acos, atan, atan2 | 0 | 2.2–3.9% | 2.3–4.6% |
   | exp, expm1 | 0 | 6.0%, 0.03% | 6.9%, 3.3% |
   | log, log2 | 0 | 1.7%, 0.13% | 1.9%, 0.16% |
   | cbrt, tanh | 0 | 48.7%, 2.0% | 6.0%, 5.1% |
   | pow | 6.2% | **0** | 0.11% |

   ("Correctly rounded" is a 300-bit mpmath reference on a 20,000-row sample. A correctly
   rounded result is unique, so it stands for LLVM libc or CORE-MATH, which current V8
   `main` forwards to; Node 24 predates that switch.) Conclusions: Node 24's V8 does
   **not** define `V8_USE_LIBM_TRIG_FUNCTIONS`, so thirteen functions are V8's fdlibm port,
   and `engine/core/libm` vendors exactly that file. **`Math.pow` is different**: V8 13.6
   defaults to `--use-std-math-pow`, so `Math.pow` (and `**`) is V8's `math::pow` wrapper
   (`x ** 2` → `x * x`, `x ** 0.5` → `sqrt`) over the host's `std::pow`. Node 24's pow
   therefore depends on the machine game5 runs on: glibc on Linux, Apple's libm on macOS,
   the UCRT on Windows. game5's digests are cut on Linux CI, so the reference is glibc's
   pow, which is Arm's optimized-routines pow; on an x86-64 with FMA glibc runs the build
   that GCC contracted into FMAs, and without those FMAs 1 call in 2,400 differs. The port
   (`engine/core/libm/pow.cpp`) writes each of GCC's contractions as an explicit
   `std::fma` and matches glibc on 1M Node rows plus 40M direct comparisons. A correctly
   rounded libm would have been the cleaner target, but it is not what the oracle runs.
5. **`Math.hypot` is already solved.** The framework's `hypot`/`hypot3`/`hypot4` use only
   correctly rounded operations. The C++ port (`engine/core/math.hpp`) matches the
   TypeScript bit for bit on 4,000 rows spanning magnitudes from 1e−20 to 1e20 (M0's
   test), on every CI row.

Build flags that are part of the contract: `-ffp-contract=off -fno-fast-math` (GCC also
`-fexcess-precision=standard`) and `/fp:strict` on MSVC. No `-march` that implies FMA
contraction. x86-64 and AArch64 both use SSE2/NEON doubles, so there is no x87.

## 3. How established engines are built, part by part

### Architecture and the game-object model

- **Unreal / Unity (classic)**: an object hierarchy (actors and components, or
  GameObjects and MonoBehaviours) with virtual update calls. It is flexible for
  editors, but the data is scattered in memory and the update order is implicit.
- **Unity DOTS, Bevy, flecs, EnTT**: entity-component-system. Components live in packed
  arrays (archetypes or sparse sets), and systems iterate them. It is cache-friendly and
  parallel-friendly, but dynamic: an agent reading a system cannot see which entities
  it touches without tracing queries, and iteration order is a property of the storage.
- **Godot**: a scene tree of nodes plus servers (RenderingServer, PhysicsServer,
  AudioServer) that own their data in flat arrays behind handles. The **server** split
  (game logic talks to subsystems through narrow APIs, and the subsystems keep
  data-oriented internals) is the part worth copying.
- **Data-oriented "handmade" engines** (e.g. the approaches popularised by
  Our Machinery, Molecule, and many shipped racing games): plain structs, explicit arrays
  per kind of thing, free functions and an explicit frame pipeline.

**Decision: no generic ECS.** game5's simulation is already data-oriented:
`GameState` is a typed struct, every kind of thing (rivals, trees, gates, birds) is an
explicit array, and `step` calls systems in a fixed written order. That order is part of
the determinism digest. The port keeps exactly that model as POD structs, typed arrays
(SoA where a loop is hot) and free functions in a stated order. It is faster than an ECS
for a known, closed set of kinds, every access can be found with grep (G3), and the order
stays explicit (G1). The renderer copies Godot's server idea: each draw system owns its
GPU buffers and reads `GameState` through one extraction call a frame.

### Main loop and time

Glenn Fiedler's "Fix Your Timestep" accumulator is what nearly every deterministic game
uses, and it is what the framework's `loop/run-clock` already is: clamp the frame delta,
step whole fixed steps, and give the renderer `alpha` for interpolation. It ports as is.

### Platform layer (window, input, audio device, files)

| Option | Notes |
| --- | --- |
| **SDL3** (3.2 stable Jan 2025, 3.4 early 2026) | Windows, macOS, Linux, iOS, Android and Emscripten. Window, keyboard, mouse, touch, pen, gamepad (with rumble), audio streams, storage paths, file dialogs. zlib licence. |
| GLFW | Window and input only; no audio, touch or mobile. |
| Native per-OS | Most control and most code; not justified. |

**Decision: SDL3**, statically linked. It is the one dependency that covers window,
input, gamepad rumble (game5 has `rumble.ts`), audio output and the web target.

### GPU API

| Option | Web | Notes |
| --- | --- | --- |
| **WebGPU via `webgpu.h`** (Dawn / wgpu-native; Emscripten's emdawnwebgpu) | yes | The standard C header, implemented by Dawn (Chrome's WebGPU, on D3D12, Metal and Vulkan) and wgpu-native (Firefox's). Compute shaders, storage buffers, instancing, timestamp queries. Shaders in WGSL. |
| SDL3 GPU | not in a stable release as far as we found | Vulkan, Metal and D3D12, with shaders cross-compiled per backend. |
| bgfx | WebGL2 only | Mature, with its own shader dialect and build. |
| sokol_gfx | WebGL2/WebGPU | Small and pleasant, but a lowest-common-denominator feature set. |
| Vulkan + MoltenVK | no | Most work; no web target. |

**Decision: WebGPU through `webgpu.h`.** It is the only option that is the same API on
desktop and in the browser (G5). game5 is already moving its renderer onto WebGPU
(`docs/webgpu.md` there), so the shader logic it ports into TSL/WGSL carries across.
Native builds start on **wgpu-native's prebuilt release** (a download, so CI stays
quick) and keep to the header so that Dawn can be swapped in. SDL3 provides the window
and the surface.

### Rendering techniques game5 uses (what we must build, no more)

Terrain: a geometry clipmap over the engine's heightfield. Forest: GPU instancing with
LOD bands. Figures: linear-blend skinning. Snow: a custom shader (packed share, glitter,
grooves from a trail-map texture). Sky: a baked transmittance/radiance table and haze
applied per material. Shadows: a sun shadow map plus a shadow box for the riders.
Particles: spray, snow cloud and snowfall. Post: a colour grade. Each becomes one draw
system in `engine/render` (or a plugin's `app/` when a genre shares it, or
`games/fall_line/app` when it is Fall Line's own look; `docs/architecture.md` § Plugins).
The pipeline is written out as a fixed, ordered list of passes. No render graph is built
until the pass count makes one worth it.

### Audio

game5 synthesizes everything on the framework's `audio/synth.ts`: oscillators, noise,
biquad filters, envelopes, an echo, layered racks, and beds steered by the game state.
FMOD and Wwise are built for authored assets and would add nothing here. miniaudio and
SDL3 audio both offer a callback or stream to fill. **Decision: our own DSP graph
(a port of synth.ts), fed through SDL3's audio stream.** It is checked against an offline
WebAudio render of the same definitions.

### UI and text

game5's HUD and menus are Preact DOM. The options are Dear ImGui (great for developer
tools, not for a player-facing look), RmlUi (HTML/CSS-like, heavy) and a custom
immediate-mode 2D layer. **Decision: a small immediate-mode 2D layer on the GPU** (rects,
images and SDF or MSDF text) that builds only the cards and HUD game5 has, plus Dear
ImGui behind a flag for the developer page.

### Assets

Almost everything in game5 is generated in code: the trees, the wildlife, the gates,
the clothes and every sound. The exceptions are the glTF skis (`pwa/models/`), the title
plates (WebP), the baked real-face tables and the far-ranges tables. **Decision:**
`cgltf` for the skis. The other baked data is converted offline by `tools/` into binary
blobs embedded at build time, and plates are converted to PNG or KTX2 at the same step.
No runtime asset database.

### Tests, tools and profiling

- **Tests:** a zero-dependency harness (`tests/check.hpp`): one executable per topic,
  registered with CTest, and golden files generated from the TypeScript oracle. Catch2
  or doctest would also work, but our assertions are mostly bit comparisons, and a
  60-line header keeps the build and the agent's mental model small.
- **Profiling:** Tracy, behind a compile flag, from M5 on. Bench numbers are recorded in
  `docs/perf.md` once it exists.
- **Build:** CMake 3.25+ with Ninja and presets. Dependencies are fetched by
  `FetchContent` pinned to a release tag or commit, and none are vendored by hand.

## 4. Summary of decisions

| Part | Decision |
| --- | --- |
| Language | C++20, no exceptions or RTTI in engine code paths (they may stay enabled for tools) |
| Game-object model | POD state structs and explicit arrays, systems as free functions in a written order; no ECS |
| Determinism | Own `core/libm` matched to the oracle's V8; FP flags in CMake; golden parity tests |
| Loop | Fixed 120 Hz accumulator (port of `loop/run-clock`), interpolated rendering |
| Platform | SDL3 |
| GPU | WebGPU through `webgpu.h` (wgpu-native first, Dawn optional, emdawnwebgpu on web) |
| Shaders | WGSL, embedded at build |
| Audio | Port of the framework synth as a DSP graph on SDL3 audio |
| UI | Own immediate-mode 2D + SDF text; Dear ImGui for dev tools |
| Assets | cgltf for skis; everything else generated in code or baked to embedded blobs |
| Tests | Own header harness, CTest, golden files from the TS oracle |
| Build/CI | CMake + Ninja + presets; GitHub Actions on Linux x64/arm64 (GCC, Clang), macOS, Windows |

## Sources

- SDL 3 release coverage: [linuxiac](https://linuxiac.com/sdl-simple-directmedia-layer-3-released/),
  [SDL 3.4](https://www.linuxtoday.com/blog/sdl-3-4-brings-native-png-support-and-expanded-gpu-rendering-features/)
- webgpu.h and its implementations: [webgpu-headers](https://github.com/webgpu-native/webgpu-headers),
  [Dawn](https://dawn.googlesource.com/dawn)
- V8's libm: [`src/base/ieee754.cc`](https://github.com/v8/v8/blob/main/src/base/ieee754.cc)
  (read 2026-10-21; forwards to LLVM libc),
  [Chromium's fdlibm `ieee754.h`](https://chromium.googlesource.com/chromium/src/+/refs/tags/93.0.4577.129/third_party/fdlibm/ieee754.h)
- SpiderMonkey's move to fdlibm `pow` for cross-platform bits:
  [fdlibm-for-pow.js](https://searchfox.org/firefox-main/source/js/src/jit-test/tests/basic/fdlibm-for-pow.js)
- The fixed-timestep accumulator: Glenn Fiedler, "Fix Your Timestep!" (gafferongames.com)
