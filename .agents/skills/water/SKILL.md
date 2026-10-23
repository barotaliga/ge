---
name: water
description: "Use for water and wind in gameengine2 or a game on it: engine/wind (the wind as a pure function: the mean, gusts and veer, the log-law profile, shelter, local sources such as a downwash, a whirl or a blast), engine/water as its rows land (bodies of water and the one sample query, waves off the wind and the fetch, floating, swimming and its per-game presets, the colour from what is in the water, flowing water), the water's render passes and sound, and their labs. Load it before adding water or wind to a game, or changing how either behaves."
---

# Water and wind

The design is [`docs/water.md`](../../../docs/water.md) and the roadmap is `docs/goals.md`
§ M24 (one status file a row, `docs/goals/M24<x>.md`). This skill is the procedure; read the
doc's section for the subject before the first edit.

**Read this skill's lessons first**: `make lessons ARGS=water`. Load `determinism` beside it
for anything the simulation reads, `render` for a pass, `audio` for a voice, `labs` for a lab.

## Where it lives

| Subject | Home | Lab | Test |
| --- | --- | --- | --- |
| The wind: the field, gusts, veer, the profile, shelter | `engine/wind/field.hpp` | `wind` | `tests/wind_field_test.cpp` (golden) |
| Local wind sources: jet, whirl, blast | `engine/wind/sources.hpp` | `wind` | `tests/wind_sources_test.cpp` |
| A world's wind as a field (the mountain's) | `plugins/mountain/air/air.hpp` (`field_of`) | `wind`, `snowfall` | `tests/mountain_air_test.cpp` (golden) |
| Bodies, waves, floating, swimming, colour, flow, drawn, sound | the rows M24b–M24l | the doc's § Labs | as each row lands |

## The rules

- **The wind is a pure function.** Everything the air does at a point is computed from a
  `wind::Field` (data, built once), the point and the time: nothing is stepped, nothing draws a
  random number, and only `core::libm` is called. A local source comes from the game's state
  (a rotor's thrust, a blast's moment), so it is as deterministic as the state.
- **`open_at` keeps its order of operations.** The mountain's wind was moved onto it with its
  golden unchanged; a change to the sum, the veer or the fold's place moves
  `tests/golden/mountain_air.txt` and every Fall Line digest that reads the wind. Change it only
  on purpose, rewrite both goldens and say so in the PR.
- **A world's rule stays in the world.** The engine knows no slope: the mountain's "never up
  the hill" is a `Fold` the mountain passes in. Do the same for any world rule (a valley's
  channel, a coast's sea breeze): a field the world fills, never a branch in the engine.
- **The simulation's level of detail is measured from the game's actors, never the camera**
  (docs/water.md § Level of detail): a camera is one machine's, an actor is every machine's.
- **Per-game numbers live in the game or its plugin.** The engine gives a mechanism and a
  default; a preset (the classic swim, the clothed skier) is the game's.
- **Adding water or wind to a game is one call.** If it takes more, the API is what to fix.

## Checking a change

```sh
make test T='wind_|mountain_air'            # the wind's tests and the mountain's golden
make lab LAB=wind ARGS=--tag=before          # before the change
make lab LAB=wind ARGS=--compare=before      # after: the gusts' intensity, the profile, the plan
```

Read the `wind` lab's table: the measured intensity sits within a few per cent of the asked
(1 / ln(10 / z0) for the ground), every profile crosses 1 at 10 m, and the plan's wood and its
lee are calm while the arrows bend at their edges.
