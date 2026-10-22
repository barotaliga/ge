---
name: mountain
description: "Use when the game links ge::mountain (`ge use mountain`): the generated ski mountain. generate_level turns a seed into a Level (the heightfield, the piste, the gates, the trees, the kickers, the day and the weather), held to a rule book by analyze_level; level_digest pins a map."
plugin: mountain
---

# The mountain plugin

`ge::mountain` (headers `mountain/mapgen/*.hpp` and `mountain/analysis/*.hpp`) turns a seed
into a clean snow mountain. It speaks the ski world's words (piste, gate, kicker, tree line)
and knows nothing about who rides it: which maps a game races, its modes and its words stay in
the game.

| Header | What | Start with |
| --- | --- | --- |
| `mountain/mapgen/generate.hpp` | `generate_level(seed, opts)`: the attempts, each drawn off its own sub-seed in a fixed order, accepted only when the analysis finds it clean | `ge explain generate_level` |
| `mountain/mapgen/types.hpp` | The `Level`: the heightfield and its queries, the open piste (a point every 2 m), the checkpoints (start gate to finish line), the spawn and grid, trees, kickers, the sun, the weather | `ge explain mountain/mapgen/types.hpp` |
| `mountain/mapgen/digest.hpp` | `level_digest`: one hash for a whole map | `ge explain level_digest` |
| `mountain/mapgen/versions.hpp` | Which generator built a map; a pinned map names its version | `ge explain mountain/mapgen/versions.hpp` |
| `mountain/analysis/index.hpp` | `analyze_level`: the rule book re-checked on a finished map | `ge explain analyze_level` |

## Use it

- **A map is a pure function of its seed and options**, and generating one costs seconds.
  Build it once at `create_game` and read it after; never regenerate inside a step.
- **Pin what you race.** A map a player races (a record book, a ghost) names its generator
  version and its `level_digest`, and a test rebuilds it and compares the digest. An engine
  release that moves a digest says so in its notes: re-pin on purpose, never silently.
- **Read the map through its queries** (the ground's height and normal, where on the piste a
  point is), never by walking the grids a second way.
- `ge find mountain <word>` finds the rest; the rule book's numbers are `mapgen/rules.hpp`.
