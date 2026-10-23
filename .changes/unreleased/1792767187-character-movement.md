---
type: Added
title: Character movement for any genre, with strafe jumping
---

`engine/move` (`ge::move`) moves a character's body for any game: ground friction and acceleration, the air's acceleration with its cap (strafe jumping and bunny hopping), air control and strafe-only air rules, jumps with a buffer, coyote time, a cut and air jumps, gravity applied by halves so a jump's height does not depend on the step rate, stairs, walkable and sliding slopes, ground snap, crouching and crouch jumps, slides, free flight and noclip. Every number is a knob in `MoveSpec`, named once with its unit and range for a console to bind, with five preset rows (an arena shooter's, its strafe-and-turn variant, a modern shooter's, a platformer's and a walker's). The world is one swept-box trace callback; a box world of boxes, ramps and stairs, the strafe jumper's best angle and staged scenarios come with it, and the `move-strafe` and `move-trace` labs chart and draw them.
