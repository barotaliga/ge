---
type: Added
title: profile times the window's frames part by part
---

The console's `profile [frames]` times the next frames (600 by default) part by part: the input, the server's frames and the bots' thinking in them, the effects, the world's preparing, encoding and submit, the acquire and the present, and the time in no part. It prints each part's median, 99th percentile, worst, share and heap allocations a frame, and the five slowest frames with what made each slow, so a stutter on the player's own machine names its cause. Any game's window gets it with its pacing (`engine/platform/pacing.hpp`); the counting is `engine/prof/frames.hpp`.
