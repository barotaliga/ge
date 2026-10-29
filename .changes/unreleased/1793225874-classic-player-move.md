---
type: Added
title: The classic player move over clip hulls
---

`plugins/shooter` (new) moves a first-person player the way the classic game does: one command in 32-bit floats over clip hulls, with every move type (normal, dead, fly, both spectators, none, lock), every server setting at the modern defaults (gravity, stop speed, max speeds, the accelerations and frictions, `pm_ktjump`, `pm_bunnyspeedcap`, `pm_slidefix`, `pm_airstep`, `pm_pground`), the unstick, the water level and the water jump, edge friction, the 30 unit/s air wish that makes strafe jumping work, stairs and air steps, and the 50 ms command split. Clip hulls, the box hull, point contents and the line trace (with its 1/32 back-off at an impact) come with it, and hulls can be built from convex brushes. Every one of the 4033 recorded lines of the classic move's 35 scenarios agrees in all nine printed digits; the new `pmove` lab tables each scenario's worst error and draws its path from above and from the side.
