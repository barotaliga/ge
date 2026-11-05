---
title: A match's gunshots, spikes and beams never reached the window: only its explosion was read
date: 2026-11-05
scope: games/shooters_edge/client/listen.cpp, games/shooters_edge/client/world_fx.cpp
concepts: [temp-entity, multicast, arena, gunshot, beam, lightning, effects, classic-effects]
---

The listen client read a campaign's temporary entities off its unreliable stream, but a match's
(the baseline mod multicasts them in the multiplayer layout: a gunshot and blood carry a count) only
for TE_EXPLOSION, so the arc gun drew nothing and the shots hit silently. `arena_effect` reads
them into `Listen::effects`. For the classic look's effects, read the pack (its palette, its
`s_explod.spr`, its `bolt*.mdl`) rather than restating colours: `kEffectModels` loads them whether
the program precached them or not. The classic roots the player's own bolt at his origin; at the
eye its first segment fills the view.
