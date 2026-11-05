# The sequel's player move and its trace, specified

What the sequel's player move does, as rules and numbers in our words (README.md, the rules), and
the brush trace it moves through. For M18t4: the move becomes a third `sv_playermove` value, and
our client predicts with it. **Where these facts came from**: the research session of
`sequel.md`, which read the shared move, the collision model and the game's half of the move in
the sequel's GPL source. The move is the same code on the server and in the predicting client,
so its state is integers; exact parity is the goal, and every float rule below is stated with
its precision so the bits can match.

## Numbers and precision

- Positions, velocities and directions are 32-bit floats, and every operation is done in 32-bit
  float unless this note says otherwise (IEEE single, no fused multiply-add, as the engine's
  determinism contract wants).
- **Where doubles enter** (each result rounded to float when stored):
  - an **angle to radians**: the angle (float) times 2π/360 in double, **rounded to float first**;
    then its sine and cosine in double from that float, each rounded to float. (This differs from
    the first game's note, `movement.md`, where the radians stay double.)
  - a **vector's length** (normalising, friction's speed, a length): the sum of squares in float,
    the square root in double, rounded to float; normalising then multiplies by `1 / length`
    computed in float. A zero length leaves the vector as it is and returns 0.
  - **`msec × 0.001`** (the command's seconds) in double, rounded to float.
  - **`short × 0.125`** (unpacking a position or velocity) is exact.
  - **`(float) × 0.5`** and other products with a double constant (0.25, 1.5 written in the code as
    doubles: the water wish speed's halving, the 0.25 ground probe) are exact or rounded once.
- **Packing to integers** truncates toward zero (C's float-to-int conversion).
- The engine's libm (`engine/core/libm`) gives the double sine, cosine and square root; nothing
  here needs `<cmath>`.

## The packed state

Carried from command to command, and sent to the predicting client bit for bit (no floats):

| Field | Type | Meaning |
| --- | --- | --- |
| type | enum | normal 0, spectator 1, dead 2, gib 3, freeze 4 |
| origin | 3 i16 | position in eighths of a unit (12.3 fixed point: ±4096 units) |
| velocity | 3 i16 | units/s in eighths |
| flags | u8 | 1 ducked, 2 jump held, 4 on ground, 8 time is a water jump, 16 time is a landing, 32 time is a teleport hold, 64 no prediction |
| time | u8 | the timer the flags 8, 16, 32 name, in units of 8 ms |
| gravity | i16 | units/s² down (the server sets it to `sv_gravity` each command) |
| delta angles | 3 i16 | added to the command's angles to give the view (set by spawns and teleports) |

## The command

| Field | Type | Meaning |
| --- | --- | --- |
| msec | u8 | the command's length, 0..255 ms; seconds = msec × 0.001 |
| buttons | u8 | 1 attack, 2 use, 128 any key |
| angles | 3 i16 | the view in 65536ths of a turn (before the delta angles) |
| forward, side, up | 3 i16 | the movement axes in units/s (the client sends ±200 walking, ±400 running by default: verify the client's speeds) |
| impulse | u8 | unused by the base game |
| light level | u8 | the light where the player stands (the monsters' sight reads it) |

There is no splitting of long commands (the first game splits at 50 ms: `movement.md`); the
server runs each command as it is. An optional server setting `sv_enforcetime` (default 0, off)
gives each player a budget of 1800 ms of commands every 16 server frames and drops commands past
it.

## The move's constants

None of these is a server setting; the move's own numbers:

| Constant | Value |
| --- | --- |
| step height | 18 |
| stop speed | 100 |
| max speed | 300 |
| duck speed | 100 |
| accelerate | 10 |
| air accelerate | 0 (see below) |
| water accelerate | 10 |
| friction | 6 |
| water friction | 1 |
| water speed (currents) | 400 |
| stop epsilon (clipping) | 0.1 |
| overbounce | 1.01 |
| clip planes at most | 5 |
| bumps a slide | 4 |
| a walkable slope | normal z ≥ 0.7 |
| touched entities at most | 32 |

**Air acceleration** is a value the server sets when a map starts: `sv_airaccelerate` (default 0,
latched) in deathmatch, always 0 otherwise; it is sent to the clients so they predict the same.
Only whether it is 0 matters (§ Air).

## The box and the view height

Set at the start of every command (§ One command, step 6) from the state:

| State | Box mins | Box maxs | View height |
| --- | --- | --- | --- |
| standing | (−16, −16, −24) | (16, 16, 32) | 22 |
| ducked | (−16, −16, −24) | (16, 16, 4) | −2 |
| gib | (−16, −16, 0) | (16, 16, 16) | 8 |
| spectator | as standing | as standing | 22 |

**Ducking**: the dead are always ducked. A living player ducks when the up move is below 0 and
the on-ground flag is set. Otherwise a ducked player stands up when a box with the standing top
(maxs z 32) at the current origin is not all solid (a zero-length trace).

## The view angles

Each view angle is the command's angle plus the delta angle, **added and kept as a signed 16-bit
integer (wrapping)**, then turned into degrees (`× 360/65536` in double, rounded to float): so
every angle is in −180..180. A pitch above 89 (and below 180) becomes 89. (The code also turns a
pitch from 180 up to 271 into 271, which a signed angle never reaches; the client holds its own
pitch within ±89 before sending: verify.) With the teleport-hold flag set, the view is the yaw
alone, **its sum not wrapped** (the two values added as plain integers, so it may pass 180), with
pitch and roll 0. Forward, right and up are computed from the view (the formula is
`movement.md`'s § Units).

For walking and in the air (not in water), the move uses **a third of the pitch**: the pitch
divided by 3 (float; the code first subtracts 360 from a pitch above 180, which a signed angle
never is), and forward, right and up
are computed again from (pitch / 3, yaw, roll). Swimming and the ladder check use the full view.

## One command, in order

1. Zero the outputs (touched list, view angles, view height, ground entity, water type and
   level) and the move's locals.
2. Unpack origin and velocity (× 0.125); keep the packed origin as the **previous origin**;
   seconds = msec × 0.001.
3. The view angles (§ above).
4. **Spectator**: the fly move without collision (§ Flying), then pack (§ Snapping), done.
5. **Dead, gib or freeze** clear the forward, side and up moves. **Freeze** then stops (no move,
   no packing: the state is left as it came).
6. The box and view height (§ above).
7. When the game changed the state since the last move (the server compares the whole packed
   state with the one it kept: a teleport, a spawn, a push), **find a good start** (§ Snapping).
8. **Categorise** the position (§ Categorising).
9. **Dead**: the dead friction (§ Dead).
10. **Ladder and water jump check** (§ Special movement).
11. **The timer**: when the time is not 0, take `max(1, msec >> 3)` from it; when that would reach
    0 or below, clear the water-jump, landing and teleport flags and the time.
12. By the flags: a **teleport hold** does nothing; a **water jump** adds gravity (velocity z −=
    gravity × seconds), ends itself (flags and time cleared) as soon as velocity z is below 0,
    and slides (§ Step slide); otherwise **jump** (§ Jumping), **friction** (§ Friction), then
    the **water move** when the water level is 2 or more, else the **air move** with the pitch
    divided by 3 (§ The view angles).
13. **Categorise** again, then **pack** (§ Snapping).

## Categorising

- **The ground**: when velocity z is above 180, the player is off the ground. Otherwise a trace
  of the box from the origin to 0.25 below it: the plane, surface and contents it hit are kept as
  the **ground plane, surface and contents**; when it hit nothing, or hit a plane steeper than
  0.7 (normal z below 0.7) without starting in a solid, the player is off the ground. Otherwise
  the hit entity is the ground entity, and:
  - a running **water jump** ends (the water-jump, landing and teleport flags and the time
    cleared);
  - **landing**: when the on-ground flag was not set, it is set now, and when velocity z is
    below −200 the landing flag is set with a time of 25 (200 ms) below −400 or 18 (144 ms)
    otherwise: no jump until it runs out.
  - The hit entity, when there is one, joins the touched list (also when off a steep plane).
- **The water**: with h = view height − box mins z (as an integer: 46 standing, 22 ducked), the
  point contents at origin z + mins z + 1 decide level 1 (any of water, slime, lava: the water
  type is that point's contents); at mins z + h / 2 (integer halving) level 2; at mins z + h
  level 3.

## Special movement

Skipped while the time is not 0. The **ladder** flag is cleared; with the flat forward (forward
with z 0, normalised), a trace of the box from the origin to 1 unit along it that stops short and
whose contents have the ladder bit sets it.

The **water jump**, only at water level exactly 2: the point 30 along the flat forward and 4 up
must be solid (the solid bit), and 16 higher must be empty (contents 0). Then velocity = flat
forward × 50 with z 350, the water-jump flag set and the time 255.

## Jumping

- With the landing flag set: nothing.
- An up move below 10: the jump-held flag is cleared, nothing more.
- Jump held, or dead: nothing.
- **In water** (level 2 or more): off the ground; if velocity z is −300 or lower, nothing; else
  velocity z becomes 100 in water, 80 in slime, 50 otherwise (the water type compared exactly:
  water alone, slime alone).
- In the air: nothing.
- **On the ground**: the jump-held flag is set, off the ground, velocity z += 270, and raised to
  270 if still below.

## Friction

Speed = the whole velocity's length (three axes). Below 1: x and y become 0 (z is kept), done.
Drop starts at 0:

- **Ground friction** when on the ground with a ground surface that is not slick, **or on a
  ladder**: drop += max(speed, 100) × 6 × seconds.
- **Water friction** when in water (level 1 or more) and not on a ladder: drop += speed × 1 ×
  water level × seconds.

New speed = max(0, speed − drop) / speed, and the velocity (all three axes) is scaled by it.

## Currents and ladders

Applied to the wish velocity in the air and water moves:

- **On a ladder** with |velocity z| ≤ 200: the wish z is 200 when looking 15 or more up (pitch ≤
  −15) and moving forward; −200 when looking 15 or more down and moving forward; else 200 for an
  up move above 0, −200 below 0, otherwise 0. The wish x and y are each held to ±25.
- **Water currents**, when the water type has any current bit: a unit step along each current's
  axis (bits combine), times 400, halved when the water level is 1 and on the ground, added to
  the wish.
- **Conveyors**, when on the ground: a unit step along each current bit of the ground contents,
  times 100, added to the wish.

## The water move

- Wish = forward × forward move + right × side move (three axes, full view).
- No forward, side or up move: wish z −= 60 (a slow sink). Otherwise wish z += up move.
- Currents (§ above).
- Wish speed = the wish's length, wish direction = the wish normalised; a wish speed over 300 is
  scaled down to 300; then the wish speed is halved.
- **Accelerate** (§ Accelerating) with the water acceleration 10, then **step slide**.

## The air move (walking and falling)

- Wish = forward × forward move + right × side move, x and y only, z 0 (the forward and right of
  a third of the pitch: § The view angles).
- Currents (§ above).
- Wish speed and direction as in water; the cap is 100 when ducked, 300 otherwise (no halving).
- **On a ladder**: accelerate with 10; when the wish z is 0, velocity z moves toward 0 by gravity ×
  seconds (from above: subtract and stop at 0; from below: add and stop at 0); step slide.
- **On the ground**: velocity z = 0, accelerate with 10, then velocity z = 0 again when gravity is
  above 0 (else z −= gravity × seconds); **when x and y are both exactly 0 the move ends here**
  (no slide); otherwise step slide.
- **In the air**: when the air acceleration is not 0, the **air accelerate** with 10 (§
  Accelerating); otherwise accelerate with 1. Then velocity z −= gravity × seconds, and step
  slide.

## Accelerating

- **Accelerate** (wish direction, wish speed, a): current = velocity · wish direction; add = wish
  speed − current; nothing when add ≤ 0; amount = a × seconds × wish speed, at most add; velocity
  += wish direction × amount.
- **Air accelerate**: the same, except add = min(wish speed, 30) − current, while the amount still
  uses the whole wish speed (a × wish speed × seconds, at most add). This is the strafe-jumping
  rule, live only in deathmatch with `sv_airaccelerate` not 0.

## Clipping a velocity

To a plane with normal n and overbounce o: back = (v · n) × o; each axis v − n × back, and an
axis that lands strictly between −0.1 and 0.1 becomes 0.

## The slide

Up to 4 bumps with the time left starting at the command's seconds and the **primal velocity**
the velocity at the start; the plane list starts empty:

1. Trace the box from the origin to origin + velocity × time left.
2. All solid: velocity z = 0, stop (the whole slide ends here; the primal-velocity rule below is
   not applied).
3. When it moved (fraction above 0): the origin is the end point, and the plane list empties.
4. Fraction 1: done.
5. The hit entity joins the touched list (when there is one, up to 32).
6. Time left −= time left × fraction.
7. Already 5 planes: velocity zero, done. Else add the hit plane.
8. **Find a plane to slide along**: for each plane in order, clip the velocity to it with 1.01
   (the velocity itself is changed each time); it is good when the result is not heading into
   (dot below 0) any other plane in the list. The first good one ends the search. When none is
   good: with exactly 2 planes, the velocity becomes its projection on their crease (the cross
   product of the two normals times the velocity's dot with it); with any other count, the
   velocity becomes zero and the slide ends.
9. When the velocity now points against the primal velocity (dot ≤ 0): zero, done.

After the bumps, **when the time is not 0** (a water jump or a landing), the velocity is reset to
the primal velocity (so a timed move keeps its speed through the slide).

## The step slide

1. Keep the start origin and velocity; slide (§ The slide); keep the result as the **down**
   origin and velocity.
2. Up = start origin + 18 in z. A zero-length trace of the box at up: when all solid, keep the
   down result, done.
3. From up, with the start velocity, slide again. Then trace down 18 from where it ended: unless
   that trace is all solid, the origin is its end point.
4. Compare the horizontal distances squared from the start: down's and the stepped one's. When the
   down move went farther, **or the last downward trace's plane normal z is below 0.7**, keep the
   down origin and velocity. Otherwise keep the stepped origin and velocity, but with **velocity z
   taken from the down result**.

## Flying (the spectator)

- View height 22.
- Friction: speed = the velocity's length; below 1, velocity zero; otherwise drop = max(speed, 100)
  × 9 × seconds (the friction 6 × 1.5), and the velocity is scaled by max(0, speed − drop) /
  speed.
- Forward and right are normalised (full view), wish = forward × forward move + right × side move,
  wish z += up move; the wish speed is capped at 300.
- Accelerate with 10 (the same rule, written out).
- The spectator moves without collision: origin += velocity × seconds. (A clipped variant, the
  slide's single trace, exists in the code but nothing in the base game uses it.)

## Dead

When on the ground: speed = the velocity's length − 20; when that is 0 or below the velocity
becomes zero, otherwise the velocity is normalised and scaled to it.

## Snapping (packing the state)

- **Velocity**: each axis × 8, truncated to an integer.
- **Origin**: each axis × 8, truncated toward zero; a **sign** is +1 for an origin of 0 or more,
  −1 below 0, and 0 when the packed value × 0.125 equals the float exactly. Then try the packed
  origin with a nudge of the sign on a subset of the axes, in the order: none, z, x, y, x+y, x+z,
  y+z, x+y+z. The first whose box (at the packed origin × 0.125) is not all solid (a zero-length
  trace; a spectator is always good) is kept.
- None good: the previous origin (the packed origin this command started from) is kept.
- **A good start** (step 7): try the packed origin with z offsets 0, −1, +1 (outer), y offsets
  0, −1, +1, x offsets 0, −1, +1 (inner), in eighths; the first good one becomes both the float
  origin and the previous origin. None good: the origin stays as it came.

## The game's half of a command

Around each command the game (the server's rules, `sequel-rules.md` § A player's command) does:

1. During an intermission: the type becomes freeze, and the player may leave after 5 seconds
   with any button; no move.
2. The type: spectator for a no-clip player, gib when the player's model is not the player
   model (a gibbed body), dead when dead, else normal. Gravity = `sv_gravity`.
3. Origin and velocity are packed from the entity (× 8, truncated: the server's float origin may
   have been moved by the game since the last command) into the kept state; when that differs
   from the state the last move returned in any byte, step 7's search runs.
4. The trace the move uses is the world's trace with the player as the pass entity and the mask
   **player solid** when alive, **dead solid** when dead (`sequel-formats.md` § Contents).
   Point contents is the world's (§ Point contents).
5. After the move: the entity's origin and velocity are unpacked from the state; its box,
   view height, water level and type and ground entity are the move's; a living player's view
   is the move's view angles (a dead one looks at its killer, rolled 40 and pitched −15).
6. **The jump's sound** plays when the player had a ground entity, now has none, the up move is
   10 or more and the water level is 0.
7. The entity is linked; unless no-clip, the triggers it touches fire; then each distinct entity
   in the touched list with a touch function is touched, in list order.

## The trace

A trace sweeps a box (mins, maxs about a point) from a start to an end through every brush whose
contents share a bit with the mask, and reports: all solid, start solid, the fraction (1 when
nothing was hit), the end point, the hit plane, the hit surface (its name, flags and value), the
contents of the hit brush, and the hit entity.

**The world** (one model's head node):

- A **position test** (start equals end): every leaf the box, grown by 1 each way, touches is
  tested; a brush in a leaf whose contents share a bit with the mask, not tested yet in this
  trace, and with sides, is entered when the box's corner for each side plane (the box's max on
  axes where the normal is negative, its min elsewhere) is behind or on every side plane (the
  start's distance to the plane, less the corner's dot with the normal, not above 0). Entering
  one sets all solid, start solid, fraction 0 and its contents, and the test stops. The end point
  is the start.
- **A point trace** is a box of zero size: no corner offset, and node offsets of 0.
- **The sweep** walks the node tree from the head node with the fractions 0 and 1 and the
  points start and end. At a node: when the trace's best fraction so far is at or below this
  piece's start fraction, stop. In a leaf whose contents share a bit with the mask, test each
  brush of the leaf not yet tested in this trace whose contents share a bit with the mask (§
  Clipping to a brush), stopping at a fraction of 0. At a node, with t1 and t2 the start's and end's distances
  to the plane and the **offset** the box's reach across it (for an axial plane the box's extent
  on that axis, the larger of −mins and maxs; for a sloped plane the sum of the extents times the
  absolute normal components, **summed in double** and rounded to float, 0 for a point):
  - both at or over the offset: front child only; both under −offset: back child only;
  - otherwise, with ε = 1/32: t1 < t2 goes to the back side first with idist = 1 / (t1 − t2) in
    double rounded to float, the near fraction (t1 − offset + ε) × idist and the far one (t1 +
    offset + ε) × idist; t1 > t2 goes to the front first with the near (t1 + offset + ε) × idist
    and the far (t1 − offset − ε) × idist;
    t1 = t2: the front first, near 1, far 0. Precision: `t1 ± offset` is a float sum, the ± ε and
    the product with idist are in double, and the fraction is rounded to float. Each is held to
    0..1. The near side gets the piece
    from the start to the point at the near fraction, the far side the piece from the point at the
    far fraction to the end (midpoints as float: p1 + frac × (p2 − p1), the piece's fractions as
    p1f + (p2f − p1f) × frac).
- The end point is the end when the fraction is 1, else start + fraction × (end − start).

**Clipping to a brush** (a box from p1 to p2): enter = −1, leave = 1. For each side: its plane's
distance pushed out by the box (the plane's distance less the corner's dot with the normal, the
corner chosen as in the position test; a point uses the plane's distance), and d1, d2 the ends'
distances to it. An end in front (d2 above 0, d1 above 0) marks "gets out" and "starts out".
Entirely in front and not coming closer (d1 above 0 and d2 ≥ d1): no hit, the brush is done. Both
behind or on (d1 ≤ 0 and d2 ≤ 0): next side. Entering (d1 above d2): f = (d1 − ε) / (d1 − d2) in
double rounded to float; the largest f is the enter fraction, its plane and side kept. Leaving:
f = (d1 + ε) / (d1 − d2); the smallest is the leave fraction. After all sides: never started
out → start solid, and all solid when it never got out, no fraction; otherwise when enter is
below leave, enter above −1 and below the trace's fraction, the trace takes the fraction (held at
0 or more), the plane, the side's surface and the brush's contents.

**Entities**: the world is traced first with the mask, and its result names the world entity as
the hit **even when nothing was hit** (so "hit nothing" is told by the fraction and the plane, never
by the entity; the predicting client's trace leaves the entity empty instead, with the same effect
on the move). A world fraction of 0 is final. Otherwise
each solid entity in the area tree whose box overlaps the move's bounds (the box over start and
end, grown by the mover's box and 1 more), in the area tree's order (which decides only ties:
verify our tree's order against a recording):

- skipped: not solid, the pass entity itself, an entity the pass entity owns, the pass entity's
  owner, and a corpse (dead-monster flag) unless the mask has the dead-monster bit; once the
  trace is all solid, the rest are skipped;
- **a brush entity** (a door, a platform: solid by its model) is traced through its model's head
  node, with start and end moved into its frame (less its origin, and rotated by its angles when
  any is not 0: the point's dots with forward, −right and up), and the hit plane rotated back by
  the negated angles; the end point is recomputed in the world from the fraction;
- **a box entity** (a player, a monster, an item that blocks) is traced as a brush with six sides
  at its bounds, whose **contents is the monster bit**, so a mask without the monster bit passes
  through every box entity; boxes never rotate;
- the entity's result replaces the trace's when it is all solid, start solid, or nearer; the hit
  entity is then that entity; when the trace so far started solid it stays start solid. A
  farther result that starts solid only sets start solid.

## Point contents

The world's contents at the point (the leaf's contents through the world's head node), OR'd with
the contents of every solid entity whose box contains the point (a brush entity through its
model, moved and rotated as above; a box entity answers the monster bit). An unloaded world
answers 0.

## Test vectors

The first game's move is held to measured vectors (`vectors/pmove.txt`). The sequel's will be the
same: a lab builds hand-made worlds as BSP 38 brushes (`plugins/brush`'s writers grow brushes for
it), runs scripted commands through our move, and compares with vectors the research session
records by building the sequel's released move outside the tree over the same worlds. Those
vectors are a follow-up to this note (M18t4's first step), filed as `vectors/sequel-pmove.txt`.
