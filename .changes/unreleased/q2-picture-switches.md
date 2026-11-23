---
type: Added
title: Quake II's picture settings for particles, beams, the sky, textures, fog and the status bar
---

Quake II now has `gl_partscale`, `gl_partstyle` and `gl_partshape` for its particles,
`gl_beamstyle` for lasers and the rail's beam, `gl_drawsky` and `gl_bilerp_skies` for the sky,
`gl_saturation`, `gl_invert` and `gl_anisotropy` for the walls' and floors' textures,
`gl_waterwarp` to ripple the view under water, `gl_fog` to turn fog off, and `gl_bilerp_pics`
and `gl_gamma_scale_pics` for the status bar's pictures. The engine's particles gain shapes and
an additive blend, and its post pass gains a ripple, for any game.
