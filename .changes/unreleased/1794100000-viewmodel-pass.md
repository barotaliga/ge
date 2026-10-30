---
type: Added
title: The view-model pass
---

`render/viewmodel.hpp` draws a first-person held weapon on the GPU after the world. It has its own lens and depth range, so a wall the player stands against never cuts it. Each pixel is lit the way `viewmodel::shade_held` lights it, plus the frame's clustered lamps and its sun (through the sun's shadow maps) and sky. A resolved twin draws the same pixels the way the CPU reference resolves them, and the `gpu-viewmodel` lab holds the two within a stated difference.
