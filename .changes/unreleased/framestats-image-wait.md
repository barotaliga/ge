---
type: Fixed
title: the vsync lag fix works in demos and joined games
---

`vid_vsync_lag_fix` now measures every frame the player watches, including a played demo, a game joined on a server and the title screen; before, it saw only the listen server's frames and sat idle elsewhere. `framestats` adds the time a frame waited for a free swap-chain image (`waits ... image`), so a frame held by the display reads apart from one held by its GPU cost, and the lag fix says when it is still measuring instead of reporting a 0 ms frame.
