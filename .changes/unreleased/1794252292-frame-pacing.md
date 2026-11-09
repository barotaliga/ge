---
type: Added
title: Frame pacing a player can set
---

A window's frames now meet the display evenly and with less lag, and the player can tune how: `vid_vsync`, `cl_maxfps`, `cl_maxfps_hz` (the cap as a multiple of the display's refresh), `vid_framelatency` (frames the GPU may queue; the swap chain keeps two images), `vid_steady` (at vsync the game moves in whole refreshes, so motion is even whenever a frame starts), `scr_showfps 1|2` (the rate, and a frame-time graph) and `framestats` (one line of rate, percentiles, jitter and hitches). Shooter's Edge uses them; the `pacing` lab compares every strategy's lag and judder.
