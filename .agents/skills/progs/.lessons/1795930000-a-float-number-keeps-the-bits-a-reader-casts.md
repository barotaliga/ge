---
title: Keep the view's numbers as the program's floats; let each reader cast
date: 2026-11-04
scope: games/shooters_edge/server/rules_world.hpp, games/shooters_edge/bots/, games/shooters_edge/net/
concepts: [rules-world, player-numbers, bots, digests, float-bits]
---

The wire shows a whole number, but a bot weighs fractional health, armour and its type
(`fighter_strength`), so a view that held ints would move the bots' digests once they read
it. `PlayerNumbers` holds the program's floats bit for bit; the wire's reader truncates with
`static_cast<int>` as before. A view field that two readers share keeps the widest form either
reads.
