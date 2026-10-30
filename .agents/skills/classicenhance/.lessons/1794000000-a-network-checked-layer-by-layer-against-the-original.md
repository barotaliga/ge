---
title: Check a restated network against the original's outputs; a GPU buffer too small reads as a wrong picture
date: 2026-10-30
scope: plugins/classicenhance/upscaler.cpp
concepts: [upscaler, network, transformer, reference, tolerance, gpu, buffer, clamp, seeded]
---

The upscaler was restated in C++ from the network's description and checked against the
original's output on two pictures (one not a whole number of windows) before anything else: it
agreed within 1.2e-6 the first time because every layer was compared, not looked at. A small
network of the same form with seeded random weights (`seeded_upscaler_weights`) runs the same
paths fast enough for a debug test, and the original's values for those weights pin it. On the
GPU, a map buffer sized for the embed width but written 64 channels wide (the small network's
embed is 32) gave a picture off by tenths with no error at all, because the GPU clamps an
out-of-range access. Bisect by running the network with groups and blocks removed on both
devices; size every buffer for the widest layer that writes it.
