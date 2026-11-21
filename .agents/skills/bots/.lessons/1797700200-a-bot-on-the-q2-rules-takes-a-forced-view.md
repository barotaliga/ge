---
title: A bot driving Quake II's rules takes the view the rules forced, or it walks the wrong way after a level change
date: 2026-11-15
scope: games/shooters_edge/bots/q2_unit.cpp, plugins/shooterbots/way_through.cpp, games/shooters_edge/server/q2_map.cpp
concepts: [forced-view, level-change, way-through, brush-box, q2]
---

Quake II's rules turn a player's view at a spawn (`Q2Player::forced`) and read the
command's angles relative to it, so a fighter that keeps its own yaw across a level change walks
sideways or back: copy the edict's `v_angle` into the fighter's yaw and the command's angles for
every forced frame (as `client/q2_party.cpp` does). Quake II's runner keeps no hulls in
`MapSpec::models`, so a plan that boxes a brush entity reads `MapSpec::brushes->models`; and its
world rows had no `absmin`/`absmax` for movers, which left every button's middle at the origin
(fixed in `map_rows`). A fixture of three halls found all three faults before the real maps did.
