---
title: A campaign rule written into the map's entities reaches the packs' own program too
date: 2026-11-10
scope: games/shooters_edge/server/campaign_episodes.cpp, games/shooters_edge/server/campaign.cpp
concepts: [episodes, start-map, bars, entity-text, serverflags, brush-model-size, door-link]
---

The window's NEW GAME runs the packs' own single-player program, so a rule written in our QC
(`episode_shut`) never reached it. The host owns two things every program reads: the map's entity
text and `serverflags` before the world spawns. Rewriting the start map's triggers (changed in
place, never added or removed, so entity numbers and saves hold) and setting the runes there gave
the episode order to any program. Look at the real map with the full game's packs before trusting
the picture in your head: episode 2's bars are two doors, and ours opened half of them on
approach because our inline models are a unit smaller than the classic engine's, so touching
doors do not join. Widening them is right but moves the set pieces and the coop goldens: its own
change.
