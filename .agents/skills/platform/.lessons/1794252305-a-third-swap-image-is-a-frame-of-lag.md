---
title: The swap chain's third image is a whole refresh of lag that no GPU wait removes
date: 2026-11-09
scope: engine/platform/pacing.hpp
concepts: [vsync, swap-chain, maximum-drawable-count, frames-in-flight, latency, judder, steady-clock]
---

Dawn leaves Apple's Metal layer at its own three drawables, so a vsynced window that draws faster
than the refresh fills them and every picture is ~25 ms old at 120 Hz. Waiting on the GPU's queue
(the frame gate) does not help on its own: finished frames still wait in the layer for the
display (the `pacing` lab's "3 images, 2 in flight" row stays at ~25 ms). Two images
(`platform::set_swap_images`, after every surface configure) plus the gate take it to ~16.6 ms.
The other half of smoothness is not the frame rate: a game stepped by the measured time between
frame starts moves unevenly at a perfect 120 fps, because the start wanders with the load while
the picture lands on the refresh; whole-refresh steps (`loop::SteadyClock`) remove that judder.
