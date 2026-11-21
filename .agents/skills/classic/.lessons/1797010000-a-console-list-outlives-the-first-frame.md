---
title: A list a console command fills must survive the server's first frame
date: 2026-11-21
scope: plugins/classic/protocol/masters.cpp, games/shooters_edge/net/qw_game.cpp, games/shooters_edge/net/q2_game.cpp
concepts: [setmaster, heartbeat, masters, first-frame, server-console]
---

A server's config runs `setmaster` before the first frame, and the first frame starts the
masters' clock. Resetting the whole model there (`m = Masters{}`) threw the list away, so a
master was pinged and never heard a heartbeat; and Quake II's server, its family set only at
that frame, took the list in Quake's form. Start the clock without touching the list, and give
the family when the server is made (`serve`), not when it first steps. The end-to-end tests
(a master on the simulated network hearing ping, heartbeat and shutdown) caught both; the model's
own test could not.
