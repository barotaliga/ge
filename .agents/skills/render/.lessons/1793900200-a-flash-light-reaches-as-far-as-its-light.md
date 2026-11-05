---
title: A flash's light reaches as far as its light does, and the eye must be inside it to adapt
date: 2026-11-05
scope: engine/render/explosions
concepts: [flash, local-lights, range, lamp-lux, exposure, night, explosion, adaptation]
---

The first time `flash_at` lit anything, the yard at night was white and the dusk wall carried a
lit dome with a hard rim. The light's range was the look's fixed 22 m while its 2 million cd still
put thousands of lux at that edge: the window cut it, and the eye 20 m off stood outside the
sphere, so `lamp_lux_at` gave it 0 and the night's dark gain blew the frame out. The reach now
follows the light (`sqrt(cd / 1 lx)`, between the look's range and four times it), and the scene
feeds `lamp_lux` at the eye before `fill_uniforms`. Turning the flash's strength down would not
have fixed either: with the eye adapted, only the ratio of the lit floor to the eye's own light
shows. A lab metric that counts the pixels a blast changes (to price its march) must switch the
flash off, or the lit ground counts as march (6,495 px became 91,764).
