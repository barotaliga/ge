---
title: A picture the hub draws once is kept by a key of its input and of the source that draws it
date: 2026-11-22
scope: engine/hub/stills.cpp
concepts: [hub, stills, cache-key, source-stamp, match-card, invalidate]
---

The maps' stills under the match cards are drawn once (about 20 s a map in the debug build on
lavapipe) and kept on disk, never committed. Their key is the game's (`HubStills::key`): the map
file's bytes, `ge_source_stamp(... SHADERS DIRS ...)` over every directory whose code draws them
(the WGSL too, so a shader change counts), and the size. A new build of the renderer then draws
them again with no version number to bump, and the old file is deleted beside the new one. The
board over the still is composed per request (`compose_card`) and not cached: it is as live as the
poller, and engine/hub is not in the stamp, so changing the board never redraws a still.

A map's name comes from a server's record: `still_name_ok` before it ever reaches a path.
