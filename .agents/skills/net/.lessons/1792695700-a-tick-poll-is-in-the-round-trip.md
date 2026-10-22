---
title: A node that polls once a tick measures each way rounded up to whole ticks
date: 2026-10-22
scope: engine/net/connection.cpp, engine/net/traffic.cpp, engine/lab/labs/net_link_lab.cpp
concepts: [rtt, round-trip, tick, poll, ack-delay, model]
---

The first `net-link` run showed a 33 ms round trip on the `lan` preset (0.25 ms each way). The
links were right: the fake-traffic session updates and flushes every node once a 60 Hz tick, so a
packet sent on a tick is read on the first tick after it lands, each way. A round trip is
therefore twice the latency rounded UP to whole ticks (and, with jitter, up to twice the latency
plus the jitter rounded up). Hold a measured round trip to that range (the lab's `want` column,
`tests/net_traffic_test.cpp`'s bounds), not to twice the latency; a round trip past the range is
a queue. Do not subtract the poll from the stats: a game feels it too.
