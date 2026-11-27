---
type: Added
title: TAA-U's resolve can show each drawn pixel as a crisp square
---

`TaaSettings::blocks` makes the resolve copy the render pixel each output pixel lies in, with no history and no filter, so a picture drawn at a whole ratio of the output (0.5, 0.25) is shown as crisp squares. It is read live each frame. Shooter's Edge's `r_scale` now draws only at 1, 0.5 or 0.25 and shows crisp squares by default; `r_scale_smooth 1` keeps the smoothed, sharpened upscale.
