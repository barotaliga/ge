---
title: Scope the registry by what the multiplayer code reaches, not by what the content holds
date: 2026-10-24
scope: games/shooters_edge/content
concepts: [registry, scope, multiplayer, deathmatch, creature, skip]
---

The content set's maps and single-player logic name about twice what a multiplayer game ever
plays: every creature, its projectiles, and a debug noise maker. List every string the
multiplayer game logic and the client's own code pass to a sound, a precache or a model call,
then follow each one that names a creature's file to its caller: a wall trap firing a creature's
laser sound, or a deathmatch bonus playing a boss's cry, is in scope under a name of its own,
while a landing thud only creature physics plays is not. Leave the rest out of the registry: on
2026-11-28 the `skip` rows (95 creature sounds, 26 models) were deleted when Shooter's Edge
stopped standing in for a classic game, and no single-player mode of its own is planned.
