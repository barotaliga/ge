---
name: mountain
description: "Use for plugins/mountain in gameengine2: the generated ski mountain any game on one links. The world generator (mapgen: generate_level, the terrain, the piste, kickers, cliffs, the forest, the day and the weather, the gates, versions, level_digest) and the rule book every finished map is held to (analysis: analyze_level)."
---

# The mountain plugin

`plugins/mountain/` (`ge_mountain`, namespace `ge::mountain`) turns a seed into a clean ski
mountain. It depends on the engine's core only (`ge_add_plugin(mountain ENGINE core)`) and on no
plugin: a game on a generated snow mountain links it whatever it rides down it, so it never
learns about the rider. It speaks the ski world's words (piste, gate, kicker, tree line), never
one game's: which maps a game races, its modes and its strings stay in the game.

- `mountain/mapgen/` (`ge::mountain::mapgen`): `generate_level(seed, opts)` returns the `Level`
  (`types.hpp`): the heightfield, the open piste (a point every 2 m), the checkpoints (start
  gate to finish), the spawn and grid, the trees, the kickers, the sun and the weather. Each
  attempt draws off `sub_seed(seed, attempt)` in a fixed order and is accepted only if
  `analysis::analyze_level` finds it clean. `versions.hpp` says which generator built a map;
  `digest.hpp` is `level_digest`.
- `mountain/analysis/` (`ge::mountain::analysis`): the rule book (`mapgen/rules.hpp`, the
  R-rules) re-checked on the finished map, reading only what the `Level` publishes.
- A SKI AREA RACED AS A MAP (version 8): the build (`resort_build.hpp`) runs its stages, then
  `resort_level.hpp` moves the grids and woods out of the attempt ONCE into a base map
  (`resort_base`) and stands each course up on it in place (`set_course`: the course's line, its
  slalom gates from `course_gates.hpp`, its own day from `course_day.hpp`, the kickers and drops
  it skis named by its arc). `generate.cpp` holds the area to `analysis::analyze_resort` on the
  first course's map, each course to `analysis::analyze_course` (one that fails is not offered),
  and races `choose_course`'s pick (`GenerateOptions::course`, `grade`, else `deal_grade`).
  `analyze_level` answers both on a map with a `resort`; `analysis/shared.hpp` holds the checks
  every map shares (the start line, the day, the weather).
- A RACE COURSE SET OVER A BUILT MAP (R31–, the rule book's numbers in
  `discipline_rules.hpp`): `course_prep.hpp` prepares the hill for any discipline (the piste's
  kickers levelled, the start drop, groomed, combed, the woods cut), and each discipline sets its
  course over it as a new map whose `Level::race` names it and keeps the map it was set over
  (`RaceCourse::base`, so a second run is set over the same mountain). `slalom.hpp` is the slalom
  (R31); `speed_events.hpp` the downhill (R32), the super-G (R33) and the giant slalom (R36),
  their racing line, gate spacing and speed trap from `speed_course.hpp` (the crests shaved round
  by `course_prep`). `analysis/race.hpp` holds a course to its discipline's rule; `analyze_level` answers it
  on a map with a `race`.

## Order of work (docs/goals.md M4a to M4d)

1. Bottom-up: types, query (nearest track point), the steps, the compile, then generate.
2. Tests: `tests/mountain_generate_test.cpp` against `tests/golden/mountain_generate.txt`
   (`make golden-libm`): `level_digest`, every grid and every section of the level over a
   spread of seeds, including seeds whose first attempts are refused.
3. M4b adds the ski area, M4c the ski routes, M4d the real faces. Each lands here.

## Rules

- The order of draws inside an attempt is fixed. It is part of every digest.
- A generator change that moves what a seed builds is a new version row (`versions.hpp`);
  never a regenerated golden.
- The generator and the analysis read one rule book and share nothing else.
- A game's lists of which seeds it races belong to the game; their digests are this plugin's
  tests.

## Labs

`plugins/mountain/labs/`: `level` (the map from above, captioned, with its gate table),
`analyze` (the rule book over seeds, `--refusals` for why attempts were thrown away; both take
`--version=1,8`, the single piste and a ski area's race map) and
`mapgen-cost` (`PRESET=release`), and for the ski area `resort` (the area from above and its
courses, with its lifts, runs and courses as tables and the attempts tried) and `junctions`
(where the groomed snow steps where runs meet, the worst drawn as relief). A change to mapgen or
analysis runs `level` and `analyze` before and after (`--tag=before`, then `--compare=before`);
a change to the ski area's build runs `resort` and `junctions` the same way, and a change to a
discipline runs its lab (`slalom`: both runs straightened, the map from above, the gate table;
`speed-events`: the downhill, super-G and both giant slalom runs straightened, with their
jumps, nets and speed trap). The `labs` skill
owns how.
