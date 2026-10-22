---
title: Over a stream link the connection still counts loss and resends
date: 2026-10-22
scope: engine/net/connection.cpp, engine/net/conditions.cpp
concepts: [tcp, stream, head-of-line, loss, resend, rto]
---

A stream link loses nothing, yet `net-link --transport=tcp` reports a few percent "loss" and
more resends than over datagrams. This is not a bug. A drop on a stream arrives a resend
timeout (200 ms or more) late, and every packet behind it waits. The connection's own timeout
(SRTT + 4 RTTVAR) fires first, so its reliable messages are sent again. A packet held longer
than the audit's expiry is also counted lost, even though it arrives later.

Read loss over TCP as head-of-line delay. The proof is the snapshots: their share is 100% on
a stream, and the test `traffic_big_messages_arrive_whole_over_datagrams_and_streams` holds it
there. Do not "fix" the extra resends by special-casing streams inside the connection.
