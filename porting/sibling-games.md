# Working notes: what the sibling games imply for the plugin boundaries

A WORKING DOCUMENT for the porting phase (porting/ is deleted when porting ends). It records
the survey behind `docs/goals.md` § Open for extension: niclaslindstedt's earlier games, read
(never written) as candidates for porting onto the engine later.

game2 (a rally game), game3 (a jet-ski racer) and game4 (a snowmobile racer) are the same shape
as game5 and may be ported later (read-only, cloned beside this checkout like the oracle). None
of them is ported now (G4), but the plugin boundaries are drawn so they would fit:

- **Every one** has `engine/{mapgen, analysis, rating, sim}` with a versioned generator, a
  `levelDigest`, a rule book re-checked on the finished map, a headless bot and `simulate`, and
  the same fixed-step `step`. The engine's core, run clock, platform, GPU, render passes, synth
  and 2D layer serve all four, so none of them may learn a ski word.
- **`racing`** serves all four: each has rivals, standings, a ghost tape (game2 and game3 keep
  their own `sim/tape.ts`), checkpoints (game5 gates, game3 buoys).
- **`mountain`** is game4's generator too: game4's `engine/mapgen/` is the same file list as
  game5's single piste (terrain, track, kickers, cliffs, drift, forest, regions, weather, sun,
  versions, digest). The mountain plugin must therefore never import the skier.
- **The snow under a vehicle** (game4's `snow.ts`, `snowfall.ts`, `trench.ts`, `suspension.ts`,
  `flight.ts`, `crash.ts`, `ragdoll.ts`) is the same idea game5 restates for skis. When M5 ports
  game5's, the snow's surface model (`snow.ts`'s sink, drag and the packed share) is a candidate
  plugin of its own (`snow`) that `skier` depends on, rather than a part of `skier`.
- **Tricks and strokes** (game3, game4, game5) and **wind and weather** (all four) recur. Each
  becomes a plugin when the port that needs it starts, not before.
- game2's roads and buildings and game3's water and waves are other worlds: they would be new
  plugins beside `mountain`, which is why world generation is not in the engine.

