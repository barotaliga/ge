---
title: A recording keeps the eye, never the view; rebuild the first game's bob, roll and kick from the frames, and count blows, not frames
date: 2026-11-13
scope: games/shooters_edge/client/demo_eyes.cpp, games/shooters_edge/client/demo_client.cpp, games/shooters_edge/labs/se_demo_eyes_lab.cpp
concepts: [demo-eyes, classic-view, demo, multiview, joined-server, velocity-estimate, ground-trace, blows]
---

A first-game demo and a joined server's stream carry the player's place and look only, so the
window draws a flat eye unless it runs the classic rig on a subject built from the frames. What
the rig needs comes from different places per source: a joined game's prediction
(`Remote::ahead`: velocity, `ground >= 0`), a client recording's player state (its velocity
fields, a part left out is 0), and for a multiview recording the change of place between the last
two frames READ (never the drawn, interpolated state, whose time does not move between frames).
The ground is the level's player hull (`spec.models[0].hulls[1]`) traced a couple of units under
the origin. The damage message is per frame, and the window, the video and the lab read several
frames between two pictures, so a per-frame list loses blows: keep the last few with a running
count (`DemoState::blows_taken`) and let the view take what it has not seen. The sequel's demos
carry their rules' view (`kick`): leave them as read.
