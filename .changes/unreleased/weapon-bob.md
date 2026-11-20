---
type: Added
title: The weapon bobs with your stride, cl_bobhead puts its bob on your head, and Quake II's left hand draws the gun on the left
---

Walking now pushes a classic game's own weapon forward and back with your stride, as the classic games do. `cl_bobhead 1` makes your head carry the weapon's bob as well, and `v_viewheight` now stills only your head's bob, not the weapon's.

In Quake II, `hand 1` now draws your weapon on the left, mirrored, lit from that side and firing from where you see it. Its `cl_gun` is its own setting again: 1 follows your hand, 2 always draws the weapon on the right, 3 always on the left, and 0 hides it.
