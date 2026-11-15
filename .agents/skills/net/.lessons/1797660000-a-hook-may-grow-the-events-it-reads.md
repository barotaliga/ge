---
title: A feature's hook may add session events: read the list to its end as it grows
date: 2026-11-15
scope: engine/server/feature.cpp, engine/server/admin.cpp
concepts: [feature, hook, session-events, kick, disconnect, rcon]
---

The server's features read the frame's session events after `session_host_update`, and a hook
can act on the session in the middle of that walk: an rcon `kick` frees a seat, which pushes a
`left` event. Walking a count taken before the loop missed it, so the log never said who was
removed. Copy each event out and loop while `i < events.size()`, so every feature hears of a seat
its own act freed in the same frame (the vector may grow, so never hold a reference into it).
A trial's scripted commands also share the chat's bucket (5 lines, one a second): space a
player's lines so the bucket never runs dry, or a refused-for-rate line looks like a lost answer.
