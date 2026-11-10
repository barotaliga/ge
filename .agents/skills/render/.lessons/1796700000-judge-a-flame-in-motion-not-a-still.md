---
title: Judge a flame by its motion: no frozen noise slid up, no whole-body pumping, no per-frame grain
date: 2026-11-09
scope: engine/render/flame.wgsl, engine/render/flame.cpp
concepts: [flame, fire, torch, motion, puff, buoyancy, taa, reactive, jitter, film]
---

A flame that looks right in a still can look wrong played: (1) a 3D noise translated up at one
speed through a fixed shape reads as a texture sliding; read the noise in the gas's own frame
(when it left the fuel, `born`) with an age that grows as sqrt(height), so licks stretch as they
speed up. (2) Scaling the whole flame with its light's flicker reads as pumping; let the puffs
(ring vortices at 1.5 / sqrt(width) Hz) swell and pinch it locally. (3) A fresh white jitter every frame on
a wholly reactive pass shows a new grain every frame; keep a fixed white hash a pixel stepped by
the golden ratio each frame and a couple of frames of TAA history (reactive 0.5). Interleaved
gradient noise is worse here: 14 steps through thin gas show its diagonal lattice in a still. Film it
(`se-client --film=60`) and look at a crop's frames side by side, not one picture.
