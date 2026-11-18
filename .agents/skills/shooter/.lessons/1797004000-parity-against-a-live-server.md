---
title: Parity against a live server lines up by the acknowledged command, on a map with one start
date: 2026-11-18
scope: games/shooters_edge/labs/se_parity_lab.cpp, plugins/brush/compile/write29.cpp
concepts: [se-parity, parity-hall, existing-server, acknowledged-sequence, clip-hull-order, map-compiler]
---

A player's move depends only on its commands, so two servers' paths line up when each frame's
own-player record is filed under the command its acknowledged sequence carried; wall time and
how packets met the server's frames then do not matter, and a server that acknowledges several
packets in one frame simply answers fewer commands. A match mod picks a random start, so the
map for it has exactly one (`content/maps/parity.plan`).

The first map our compiler built would not load on an existing server: each clip hull was
written with its root last, and a server takes the root's number as the hull's first node and
refuses a trace into a lower one. Our own reader walked it happily, so only a live server found
it; `tests/brush_compile_test.cpp` now holds the order.
