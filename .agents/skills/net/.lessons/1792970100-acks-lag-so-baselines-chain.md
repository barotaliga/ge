---
title: Acks lag, so delta snapshots chain on several baselines at once
date: 2026-10-23
scope: engine/net/replicate_server.cpp
concepts: [replicate, snapshot, delta, baseline, priority, budget, starvation]
---

Under a byte budget, the first replication pass reset a row's priority to zero when the row was
sent and grew it by one each snapshot it was left out. Movers fell 120 ticks behind on the
client, though every snapshot decoded. With a snapshot every two ticks and an ack two snapshots
late, snapshot N is written against N-2: odd and even snapshots form two chains of baselines. A
row sent in one chain reset its priority and so was starved in the other for ever, and the
client's newest copy flipped between a fresh and a stale value. The priority now reads the age of
the row in the baseline itself (`Row::fresh`), which is right whatever chain a snapshot is on.
Anything a snapshot decides per row must read the baseline, never per-client state that the
other chain also changes.
