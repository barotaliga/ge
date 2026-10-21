---
title: A filter's gain read off a sampled peak misses the crest; measure it by RMS
date: 2026-10-21
scope: tests/audio_dsp_test.cpp, engine/audio/meter.hpp
concepts: [filter, svf, test, meter]
---

A sine near the cutoff sampled at 48 kHz rarely lands a sample on its crest, so a gain
read as `peak(out) / peak(in)` comes out a few percent low and a highpass check at the
corner fails for no reason. Compare RMS over whole cycles instead (`gain_at` in
`tests/audio_dsp_test.cpp`). The same holds for any level claim in a test: the meter's
`loudest_db` is a 10 ms RMS for this reason.
