---
title: Deal the shadowed lights by the eye's visible set first, and judge the deal with a fresh draw per view
date: 2026-11-09
scope: plugins/brush/app/bsp_draw.cpp, plugins/brush/labs/gpu_bsp_lab.cpp, games/shooters_edge/labs/gpu_se_client_lab.cpp
concepts: [shadow, shadowed, cube, pvs, cluster, cluster-sees, popping, fresh-cubes, dm4]
---

Ranking lights by distance less reach favours big lights wherever they are: on an open map of a
few score lights, half the 32 shadow slots went to lights behind walls and the room in view lit
flat until a few steps on. `prepare_bsp` now takes the lights whose cluster the eye's cluster sees
(`cluster_sees`, a light with no cluster counted as seen) before the rest. To measure a deal, count
the seen lights left without a slot over a grid of eyes (a scratch tool on `load_scene`, no GPU).
Pictures lie about it unless each view starts fresh: `gpu-bsp` keeps one `BspDraw` across its
views, so after the first only `fresh_cubes` (2) new cubes come per view; `se-client --game`
with `--settle=40` lets the cubes fill as standing still in the window does.
