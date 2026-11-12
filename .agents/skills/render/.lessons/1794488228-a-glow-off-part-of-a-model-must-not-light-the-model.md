---
title: A light that stands for part of a model glowing must not light that model
date: 2026-11-12
scope: games/shooters_edge/client/world_fx.cpp
concepts: [dynamic-light, grenade, glow, fullbright, self-lighting, model-lighting, classic-look]
---

A grenade's glowing ends lit as a dynamic light at its middle washed the whole grenade in that
light (the models take the frame's lights at their middle, `dynamic_light_at`), so the body glowed
instead of the ends. A light that stands for a glowing part rides on its entity
(`DynamicLight::entity`) and that entity's own model skips it; the part itself already glows by
its fullbright texels. Take the light's colour from the skin's own glowing texels
(`models::glow_colour`) rather than a constant, so another game's pack paints its own. Before
lending a classic effect a light, check that the classic one has it: the classic grenade has none,
only the rocket does. To judge a small thing in flight, crop and upscale a lab frame round it.
