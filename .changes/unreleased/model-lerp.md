---
type: Added
title: Models animate smoothly between their frames
---

Every model in Shooter's Edge (players, monsters, items, flames, the held weapon) is now drawn between its animation frames instead of jumping from pose to pose ten times a second: the window blends the frame it left into the new one over the frames' time. In your own game the models' places are drawn between the server's frames too, so nothing judders when the server's rate and the screen's differ, and a walking monster glides through each stride instead of hopping. `r_lerpmodels 0` and `r_lerpmove 0` turn them off. The `se-model-lerp` lab films a monster both ways side by side and measures the judder.
