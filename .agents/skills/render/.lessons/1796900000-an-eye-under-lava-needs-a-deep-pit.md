---
title: An eye under a classic map's lava needs a pit deeper than a player stands
date: 2026-11-09
scope: engine/render/liquid_under.wgsl, games/shooters_edge/client/world_extras.cpp
concepts: [lava, from-below, eye-under, under-reach, goo, se-client, views, start-map]
---

To picture the view from under lava on a real map, put the eye under the level, not just the
feet. e1m7's lava is 32 units deep, so a player standing in it keeps his eyes 14 units over the
surface and the view from below never draws. The shareware `start` map's pit is 128 units deep:
`se-client --game=<id1> --map=start --views="1544,2470,-104,90,0;1544,2420,-104,90,-35"` stands the
player on its floor with the eyes 0.6 m under. Find such pits by reading the map's leaves for
contents -5 (lava), -4 (slime) and taking the deepest boxes. A map unit is 0.025 m
(`brush::kUnitM`), so a 3 m reach is 120 units.

The player asked for lava as a see-through goo from inside but the surface overhead opaque ("just
hell"): a see-through skin read as wrong. Keep a liquid's surface from below its own look, and let
only the body's volume be a game's choice.
