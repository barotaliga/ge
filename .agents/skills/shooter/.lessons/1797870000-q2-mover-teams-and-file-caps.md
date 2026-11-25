---
title: Quake II mover teams move as one; split near-cap files by vocabulary headers
date: 2026-11-20
scope: games/shooters_edge/server/q2_map*.cpp
concepts: [team-movers, door-team, train-team, blocked, undo, line-cap, precheck]
---

Doors and trains sharing `team` have one `team_master` (`link_doors`); `map_frame` takes a
`TeamFrame` snapshot at the master (only while a part moves) and, when any part is blocked,
`team_undo` puts every entity, player move and part back, slides every part's think a frame and
skips the later parts; the parts step only after the whole team moved. Only the master sounds
(`door_sample` returns null for the rest). When a file sits at the 2000-line cap, move its enums
and small structs into a `*_kinds.hpp` in a named namespace and `using namespace` it (the
`q2map`, `roster` pattern) before adding code. Run precheck once at a time: two at once write the
same static library and `ranlib` calls it a malformed archive; a full precheck reaching 100+ labs
can pass 30 minutes, so give its background run a two-hour limit.
