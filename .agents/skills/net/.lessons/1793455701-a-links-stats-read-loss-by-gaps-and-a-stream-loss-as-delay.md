---
title: Loss toward a capture is a gap in sequences, and loss over a stream is head-of-line delay
date: 2026-10-22
scope: engine/net/analyze.cpp, engine/net/connection.cpp, engine/net/conditions.cpp
concepts: [capture, loss, ack, sequence, stats, tcp, stream, head-of-line, resend, rto]
---

- A capture at a node sees every packet it SENDS and only those that REACH it, so acks settle loss
  on its own sends alone; an ack-based count reads 0 % for loss toward it (22 missing sequences
  showed 0.00 %). Measure that direction by the gaps between the first and newest sequence seen,
  less one a wrap (sequence 0 is never sent).
- A stream link loses nothing, yet `net-link --transport=tcp` shows a few percent "loss" and extra
  resends: a drop arrives a resend timeout (200 ms or more) late, the connection's own timeout
  (SRTT + 4 RTTVAR) fires first, and a packet held past the audit's expiry counts as lost. Read it
  as head-of-line delay; the snapshots' share stays 100 % (held by
  `traffic_big_messages_arrive_whole_over_datagrams_and_streams`). Do not special-case streams
  inside the connection.
