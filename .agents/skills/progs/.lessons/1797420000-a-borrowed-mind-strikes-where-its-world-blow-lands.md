---
title: A monster on another game's mind begins a blow only where its own world's blow lands
date: 2026-11-27
scope: games/shooters_edge/server/q2_monsters.cpp, games/shooters_edge/server/q1_monster_mind.hpp
concepts: [remix, cr-monsters, monster-mind, melee-reach, leap, lodged, se-q2-quake-monsters]
---

Quake's monsters under Quake II's rules on Quake's mind call a blow at Quake's melee range (120 eye
to eye), but Quake II's blow lands within 80 and does not close in, so a brute stood swinging at
air (hits 14 → 1 in `se-q2-quake-monsters`). Begin the blow only within the world's own reach and
let the run close the gap. Quake's hound leaps from 80, so its leap met the player's box and lodged
there for good (no step made, never on ground): Quake II's `fall` had no way out; a leap that makes
no step against a player now lands. Run each kind alone in a hall before and after: a count that
collapses to 1 is a monster stuck after its first attack.
