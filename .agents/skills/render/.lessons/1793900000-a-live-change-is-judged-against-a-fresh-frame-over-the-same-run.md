---
title: A live change is judged against a fresh frame drawn over the same run, and proved by the pipelines it made
date: 2026-11-05
scope: games/fall_line/app/frame.cpp, games/fall_line/labs/gpu_ladder_lab.cpp, engine/render/post.cpp
concepts: [quality-ladder, set-picture, live-change, pipelines-after-load, bloom-switch, lab-check]
---

- To prove a setting changes on a live frame, draw the changed frame and a frame made fresh with
  the options the change left it with over the SAME run (both draw the first half, both are cut
  where the change was): a fresh frame cut and drawn only the second half differs by 3 % of the
  pixels even with no change at all (the trails laid, the spray and the snow in the air, the
  cached cascades). Over the same run the two agree within one level.
- The proof that nothing was built mid-run is `device.pipelines_after_load` before and after
  the change, not the absence of a crash.
- A feature compiled in by an override (the post pass's bloom share) is turned off live by a
  second pipeline with it compiled out, made at the load; binding a black input instead is not
  "off" where the shader mixes toward it (`mix(c, bloom, k)` darkens).
- A GPU timing on the Mac is only a measurement with nothing else drawing on its GPU: check for
  other threads' game windows (`ps`, `build/*/games/*`), not only for builds.
