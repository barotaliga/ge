---
title: A very bright thin light is capped on screen, kept out of TAA and sharpening, and glows below a blackbody
date: 2026-10-28
scope: engine/render/taa.wgsl, engine/render/post.wgsl, engine/render/bolts.wgsl, engine/render/beams.wgsl, engine/render/explosions.wgsl, engine/render/muzzle.hpp, games/shooters_edge/app/effects.cpp
concepts: [taa, sharpen, black-rim, lightning, beam, emissive, bloom, blackbody, kelvin, explosion, knobs, raymarch]
---

- A lightning channel's dark rim came from two filters: TAA's clip box widened by the channel
  clipped nothing, and sharpening's negative lobe in linear light pulled neighbours black. A
  flashed frame takes no history, and sharpening works on light squashed to 0..1. Find such a
  culprit by turning filters off one at a time and printing a pixel row across the edge.
- An emitter near the eye (a first-person beam) is capped in pixels (sheath a thirtieth of the
  height, core a hundredth), keeping brightness, with no forks in the first 1.5 m. A walked helix
  needs three steps a quarter turn, and fork chance scales with the step.
- Gas at flame heat at full blackbody luminance blows out white: a per-instance `glow` (about
  0.05 for a muzzle flash) keeps the orange. At 2300 K a spark outshines a torch-lit room 1e5
  times; explosions near 1650 K, embers 1400 K read as fire under bloom.
- A stretched volume intersects its stretched ellipsoid, not the sphere, or the steps land in air.
- An effect several games share is one code over named, clamped knobs with presets, every knob in
  the per-instance record; a lab sweep of each knob's two ends proves its name.
