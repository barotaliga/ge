---
title: A keyframe's cost is the models learning again, so make them learn fast
date: 2026-11-11
scope: engine/replay/codec.cpp, games/shooters_edge/client/demo_v2.cpp
concepts: [replay-v2, field-codec, keyframe, adaptive-probability, context-dilution, measure]
---

On a 20-minute 4on4, starting the field codec's models over every 10 s cost 25% against never
(2.315 MB against 1.845 MB): probabilities that moved 1/32 a bit took hundreds of values to learn
again each time. Letting a probability count the bits it has seen and move half, then a quarter,
down to 1/32 after fifteen, took the 10 s cost to 10% and the 30 s cost to 5%. Splitting a
model's context finer (a player's angle by its own and its neighbour's last residual, 256
contexts) made it worse, not better, because each context had too few values between keyframes:
measure every context change with the `replay-codec` lab's parts table before keeping it. The
lab's parts are counted in bits from the coder's range (`Pack::mark`), not from the bytes it has
flushed, which lag the field that spent them.
