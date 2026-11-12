---
type: Added
title: Shooter's Edge draws a mod's model scale and colour tint
---

A QC mod's `scale` (`DP_ENT_SCALE`) and `colormod` (`DP_ENT_COLORMOD`) now show in the window's own game: the model sized about its origin and its light tinted, its glowing texels kept. Demos and joined servers read the scale and colour-mod bytes too. Fixed with them: a player's transparency byte now reads on the right scale, and our server no longer sends a body turning solid again as a byte other clients draw as invisible.
