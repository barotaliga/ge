---
title: Pace a rate-limited update, never burst it
date: 2026-10-23
scope: engine/services/discord.cpp
concepts: [discord, rate-limit, activity, pacing, coalesce, stale]
---

Discord takes five activity updates in 20 seconds. The first client sent while the sliding window
had room, so a game changing its line every half second spent all five in two seconds and then
showed a stale line for the rest of the window: the `discord` lab's `stale-max` was 15.0 s. Pacing
to one update every window / limit (4 s) and always sending the newest keeps every window within
the limit by construction and brought `stale-max` to 3.5 s with the same number of updates. For
any service limit of N calls per window whose calls replace one another (presence, a status
line), pace at window / N and coalesce; keep a burst only for calls that do not replace each other.
