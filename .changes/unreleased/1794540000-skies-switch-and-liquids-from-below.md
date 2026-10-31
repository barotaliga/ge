---
type: Added
title: ce_skies picks our sky or the pack's, and lava and slime are seen from below
---

`ce_skies` now works in the classic game window's modern look. When it is on, the sky faces show our sky, clouds and night. When it is off, they show the pack's own scrolling sky (`Options::pack_sky`). An eye under a lava's or a slime's level now sees the liquid round it, where before nothing was drawn. A slime is a fog of its own colour with Snell's window overhead, and a lava is its melt's glow all round. `Liquids::under_reach` sets a game's least view; Shooter's Edge's is 3 m. The `se-world` lab adds `skies`, and the `gpu-lava` and `gpu-slime` labs add `from-below`.
