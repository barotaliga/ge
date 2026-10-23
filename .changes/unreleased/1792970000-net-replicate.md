---
type: Added
title: Replication, the server's word (snapshots, prediction, interpolation, rewind)
---

`net/replicate.hpp` runs a server-authoritative world over a node: a game names its entities' fields (each quantized to its bits) and its step, and the server sends each client delta snapshots against the newest one it acked, only what interest lets it see, the stalest rows first under a byte budget. Clients send each input with the last few beside it, predict their own entity through the same step and smooth any correction, draw everyone else interpolated a few ticks behind, and the server rewinds any entity to the tick a client drew for lag compensation. The state is kept on the quantization grid, so a lossless link never corrects a prediction. The `net-replicate` lab measures 32 clients under each condition preset: bandwidth, corrections, dropped snapshots and how far a rewind lands from what was drawn.
