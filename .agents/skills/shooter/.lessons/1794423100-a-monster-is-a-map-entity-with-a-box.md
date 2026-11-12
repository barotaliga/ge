---
title: In the sequel a monster is a map entity whose box stops traces that carry the monster bit
date: 2026-11-12
scope: games/shooters_edge/server/sequel_monsters.cpp, games/shooters_edge/server/sequel_map.cpp, games/shooters_edge/server/sequel_fight.cpp
concepts: [sequel, monsters, map-trace, monster-bit, sweep-box, skip-entity, shot-mask, opaque-mask]
---

The world's rows are sized at the map's start, so a monster is not a new kind of row: it is the
map entity its text spawned, with `box` set while it lives. `map_trace` sweeps against that box
only when the mask carries `kSqMonster` and the entity is not `skip` (the monster itself), so the
choice of mask decides who a monster stops: a living player's move (`kSqMaskPlayerSolid`) and a
shot (`kShotMask` in sequel_fight.cpp) stop at it; a monster's sight line (solid, slime, lava) and
a blast's reach (`can_damage`) do not. A corpse clears `box` and shots pass it.

Damage reaches a monster through `map_damage` (bullets, rails, blasts) and through the missiles'
direct hit; both route to `monster_damage`. A probe that drops a player into a real map must put
him where the map has floor: the start of the probe at an arbitrary point fell out of the world.
