---
name: mountain
description: "Use for plugins/mountain in gameengine2: the generated ski mountain any game on one links. game5's world generator (mapgen: generateLevel, the terrain, the piste, kickers, cliffs, the forest, the day and the weather, the gates, versions, levelDigest) and the rule book it is held to (analysis: analyzeLevel), bit for bit."
---

# The mountain plugin

`plugins/mountain/` (`ge_mountain`, namespace `ge::mountain`) is game5's `engine/mapgen/` and
`engine/analysis/` in C++: a seed in, a clean ski mountain out, bit for bit with the oracle. It
depends on the engine's core only (`ge_add_plugin(mountain ENGINE core)`). It speaks the ski
world's words (piste, gate, kicker, tree line), never one game's: which maps a game races (its
pinned maps), its modes and its strings stay in the game.

- `mountain/mapgen/` (`ge::mountain::mapgen`) ports `game5/engine/mapgen/` file for file.
  `generate_level(seed, opts)` returns the `Level` (`types.hpp`): the heightfield, the open
  piste (a point every 2 m), the checkpoints (start gate to finish), the spawn and grid, the
  trees, the kickers, the sun and the weather. Each attempt draws off `sub_seed(seed, attempt)`
  in the oracle's fixed order and is accepted only if `analysis::analyze_level` finds it clean.
  `versions.hpp` says which generator built a map; `digest.hpp` is `levelDigest`.
- `mountain/analysis/` (`ge::mountain::analysis`) ports `game5/engine/analysis/`: the rule book
  (`mapgen/rules.hpp`, R-rules) re-checked on the finished map, reading only what the `Level`
  publishes, in the oracle's own message text.

## Order of work (docs/goals.md M4a to M4d)

1. Bottom-up: types, query (nearest track point), the steps, the compile, then generate.
2. Parity: `tests/mountain_generate_test.cpp` against `tests/golden/mountain_generate.txt`
   (`make golden-libm`, which runs `tools/parity/golden-mapgen.mjs` under game5's pinned Node):
   `levelDigest`, every grid and every section of the level on `LEVEL_SEEDS`, the benchmark's
   map, a map in each crusted region and seeds whose first attempts are refused.
3. M4b adds the ski area (v8), M4c the ski routes (v9, v10), M4d the real faces. Each lands here.

## Rules

- The order of draws inside an attempt is the oracle's exactly. It is part of every digest.
- A generator change that moves what a seed builds is a new version row (`versions.hpp`), as in
  game5; never a regenerated golden.
- The generator and the analysis read one rule book and share nothing else.
- Fall Line's pinned map lists (`race-maps.ts`, `trick-maps.ts`, `jump-hills.ts`) name WHICH
  seeds a game races; they belong to the game, and their digests are this plugin's tests.
