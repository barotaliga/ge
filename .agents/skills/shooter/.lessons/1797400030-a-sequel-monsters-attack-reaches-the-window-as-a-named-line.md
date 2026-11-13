---
title: A sequel monster's attack the window should draw is sent as a named line effect
date: 2026-11-13
scope: games/shooters_edge/server/sequel_monsters.cpp
concepts: [sequel, monster, line-effect, cable, muzzle-flash, named-trail, ce-monsters]
---

The sequel's server ran the parasite's drain, the medic's revive and every monster's shot
without sending anything a window could draw: the damage happened and the screen showed
nothing. `fight_monster_line(s, name, from_entity, a, b)` sends a named line (`trail_effect`),
which arrives in `client/world_fx.cpp` as `EffectKind::named_trail` with `at`, `end` and the
name. Send each visible attack this way, from `shoot()` or the monster's own step, and key the
drawing by name (`cable_of`, `flash_of` in `app/monster_fx.hpp`). Classic windows ignore
named trails, so the first game is unaffected. A test in
`tests/shooters_edge_sequel_monsters_test.cpp` reads `s.out.events` for the line.

A sequel monster's shot leaves from its eye, inside its head, so a flash drawn there is hidden by
the model. Draw it at `held_tip` with a forward a little raised (`ahead + 0.4 up`): along the
level facing alone, a foot striding out is the furthest vertex and the fire lands on the floor.
