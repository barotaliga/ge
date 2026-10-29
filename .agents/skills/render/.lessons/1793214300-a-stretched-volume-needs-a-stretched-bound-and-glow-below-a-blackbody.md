---
title: A volume stretched along an axis needs a bound stretched with it, and thin flame glows below a blackbody
date: 2026-10-28
scope: engine/render/explosions.wgsl, engine/render/muzzle.hpp
concepts: [muzzle-flash, raymarch, ellipsoid, bound, stretch, blackbody, glow]
---

A muzzle flame drawn as the fireball squeezed along the barrel first kept the fireball's sphere
bound. A long rocket jet then spent most of its ten steps in the air beside it, and came out thin
and banded from the side. Intersect the ray with the ellipsoid instead: squeeze the ray's origin
and direction the same way as the march's points and solve the quadratic in that space. The steps
then land on the flame. Also, gas at a flame's temperature drawn at full blackbody luminance blows
out white whatever the heat. A per-instance `glow` (about 0.05 of a blackbody for a gun's flash)
keeps the orange; the light the flash throws on the scene is a separate knob.
