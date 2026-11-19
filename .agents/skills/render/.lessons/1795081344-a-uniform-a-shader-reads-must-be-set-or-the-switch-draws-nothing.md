---
title: A new Options field reaches the shader only through a uniform lane someone sets; check it before shooting
date: 2026-11-19
scope: plugins/brush/app/bsp_draw.cpp, plugins/brush/app/bsp.wgsl
concepts: [uniforms, ce-switches, before-after, lab]
---

`ce_reflections` was wired from the look (`Options::reflections`) to the shader (`bsp.sky_box.w`)
but `fill_uniforms` still wrote 0 into that lane, so the switch drew nothing and a full round of
before/after shots had to be thrown away. When a switch rides a spare lane of an existing vec4,
grep the shader's reader (`sky_box.w`) AND the `set4(...)` that fills it before taking any shots,
and take one quick pair first: a before/after diff of 0 % is the uniform, not the effect.
