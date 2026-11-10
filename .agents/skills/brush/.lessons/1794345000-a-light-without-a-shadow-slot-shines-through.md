---
title: A shadow that appears as you walk in is a light that had no slot before
date: 2026-11-10
scope: plugins/brush/app/bsp_draw.cpp, games/shooters_edge/labs/gpu_se_client_lab.cpp
concepts: [shadow, shadowed, cube, slot, pvs, frustum, in-view, start, slipgate, leak]
---

A light that gets no shadow cube lights everything in its reach, through walls and frames. When a
shadow "appears" as the player steps somewhere, first check the classic look: if the map's own
lightmap has the shadow, the bug is the light lacking a slot from farther away, not the shadow.
Find the light by printing `casters` per eye from `prepare_bsp` and dropping candidates from it
one at a time (a throwaway env var), and find the caster with a slice of `leaf_at` over the map.
start's corridor sees 57 lights in its visible set, so the cap, not the ranking, decided it: the
slots now go to lights whose reach meets the view's cone first, and the cap is 48 (the cubes were
always allocated for 48). `se-client --game=<shareware id1> --map=start --views="544,700,40,90,0;544,1000,40,90,0"`
shows it.
