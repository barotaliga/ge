# Player movement, specified

What the player's movement does, as numbers and rules in our words (README.md, the rules). The
feel is matched by measurement (`se-parity`), so every number here is a starting value that
the lab confirms or corrects.

## The server's movement settings (spec: their documented defaults; verify each)

| Setting | Default | Meaning |
| --- | --- | --- |
| gravity | 800 units/s² | down |
| max speed | 320 units/s | the wish speed's cap on the ground |
| accelerate | 10 | ground acceleration factor |
| air accelerate | 0.7 (multiplayer) | air acceleration factor; with the 30 units/s cap below it gives air strafing |
| friction | 4 | ground friction factor |
| stop speed | 100 units/s | friction acts as if the speed were at least this |
| water accelerate | 10 | |
| water friction | 1 | |
| jump speed | 270 units/s | upward speed set on a jump |
| step height | 18 units | |

## The model (spec: the well-known form of this family of movement; verify by measurement)

- **Wish direction** from the view's yaw and the forward and side axes; the up axis only in
  water and flight.
- **Friction** on the ground: the speed drops by `max(speed, stop) × friction × dt`, clamped at
  zero; doubled at an edge with a drop below (verify).
- **Acceleration**: the speed already along the wish direction is subtracted from the wish speed;
  what is left, if positive, is added up to `accel × wish speed × dt`. In the air the wish
  speed used for the subtraction is capped at 30 units/s, which is what lets a player gain speed
  by turning while strafing.
- **Jumping** is held off until the button is released and pressed again (verify against
  holding the button: today's servers differ).
- **Stepping**: a blocked move is retried raised by the step height and set down again.
- **Clipping** against the player hull's planes, up to four bump iterations a move (verify).
- **The step's length** is the usercmd's milliseconds, capped (verify the cap) and split when
  long.

`se-parity` records an existing server's trajectories for scripted inputs (standing start,
strafe jump, circle jump, bunny hop on flat ground, a ramp, stairs, water entry, a knock-back)
and holds ours to them within a tolerance.
