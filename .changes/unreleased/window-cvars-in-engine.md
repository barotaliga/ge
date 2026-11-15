---
type: Changed
title: The window's fullscreen, display and volume settings are the engine's
---

`vid_fullscreen`, `vid_display`, `vid_displays` and `volume` now come from the engine's platform layer, so every game with a window gets them the same way; Shooter's Edge binds them and keeps its own `scr_levelstats` beside them. The settings work as before.
