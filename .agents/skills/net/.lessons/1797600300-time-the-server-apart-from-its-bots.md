---
title: Bots as a load test's players: time the server's frame apart from the bots' thinking
date: 2026-11-15
scope: games/shooters_edge/net/qw_team.cpp, games/shooters_edge/labs/se_load_lab.cpp
concepts: [load, bots, server-frame, se-load, clock, measured, capacity]
---

When a game's bots are the clients of a load test they run in the same process as the server,
and their thinking (pathing, aim, the mirror of what they were sent) costs as much as the
server's own frame. A real server carries only its frame, so time `qw_frame` alone and the
clients' ticks apart (`frame_us_*` and `bots_us_mean`), or the capacity reads half what it is.
Read the clock only through a caller's function pointer and never branch on it; a test holds
that a timed match's digest equals an untimed one's.
