---
title: A body sent over the network must carry every field the step reads, and the hand must read the prediction
date: 2026-11-21
scope: engine/move
concepts: [prediction, wire, pack-body, quantize, replicate, misprediction, lab, correction]
---

A predicted body is exact only when the row the network carries holds everything `step_move`
reads (the timers, the jump's button state, the ground's normal and velocity, the air jumps), so
test it by REPLAYING every scenario's step from `unpack_body(pack_body(prev))` against the real
next state (`tests/move_wire_test.cpp`), not by comparing fields one by one. In a networked lab,
drive the scripted hand off the client's own prediction (`unpack_body(rc->predicted)`), never the
server's body, as a player reads the screen: then a lossless link shows zero corrections to the
bit, and anything else on `lan` is a missing field. `ReplicateClient::last_error` is summed over
every field (flags and timers too), so measure a correction's distance yourself from the feet.
