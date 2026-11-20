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
the launchers' USE (held 46 / 25 %, fired 17.8 / 9.3 a minute held), not the jumps or the aim
point. Read what is fired, and how often while held, before studying how it is aimed.

A multiview recording leaves a player out of a frame's list a while: a player not sent keeps his
state (`present` false is not a death); a burst in the frame that kills is put to the one it
killed (he is dead in that frame).
