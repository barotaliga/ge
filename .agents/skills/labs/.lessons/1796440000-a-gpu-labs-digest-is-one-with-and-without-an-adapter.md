---
title: A GPU lab's digest is one with and without an adapter; check it with GE_GPU_OFF=1 before the push
date: 2026-11-05
scope: engine/lab/labs/gpu_morph_lab.cpp, engine/lab/labs/gpu_far_land_lab.cpp, games/shooters_edge/labs/gpu_se_weapon_fx_lab.cpp, engine/lab/lab.cpp
concepts: [digest, placeholders, no-gpu, windows-ci, det-measured, metric-order, spawn]
---

- CI's Windows row has no GPU (`GE_GPU_OFF=1`), and the container has lavapipe, so a GPU lab's placeholders branch is only run in CI. Run `GE_GPU_OFF=1 build/debug/ge digest <lab>` beside the plain `ge digest <lab>` before a push: the two must agree.
- The digest is one FNV stream over every deterministic metric, table row and shot id, in the order the lab makes them. The placeholders must give the same ids in the same order as the device branch (far-land listed them in another order).
- A number read off a picture (pixels changed, a colour) is `Det::measured`; the check that it is right is a `lab.fail`, not the digest. gpu-morph's warp check counted pixels as deterministic, and a machine with no adapter has no pixels.
- A lab that steps a game from a fresh listen server sees the player spawn. When another PR makes a spawn throw something (#520's ash), every count gathered over the spawn moves: zero the counts once the spawn has settled, so the lab counts only its subject.
