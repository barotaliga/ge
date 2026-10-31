---
title: A fog's glow in a classic map's room must be in the day's units, tens of thousands of cd/m^2
date: 2026-10-31
scope: plugins/brush/app/bsp_fog.hpp, plugins/brush/labs/gpu_bsp_fog_lab.cpp
concepts: [fog, glow, lava-haze, exposure, white-luminance, units, sky-share, sun-map]
---

The v2 renderer lights a classic map's rooms in a white face's units (a lightmap's 1 is a white face under the open day, times `brightness`). So a lamp-lit hall sits at the day's luminance, and the eye is exposed for it. A glowing fog volume's `glow` is physical, cd/m^2 a metre of extinction. A glow chosen for a night on the snow (about 1 cd/m^2/m in Fall Line's fog lab) adds nothing visible. The haze over lava started to show at about 1e4, and a flat orange slab hid the melt at 4e4. `FogRows` uses 25000 (red), set by eye on the fixture's bright store and the pack's dim start map together. Tune it on both, because one map alone fits the wrong exposure.

The fog's sky light is unshadowed (the inject has no zenith map), so a closed room's dust glowed evenly blue-grey until `FogVolume::sky` scaled it. Measure the share by rays straight up through the scene's faces, not by leaf contents: the fixture's yard has a sky face for a ceiling but no sky leaf.

To find views of a pack's lava without guessing, run the lab with `--only=zzz` (no pictures). Its table lists every volume's centre and size, and a view standing a few metres back from the centre, pitched down, usually lands in the room.
