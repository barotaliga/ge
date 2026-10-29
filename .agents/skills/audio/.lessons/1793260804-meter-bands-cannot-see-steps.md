---
title: The meter's octave bands cannot show a resampler's steps; measure the error against the exact signal
date: 2026-10-29
scope: engine/lab/labs/audio_samples_lab.cpp, tests/audio_channels_test.cpp, engine/audio/meter.hpp
concepts: [meter, resample, interpolation, sampled-voice, test]
---

The meter's bands are the synth's own state-variable band passes, about an octave wide with
12 dB an octave skirts, so a pure 1 kHz sine already reads only 14 dB down in the 250 Hz and
4 kHz bands and 21 dB down at 8 kHz, whatever the interpolation. A "the bands away from the
tone are 30 or 40 dB down" check fails on a perfect resampler. To judge a resampler, render a
known tone and compare it sample by sample with the same tone computed at 48 kHz: the error's
power under the tone's (`rate-<hz>.error-db` in the `samples` lab: Catmull-Rom gives about -44
dB at 11 kHz, -65 at 22 kHz, -84 at 44 kHz). The bands still answer where the energy is.
