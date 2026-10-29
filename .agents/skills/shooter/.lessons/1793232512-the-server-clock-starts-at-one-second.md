---
title: The server's clock starts at start_time (1 s), a client's at 0; chart each on its own axis
date: 2026-10-29
scope: games/shooters_edge/sim/match.cpp, games/shooters_edge/labs/se_loop_lab.cpp
concepts: [server-time, client-time, start-time, chart, prediction, correction, se-loop]
---

`ServerSettings::start_time` is 1.0 s, so the match's events (server time) read 1 s later than
the clients' corrections (the match's millisecond timeline from 0). The rocket fired at
client 150 ms shows as server 1.195 s, and the 30-unit correction it causes sits at 0.2 s on the
corrections chart. To tie a correction to its cause, take the frames at `1.0 + t_ms / 1000 -
latency` (a probe over `run_match`'s `frames` does it in a minute) and name each axis's clock in
the chart's label, as `se-loop` does.
