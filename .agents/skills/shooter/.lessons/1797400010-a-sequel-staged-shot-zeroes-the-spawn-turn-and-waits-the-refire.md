---
title: A staged shot on the sequel's runner zeroes the spawn's turn and waits out the refire
date: 2026-11-12
scope: games/shooters_edge/labs/se_midair_lab.cpp, tests/shooters_edge_midair_test.cpp, games/shooters_edge/server/sequel_fight.cpp
concepts: [sequel, staged-shot, delta-angles, refire, rocket, knockback, midair, scenario]
---

Placing players by hand on the sequel's runner: a spawn turns the view through `delta_angles`
(and `forced`), so on a real map, where the spawn's yaw is not 0, a command's yaw is added to the
spawn's and every staged rocket flies the wrong way; zero both when you place a player. The
rocket launcher refires after about 0.8 s, so a shot pressed for one command right after the last
is silently not fired: tick a second before each shot. A direct rocket hit on the sequel's rules
pushes nothing (its knockback is 0 and the blast leaves the one it hit out), so read a hit's push
from a blast at the feet, and detect a blow by the armour's wear, not by the speed.
