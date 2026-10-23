---
name: bots
description: "Use for anything a bot DECIDES in gameengine2 or a game on it: engine/ai (the decision record ai::Why every bot writes, and the pieces bots share as they land: the hands, thinking on a schedule, sight and memory, paths, steering, choosing), a game's Run::trace hook, ge why and the play protocol's why op, and the loop that makes a bot behave more like a person. Load it before writing or changing a bot, a pilot, a rival or a pursuer, and to find out why one did what it did."
---

# Bots

A bot is the game's stand-in for a player: it reads what a player could see and writes the
controls a player would. Every game has them (a pilot, the rivals, a crowd, pursuers), and the
engine's half is `engine/ai/` (`ge::ai`, target `ge_ai`): what every bot shares, so no game
writes it twice. The roadmap is `docs/goals.md` § M16.

**Read this skill's lessons first**: `make lessons ARGS=bots`. Load `sim` (or the game's own
skill) beside it for the bot itself, `debug` for the log and the charts, `agent-interface` for
`ge why`.

## The rule: every decision is traceable

A bot that chooses records WHY, where it chooses, on an `ai::Why*` it is handed
(`engine/ai/why.hpp`):

```cpp
Controls bot_controls(const GameState& s, int32_t i, const BotPlan& plan, ge::ai::Why* why) {
  ...
  if (why) {
    why->decide("speed");               // a decision: one name, stable across steps
    why->fact("cap", cap);              // what it read and planned on, by name
    why->option("brake", v - cap - 0.5);  // each option weighed, with its score
    why->term("over_plan", v - cap);      //   and the named parts of that score
    why->option("tuck", cap - 1 - v);
    if (turning) why->bar("turning");     //   or why it was ruled out
    why->option("glide", 0);
    why->choose(chosen, because);       // the bot's own choice, and what settled it
  }
```

- **Off is a null pointer, and recording decides nothing.** Compute the choice first, record
  it after; never branch on whether a trace is on. A game's test replays a field traced and
  untraced and holds the digest equal (`tests/fall_line_why_test.cpp`; a machine's pilot the
  same, `tests/helicopter_scenarios_test.cpp` and `tests/fall_line_heli_test.cpp`).
- **A plugin's pilot takes `ai::Why*` last, defaulted to null**, and its game passes
  `why->follow(agent, step)` down from its own decision: the helicopter's `heli_pilot` records
  `way` (the four speed limits, the slowest chosen) and `height` (landed, settle, climb, sink or
  hold), and Fall Line's heli outing its `outing` (board, fly, drop, ski down) above them.
- **Names are string literals** (the record keeps the pointer), snake_case, stable across steps:
  `ge why --flips` compares choices by name, and the timeline makes a column of each fact.
- **Score options so the best one wins.** Where the bot's rule is a chain of ifs, score each
  option by how far past its threshold it is, so the chosen one is the highest and the MARGIN
  reads as how close the call was. A guard or a continuous output (a steer) is a decision with
  facts and a choice and no options.
- **Agent ids**: 0 is the game's pilot (the player's stand-in), then whatever the game's other
  bots are (Fall Line: each rival's racer index). `Why::follow(agent, step)` hands out the
  recorder or null; pass the result straight to the bot.
- A game opts in with `Run::trace(ai::Why*)` (`engine/play/game.hpp`): store the pointer, hand
  it to the pilot and to every other bot, return true.

## Reading why a bot did it

`ge why <log>` (the play executable's `why`, `engine/play/why.hpp`) replays an input log with
the trace on. Nothing is kept in a normal run: a run replays to the bit, so the records are
rebuilt when asked for.

| Command | What it answers |
| --- | --- |
| `ge why <log> --step=S --around=K` | Agent 0's decisions at steps S-K..S+K: what it saw, every option's score and terms, the choice and its margin |
| `ge why <log> --agent=N --decision=speed` | One bot, one decision |
| `ge why <log> --flips [--close=X]` | Every step it changed its mind over the whole run, and the close calls under margin X: dithering and odd choices are there |
| `ge why <log> --timeline` | `runs/<log>-why-<agent>.tsv`: one row a step, each decision's choice, choice index, margin and facts; `ge graph` it |
| `ge shot <log> --step=S --view=bot` | Fall Line: the pilot's mind drawn at a step: its line coloured by the plan's speed, the stretch it reads its cap off, the point it aims at; the caption its line and speed calls (the aim is the `line` decision's `aim_x`, `aim_z`) |
| `{"op":"why"}` in `ge play` | What the pilot would do now and why, without stepping: an agent asks before taking over |
| `--log=why=debug` on a lab | Each decision as a log line, by step, beside every other channel |

**The pilot is asked at every step of the replay.** On a log the pilot played (`ge sim`) that
is what it did. On a log a person or an agent played it is what the bot WOULD have done, and the
answer names the first step it would have played otherwise (`first_disagree`): the quickest way
to see where a bot and a person part.

## The hands: how a decision becomes controls

A person does not press what they decide. `ai::Hands` (`engine/ai/hands.hpp`) turns what a bot
WANTS into what it PRESSES, per axis (`ai::AxisHands`), in this order: a reaction delay (a fixed
ring, up to 0.8 s), a slow wander off what was meant (faded near an end and near rest, so a hand
let go or pressed to a stop sits still), a damped spring (`settle_s`, `damping`: under 1
overshoots a quick move), a slew limit (travel a second), the reach (`lo`, `hi`). A button is
`pass`. Everything is fixed-size and copies cheaply, its noise off its own `core::Rng` stream.

- **The bot records its intent before its hands**: `ai::record_intent(why, names, wanted)`
  writes a decision called `hands`, one fact an axis by the axis's name. The humanness report
  measures the lag against it.
- **A game names its profiles** (Fall Line: `games/fall_line/sim/hands.hpp`, `none` and
  `human`; `--hands=` on the play contract, `SimOptions::hands` in the sim). The default profile
  presses what the bot wants, so no digest moves until a run asks for hands.
- **A pilot asked off the step works on a COPY of its hands** (`ge why` asks the pilot at every
  step, the protocol's `pilot` op asks without stepping): pilot() presses on a copy, and the
  step commits it only when it was asked at that step. Otherwise asking moves the run.
- **Late hands need a bot that looks further ahead.** The bot steers from where it will be when
  its hands catch up (`BotHands::lead()`, Fall Line's `bot_controls(..., lead_s)`): without it a
  0.2 s delay turns a pursuit into a limit cycle from one end of the stick to the other.

## How human it looks

`ai::axis_humanness` and `ai::decision_humanness` (`engine/ai/human.hpp`) measure a run;
`kHumanBands` holds each metric to a person's band, every band with the assumption it rests on.
`check_axis` / `check_decision` list what is outside as `OutOfBand`.

| Metric | What it is | A person's band |
| --- | --- | --- |
| `saturated` | the share of the steps in use spent at an end of the travel | a stick 0..0.35 |
| `jumps` | steps that cross more than half the travel at once | 0..0.01 |
| `reversals_min` | changes of direction a minute (0.05 hysteresis) | 0..120 |
| `hold_s` | how long a stick sits part-way (a time-weighted median) | a stick 0.05..3 s |
| `lag_s` | the delay at which the pressed best follows the wanted (`hands`) | 0.15..0.4 s |
| `flips_min` | a decision's changes of mind a minute | 0..20 |

`ge sim --human` (the play executable's `sim --human`, `engine/play/human.hpp`) prints them for
every run and warns on each one out of band; it reads a person's log the same way (the wanted is
then what the bot would have meant there). A signed axis is a stick, a lever a trigger, a button
is left out. Fall Line's `humanness` lab sweeps seeds with a field of four, every profile side by
side, and charts the steer meant against pressed.

## The loop: a bot that behaves more like a person

1. Run the bot's labs with `--tag=before` (Fall Line: `make sim`, `make lab LAB=free`,
   `make lab LAB=humanness`), and `ge sim --human` on the seed.
2. Find a robotic moment: a flagged metric, a line too perfect, a stop too late. `ge why --flips`
   on that seed lists where the bot changed its mind; `--timeline` and `ge graph` show the facts
   it chose on round there.
3. Read the decision at that step (`ge why --step=S --around=12`): which term settled it, how
   close the call was.
4. Change the bot (its decisions) or its hands (how a decision becomes controls: the delay,
   the spring, the slew, the wander), never both in one round. An axis out of band is the
   hands'; a decision flipping is the bot's.
5. Run with `--compare=before`, `ge graph` both timelines on one chart, put both in the PR.

## What lives where

| Piece | Home | Status |
| --- | --- | --- |
| The decision record and its forms | `engine/ai/why.hpp` | built (M16a) |
| `ge why`'s engine half: replay with the trace, timeline, flips | `engine/play/why.hpp`, `play why` in `main.cpp`, `ge why` in `engine/cli/play.cpp` | built (M16a) |
| The hands, the humanness metrics and bands | `engine/ai/hands.hpp`, `human.hpp` | built (M16b) |
| The report off a log, `ge sim --human` | `engine/play/human.hpp`, `sim --human` in `main.cpp`, `ge sim` in `engine/cli/play.cpp` | built (M16b) |
| A skier's pilot (pure pursuit, the speed plan on the limits) | `plugins/skier/pilot.hpp` | M16c |
| Paths (one heap, grid and graph search), thinking on a schedule | `engine/ai/path.hpp`, `think.hpp` | M16d, with the free ride's lifts |
| Sight and memory, steering, choosing (utility, state machines) | `engine/ai/sight.hpp`, `memory.hpp`, `steer.hpp`, `choose.hpp` | M16e, with the army on foot |

A piece lands with its first user, never before (G4). A bot a game writes before its piece exists
still records its decisions, and moves onto the piece when it lands.
