---
title: A timeout runs on the server's clock, since the paused game's clock stops
date: 2026-11-15
scope: engine/server/timeout.hpp, games/shooters_edge/net/qw_game.cpp
concepts: [timeout, pause, clock, sv-time, now-us, kpausebyserver]
---

Shooter's Edge's rules stop `sv.time` while paused, so anything that must end a pause cannot
count on the game's own clock: feed it the frame's `now_us` (`net_frame`). The rules' unpause
from a slot below 0 waits until no players are left (the classic rule), so the server's own
pause and unpause go through `kPauseByServer`, which both pauses and lets go whoever is on.
