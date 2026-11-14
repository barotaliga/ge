---
title: A classic client needs a packet back for each of its own, or its view stops
date: 2026-11-14
scope: games/shooters_edge/net/classic_tv_udp.cpp
concepts: [classic-tv, face, prediction, frame-rate, view-frozen, black-box]
---

A face that sent a frame only when its source had a new one worked for a multiview recording (77
frames a second) and froze an existing client's view on a single-player recording (about 10 a
second): the HUD, the prints and the stats kept coming, the view stayed put. A classic client
stops moving its view when the server's packets fall too far behind its own. The face now answers
every viewer packet with the newest frame (`udp_tv_frame`). Our own test client did not show it;
only the black-box run did, so a source slower than a client's rate needs a run with a real client.
