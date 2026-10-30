---
type: Added
title: Shooter's Edge's first playable window
---

`shooters_edge` opens a window on macOS (Metal) and Linux (Vulkan) and plays a map from your own packs (`-basedir <dir> +map dm6`): the baseline mod runs as a listen server in the same process, the player looks with the raw mouse and moves, jumps, fires and switches weapons, the map is drawn in its classic look from the first-person rig with the HUD, the console's `map` changes the map, and rockets and their blasts are marked; `tests/shooters_edge_listen_test.cpp` holds a rocket jump headless and the `se-client` lab draws the window's frame.
