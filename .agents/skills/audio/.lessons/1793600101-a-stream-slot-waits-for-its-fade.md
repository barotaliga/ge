---
title: A stopped stream's slot is reused only after the render thread frees it
date: 2026-10-29
scope: engine/audio/synth.cpp
concepts: [synth, stream, ring, spsc, stop, reuse, fade, threads]
---

A stream's ring has two owners: the game thread writes its head, the render thread its tail. If
`stream` handed out a slot the moment `stop` was called, samples fed for the new stream could be
discarded when the render thread applied the old stop and emptied the ring. The render thread
sets the slot's `stream_free` once the fade has run and the ring is emptied, and `stream` takes
only a slot that is both unused and free: a stop and a new stream in the same frame use two slots.
