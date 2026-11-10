---
title: An emissive in cd/m^2 that reads right by day is a blinding blot at night; key it to the eye
date: 2026-11-09
scope: engine/render/beams.wgsl, engine/render/particles.wgsl, games/shooters_edge/app/weapon_fx.cpp
concepts: [exposure, night, emissive, keyed, bloom, beam, spark, ember, core-keys, glow-keys]
---

The bolt was reported as "a huge glowing thing on the Mac". It was not the Mac: it was night. The
eye adapts to a dark room (`exposure()` grows by orders of magnitude) while the beam's core,
sheath, sparks and ember stayed in absolute cd/m^2, so at `sv_hour 23` they were hundreds of
whites and the bloom spread them over a third of the screen. By day the same frame looked fine,
so every daytime lab missed it. Reproduce a "too much glow" report at night first
(`--cvars="sv_hour 23"`), then express the emissive as the eye sees it: `keys / exposure()`
(BeamLook's `core_keys`, `glow_keys`), or cap it there (`kGlowKeys` on particles, `kEmberKeys` on
burns). A Shooter's Edge dynamic light is scaled by `r_fx_lights` (0.2 by default): a light that
must out-shine the other effects needs `DynamicLight::boost`, not a larger level (capped).
