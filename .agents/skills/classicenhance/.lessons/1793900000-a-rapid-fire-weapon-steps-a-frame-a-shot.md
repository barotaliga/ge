---
title: A weapon that fires every frame steps its firing frames a shot; a lab of its own logic hides the game's
date: 2026-11-05
scope: games/shooters_edge/view
concepts: [held-weapon, firing-frames, frame-a-shot, barrels, nailgun, muzzle-fire, classic-fire-table]
---

The classic nailguns fire one nail a firing frame, the frame stepping on each shot (the nailgun's
two barrels in turn, the super nailgun's cluster one barrel a shot). The game's hand played the
`fire` clip from its start on every shot, so at ten shots a second it never left frame 1: one
barrel, and the puff on the model's middle. `classic-weapons-fire` looked right all along because
it animates with `classic_stream_frame` itself, never through `step_hand`; judge a held weapon in
`se-weapon-fx --game=<pack>` (the game's own path). Two more traps: `load_held_fire` gave the
pack's weapons the built set's rows (the minigun's bright spit at kBuiltScale) until it was handed
the pack's names, and blending a stepped frame toward the next slides the file's flash sideways
between barrels, so a stepped frame is held, not blended.
