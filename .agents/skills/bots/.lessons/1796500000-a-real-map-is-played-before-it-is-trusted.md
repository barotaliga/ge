---
title: A bot that passes the test hall stalls on a real map's set pieces: play the map, read where it stood
date: 2026-11-05
scope: games/shooters_edge/bots/way_through.cpp, games/shooters_edge/bots/coop_episode.cpp, plugins/shooter/pilot.cpp, games/shooters_edge/rules/common_map.cpp
concepts: [way-through, campaign, episode, real-map, set-piece, stuck, water, bridge, zombie, seeds]
---

The leader passed the coop hall at once and then stalled on every real map for a new reason: no
jump in the campaign program at all, a button only a shot presses, a door that sinks to a step
below the floor read as in the way, an item errand kept from a retreat freezing him at its shot
button, a jump lined up for again and again and never made, zombies fought to his death, trap
bars that start open standing shut, a pool that let him out on a ledge whose doors open from the
other side. None showed in a test map. What found each: `--plan=1`'s log of his changes of plan
with the pilot's state and the next links whenever a step was left, then the map's entities near
where he stood (dump them by position from the pack), then one temporary line a few seconds apart
with his command, health and the pilot's counters. Judge a fix over several seeds, never one:
the run is chaotic (a different first explore drops him in e1m1's water), so one seed passing or
failing says little; four of eight seeds is a number. And a program gap (a door kind, a button)
belongs to the campaign's builder too: fix it where the maps need it and tell that thread.
