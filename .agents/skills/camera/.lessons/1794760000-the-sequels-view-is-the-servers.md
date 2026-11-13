---
title: On the sequel's rules the view's motion is the server's; the window only eases it, and a lab must hand blows to the rules
date: 2026-11-13
scope: plugins/shooter/sequel_view.cpp, games/shooters_edge/labs/gpu_se_view_lab.cpp
concepts: [sequel-view, view-offset, kick-angles, interpolation, se-view, brush-world, longest-run]
---

The first game's look works the bob, roll and kick out in the client; the sequel's rules work the
eye's offset and the kick angles out on the server each 0.1 s frame and send them, so a client
that adds its own bob on top doubles it. The window turns its row's bob, roll and kick off when the
player state says the rules set the view (`PlayerNumbers::view_set`) and eases the sent values
between rules frames. In the lab: a blow pushed into the window's `l.blows` does nothing on the
sequel (hand it to `sequel_view_blow` on the server's player); the sequel's view trails what was
sent by up to a frame, so check it by its own numbers, not frame by frame; a `br::Fixture` written
as version 38 has faces but no brushes (the sequel's player falls through it: build the hall with
`write_brush_world`); and `longest_run` must trace the sequel's map (`map_trace`), since the
classic server's edicts are empty there. Real maps cut a scripted strafe short, so hold only the
fixture to the numbers.
