---
title: Rain's rings show only within a few metres and only where the water mirrors the sky
date: 2026-10-30
scope: engine/render/water.wgsl, engine/lab/labs/gpu_water_lab.cpp
concepts: [rain, rings, ripples, footprint, fresnel, lab-view, drop-spectrum, saturation]
---

A ring's crests are centimetres, so the pixel's footprint keeps them only near the eye: at
640 px a lake seen from 2.5 m at a low angle showed none, and the smoke's 256 px keeps a few.
Looking steeply down shows little either, because the water reflects 2 % there and a ring is
seen by the sky it mirrors. The view that reads is 1.5 m up, 0.5 rad down, under an overcast.
Count the ringing drops off the big end of the spectrum (2 mm and more), not all drops: every
rain past a drizzle lands hundreds of millimetre drops a m^2 a second, which saturates any cell
scheme and makes a light rain look like a heavy one.
