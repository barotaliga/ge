---
title: Weapons fired a player-minute, by their sounds, find a fight gap before any aim study
date: 2026-11-20
scope: games/shooters_edge/labs/blast_read.hpp, games/shooters_edge/labs/se_people_lab.cpp
concepts: [launcher, rocket, grenade-launcher, shots-per-min, weapon-sounds, blast, knock-up, fight]
---

Asked to copy people's launcher fight (the dance, the feet, the air finish), the first read was
se-people's `shots-per-min.<sound>` against the bots': the rocket launcher (`weapons/sgun1.wav`)
8.2 against 2.35 a player-minute, the grenade launcher (`grenade.wav`) 1.28 against 0.007, the
nailgun (`rocket1i.wav`) 1.07 against 5.4. Jumps a fight-minute were even (37 / 40). The gap was
the launchers' USE (CARRIED 46 / 25 %, fired 17.8 / 9.3 a minute carrying it: `owned.*` is the
item bit, not the weapon up), not the jumps or the aim point. Read what is fired, and how often
while carried, before studying how it is aimed. People raise the launcher to fire and drop back
to the scattergun (held up 19 % of the time, 74 % the scattergun; 58.7 rockets a minute up): a
minute HOLDING it compares nothing (`held.*`, the active weapon's stat, is sound: 97.5 % of their
rockets left with it up).

A multiview recording leaves a player out of a frame's list a while: a player not sent keeps his
state (`present` false is not a death); a burst in the frame that kills is put to the one it
killed (he is dead in that frame).
