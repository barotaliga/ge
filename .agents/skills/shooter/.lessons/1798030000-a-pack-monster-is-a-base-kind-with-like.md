---
title: A mission pack's monster is a base kind it looks like, and a lab that counts shots counts launches
date: 2026-11-23
scope: games/shooters_edge/server, games/shooters_edge/labs
concepts: [monster, mission-pack, reckoning, like, power-shield, missile, launch, lab, spawnflags, skill]
---

- **A pack's monster that reuses a base monster's body names it in `Kind::like`; every lookup of
  frames, pains, deaths, ducks, gibs, flashes and frame cues goes through `looks_like(k)`.** A
  lookup by `k.classname` silently gives the new kind no poses or a soldier no duck. Footsteps stay
  by its own classname on purpose (the pack's guards have none). Grep `k.classname` in
  `q2_monster*.cpp` and `q2_poses.cpp` when a new pack row looks frozen or never ducks.
- **Every path that fires a kind's attack must ask `shot_of(a, usual)`.** The soldier's dying shot
  launched a plain bolt for the ripper and hyperblaster guards until the lab's table showed it.
- **A shot can launch and hit in the same frame.** A monster that charged up to the player fires
  at point blank: its missile row is live for no tick a lab can see. Count launches by a row's
  `dies_at` changing (each launch sets it from the level time), not by `live` turning on.
- **A map's monster count for one skill leaves out spawn flag 512** (not on medium, skill 1). The
  pack's maps place 69 on badlands, 54 at skill 1; every one of the 54 spawns.
- **Pack rows go after `misc_actor` at the end of the kinds' table**, so the base kinds' indices
  (and the goldens and digests that hold them) never move. Gib models of a pack's own would move
  base maps' model numbers; the gekk uses the base flesh set for now.
