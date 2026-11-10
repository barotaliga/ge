---
title: A very bright thin light is capped on screen, kept out of TAA and sharpening, and glows below a blackbody
date: 2026-10-28
scope: engine/render/taa.wgsl, engine/render/post.wgsl, engine/render/bolts.wgsl, engine/render/beams.wgsl, engine/render/explosions.wgsl, engine/render/muzzle.hpp, engine/render/emitters.cpp, plugins/brush/app/bsp_flames.cpp, plugins/brush/app/bsp_draw.cpp, games/shooters_edge/app/effects.cpp
concepts: [taa, sharpen, black-rim, lightning, beam, emissive, bloom, blackbody, kelvin, explosion, sparks, near-field, point-light, knobs]
---

- A lightning channel's dark rim was TAA's clip box (widened by the channel) and sharpening's
  negative lobe in linear light: a flashed frame takes no history, and sharpening works on light
  squashed to 0..1.
- An emitter near the eye (a first-person beam) is capped in pixels (sheath a thirtieth of the
  height, core a hundredth) with no forks in the first 1.5 m; a walked helix takes three steps a
  quarter turn.
- Gas at flame heat at full blackbody luminance blows out white: a per-instance `glow` (about
  0.05 for a muzzle flash) keeps the orange. Explosions near 1650 K, embers 1400 K, a wood fire's
  sparks 1900-2400 K; 3000 K reads as a lamp, and 100 sparks at a filament's heat summed by bloom
  made a brazier's white disc.
- A point light standing in for a classic light softens over a share of its half reach
  (`Options::near_soft`, 0.4).
- A stretched volume intersects its stretched ellipsoid, not the sphere.
- An effect several games share is one code over named, clamped knobs with presets; a lab sweep
  of each knob's two ends proves its name.
