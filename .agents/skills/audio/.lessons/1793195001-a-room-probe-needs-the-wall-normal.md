---
title: A room probed by rays needs each hit's normal, and a tail's level must be measured
date: 2026-10-28
scope: engine/audio/room.hpp, engine/audio/reverb.hpp
concepts: [room, reverb, probe, mean-free-path, rt60, eyring, fdn, calibration, open-share]
---

A ray's cone covers d² dΩ of a wall only when it meets the wall square on; obliquely the patch is
d² dΩ / cos θ. Without the cosine a box's area comes out a third short and its mean free path
and Eyring decay half again too long (a 20 × 8 × 12 m hall read 11.6 m against 7.7 m), so the
world's ray callback reports the surface's normal and the probe divides by its cosine (held
under four times). The tail's level was derived on paper (a unit pulse's energy over the loop's
passes) and came out 5 dB loud against the rendered response; the constant is measured instead
(`tests/audio_reverb_test.cpp`), within 2 dB from 4 to 20 m and 0.5 to 2 s. Eyring alone also
gives an open field a 0.8 s "decay" off its grazing ground hits; the tail's level falls with the
closed share squared so open air builds no field. Read the `room` lab's table: the rendered
clap's decay against the probe's is the check that the two halves agree.
