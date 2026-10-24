---
title: A reel played off a fresh player steps the whole race again at its first cut
date: 2026-10-23
scope: games/fall_line/labs/replay_cut_lab.cpp, engine/replay/live.hpp, engine/replay/director.hpp
concepts: [reel, cut, keeper, held-frames, instant-replay, seek]
---

The `replay-cut` lab first played its reel through `Player(spec, log)`: that player holds only the
copies it takes itself, so its first cut deep into the race stepped from the start and held 23
frames in a row (42 in all). Playing the reel through `Live::watch(0)` (the live keeper lent)
took it to 2 frames held. Any after-the-run feature (a reel off the finish plate, a clip) gets its
player from the live recorder while it is still there; a player from a saved file pays one pass
on its first long seek, which an app hides behind a loading frame.
