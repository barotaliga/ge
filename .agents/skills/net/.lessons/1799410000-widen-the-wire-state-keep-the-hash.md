---
title: Widen a wire state struct without moving a digest by hashing the old widths
date: 2026-11-23
scope: games/shooters_edge/server/q2_wire.hpp
concepts: [q2, extended-limits, wire-state, digest, hash, widen]
---

Quake II's extended limits needed 32-bit origins and 64 stats in `Q2EntityState` and
`Q2PlayerState`. The structs feed `native_q2_hash` (which mixes raw bytes by `sizeof`) and a
`memcmp` over the stats in the multiview writer, so widening them silently changed every hosted
game's digest and the writer's change test. Hash the values at the ordinary widths (16-bit origins,
the first 32 stats) and compare only the first 32 stats where the ordinary wire carries 32: the
ordinary wire's bytes and every digest stay put, and only the extended path sees the wider fields.
Grep the repository for `sizeof <struct>.<field>` and `memcmp` on the fields before widening.
