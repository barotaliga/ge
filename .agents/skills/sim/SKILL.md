---
name: sim
description: "Use for games/fall_line/sim (Fall Line's own simulation): create_game and the 120 Hz step's order, the modes, the free ride, the course, the field, the bot, the headless sim CLI, and engine/loop's run clock. The mountain generator is the mountain skill's."
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
| `free.hpp/.cpp` | THE FREE RIDE (`Mode::free`): `free_map` (the ski area), `run_tracks` (its runs as lines, each index built at create), `create_free_ride` (the player alone at a run's head), `step_free` (the metres, the run under the skis, `onto`, the foot's `arrived`), `reset_free` (down the run he was last on), `run_chain` (a run and every run it flows into, to a foot) |
| `controls.hpp` | The control tape's schema (steer, tuck, brake, flags) and `snap_controls` |
| `bot.hpp/.cpp` | `plan_bot` and `plan_free` (the line a plan is laid on, `BotPlan::track`: the course's piste, or a run's chain on the free ride; the speed plan off `skier/limits.hpp`, and on a course whose slalom gates stand off the line the weave through them) and `bot_controls` (pure pursuit of the racer's lane about the weave, brake over the plan at his pace, tuck under it) |
| `field.hpp/.cpp` | The rivals: `deal_field` (each rival's pace off a stream of the map's seed, his lane off his grid slot), `meet_racers` (skier against skier: the shoulder), `standings` and `place_of` (off `plugins/racing`) |
| `simulate.hpp/.cpp` | `Driver` (THE ONE STEP every pilot drives: snapped, taped, the rivals' bots, `step`, hashed), `simulate`, `simulate_on`, `simulate_free`, `replay_tape`, the `RunReport` and its `FieldRow`s |
| `wanted.hpp/.cpp`, `wanted_state.hpp`, `wanted_rules.hpp` | THE WANTED LEVEL: `raise_heat` (the offence hook), `step_wanted` (the look, the climb, the search running out), `sees_quarry` and `through_woods` (the sight line), `under_cover`, `arrest_quarry`, `roster_of`, `ground_wave`; the state `GameState::wanted` (inactive and unhashed until raised); every number in `wanted_rules.hpp` |
| `play.hpp/.cpp` | `PlaySession`: an agent at the controls over JSON lines (`hello`, `observe`, `act`, `tape`, `end`); THE PROTOCOL is stated once in its header |

The generator is `plugins/mountain` (the `mountain` skill); a map is a pure function of its
seed. The skier on the snow is `plugins/skier` (the `skier` skill).

## Order of work

1. The skier on the snow: `plugins/skier` (M5a, done).
2. The run: the state, the course, the step, the bot, `simulate` and the `sim` lab (M5b, done).
3. The field and an agent at the controls: rivals on one grid, skier against skier, the
   standings off `plugins/racing`, and the `play` lab over stdio (M5c, done).
4. The free ride's run (M13a1, done): a run's mode, the run under the skis, the foot, the
   reset, the bot down a run's chain. The game opens PLAY on the free ride; its races and
   tricks stay behind the developer switch, kept and not grown.
5. The wanted level (M15a, done): the hunt's state and rules, scripted pursuers in the `wanted`
   lab until the army's pilots (M15c–M15e) write them.
6. The lifts (M13a2, done): the carriers off `plugins/mountain/lifts`, boarding in the load
   zone, the ride, let go at the top (`lift_ride.hpp`); the bot's outing (`outing.hpp`).
7. Next: the army on foot (M15c), which rides the lifts; then the free ride's other systems, a
   row each (M13+).

## Rules

- **The order of systems inside `step` is stated once in `game.hpp` and is part of every
  digest.** A change to it, to the course, to the bot or to the skier moves
  `tests/golden/fall_line_sim.txt`: run `make sim` before and after (`ARGS=--tag=before`, then
  `--compare=before`), rewrite the golden with
  `GE_GOLDEN_WRITE=1 build/release/tests/fall_line_sim_test` (and `fall_line_field_test` for
  the field), and put both tables in the PR (`make sim ARGS=--rivals=3` for the field's).
- **The bot plans on the skier's limits** (`skier/limits.hpp`), never on a ceiling of its own;
  its own numbers are `bot::` constants in `bot.hpp`, each with its unit.
- **The bot records why** (`mode`, `line`, `speed`) on the `ge::ai::Why*` it is handed, and
  decides the same without it (the `bots` skill): a new choice in it records its options too.
  `make lab LAB=why` and `ge why <log>` read them; `tests/fall_line_why_test.cpp` holds them.
- **The bots press through their HANDS** (`hands.hpp`: `BotHands`, a profile per run, `none`
  by default so no digest moves; `human` a person's delay, spring, slew and wander), recording
  what they meant first (the `hands` decision). Late hands steer from further ahead
  (`bot_controls`' `lead_s`). `make lab LAB=humanness` and `ge sim --human -- --hands=human`
  measure them; `tests/fall_line_hands_test.cpp` holds them.
- **The play contract's views** are `near`, `map` and `bot` (the pilot's plan, its read stretch
  and its aim over `near`, off a decision record of its own; `ge shot --view=bot`).
- **Every pilot drives through `Driver::step`** (the bot, a tape, an agent): the player's
  controls are snapped onto the tape's grid before `step` sees them, so a tape played back
  (with the same number of rivals) is the run again. Only the player is taped: a rival's
  controls are a pure function of the run.
- **The play protocol is stated once**, in `play.hpp`'s header, and mirrored in
  `docs/labs.md` § An agent at the controls. An observation carries only what a player could
  know. A new op or field changes both and `tests/fall_line_play_test.cpp`.
- No allocation in `step`: the racers are sized at create, the events reserved, a free ride's
  `skied` reserved for every run and every run's nearest-point index built at create, and its
  lifts planned at create (`GameState::lifts`).
- **A lift that has him carries him** (`lift_ride.hpp`): his skis, the run under them and the
  run's own resets wait while he is on it, and the bot's hands are off. The carriers are the
  mountain plugin's (`plugins/mountain/lifts/lift_loop.hpp`), a pure function of the clock;
  the ride stores only which carrier has him, never where it is.
- **A race's digest reads nothing of the free ride**: `state_hash` mixes `FreeRide` only when
  the mode is `free`, and a race never steps it, so a free-ride change moves only
  `tests/golden/fall_line_free.txt` and the `free` lab. Run `make lab LAB=free ARGS=--seeds=1,2,3,4,5,6,7,8,38`
  before and after any change to the free ride, the bot or the skier, and rewrite that golden
  with `GE_GOLDEN_WRITE=1 build/release/tests/fall_line_free_test`. The test and the lab are
  pinned to generator v10, so a new generator version moves neither.
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

`free` (drive, `games/fall_line/labs/free_lab.cpp`): the bot on the free ride down every piste
of each seed's ski area (`--seeds=38` by default, `--most=N` the first N), from its head to a
foot: the runs table and the ski area with every line drawn. A piste not skied to a foot fails
the lab. `--outings=N` (1 by default) rides the bot's outing from the first N pistes with a lift
at their foot (`simulate_outing`): the outings table (the lift, the run after it, the wait, the
ride) and his line, white while a lift carries him. An outing not ridden through fails the lab;
`tests/golden/fall_line_outing.txt` holds seed 38's (`GE_GOLDEN_WRITE=1
build/release/tests/fall_line_lift_ride_test`).

`lift-ride` (drive, `games/fall_line/labs/lift_ride_lab.cpp`): every lift of each seed ridden
from its load zone to its top through the real `step`: the wait, the ride, the slowest and
fastest he is carried, the biggest carried step and the let-go's distance from its point. A lift
that keeps him, a carried step over 0.5 m or a let-go off its point fails the lab. Run it, with
the `free` lab's outings, before and after a change to `lift_ride.cpp`, `outing.cpp` or the lift
loop.

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
