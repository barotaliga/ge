---
type: Changed
title: The post pass's sharpen at a strength of 0 is off
---

A post pass built with the contrast-adaptive sharpen compiled in now skips it while its live strength (`set_post_look`) is 0, so a game can compile it once and turn it on and off without making the pass again. Shooter's Edge uses this to sharpen the picture after `r_scale` upscales it (`r_scale_sharpen`).
