---
title: A bot driving the sequel's rules takes the view the rules forced, or it walks the wrong way after a level change
date: 2026-11-15
scope: games/shooters_edge/bots/sequel_unit.cpp, games/shooters_edge/bots/way_through.cpp, games/shooters_edge/server/sequel_map.cpp
concepts: [forced-view, level-change, way-through, brush-box, sequel]
---

The sequel's rules turn a player's view at a spawn (`SequelPlayer::forced`) and read the
command's angles relative to it, so a fighter that keeps its own yaw across a level change walks
sideways or back: copy the edict's `v_angle` into the fighter's yaw and the command's angles for
every forced frame (as `client/sequel_party.cpp` does). The sequel's runner keeps no hulls in
`MapSpec::models`, so a plan that boxes a brush entity reads `MapSpec::brushes->models`; and its
world rows had no `absmin`/`absmax` for movers, which left every button's middle at the origin
(fixed in `map_rows`). A fixture of three halls found all three faults before the real maps did.
