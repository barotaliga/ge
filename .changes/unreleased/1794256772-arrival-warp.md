---
type: Added
title: The arrival opens the whole view
---

`render/post.hpp` gains the arrival: `arrival_warp` gives a spawn's or a teleport's warp of the whole view at its age (a flash, the world pulled in and streaked toward the middle, a clear disc with a bright rim growing past the corners, whole by 0.65 s) and `set_post_warp` sets it on the post pass live, costing one uniform branch when off. Shooter's Edge's own arrival uses it under `ce_warps` at the new `r_arrival` (ARRIVAL FLASH in the menu), in the pack's teleport colour.
