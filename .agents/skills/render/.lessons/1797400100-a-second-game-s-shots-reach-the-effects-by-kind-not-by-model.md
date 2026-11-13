---
title: A second game's shots reach the effects by their kind, not by their model or its flags
date: 2026-11-13
scope: games/shooters_edge/client/listen.cpp
concepts: [missiles, shot-kind, sequel, weapon-effects, rail, bfg, bolt, monster, named-effect]
---

The first game's effects found a missile by its model's flags on the wire; the sequel's rules keep
their shots in their own array with a kind (bolt, rocket, grenade, rail, BFG), so `missiles_of`
reads that array first and sets the effect's flags from the kind. Keyed that way a monster's
rocket or bolt looks the same as a player's with no second hook. The sequel's temporary-entity
names map onto the first game's where one exists (its sparks to a gunshot's hit, its blasts to an
explosion) so the classic look stays classic for free, and only what the first game never had
(the rail's line, the BFG's burst, the blaster's hit) needs a modern effect of its own. A line
effect is a `trail_effect` event with its start in the origin and its end in the values; drawn as
a held beam it needs only its look (one straight filament) and a release time.
A lab firing a weapon must wait for the fire frame: the BFG takes about 90 frames from the
trigger, the shotgun about 10 (count the shots per kind before trusting a picture).
