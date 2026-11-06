---
type: Added
title: The easy jump on any server
---

`cl_speedjump 1` now works on servers that do not run the easy jump themselves: the client strafes for the player (the jump let go in the air, the side key and a sent yaw that gain about half the best strafe's gain), so holding forward and jump hops for speed with less hand motion. The player's screen never turns and every fire command carries his real view. It is disclosed, never hidden: the user info says `cl_speedjump 1`, and a server that sets `pm_speedjump 0` turns it off. Our own servers now always state `pm_speedjump` in their server info.
