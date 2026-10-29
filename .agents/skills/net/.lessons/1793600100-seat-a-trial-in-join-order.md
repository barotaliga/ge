---
title: A trial compared across presets seats its clients one at a time
date: 2026-10-29
scope: engine/net/voice_trial.cpp
concepts: [session, seat, join-order, trial, preset, team, route, compare]
---

When every client joins at once, the link's jitter decides which reaches the host first, so the
seats, the teams and everything keyed by them (who a route names, who stands near whom) differ
from preset to preset, and a lab's rows compare different sessions. The voice trial first showed
one preset's ear hearing nobody at all. Join the next client only once the one before is seated:
the seats are then the join order's on every link, and the presets differ only by what the link
does.
