---
type: Changed
title: Played demos move smoothly
---

Shooter's Edge draws a played demo between its last two frames, in the window's `playdemo` and in `-demovideo`: players, missiles, doors and the view move evenly at any display or video rate instead of repeating a frame and jumping the next, and a teleport, a respawn or a gib is not dragged across. `demo_lerp 0` draws each frame as read; the `se-demo-smooth` lab measures the difference.
