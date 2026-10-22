---
title: A stopped layer is still counted until 0.15 s of render has passed
date: 2026-10-21
scope: engine/audio/synth.hpp, tests/skier_ride_bed_test.cpp
concepts: [layer, stop, rack, stats, test]
---

`Synth::stop` fades a layer over 0.02 s and frees its slot only once 0.15 s of audio has been
rendered after the stop, so `stats().layers` read straight after `rack_stop` (or after one
short render) still counts it. A test that checks a bed was torn down renders past the free
time first (12 renders of 800 frames is 0.2 s) and only then expects zero.
