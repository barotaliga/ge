---
title: Price a quality stop by its parts first; the bill is rarely the pixels a stop would halve
date: 2026-11-06
scope: games/shooters_edge/app/ladder.cpp, games/shooters_edge/labs/gpu_se_timedemo_lab.cpp, engine/render/water.cpp, plugins/brush/app/bsp.wgsl, plugins/brush/app/bsp_views.cpp
concepts: [quality-ladder, stops, price-cvars, knockout, scissor, full-screen-pass, planar-mirror, pcss, capsules, clouds, noise]
---

- Stops guessed from what a pass does priced wrong: Shooter's Edge's first `medium` saved 0.31 ms
  where 1 was asked. `se-timedemo --price-cvars="label=cvars|..."` (each set between all-on runs,
  about 9 s a play at 1440p) found where each switch's milliseconds go before any stop was
  chosen; a scratch text cvar parsed in the code reaches parts no cvar has (remove it after).
- What it found, none of it the obvious knob: the casters' cost was their soft edges (hard from
  all eight lights: 0.02 of 0.35 ms) and the capsules (0.43); the mirror's was the map's every
  face drawn again (a quarter of its pixels saved 0.05 of 0.31); the cloud's was its passes (a
  quarter of the pixels saved nothing); the water's wet band, a full-screen triangle a pool, was
  0.26 ms for a waterline of pixels; the shadows inside the mirror, the detail waves and the
  rings' levels cost nothing.
- A full-screen pass that discards outside a known world volume: scissor it to the volume's
  corners projected (whole screen when a corner is behind the eye) and prove it byte for byte
  with `se-client --game=<packs>` views, a scratch switch drawing it whole beside it, and a run
  with the pass off to show the views can see it at all.
- A second view's clip-plane `discard` after the whole shade pays for every dropped fragment:
  test the plane first.
- Noise: one all-on run that spikes (the desktop) makes a run's noise 0.5 to 0.9 ms; sample
  `ioreg ... "Device Utilization %"` and run again. A text cvar takes one word: quote a value
  with spaces (`r_x "a=1,b=2"`), or the rest is dropped without a word.
- The 60000-byte cap on `bsp.wgsl`: look for duplicate functions in the resolved shader
  (`build/<preset>/gen/brush/app/bsp.wgsl`) before cutting code; `t_hash` was `hash12` byte for
  byte, and `kLuma` already lived in frame.wgsl.
