---
title: To zoom a 2D overview into a 3D view, lay the picture where the moving eye sees the plan
date: 2026-11-19
scope: engine/overview/fly.cpp
concepts: [live-map, fly, zoom, overview, fly-plan-box, top-pose-onto, blend, arrival-flash]
---

A fly from a map to an eye that fades to the 3D level first and then dives shows walls and another
background the whole way down. Keep the picture instead: each frame, `fly_plan_box` (the inverse
of `top_pose_onto`) says where the diving eye sees the plan's box, at a plane that sinks from a
third of the level's height to the followed player's floor as it nears him, and the picture is
drawn exactly there (`LiveView::place`). Hand over to the 3D level only near the ground
(`FlyTimes::blend_from`, `blend_to`), before the tilt, where both show the same floor.

The camera reaching the player is no teleport, but the catch-up's fast frames and a respawn close to
the landing can raise his own arrival flash: switch it off while the fly runs and an arrival's
length after (`watch_fx`), rather than hunting the one frame that raised it.
