---
title: Thunder spreads each channel segment over its arrival span, never one bin
date: 2026-10-28
scope: engine/wind/thunder.cpp, engine/wind/lightning.cpp
concepts: [thunder, lightning, channel, envelope, rumble, frame, light]
---

A 60 m segment of a lightning channel high over a listener spans up to 60 m of distance, which
is 0.17 s of arrival. Binning each segment's sound at its midpoint's arrival made the envelope a
comb of 10 ms ticks with silent bins between, and the rendered thunder clicked. `hear_thunder`
spreads a segment's intensity evenly over the bins from its near end's arrival to its far end's,
and the envelope rumbles continuously. The same rule holds for the light: a stroke's 2 ms
spike must be integrated over the frame's window (`light_over`), never sampled at the frame's
moment, or most strokes fall between 60 Hz frames.
