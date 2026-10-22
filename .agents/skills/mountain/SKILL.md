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
  by `course_prep`); `speed_ski.hpp` the speed-skiing track (R34), a track of its own searched
  and cut straight down the face, which REPLACES the map's `track`; `ski_cross.hpp` the ski cross
  (R35), a course BUILT on a stretch of the piste (its own weaving line, berms banked into the
  ground, rollers, jumps and step-downs built onto its profile), which replaces it too. `analysis/race.hpp` holds a course to its discipline's rule; `analyze_level` answers it
  on a map with a `race`.
- THE TERRAIN PARK (R20, `rules::trick`), only on a map asked for with
  `GenerateOptions::tricks`: `park.hpp` plans the kickers down a line (`plan_park`: the three
  sizes in turn, clear of the line's kickers, drops, drifts and gates) and stamps them into the
  finished ground (`stamp_park`). On the single piste the park is planned inside the attempt,
  before the drifts, and an attempt whose piste carries none is refused; on a ski area
  `set_parked_course` (`resort_level.hpp`) plans it down a course first and sets the course's
  gates round it (`course_gates` with the park as zones), and an area none of whose courses
  carries one is refused unless a course was asked for. `analysis/park.cpp` (`check_park`)
  holds the finished park; a map built without tricks moves by nothing.
- THE SKI ROUTES (R42, `resort_rules::route`), from v9 on: `ski_routes.hpp` (`lay_ski_routes`)
  finds the ORANGE lines on a built ski area, two cheapest-path searches over a grid of squares
  (from every chair's and gondola's top, and back from every run) joined through one square
  steeper than a black; v9 lays the shortest line, v10 the WILD one (a step's cost by the woods,
  the edges and the cliff faces it crosses, drops over crags, the line threaded between the
  trunks). `route_features.hpp` then fells the trunks on a wild line and cuts its jumps
  (`O1…`, `Kicker::route`) and their gaps into the ground. `generate.cpp` (`lay_routes`) lays
  them on the attempt's base map BEFORE the area is analysed, so `analysis/routes.cpp`
  (`check_routes`) and every other rule hold the map with them; the jumps ride on
  `ResortAttempt::route_kickers` into every course set. A map from v8 or before carries none,
  and the digest reads routes only where there are some.
- A FREESTYLE VENUE BUILT ON A MAP (R37 onward, the rule book's numbers in `venue_rules.hpp`):
  `straight_venue.hpp` searches a straight line of its own down the face where a profile designed
  off the horizontal cuts and fills least (`find_line`) and grades the ground to it (`grade_venue`:
  eased out, groomed, the woods and the arena cleared, lowered under a raised stretch). THE SKI
  JUMPING HILL (R43, `ski_jump.hpp`): `ski_jump_profile` builds each hill (`JumpHill::large`,
  `normal`) element by element from the construction norm's formulas over its row's free choices,
  the same on every map; `set_ski_jump` sets it over a map as a new map whose `Level::ski_jump`
  names it and keeps the map under it (`SkiJumpHill::base`). The hill is an ANALYTIC surface
  (`Level::venue`): `ground_at` and `normal_at` answer off its profile inside its width (only the
  deck up the raised in-run), and the grid under it is cut to it, never above it. Any course or
  hill already on the map is set over the map under it (`original_of`).

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
jumps, nets and speed trap; `speed-ski`: the track's profile from the side and the map from
above; `ski-cross`: the course close up, its profile with the mean fall taken out, the map). A
change to the terrain park runs `park` (the park close up, its profile, the map) at
`--version=1,8`; a change to the ski routes runs `routes` (the area with its routes, each route
close up and from the side, the routes' table) at `--version=9,10`; a change to a venue or the ski
jumping hill runs `skijump-hills` (each hill's earth moved, tower and arena as a table, its
profile at true scale from the side, the map) at `--version=1,9`. The `labs` skill owns how.
