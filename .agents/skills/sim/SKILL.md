---
name: sim
description: "Use for games/fall_line/sim (Fall Line's own simulation): createGame and the 120 Hz step's order, the modes, the course, the field, the bot, the headless sim CLI, and engine/loop's run clock. The mountain generator is the mountain skill's."
---

# The Fall Line simulation

`games/fall_line/sim/` is Fall Line's own simulation:
headless, deterministic and platform-free. It may include only `engine/core`, `engine/loop`,
the plugins it links and itself. What another game of the genre would reuse goes in a plugin
instead (the `plugins` skill): the generator is `plugins/mountain` (the `mountain` skill), the
tape and standings `plugins/racing`, and M5's skier on snow is expected to be `plugins/skier`.

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

1. `mapgen`: the `mountain` skill (M4a–M4d).
2. `game`: classify each file first (the `plugins` skill), then state, then skier physics (`skier`, `suspension`, `snow`, `poles`,
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
