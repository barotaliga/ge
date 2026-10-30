---
type: Added
title: lava and slime mirror the scene, lava shimmers, and its crust breaks
---

A liquid now mirrors the scene, with its reflected ray marched through the scene's depth (`Liquids::reflections`). The air over lava shimmers (`LavaLook::shimmer`), bending what is seen across it and on it. `break_liquid_crust` opens a torn hole of melt where something falls in, ringed by glowing shards, which skins over again. The `gpu-lava` lab adds `shimmer` and `crust-broken`, and the `gpu-slime` lab adds `reflections`.
