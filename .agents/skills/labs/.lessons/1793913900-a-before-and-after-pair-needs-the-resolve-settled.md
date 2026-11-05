---
title: A with-and-without picture pair needs each state settled through the resolve; show the difference as a picture
date: 2026-11-05
scope: games/shooters_edge/labs/gpu_se_models_lab.cpp, games/shooters_edge/client/world_draw.cpp
concepts: [taa, resolve, settle, difference-picture, shade, model-shadows, sun, world-sun, digest-affected, staging]
---

- `se-models` measured his shadow by drawing one frame with him and one without: the TAA's history
  carried the first into the second, so the darkening read low. Draw each state a dozen frames
  before its read back.
- A faint effect (a figure's shadow in a room of many fill lights: 2% of the floor's mean) is
  judged from a DIFFERENCE picture, not the frame: the frame greyed, the change in red eight times
  over (`<id>-shade`), plus a 99th-percentile metric (`-deepest`) beside the mean.
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
