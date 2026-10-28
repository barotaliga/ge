---
title: A small lake lays no waves under the default Lay; a game repo's lake keeps them to 0.25 m
date: 2026-10-28
scope: templates/water/, engine/water/waves.hpp
concepts: [water, waves, lay, lake, template, ge-add-water]
---

A 7 m/s wind over the 50 m fetch of `ge add water`'s lake grows a sea peaking at 0.43 s (about
0.3 m long), all of it under `Lay::shortest` (1.5 m), so `Waves::count` came out 0: the surface
`sample` returned was a flat plane, the shading showed nothing and only `Sample::hs` knew there
was a sea. The template sets `world.lay.shortest = 0.25` so a float or a swimmer on a pond feels
its waves; an open sea keeps the default. When a lake looks flat, print `bodies[i].waves.count`
and `waves.sea.tp` before suspecting the picture.
