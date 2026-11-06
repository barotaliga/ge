---
title: Check the Mac's GPU is idle before timing on it, and alternate builds to call a regression
date: 2026-11-06
scope: games/fall_line/labs/gpu_ladder_lab.cpp
concepts: [gpu-timing, m3-max, ioreg, device-utilization, contention, regression, a-b, price]
---

A game window left open in another checkout held the M3 Max's GPU 93 % busy, and the `ladder` lab
read the top at 21.8 ms where it was 7.8: the GPU span counts the time another process's work
holds the GPU. Before any timing on the Mac, read the GPU's load with nothing of yours running
(`ioreg -r -d 1 -w 0 -c IOAccelerator | grep -o '"Device Utilization %"=[0-9]*'`, a few times a
second apart; the desktop alone is 15 to 30 %) and close what holds it. To tell a regression from
noise, build the old tree in its own worktree (`git worktree add /tmp/x <sha>`, `cmake --preset
release -DGE_DEPS_DIR=<this checkout>/.cache/deps`, build `labs` only) and alternate the two
`labs ladder --price` runs; and keep a build off the CPU while you time.
