---
type: Changed
title: Each classic game keeps its own meaning of a shared setting name
---

The console registers its settings in layers: the engine's, the shooter kit's, the classic picture's, Shooter's Edge's own, and the loaded game's. The loaded game decides what a name means: in the sequel `crosshair 1`, `2` and `3` pick its own crosshair pictures, `scr_centertime` defaults to 2.5 seconds, and its configs' `vid_gamma`, `s_volume`, `gl_dynamic`, `gl_lightmap` and `cl_gun` reach the matching settings; loading the first game again brings its own back. A `ce_` switch set in a config or the console now also reaches classic enhance's bake.
