---
type: Added
title: The classic world pass, effects and two looks
---

The brush world's renderer plays every light style a face stores (each style a layer of the lightmap atlas, the 64 levels a uniform from their letter patterns), animates "+0".."+9" texture sequences, and culls the world by the eye's potentially visible set and then the depth pyramid in two phases, compacted in order so a frame's pixels never depend on thread order. Dynamic lights add to the lightmaps or join the real-time lights; the post pass takes a screen blend (`set_post_blend`); the particle pool runs over no ground. Shooter's Edge gets its effects (trails, explosions, hits, wounds, teleports, lava), its screen blends and its classic and modern looks, pictured in the new `se-world` lab, with the cvars `r_look`, `r_fx_lights`, `r_fx_glow`, `r_bloom`, `r_lava_glow` and `r_slime_glow` to tune the picture (`Options::dynamic_strength`, `lava_glow`, `slime_glow` and `ParticleSettings::glow` beneath them). Slime now lights the walls round it with a faint green. The fixture writer can light a second style.
