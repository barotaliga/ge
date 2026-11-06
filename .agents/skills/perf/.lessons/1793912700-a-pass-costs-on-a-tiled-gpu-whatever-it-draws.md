---
title: On the M3 Max a render pass costs about 0.12 ms whatever it draws; untaken shader code costs too; time the world frame at 1440p
date: 2026-11-05
scope: plugins/brush/app/bsp_draw.cpp, plugins/brush/app/bsp.wgsl, games/shooters_edge/labs/gpu_se_timedemo_lab.cpp, games/shooters_edge/client/timedemo.cpp
concepts: [render-pass, tiled-gpu, shadow-atlas, viewport, register-pressure, timedemo, gpu-pass-times, noise, model-shadows]
---

- M54's caster cubes were 48 render passes a frame (8 lights x 6 faces of 384²), 2k to 7k
  vertices each: 6 ms of GPU at 960x540. Passes that only cleared cost the same. As one atlas
  (a light a row, a face a column) in ONE pass with `SetViewport` a face, `bsp.shadows` went
  3.8 to 0.09 ms. Many small depth targets a frame belong in one pass.
- `se-timedemo` prints each pass's GPU time (`GPU by pass`, `gpu.pass.<name>`). On a tiled GPU
  they overlap, so they add to more than the span: use them to find a pass that changed, and
  scratch-build switches (an env var read into a spare uniform lane) to price the parts.
- Code in the per-light loop costs when its branch is not taken: the soft-shadow and capsule
  paths, all switched off at run time, still left 0.8 ms at 1440p; compiled out, the span was
  main's. Work out per pixel what the loop can share (the disc's spin, the capsules in reach),
  read tables not `sin`/`cos`, and let the cheap test (a search on the casters' cube alone)
  skip the costly filter for most pixels.
- At 960x540 runs moved 2 ms with another GPU client on the Mac (main read 3.3 to 5.1 ms);
  at `--size=2560,1440` they repeat within 0.3 ms. Build main in a scratch worktree
  (`git worktree add /tmp/<x> origin/main`) and run the two alternately.
- After `git checkout <rev> -- <dir>` to build an old version, rebuild when restoring: a run
  read the old binary as the new one once.
