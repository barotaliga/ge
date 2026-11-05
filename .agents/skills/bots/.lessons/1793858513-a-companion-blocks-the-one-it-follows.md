---
title: A companion standing still at the leader's shoulder blocks him the moment he walks off
date: 2026-11-05
scope: games/shooters_edge/bots/companion.cpp, games/shooters_edge/bots/coop_run.cpp
concepts: [companion, coop, follow, give-way, bored, idle, leader, still]
---

The first co-op runs passed every check but one: the companion stayed "bored" a second after the
leader's tour had him walking on. He was not walking: the companion stood between him and the
doorway, and his pilot pushed against it for over a second, moving under the 48 units that ended
"standing still". Two fixes, both needed. Read the leader's standing still off his speed (any
move faster than 60 units/s ends it), not only off a distance from where he stopped, so a leader
pressing against a body is not "still". And give way in every mode: ahead of him and within 44
units of his line, step aside off it. A follower test that scripts the leader must check that he
reached each leg's end in about the time the walk takes, or a blocked leader reads as a bored
companion. Likewise, a button pressed "for him" must wait until he stops near it: pressed as he
walked past, it opened the door before he got there, which reads as playing the level for him.
