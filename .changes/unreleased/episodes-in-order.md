---
type: Changed
title: the start map opens the episodes in order
---

Shooter's Edge's campaign now opens the classic start map's episodes one at a time, alone as in coop: episode 1 first, each next one once the one before is done, and a done episode's hall shuts. The bars in front of an episode not yet open stay down and tell the player which episode opens them. What you have finished is kept in the archived `sv_episodes_done` (a bit an episode), so it outlives the session and a new game; `sv_episodes_done 0` opens episode 1 again. It works with the pack's own program as well as ours.
