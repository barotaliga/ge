---
title: A with-and-without picture pair needs each state settled through the resolve; show the difference as a picture
date: 2026-11-05
scope: games/shooters_edge/labs/gpu_se_models_lab.cpp, games/shooters_edge/client/world_draw.cpp
concepts: [taa, resolve, settle, difference-picture, shade, side-pair, model-shadows, sun, world-sun, digest-affected, staging, lab-state, power-ups, glow]
---

- `se-models` measured his shadow by drawing one frame with him and one without: the TAA's history
  carried the first into the second, so the darkening read low. Draw each state a dozen frames
  before its read back.
- An effect on part of the frame is judged from a DIFFERENCE picture, not the frame: the frame
  greyed, the change in red eight times over (`<id>-shade`), a with/without pair side by side
  (`<id>-side-pair`), and a 99th-percentile metric (`-deepest`) beside the mean.
- A lab that runs its views in one world carries state from one to the next. `se-models`' shadow
  views ran after the `powered` view had handed the player both power-ups: their glow lit the
  floor round him, him drawn or not, and the shadow read 9 to 19% at its deepest instead of 25 to
  76%. A view resets what earlier views set on the actors it uses, and a leftover magenta patch
  where a removed figure stood is a light, not the resolve (48 frames left it the same).
- A staged view needs its place checked, not only reached: the first spot a pace from a light can
  face a wall. Score the candidates (open line ahead at the eye, level floor where the effect
  falls) and keep the best; try a side camera on both sides at a few distances.
- `draw_world` dresses the map's sky (its pick, `sv_hour`) on its own copy of the view, so a lab's
  `FrameView` does not hold the sun the frame lights with: `world_sun` does. For sun-lit views,
  set `look.weather.hour` (noon) and check with a hull trace toward the sun that the point just
  past the hit is a sky leaf (contents -6); a 16-unit leaf walk skips thin walls.
- `std::getenv` returns non-null for `VAR=` (empty): an A/B switch read that way is on in both
  runs.
- On the Mac `ge digest --affected` named 0 labs for a change to `bsp.wgsl` and a lab's own file
  (it compares objects with a base build that was not there): run the reached labs by hand
  (`labs <id> --smoke --quiet --check-digests=tests/digests`).
