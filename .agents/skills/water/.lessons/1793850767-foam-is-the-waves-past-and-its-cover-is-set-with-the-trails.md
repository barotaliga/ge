---
title: Foam is the waves' own past, and its threshold is set with the trails counted
date: 2026-11-05
scope: engine/render/water.cpp, engine/render/water.wgsl, engine/render/water_shore.wgsl
concepts: [water, foam, whitecaps, jacobian, surf, wet-band, wgsl, embed-cap, lab]
---

Foam with history needs no texture: look back at a few moments over the last seconds, at the
places the surface's drift brought the water from, and keep the strongest fold or surf found,
faded by its age. It is a pure function of place and clock, so a lab's single frame, a cut and a
replay's seek all show the trails.

Set the fold's threshold from the whitecap law, but count the trails. Taking the maximum over ten
moments spreads the foam several times wider than one moment's fold, so a threshold calibrated to
the whole cover whitened half the gale's picture. The share breaking now is about 0.15 of the
cover. Fold the detail band too, down to about 1.5 m. Folds of the laid band alone are a long
wave's size, and they whiten whole crests as smooth sheets instead of scattered patches. Average
the lace far off as the share itself: averaging it lower turns distant patches into grey smudges.

`water_surface.wgsl` is shared by `water.wgsl` and `water_below.wgsl`, and the second is near the
60 KB embedding cap. A function only the view from above uses belongs in `water.wgsl`, or the
build fails on `water_below`.
