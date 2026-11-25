---
type: Fixed
title: Joining a server or a demo's next level no longer freezes Shooter's Edge's window
---

Joining a server, a demo reaching its next level, or a server adding a model mid-game made the map's world again on the window's own thread, freezing it for seconds (2.8 s each on an M3 Max, twice on a join). The world is now read on a thread and built a step a frame, the old world drawn meanwhile when only the models changed; a timedemo still makes it before its next frame. The `se-load` lab also holds the server's heap allocations a frame to a budget, and `profile` names the world's build steps.
