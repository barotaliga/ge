---
type: Added
title: Moving ground, pushes, jump pads, trampolines and ladders for character movement
---

`ge::move` carries a body on moving ground (a platform, a lift: its velocity is relative to the ground while it stands, and leaving the ground keeps the ground's speed), shoves it with pushers (`carry`, which refuses a crush), knocks it back (`push`), launches it on a jump pad's arc to a target (`launch`, `arc_velocity`), bounces it off a trampoline's surface, and climbs ladders (`ladderspeed`, `ladderjump`, `ladderprobe`). A trace's hit names the solid, its velocity, its surface flags and its bounce; the box world's brushes carry them and `move_brush` moves one. `move-trace` shows seven new scenarios.
