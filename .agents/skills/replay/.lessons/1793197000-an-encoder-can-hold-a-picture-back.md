---
title: A picture fed to an encoder is still to come until its packet comes out
date: 2026-10-28
scope: engine/capture/interleave.cpp, engine/capture/live.cpp
concepts: [interleave, av1, encoder, packet, live-capture, time-order, worker, race]
---

The interleaver writes a sound packet once no picture still to come can be shown before it. With
libaom in realtime mode and several threads, the second picture's packet came out a call late,
so "the newest picture encoded" was not a floor: a sound packet at 80 ms went into the file
before the picture at 33 ms. `Interleave::fed` now records every picture handed to the encoder,
and the floor is the oldest one whose packet has not come out. In a capture fed from other
threads, read the "what is still waiting" snapshot (the oldest queued picture and the last stamp
handed out) together under the lock: read apart, the game thread races ahead between them and
the floor jumps past a picture it has not handed in yet. Feed a test faster than real time, so
the worker falls behind and the race shows.
