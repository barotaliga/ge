---
title: Particles collide with the last frame's depth; prove it by marking the bounces, not by stills
date: 2026-11-05
scope: engine/render/particles
concepts: [particles, collision, depth-buffer, screen-space, sparks, debris, brushes, floor]
---

The particle sim runs before anything is drawn, so `targets.depth_view` still holds the last
frame's depth. Keep the camera that drew it (`prepare_particles`'s view overload) and bind the
depth to the sim when the targets are bound. Stills cannot show sparks bouncing, because the smoke
covers them. What told the truth was a throwaway count of bounces in a padding field, read back with
`world_particles`: up to 8 bounces each, coming to rest at the floor's height. Two traps on the way.
A rocket's blast point lies about 4 units inside the floor it struck, so a floor trace must start
over it. And a lift's floor is a brush entity: the world model's hull lacks it, so trace with
`trace_move` against the server. What is flung out of the view over another floor still falls
through; that is screen-space collision's limit, and `se-weapon-fx`'s `-sunk` metric counts it.
