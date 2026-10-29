---
type: Added
title: Ambient occlusion
---

Ambient occlusion for any game: `render/ao.hpp`'s half-size `ao` pass (a ground-truth horizon search over the depth, turned per frame, then a depth-aware 3 x 3 mean) and `sky_seen` in the frame group (binding 10, a depth-aware read), which the terrain's, the instances' and the snow's materials multiply their sky light by; `frame_group` takes the target, and `read_red` can read an RGBA16F target's other channels. Fall Line's `r_ao` (0.6) and `r_ao_radius` (1.5 m).
