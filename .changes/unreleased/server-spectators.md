---
type: Added
title: Spectators on the server itself
---

Every dedicated server on the server core can be watched from itself: `sv_tvport` (`--tvport=N`) gives it its own TV node, fed by its recording and held back by `sv_tvdelay` seconds (10 by default, never voted on), for up to `sv_tvmaxviewers` spectators. A spectator who comes late starts at the newest keyframe; a server listed on masters lists its TV beside it for the hub's Watch; the status line counts the spectators, and a stopping server sends them the match's end first.
