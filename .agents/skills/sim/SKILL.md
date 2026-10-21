---
name: sim
description: "Use for games/fall_line/sim (Fall Line's own simulation): create_game and the 120 Hz step's order, the modes, the course, the field, the bot, the headless sim CLI, and engine/loop's run clock. The mountain generator is the mountain skill's."
---

# The Fall Line simulation

`games/fall_line/sim/` is Fall Line's own simulation:
headless, deterministic and platform-free. It may include only `engine/core`, `engine/loop`,
the plugins it links and itself. What another game of the genre would reuse goes in a plugin
instead (the `plugins` skill): the generator is `plugins/mountain` (the `mountain` skill), the
tape and standings `plugins/racing`, and the skier on the snow `plugins/skier` (the `skier`
skill: `step_skier`, the snow, the limits a bot plans on, the ride scenarios).

## What it is

- `generate_level(seed, opts)` → `Level`: the heightfield (`ground`, `packed`), the open
  piste (`track`, a point every 2 m), the `checkpoints` (start gate through finish),
  the spawn and grid, the trees, kickers, the sun and the weather. A map is a pure
  function of its seed: each attempt draws off `sub_seed(seed, attempt)` and is
  accepted only if `analyze_level` finds it clean.
- `create_game(opts)` → `GameState`, and `step(state, input)` advances exactly one
  1/120 s step and leaves its events on `state.events`.
- `sim/` (the bot and `simulate`) drives the same `step` headlessly.

While porting, `porting/subsystems.md` § sim says what to read in the reference before a
system is carried over.

## Order of work (docs/goals.md M4 → M5)

1. `mapgen`: the `mountain` skill (M4a–M4d).
2. The skier on the snow: `plugins/skier` (M5a, done), held by its scenarios on the hand-built
   maps (`tests/skier_scenarios_test.cpp`).
3. `game`: classify each file first (the `plugins` skill), then the state, the course, the
   step, held by a per-step state hash on generated maps.
4. `sim`: the bot and `simulate`, which gives the `sim` CLI (`tools/sim/`) printing the
   `RunReport` table. Its digests are golden.

## Rules

- The order of systems inside `step` is fixed and part of the digest.
- No allocation in `step`. `GameState` holds fixed-capacity storage sized at create.
- Tuning numbers live in `defs/` tables (`tuning`, `skis`, `race`), with their units.
- The renderer and the app never write `GameState`.
- `engine/loop/run_clock.hpp` is the run clock: the clamp (`kMaxFrameSeconds` = 0.1),
  whole steps, and `alpha` for interpolation only.

## Labs

`run-clock` covers the run clock (the generator's labs are the `mountain` skill's). M5 lands
the `sim` drive lab (the bot over seeds), the `ride` scenario lab and `play` (an agent over
stdio); their design is `docs/labs.md`. The `labs` skill owns how.
