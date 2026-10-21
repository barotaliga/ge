---
name: sim
description: "Use for games/fall_line/sim (the port of game5's engine/): mapgen and generateLevel, createGame and the 120 Hz step, the skier, the course, the field, the bot, the headless sim CLI, and engine/loop's run clock."
---

# The Fall Line simulation

`games/fall_line/sim/` is game5's `engine/` in C++: headless, deterministic and
platform-free. It may include only `engine/core`, `engine/racing` and itself.

## What it is (from game5)

- `generate_level(seed, opts)` → `Level`: the heightfield (`ground`, `packed`), the open
  piste (`track`, a point every 2 m), the `checkpoints` (start gate through finish),
  the spawn and grid, the trees, kickers, the sun and the weather. A map is a pure
  function of its seed: each attempt draws off `sub_seed(seed, attempt)` and is
  accepted only if `analyze_level` finds it clean.
- `create_game(opts)` → `GameState`, and `step(state, input)` advances exactly one
  1/120 s step and leaves its events on `state.events`.
- `sim/` (the bot and `simulate`) drives the same `step` headlessly.

Read `../game5/AGENTS.md` (its "Stated once" list is the map of where every answer
lives) and `../game5/docs/architecture.md` before porting a system.

## Order of work (docs/goals.md M4 → M5)

1. `mapgen` bottom-up: types, query (nearest track point), the compile step, then
   generate. Parity is checked by a structure dump plus `levelDigest` on
   `LEVEL_SEEDS` and the pinned maps.
2. `game`: state, then skier physics (`skier`, `suspension`, `snow`, `poles`,
   `flight`, `chassis`), then collision, course, run and step. Parity is a per-step
   state hash on synthetic levels (port `tests/support/synthetic.ts` first) and then
   on generated ones.
3. `sim`: the bot and `simulate`, which gives the `sim` CLI (`tools/sim/`) printing the
   oracle's `RunReport` table. Its digests match game5's `make sim`.

## Rules

- The order of systems inside `step` is the oracle's, exactly. It is part of the
  digest.
- No allocation in `step`. `GameState` holds fixed-capacity storage sized at create.
- Tuning numbers live in `defs/` tables mirroring `TUNING`, `SKIS` and `RACE`, with
  their units.
- The renderer and the app never write `GameState`.
- `engine/loop/run_clock.hpp` is the port of `loop/run-clock`: the clamp
  (`MAX_FRAME_SECONDS` = 0.1), whole steps, and `alpha` for interpolation only.
