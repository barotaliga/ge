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
  untraced and holds the digest equal (`tests/fall_line_why_test.cpp`).
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
| `{"op":"why"}` in `ge play` | What the pilot would do now and why, without stepping: an agent asks before taking over |
| `--log=why=debug` on a lab | Each decision as a log line, by step, beside every other channel |

**The pilot is asked at every step of the replay.** On a log the pilot played (`ge sim`) that
is what it did. On a log a person or an agent played it is what the bot WOULD have done, and the
answer names the first step it would have played otherwise (`first_disagree`): the quickest way
to see where a bot and a person part.

## The loop: a bot that behaves more like a person

1. Run the bot's labs with `--tag=before` (Fall Line: `make sim`, `make lab LAB=free`).
2. Find a robotic moment: a flagged metric, a line too perfect, a stop too late. `ge why --flips`
   on that seed lists where the bot changed its mind; `--timeline` and `ge graph` show the facts
   it chose on round there.
3. Read the decision at that step (`ge why --step=S --around=12`): which term settled it, how
   close the call was.
4. Change the bot (its decisions) or, once M16b lands, its hands (how a decision becomes
   controls: a reaction delay, a slew, a little noise), never both in one round.
5. Run with `--compare=before`, `ge graph` both timelines on one chart, put both in the PR.

## What lives where

| Piece | Home | Status |
| --- | --- | --- |
| The decision record and its forms | `engine/ai/why.hpp` | built (M16a) |
| `ge why`'s engine half: replay with the trace, timeline, flips | `engine/play/why.hpp`, `play why` in `main.cpp`, `ge why` in `engine/cli/play.cpp` | built (M16a) |
| Humanness metrics off an input log, the hands | `engine/ai/human.hpp`, `hands.hpp`, the `humanness` lab | M16b |
| A skier's pilot (pure pursuit, the speed plan on the limits) | `plugins/skier/pilot.hpp` | M16c |
| Paths (one heap, grid and graph search), thinking on a schedule | `engine/ai/path.hpp`, `think.hpp` | M16d, with the free ride's lifts |
| Sight and memory, steering, choosing (utility, state machines) | `engine/ai/sight.hpp`, `memory.hpp`, `steer.hpp`, `choose.hpp` | M16e, with the army on foot |

A piece lands with its first user, never before (G4). A bot a game writes before its piece exists
still records its decisions, and moves onto the piece when it lands.
