---
title: A lab on a game's own maps sizes its movers as the window does, or the doors fault
date: 2026-11-21
scope: games/shooters_edge/labs
concepts: [wide-models, campaign-options, coop, sweep, lab, doors, real-maps]
---

A lab that runs a campaign on a game's own packs sets `CampaignOptions::wide_models` (and
`registered` from the packs) as the window does. Without it the brush models keep their built
bounds, the program's door linking finds different neighbours, and e2m1 faults with "cross
connected doors" at the start: a fault of the lab, not of the game. Before chasing a program fault
a lab shows on real maps, check that the lab's options match the window's.

To check co-op on every map quickly, put the players where things happen instead of letting bots
find the way: `se-coop-sweep` moves them onto each key and into each exit and kills with the
program's own damage (a `kill` command respawns at once in Quake, so it never shows a death).
