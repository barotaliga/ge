---
type: Changed
title: Shooter's Edge's benchmark times the machine, not the display, and names its dearest passes
---

The window's `benchmark` draws every frame but shows one in 33 ms, drawing the rest offscreen, so a window whose present waits for the display (macOS, whatever the present mode) no longer holds the frame rate to the refresh; the report adds the frame's median GPU span and its dearest passes. `vid_fullscreen` (the menu's VIDEO ▸ FULLSCREEN) puts the window over the whole screen at the desktop's mode. The modern look's shadow maps are kept between frames: a light's cube is drawn when it comes into reach, and the sun's and sky's maps when their camera moves, instead of up to 192 cube faces and two 2048² maps of the whole map every frame (on the software adapter, demo1 at 640x360 went from 1.8 to 7.6 fps). The `se-timedemo` lab charts the GPU time of every pass (`--look=classic|modern`).
