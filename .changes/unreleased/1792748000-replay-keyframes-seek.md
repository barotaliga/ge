---
type: Added
title: Replays seek anywhere off their keyframes
---

`engine/replay` (`ge::replay`) watches a run again from any moment: a game that implements `Run::clone` gets keyframes kept every few seconds and thinned with age (`Keeper`, its `KeepPolicy`), a player that seeks a run's input log to any step from the nearest keyframe in a budget of steps a frame (`Player`), and a video player's transport (`Transport`: play, pause, four speeds, skip, frame step). A run sought to a step has the digest of the run played straight there. Fall Line's run clones itself; the `replay` lab holds the seek and measures what a keyframe costs.
