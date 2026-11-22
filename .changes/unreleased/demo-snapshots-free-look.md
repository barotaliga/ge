---
type: Added
title: Demos seek back from snapshots, SHIFT looks around, and r_reload, soundlist, net_restart and recycle in the window
---

A played demo keeps a snapshot of itself every `demo_snapshots` seconds (Quake II's `cl_demosnaps`), so a jump back starts from the nearest one instead of the beginning. Holding SHIFT over a playing demo turns the view with the mouse until it is let go. In the window, `r_reload` reads the level's textures and models again, `soundlist` lists the sounds loaded, `quit` and `recycle` take a reason a hosted game's players are told, and `net_restart` opens a hosted game's port again.
