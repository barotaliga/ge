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

`games/fall_line/sim/` (`fall_line::sim`, target `fall_line_sim`, linked by the labs and the
tests):

| File | What |
| --- | --- |
| `state.hpp` | `GameState` (the map, the trees' index, the pair, the racers, this step's events, the clock), `Racer`, `Rider` (a bot's pace and lane), `Progress`, `Controls`, the events; `kHz` = 120 |
| `game.hpp/.cpp` | `race_map(seed)` (v1, the single piste), `create_game`, `create_game_on`, `step` (THE ORDER of a step, stated once in the header), `state_hash` |
| `course.hpp/.cpp` | The gates in order, a miss, the finish's clock inside its step, the reset (`reset_racer`), `bearing_to_next` |
| `controls.hpp` | The control tape's schema (steer, tuck, brake, flags) and `snap_controls` |
| `bot.hpp/.cpp` | `plan_bot` (the speed plan off `skier/limits.hpp`, and on a course whose slalom gates stand off the line the weave through them) and `bot_controls` (pure pursuit of the racer's lane about the weave, brake over the plan at his pace, tuck under it) |
| `field.hpp/.cpp` | The rivals: `deal_field` (each rival's pace off a stream of the map's seed, his lane off his grid slot), `meet_racers` (skier against skier: the shoulder), `standings` and `place_of` (off `plugins/racing`) |
| `simulate.hpp/.cpp` | `Driver` (THE ONE STEP every pilot drives: snapped, taped, the rivals' bots, `step`, hashed), `simulate`, `simulate_on`, `replay_tape`, the `RunReport` and its `FieldRow`s |
| `wanted.hpp/.cpp`, `wanted_state.hpp`, `wanted_rules.hpp` | THE WANTED LEVEL: `raise_heat` (the offence hook), `step_wanted` (the look, the climb, the search running out), `sees_quarry` and `through_woods` (the sight line), `under_cover`, `arrest_quarry`, `roster_of`, `ground_wave`; the state `GameState::wanted` (inactive and unhashed until raised); every number in `wanted_rules.hpp` |
| `play.hpp/.cpp` | `PlaySession`: an agent at the controls over JSON lines (`hello`, `observe`, `act`, `tape`, `end`); THE PROTOCOL is stated once in its header |

The generator is `plugins/mountain` (the `mountain` skill); a map is a pure function of its
seed. The skier on the snow is `plugins/skier` (the `skier` skill).

## Order of work

1. The skier on the snow: `plugins/skier` (M5a, done).
2. The run: the state, the course, the step, the bot, `simulate` and the `sim` lab (M5b, done).
3. The field and an agent at the controls: rivals on one grid, skier against skier, the
   standings off `plugins/racing`, and the `play` lab over stdio (M5c, done).
4. The wanted level (M15a, done): the hunt's state and rules, scripted pursuers in the `wanted`
   lab until the army's pilots (M15c–M15e) write them.
5. Next: the modes and their rules (the race formats) arrive with the app's cards (M10); the
   ski area's maps (M4b) are raced once `race_map` is asked for v8 and up.

## Rules

- **The order of systems inside `step` is stated once in `game.hpp` and is part of every
  digest.** A change to it, to the course, to the bot or to the skier moves
  `tests/golden/fall_line_sim.txt`: run `make sim` before and after (`ARGS=--tag=before`, then
  `--compare=before`), rewrite the golden with
  `GE_GOLDEN_WRITE=1 build/release/tests/fall_line_sim_test` (and `fall_line_field_test` for
  the field), and put both tables in the PR (`make sim ARGS=--rivals=3` for the field's).
- **The bot plans on the skier's limits** (`skier/limits.hpp`), never on a ceiling of its own;
  its own numbers are `bot::` constants in `bot.hpp`, each with its unit.
- **Every pilot drives through `Driver::step`** (the bot, a tape, an agent): the player's
  controls are snapped onto the tape's grid before `step` sees them, so a tape played back
  (with the same number of rivals) is the run again. Only the player is taped: a rival's
  controls are a pure function of the run.
- **The play protocol is stated once**, in `play.hpp`'s header, and mirrored in
  `docs/labs.md` § An agent at the controls. An observation carries only what a player could
  know. A new op or field changes both and `tests/fall_line_play_test.cpp`.
- No allocation in `step`: the racers are sized at create and the events reserved.
- Nothing in a step draws a random number: the field's only draws (the rivals' paces) are
  made once, in `deal_field`, off a stream of the map's seed. The renderer and the app never
  write `GameState`.
- The field is at most the grid's slots (`create_game_on` caps it), the player first.
- `engine/loop/run_clock.hpp` is the run clock: the clamp (`kMaxFrameSeconds` = 0.1),
  whole steps, and `alpha` for interpolation only.

## Labs

`sim` (drive, `games/fall_line/labs/sim_lab.cpp`): the bot over seeds (`make sim SEEDS=38`, the
reference seeds by default), the run report table and each run's line drawn on its map; a seed
not finished fails the lab. It is Fall Line's headless sim CLI; `--rivals=3` puts a field on
the grid and adds the field table (every racer's pace, lane, place and shoulders).

`wanted` (scenario, `games/fall_line/labs/wanted_lab.cpp`): the hunt on seed 38 against scripted
pursuers (`shadow`, `posts`, `arrest`), the hunt table, each line coloured by the stars, and the
sight sheds of a soldier and a helicopter. A change to `wanted.cpp` or its numbers moves
`tests/golden/fall_line_wanted.txt` (`GE_GOLDEN_WRITE=1 build/release/tests/fall_line_wanted_test`)
and this lab's digest: put its hunt table before and after in the PR.

`play` (drive, `games/fall_line/labs/play_lab.cpp`): an agent at the controls.
`build/debug/labs play --quiet --stdio --seed=38 --rivals=3` puts the protocol on stdio;
`--script=<file>` plays a file's lines; with neither, the bot plays through the protocol's own
text and its tape must replay to the session's digest. `run-clock` covers the run clock, `ride`
the skier (the `skier` skill). The `labs` skill owns how.
