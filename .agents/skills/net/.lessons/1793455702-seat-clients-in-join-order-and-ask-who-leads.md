---
title: Over a jittery link the seats are who arrived first, so a trial seats clients one at a time and asks who leads
date: 2026-10-22
scope: engine/net/session_host.cpp, engine/net/voice_trial.cpp, engine/lab/labs/net_session_lab.cpp
concepts: [session, leader, seat, join-order, jitter, trial, preset, lab-script]
---

When every client joins at once, jitter and handshake retries decide who reaches the host first, and
the first SEATED player leads a dedicated server. The session lab's kick and phase changes sent from
"client 0" were refused `not_leader` on wifi and bad; the voice trial's seats, teams and routes
differed from preset to preset, so its rows compared different sessions (one ear heard nobody).
Ask the clients' own mirrors who leads (`players[me].leader`), and in a trial compared across
presets join the next client only once the one before is seated, so the presets differ only by
what the link does.
