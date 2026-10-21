# Working notes: the reference behind each subsystem

A WORKING DOCUMENT (`porting/` is deleted when porting ends). The skills describe each
subsystem in the engine's own terms; this page keeps what a porting session needs on top:
which reference modules a subsystem reproduces and how to compare against them. One section
per skill, named after it.

## determinism

- The thirteen fdlibm functions are V8 13.6's fdlibm port (`src/base/ieee754.cc`), vendored
  line for line. Node 24 does not build V8 with its glibc trig, so this is what `Math.sin`
  runs under game5's pinned Node.
- pow: Node 24 runs `Math.pow` on the host `std::pow` (V8's `--use-std-math-pow`, after the
  `y == 2` and `y == 0.5` shortcuts), so the reference is glibc's pow on game5's Linux x86-64
  CI: Arm optimized-routines pow, which glibc compiles with FMA contraction.
- A correctly rounded libm (LLVM libc, CORE-MATH) misses Node 24 in 0.1–7% of calls, so it is
  not an option while game5 pins Node 24 (`research.md` §2 has the measurement).
- **When game5's `.nvmrc` moves**, regenerate the tables with `make golden-libm NODE=<that
  node>` on Linux x86-64. If they change, find what the new V8 runs (`src/base/ieee754.cc`,
  the `use_std_math_pow` and `use_libm_trig_functions` flags, and Node's
  `tools/v8_gypfiles/features.gypi`), vendor that, and say so in the PR.
- A function game5 starts calling that is not in `libm.hpp` is vendored from the same
  upstream, with its golden table, before the code that calls it.
- `engine/core/num.hpp` is JavaScript's number semantics (`Math.round`, `|0`, `>>>0`,
  `Math.imul`, `Math.max`/`min`, `Math.sign`, `%`) and `num_text.hpp` is `toFixed` and
  `String(n)`, so the mountain's diagnostic lines read as game5's do. `README.md` § JavaScript
  semantics that bite lists the traps.

## testing

- The golden generators (`parity/*.mjs`) import the reference `.ts` sources directly (Node
  with `--experimental-strip-types`), loaded through `parity/oracle-import.mjs` (it resolves
  the extensionless relative imports). Anything that reaches a transcendental goes in a script
  that calls `requireOracleNode` (`parity/oracle-node.mjs`) and runs under `make golden-libm`,
  and its table's first line records the Node version.
- A behaviour game5's own tests assert (`../game5/tests/`) gets the same assertion in C++.

## sim

- Read `../game5/AGENTS.md` (its "Stated once" list is the map of where every answer lives)
  and `../game5/docs/architecture.md` before porting a system.
- The synthetic levels are game5's `tests/support/synthetic.ts` (`syntheticLevel`,
  `flatLevel`); port them before the skier.
- The order of systems inside `step` is game5's, exactly. The `sim` CLI prints game5's
  `RunReport` table, and its digests match game5's `make sim`.
- `defs/` mirrors game5's `TUNING`, `SKIS` and `RACE`; the run clock is the framework's
  `loop/run-clock` (`MAX_FRAME_SECONDS`).

## render

The references, by pass: sky and haze `pwa/src/game/sky.ts`, `haze.ts` and the baked
`sky-table.ts`; terrain `terrain.ts`, the snow shader `snow-glsl.ts`, the grooves
`trail-map.ts` and `trail-stamp.ts`; shadows `shadow-box.ts`; the forest `tree-shapes.ts` and
`tree-bands.ts`; the figures `skier-*.ts` and `skier-dress.ts`, and the skis' glTFs in
`pwa/models/`. To compare a pass, run game5's `make world SEED=38` in `../game5` (read-only)
and the engine's screenshot tool on the same views.

## gpu

Device creation follows game5's `gpu-device.ts`. game5's TSL/GLSL is the reference for each
look: port the maths and keep the names.

## ui

game5's UI is Preact DOM. The HUD press and thumb guard are the framework's
`input/hud-press` and `input/thumb-guard`, the menu cursor `input/menu-cursor`. The strings
table is game5's `strings*.ts`, the surfaces and the pause rule `shell.ts`, the settings'
merge `mergeSettings`. The developer page is game5's; its reference viewports are game5's.

## platform

The default keys are game5's `DEFAULT_KEYS`, rumble `rumble.ts`, the screen-to-engine signs
`input-model.ts`. The save directory replaces game5's `localStorage` keys, one file per key,
with the same names.

## audio

`engine/audio/` ports the framework's `src/audio/` (voice, synth, rack, play, types, view);
`synth.ts` builds WebAudio graphs, and the DSP graph keeps WebAudio's biquad formulas (the
Audio EQ Cookbook), `envelopeShape` and its 128-frame render quantum. Fall Line's sound ports
game5's `pwa/src/game/audio/`, and the faders are `mixOf`. A sound is checked against a
headless Chromium `OfflineAudioContext` render of the same `SoundDef` (a script here).

## perf

For a reference number, run game5's `make cpu-cost` or `make sim` in `../game5` on the same
machine. The early-out on the nearest ring is the framework's `blockOffsets`.
