---
title: Count a stream's loss by the second each packet was sent in, not the second it arrived
date: 2026-11-21
scope: engine/lab/labs/net_config_lab.cpp
concepts: [loss, measure, window, latency, lab, fake-link, chart]
---

A per-second loss chart that counts arrivals in the second they arrive shows negative loss and
spikes at every change, because a link's latency moves packets across the window's edge. Put the
second each packet was sent in into its payload and count arrivals against that second, run the
loop half a second past the last send so the last packets land, and the windows read the loss
alone (`net-config`'s fake-loss chart). Before building a cheat cvar for a link, look for the
transport's own `FakeLink` (`transport_fake`): it already puts a live game's sends through a
link's fates, so the cvar is only its binding.
