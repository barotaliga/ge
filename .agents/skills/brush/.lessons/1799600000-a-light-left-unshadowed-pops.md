---
title: A light the frame cannot shadow must go dark, not shine through the walls
date: 2026-11-26
scope: plugins/brush/app/light_pick.cpp
concepts: [light-pick, light-pop, shadow-slot, light-fade, bsp-light-set, pvs-lights]
---

The modern look once drew the nearest 160 lights and shadowed 48; the rest lit unshadowed, so
they shone through the walls, and since both sets were picked by distance every step changed
them: the player saw the lighting change as he walked (dm2's big room). A cap by distance or count
is always a pop somewhere. `choose_lights` draws every light whose reach is in the view's cone
and in a cluster of the eye's visible set (each map light's clusters listed once by `light_reach`
over `box_leaves`), and `deal_cubes` makes every change of the shadowed set a fade: a light
without its cube is dark, one new to a slot fades in, one giving its slot to a more important
light fades out first. Check a change here with `labs bsp-light-set --pak=<pak1> --maps=dm2`: it
walks the map and counts a light that could show whose share jumps between two steps; it must
stay 0. Its `light_fade_s` must be the renderer's: run with 0 and every fade-in counts as a pop.
