---
title: A learned link coder keeps its models beside each reference, and short datagrams pay for the header
date: 2026-11-14
scope: engine/proxy/link_code.cpp
concepts: [proxy, link-codec, range-coder, adaptive-models, loss, frame-header]
---

An adaptive coder on a lossy link cannot carry one set of models from frame to frame: the receiver
misses frames, and the two ends' models part. Start each datagram's coding from the models as they
stood after its REFERENCE was coded (the datagram the other end confirmed), and keep a copy beside
every kept datagram at both ends (8.7 KB each at 17 contexts). Both ends then reach the same models
whatever was lost, and a frame whose reference is gone is dropped as no reference, never decoded
wrong. Check it with a test that loses frames both ways and counts zero broken frames.

Measure the body and the header apart before tuning the model: a classic datagram is under 60
bytes, so with the coded body at 0.4 of the game's bytes an eighteen-byte header still left the
link at the game's size. Cut the header first (16-bit numbers widened to the nearest the reader
knows, the reference as a distance back, the kind byte saying how the datagram is written, the
length coded inside the stream); position contexts for the datagram's first bytes moved nothing.
