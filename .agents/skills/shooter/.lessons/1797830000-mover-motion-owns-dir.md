---
title: A Quake II mover's `dir` is the move's own; keep a mover's extra place in another field
date: 2026-11-20
scope: games/shooters_edge/server/q2_map.cpp
concepts: [mover, begin-move, func-door-secret, dir, mover-motion, line-cap, split]
---

`sh::begin_move` (shooter/mover_motion.hpp) writes the unit direction of the move it starts into
the entity's `dir`, so a value stored there at spawn is gone after the first move: the secret
door's second leg, kept in `dir`, sent it one unit sideways instead of along its arrow. A mover
that needs a third place keeps it where its class leaves a field unused (the secret door uses
`open_angles`, which only a turning door reads) and says so where it is set. Also: q2_map.cpp and
the client's world_fx.cpp both passed the 2000-line cap in one bundle; the trace through movers
moved to `q2_map_trace.cpp` and the lasers to `client/world_lasers.cpp` (a GPU-library file: it
goes in the client library's EXCLUDE regex and the `shooters_edge_client_gpu` list).
