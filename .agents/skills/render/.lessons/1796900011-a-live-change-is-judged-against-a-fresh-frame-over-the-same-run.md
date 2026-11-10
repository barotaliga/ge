---
title: A live change is judged against a fresh frame drawn over the same run, and a fit rounds its room down
date: 2026-11-05
scope: games/fall_line/app/frame.cpp, games/fall_line/labs/gpu_ladder_lab.cpp, engine/render/post.cpp, engine/render/ladder.cpp
concepts: [quality-ladder, set-picture, live-change, bloom-switch, fit-ladder, rounding, budget, lab-check]
---

- To prove a setting changes a live frame, draw the changed frame and one made fresh with the
  same options over the SAME run (both draw the first half, both cut where the change was): a
  fresh frame drawn only the second half differs by 3 % of pixels with no change at all (trails,
  spray, cached cascades). Over the same run they agree within one level.
- Judge a material where it is seen: the ladder's own view shows almost no ground, so use
  `--view=chase --fall=0` for the snow.
- A resource sized by a stop is made at load at a few texels when the stop is off, so turning it
  on is a resize, not a pipeline; a MAX-blended map is redrawn from its history safely.
- `fit_ladder` counts in grains: round a demand up and a capacity down (`room_grains`), or a room
  of 0.314 ms takes a 0.32 ms picture (`tests/render_ladder_test.cpp`).
