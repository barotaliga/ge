---
title: A cloud deck is lit by diffusion, and a sheet has to agree with the ground's flat light
date: 2026-10-24
scope: engine/render/clouds.wgsl, plugins/mountain/app/cloud_field.cpp
concepts: [clouds, cloud-kinds, multiple-scattering, deck, stratus, reference-photos, exposure]
---

A heap's light model (Beer's law plus one wide forward lobe at the reduced extinction) drew a
stratus or a nimbostratus from below as brown and nearly black: the forward lobe dies as
exp(-σ′τ) through a slab of optical depth 30 to 90, while a real deck's base is a bright grey
because the light that reaches it has diffused down, and diffusion falls as 1/(1 + σ′τ). Mix
the two by the kind's dome (a sheet diffuses, a heap scatters forward), take the path up to the
slab's top for it (one read halfway there), and dim the sky's own light by the same path. Even
then a thick sheet over a lid read darker than the ground under it, because the exposure keys
to the sunlit white and the ground is lit by the deck's horizon grey: blend a lid's sheet toward
the deck's own luminance (0.6 of it, not all, or the base loses its texture). Compare a kind
with its photograph from where a photograph is taken (the valley, the sun behind and to one
side: the `world` lab's `clouds` view), never from the summit, which stands inside the low ones.
