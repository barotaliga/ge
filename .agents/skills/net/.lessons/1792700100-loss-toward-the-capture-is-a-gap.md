---
title: Loss toward the capturing node is a gap in sequences, not a missing ack
date: 2026-10-22
scope: engine/net/analyze.cpp
concepts: [capture, loss, ack, sequence, stats]
---

A capture taken at a node sees every packet it SENDS and only the packets that REACH it. So
the acks can settle loss one way only, on the node's own sends. A packet lost on its way in
never appears, is never acked, and an ack-based count reads 0% however bad the link is (the
first `ge net stats` on the lab's bad.pcap showed 0.00% in beside 22 missing sequences).
Measure loss toward the capture from the gaps between the first and newest sequence seen,
less one for each wrap (sequence 0 is never sent).
