---
title: A classic held weapon's firing frames can be several animations; follow the mod's weapon frame
date: 2026-11-09
scope: games/shooters_edge/view/hand.cpp, games/shooters_edge/client/classic_adapter.cpp, plugins/classicenhance/weapons.cpp
concepts: [axe, swing, weaponframe, held-clip, takes, viewmodel, fire-clip, classic-weapons]
---

The axe's file holds two swings back to back (frames 1-4 and 5-8). Treating every frame after the
rest frame as one `fire` clip made each swing play both. The program picks the swing (random, even
odds) and writes it as the player's `weaponframe` in the same frame `attack_finished` moves, so the
client reads it there (`step_held`'s `fire_frame`) and plays the take holding that frame; only a
mod that never sets `weaponframe` (our baseline) leaves the hand to pick. Which weapons are split is
`ClassicFire::takes`. To see it on a real map: `se-client --game=<dir>/id1 --map=e1m1
--progs=pack --swings=16` (the pack's own program; the arena variant never sets the frame).
