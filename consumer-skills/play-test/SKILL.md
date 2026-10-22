---
name: play-test
description: "Use to test this game by PLAYING it without a screen: the play contract a game fills (play/game.cpp: its axes, options, observe, state, report, pilot), ge play's line protocol, ge sim's pilot runs, the input log, ge replay to a digest, ge state at a step, and ge diverge to find the step two runs part. Load it to add an axis or an option, to make a replay hold, or to chase a determinism break."
---

# Play-testing the game

The game is tested by playing it. Its `play` executable (`play/main.cpp` and `play/game.cpp`)
registers the game on the engine's **play contract**, and `ge` drives it: an agent plays over a
line protocol, the game's own pilot plays whole runs, and every run is written down as an
**input log** that replays to the same digest. Nothing here needs a window.

## The contract (`play/game.cpp`, the engine's `play/game.hpp`)

A `GameSpec` names the game, its step rate, its longest run, its **axes** (the whole input, one
number each: `signed_unit` -1..1, `lever` 0..1, `toggle` held, `press` taken once a hold) and its
**options** (what a run is set up with, as `--<name>=<value>`). `open` makes a `Run`:

| `Run::` | What it gives | Who reads it |
| --- | --- | --- |
| `step(input)` | one fixed step on one snapped value per axis | everything |
| `over()` | nothing is left to play | the protocol, `ge sim` |
| `digest()` | a hash folded over every step's state | `ge replay`, `ge diverge` |
| `observe()` | what a player sees now (JSON object), events since the last call | an agent |
| `state()` | everything that decides the next step (JSON object of scalars) | `ge state`, `ge diverge` |
| `report()` | the run's result (JSON object) | `ge sim`, `ge replay`, `end` |
| `pilot(input)` | the game's bot: fills the axes, returns true | `ge sim`, the `pilot` op |

Rules:

- **The axes are the whole input.** Anything a run reads that is not its seed, its options or its
  axes (a clock, an unseeded random number, an uninitialised field) breaks the replay. The engine
  snaps every value onto its axis's grid before `step`, so do not snap again in the game.
- **`state()` lists every field `step` reads**, in full precision (`num::to_string`, not
  `to_fixed`): `ge diverge` can only name the fields it is shown.
- **`observe()` is for deciding, `state()` is for comparing.** Keep the observation small and in
  the player's terms (where the goal is from here, not the whole world).
- **A new axis or option** is a row in the spec's arrays. It changes the input log's `axes` line,
  so old logs no longer `fit`: say so in the commit.

## The loop

```sh
ge sim --seeds=1,2,3            # the pilot plays each seed; runs/<game>-<seed>.inputs each
ge replay runs/<game>-1.inputs  # played back: must reach the digest it recorded
ge play --seed=1                # you at the controls, one JSON line each way (below)
ge state runs/<game>-1.inputs --step=600   # the whole state after 600 steps
ge diverge runs/a.inputs runs/b.inputs     # where two runs part, and which fields differ
```

**The protocol** (`ge play`): the session opens with `hello` (the axes, the options, the ops).
Then send one object a line: `{"op":"observe"}`; `{"op":"act","input":{"<axis>":v},"steps":n}`
(holds the input n steps, answers the observation after them); `{"op":"pilot","steps":n}` (the
game's pilot plays n steps); `{"op":"state"}`; `{"op":"inputs"}` (the log so far); `{"op":"end"}`
(the report and the digest). A bad line answers `{"op":"error","error":...,"next":...}`.
Script a session by piping lines in: `printf '{"op":"observe"}\n' | ge play --seed=1`.

**The input log** is plain text: a header (`game`, `seed`, `option`, `hz`, `axes`, `steps`,
`digest`), then one line per run of held steps, `<count> <value per axis>`. Edit it by hand to ask
"what if": copy a log, change one line, drop the `digest` line, and `ge diverge` the two.

## Chasing a determinism break

1. `ge replay` reports a digest that differs from the recorded one, or a test's golden moved.
2. Record the same inputs twice (`ge sim` twice, or one log replayed) and `ge diverge` them: the
   `state_step` is the first step after which they part, the fields are what differs.
3. Read what `step` does to those fields at that step: the cause is an input it reads that is not
   in the log (skill: `sim-system`, determinism).
4. Fix it, rerun `ge replay` on the original log (an honest change of behaviour rewrites its
   digest: say so in the commit), then `ge check`.
