# Player movement, specified

What the player's movement does, as rules and numbers in our words (README.md, the rules). This
note is what the building session reads; it never opens the existing implementations.

**Where each fact came from.** A research session (2026-10-28) read the existing
implementations named in `survey.md`: the 1996 source release, the modern server most
competitive play runs on, and the modern client most players use. Where they differ, the
**modern server's defaults are the baseline** (that is what "plays like the classic game"
means to a player today), and each difference from the 1996 behaviour is listed as a server
setting with its default. The research session then built the modern server's player move on
its own, outside this tree, drove it with scripted commands over the synthetic worlds below,
and recorded what came out (`vectors/`, § Test vectors). Those recordings are measurements, and
they are the parity target: our movement is held to them within a tolerance.

## Units, numbers and the player

- **Units.** Map units; a player stands 56 units tall. Positions and velocities are 32-bit
  floats, as the format's entity fields are, and every operation below is done in 32-bit
  float unless it says otherwise. Three places use 64-bit doubles and round the result to
  float: the view's sine and cosine (the angle in degrees times 2π/360 in double, its `sin`
  and `cos` in double, each rounded to float), every vector length and normalisation (the
  sum of squares in float, its square root in double, rounded to float; the reciprocal
  `1 / length` in float), and nothing else in the player move.
- **The player's box** about its origin: mins (-16, -16, -24), maxs (16, 16, 32). The origin
  is 24 above the feet. A player standing on a floor at height 0 has origin z = 24.
- **Directions.** x and y are horizontal, z is up (the format's own axes, not the engine's
  y-up; a conversion is the game's job at its edges). The view: pitch (down positive), yaw
  (counter-clockwise from +x, in degrees), roll. From the angles in degrees: forward =
  (cos p · cos y, cos p · sin y, −sin p); right = (−sin r · sin p · cos y + cos r · sin y,
  −sin r · sin p · sin y − cos r · cos y, −sin r · cos p) (with roll 0 this is (sin y, −cos y,
  0)); up = (cos r · sin p · cos y + sin r · sin y, cos r · sin p · sin y − sin r · cos y,
  cos r · cos p).
- **A trace** sweeps the player's box from one point to another through every solid the move
  can touch (the world's clip hull and each nearby entity, § Traces) and reports the fraction
  travelled, the end point, the plane hit (its normal), whether it started in a solid, and
  which solid was hit (an index into this move's list of solids, 0 the world, −1 none).
- **A point's contents** is the world's point hull answer: empty, solid, water, slime, lava
  (the format's numbers −1, −2, −3, −4, −5; "water or worse" means a number of −3 or less).

## The command

One command moves one player by its own duration:

| Field | Range | Meaning |
| --- | --- | --- |
| msec | 0..255 | how long the command lasts, in milliseconds; the step's `dt` is `msec × 0.001` |
| angles | degrees, on a grid of 360/65536 | the view (pitch, yaw, roll) |
| forward, side, up | integers, ±508 on the client's grid of 4 | the movement axes, in units/s |
| buttons | bits | 1 attack, 2 jump, 4 use, 8 second attack |
| impulse | 0..255 | a one-shot command number for the game's logic |

**A command longer than 50 ms is split in two before it runs**: each half lasts `msec / 2`
milliseconds (integer division, so an odd millisecond is lost), the second half with its
impulse cleared. A half still longer than 50 ms splits again. Both the server and a predicting
client split this way.

## The move's settings

Server settings (console variables) the move reads, with the modern server's defaults:

| Setting | Default | Meaning |
| --- | --- | --- |
| `sv_gravity` | 800 | units/s² down |
| `sv_stopspeed` | 100 | friction acts on at least this speed |
| `sv_maxspeed` | 320 | the wish speed's cap (a game may set a player's own cap) |
| `sv_spectatormaxspeed` | 500 | the spectator's cap |
| `sv_accelerate` | 10 | ground acceleration factor, also used in the air |
| `sv_airaccelerate` | 10 | read and sent to clients but not used by the move (the air uses `sv_accelerate`) |
| `sv_wateraccelerate` | 10 | |
| `sv_friction` | 4 | ground friction factor |
| `sv_waterfriction` | 4 | |
| `pm_ktjump` | 1 | 0..1: how far a jump's upward speed is pulled up to 270 when it would be less |
| `pm_bunnyspeedcap` | 0 (off) | when set, air acceleration cannot raise the horizontal speed above `maxspeed × cap` |
| `pm_slidefix` | 0 | when set, downward speed on the ground is kept (ramps are walked down, not flown off) |
| `pm_airstep` | 0 | when set, a step can be climbed in the air; setting it sets `pm_pground` with it |
| `pm_pground` | 0 | when set, "on the ground" persists between moves instead of being re-tested |

Two per-player values come from the game's logic: an **entity gravity** factor (default 1) and
the player's **max speed** (default `sv_maxspeed`). All of these travel to clients (the server
info) so a client predicts with the same numbers.

## The player's move state

Carried from one command to the next: origin, velocity, view angles, **on ground** (and the
index of the solid stood on), the **jump held** flag, the **water jump time** (seconds left of
a water jump, 0 when none), the water level (0..3) and water type, and the **move type**:

| Move type | When | What it does |
| --- | --- | --- |
| normal | alive, walking | everything below |
| dead | health ≤ 0 | friction and collision but no acceleration; a held jump is remembered as held |
| fly | the game's fly movetype | three-dimensional acceleration with collision, its own friction |
| spectator | noclip, on a client that knows the newer type | free flight through everything |
| old spectator | noclip, on an old client | the same with an older acceleration rule (below) |
| none | the game's none movetype | does not move; position categorised only |
| lock | the server holds the origin and view | does not move; position categorised only |

## One command, in order

1. `dt = msec × 0.001`. The touch list is emptied.
2. **None and lock**: categorise the position (step 5) and stop.
3. The view angles are taken from the command, and forward, right and up computed from them.
4. **Spectators** run the spectator move (§ Spectators), are marked off the ground, and stop.
5. **Unstick** (§ Unsticking).
6. **Categorise** the position: on ground or not, the water level and type (§ Categorising).
7. **Water jump check**, at water level exactly 2 and not flying (§ Water jump).
8. If the vertical speed is negative, or the player is dead, the water jump time is cleared;
   otherwise a running water jump time counts down by `dt`, clamped at 0.
9. **Jump** (§ Jumping).
10. **Friction** (§ Friction).
11. By where the player is: water level 2 or more → the water move; flying → the fly move;
    otherwise → the walk/air move.
12. **Categorise** again for the final spot.
13. **Landing fix** (only when `pm_pground` is off): if now on the ground and falling faster
    than 300 units/s, and the velocity points into the ground plane (its dot with the ground
    normal below −0.1), the velocity is clipped to the ground plane (§ Clipping, factor 1).

## Unsticking

The protocol carries positions on a grid of 1/8 unit, so a client's copy of a position may sit
a hair inside a wall. Before moving, the origin is tested at 27 nearby points and moved to the
first one that is not inside a solid: the offsets per axis are tried in the order 0, −1/8,
+1/8, with z the outermost loop, then y, and x the innermost, so the order is (0,0,0),
(−⅛,0,0), (+⅛,0,0), (0,−⅛,0), … (+⅛,+⅛,+⅛); the first tried is the original itself. If none is free, the origin is tried raised by 1, 2, … 18 units (a map may spawn a
player sunk into the floor). If none of those is free either, the original is kept. "Free"
means the player's box at that point is outside every solid of the move's list.

## Categorising

- **Ground.** A trace from the origin to the origin one unit lower. When the vertical speed is
  above 180 (the ground speed limit), the player is off the ground and the trace is not used.
  With `pm_pground` on, a player who was off the ground stays off (only a move that lands sets
  it, § The walk/air move). Otherwise the player is on the ground when the trace hit something
  whose normal's z is at least 0.7 (and the trace did not run its full length); the ground
  solid's index is kept, the water jump time is cleared, and the ground normal is remembered
  for the jump and landing fixes. Standing on an entity other than the world adds it to the
  touch list.
- **Water.** The world's contents at three points above the origin: the feet plus one
  (origin z − 23), the box's middle (origin z + 4) and the eyes (origin z + 22). Level 1 when
  the first is water or worse (its contents become the water type), 2 when the second is too,
  3 when the third is too. Level 0 and type empty otherwise.
- **Snap to the ground** (only when `pm_pground` is off): a player on the ground, not flying
  and at water level below 2 is moved to the ground trace's end point, unless that trace
  started in a solid.

## Jumping

In this order, the first rule that applies decides:

1. Flying: nothing.
2. Dead: the jump is marked held (so a respawn does not jump), nothing else.
3. The jump button is not down: the jump is marked released.
4. A water jump is running: nothing.
5. Water level 2 or more: swimming up, not jumping. The player is marked off the ground and the
   vertical speed is **set** to 100 in water, 80 in slime, 50 in lava.
6. Off the ground: nothing.
7. The jump is held from before (the button was not released since the last jump): nothing.
   This is the no-pogo rule: holding the button jumps once.
8. **The jump.** If the velocity points into the ground plane (dot with the ground normal below
   −0.1) while falling (vertical speed below 0), the velocity is first clipped to the ground
   plane. The player is marked off the ground and **270 is added** to the vertical speed. With
   `pm_ktjump` above 0 (capped at 1), a vertical speed still below 270 is pulled toward it:
   `vz × (1 − k) + 270 × k`. The jump is marked held.

(The modern server also has `pm_rampjump`, off by default, which raises the ground speed limit
on ramps climbed fast and clips the jump against the ramp; it is not part of the baseline and
is not specified here.)

## Friction

Not while a water jump runs. With the speed `s` (the whole velocity's length):

- **Below 1 unit/s** the horizontal velocity is zeroed (and the vertical one too when flying);
  nothing else.
- **In water** (level 2 or more): `drop = s × waterfriction × waterlevel × dt`.
- **Flying**: `drop = s × 4 × dt`.
- **On the ground**: `drop = max(s, stopspeed) × friction × dt`, with friction **doubled at an
  edge**: a trace of the box from 16 units ahead along the horizontal velocity, starting at the
  feet (origin z − 24) and going 34 units down, that runs its whole length (nothing under the
  leading edge) doubles it.
- **In the air**: no friction.

Then the velocity is scaled by `max(s − drop, 0) / s`.

## Acceleration

Not for a dead player, nor while a water jump runs. Given a wish direction (unit), a wish speed
and an acceleration factor:

- **On the ground, in water, flying**: `add = wishspeed − v·wishdir`; if positive,
  `velocity += wishdir × min(accel × dt × wishspeed, add)`.
- **In the air**: the same, but the wish speed in the first subtraction is capped at 30 (the
  added amount still uses the full wish speed): `add = min(wishspeed, 30) − v·wishdir`; if
  positive, `velocity += wishdir × min(accel × wishspeed × dt, add)`. This is what makes air
  strafing and strafe jumping gain speed: turning keeps `v·wishdir` small.
- **The bunny speed cap** (when `pm_bunnyspeedcap` > 0, air only): the horizontal speed before
  is `h0`, after is `h1`; when `h1 > h0` and `h1 > maxspeed × cap`, the horizontal velocity is
  scaled by `max(h0, maxspeed × cap) / h1`.

## The walk/air move

The wish: forward and right flattened (z = 0) and normalised; `wishvel = forward × fwd +
right × side` (z = 0); `wishspeed` its length and `wishdir` its direction; a wish speed above
the max speed scales the wish down to it.

- **On the ground.** Without `pm_slidefix` the vertical speed is set to 0 before accelerating;
  with it the vertical speed is capped at 0, the ground acceleration applied, then gravity
  (`entgravity × gravity × dt`) subtracted. The ground acceleration uses `sv_accelerate`. If the
  horizontal velocity is then exactly zero, the vertical speed is zeroed and the move ends
  without moving. Otherwise the player **step-slides** (§ Step-slide) with "in the air" false.
- **In the air.** Air acceleration (with `sv_accelerate`), then gravity subtracted. With
  `pm_airstep` the player step-slides with "in the air" true, otherwise slides (§ Slide). With
  `pm_pground` on, a slide that touched a floor marks the player on the ground.

## The water move

`wishvel = forward × fwd + right × side` (the full three-dimensional view vectors, not
flattened); then, when not flying and with no forward, side or up input, 60 units/s straight
down (the idle sink), otherwise the up axis added to z. Capped at the max speed as above, then
the wish speed is multiplied by 0.7. Accelerated with `sv_wateraccelerate` (the ground rule),
then a step-slide with "in the air" false. No gravity.

## The fly move

As the water move without the idle sink and without the 0.7 factor; accelerated with
`sv_accelerate`; then a step-slide.

## Slide

Moves the box along its velocity for `dt` and slides along what it hits, at most 4 bumps:

- Each bump traces from the origin to `origin + velocity × time_left`. A trace that starts in a
  solid zeroes the velocity and ends the slide (blocked "floor and step").
- A trace that went any distance moves the origin to its end and forgets the planes collected
  so far.
- A trace that ran its full length ends the slide.
- The solid hit is added to the touch list (once per solid). The plane's normal sorts the bump:
  z ≥ 0.7 a floor, z exactly 0 a step (a wall), anything else "other".
- `time_left` loses its travelled share. If 5 planes were already collected the velocity is
  zeroed and the slide ends.
- **The new velocity**: for each collected plane in order, the velocity at the start of this
  slide clipped against that plane (§ Clipping, factor 1); the first that does not point into
  any of the other collected planes (dot < 0) is taken. If none qualifies: with exactly two
  planes the velocity is projected onto their crease (the cross product of the two normals,
  times its dot with the start velocity as clipped against the last collected plane, which is
  the last candidate tried), otherwise the velocity is zeroed and the slide ends.
- If the new velocity points against the velocity the slide started with (dot ≤ 0) it is
  zeroed and the slide ends (no jitter in corners).
- **After the slide**, while a water jump runs the velocity is restored to what it was when
  the slide started (a water jump is not slowed by the lip it climbs).

The slide reports which kinds of planes it hit (floor 1, step 2, other 4).

## Step-slide

Climbs a step instead of sliding along it when that goes further:

1. Remember the start (origin and velocity). Slide. If nothing was hit, done.
2. **In the air** (air step only): continue only if a step (a wall plane) was hit. Look for
   ground under the player: from the current origin when it was falling, from the start
   otherwise, trace 18 down; with nothing hit or a floor steeper than 0.7, keep the slide. The
   step height becomes `18 − (that point's z − the ground's z)`, so the total climb never
   exceeds 18. On the ground the step height is 18.
3. Keep the slide's result as "down". Go back to the start, trace up by the step height (move
   there unless it started in a solid). In the air while falling, zero the vertical speed. Slide
   again.
4. Trace down by the step height. If it hit a plane steeper than 0.7, use "down". Otherwise move
   to its end (unless it started in a solid). If that ends lower than the start, use "down".
5. Compare the horizontal distance squared from the start: when "down" went at least as far,
   use "down". Otherwise keep this "up" result, but take the vertical speed from "down".
6. In the air and off the ground at water level below 2, climbing a step this way keeps only
   `1 − 0.01 × climb` of the horizontal speed (a 16-unit step costs 16%).

## Clipping

`clip(v, n, f)`: `b = (v · n) × f`; each component `v_i − n_i × b`, and a component strictly
between −0.1 and 0.1 becomes 0.

## Water jump

At water level 2, not flying, no water jump running, and not falling faster than 180: with the
forward vector flattened and normalised, the point 24 ahead and 8 up from the origin must be
solid (any brush model's or the world's point contents), and 24 higher (32 up) empty. Then the
velocity is set to 50 along that flat forward with 310 up, the water jump time to 2 s, and the
jump marked held.

## Spectators

Friction first: below speed 1 the velocity is zeroed, otherwise `drop = max(s, stopspeed) ×
friction × 1.5 × dt` and the velocity scaled as in § Friction. The wish uses the full view
vectors (each normalised), `wishvel = forward × fwd + right × side`, plus the up axis on z;
capped at the spectator max speed. Then: `add = wishspeed − v·wishdir`; the newer spectator
adds `wishdir × min(accel × dt × wishspeed, add)` when `add > 0`; the old spectator returns
without moving at all when `add ≤ 0` (the classic quirk: a spectator coasting faster than its
wish does not move that command). Then `origin += velocity × dt`, no collision.

## Traces

**The solids of a move.** Index 0 is the world. Then, within 256 units of the origin on every
axis (by their bounding boxes), each solid entity (brush models, boxes and other players'
boxes) except the moving player and the player's own projectiles (entities it owns), up to 64
in all, in the order the server's area tree lists them (`gameloop.md` § The area tree).

**Against a brush model** (the world or a door) the trace runs through its player clip hull
(hull 1, § Clip hulls) offset by the model's origin. **Against a box** the box is grown by the
player's box (the box's mins minus the player's maxs, its maxs minus the player's mins) and
turned into a six-plane hull (§ Clip hulls). Each solid is traced in turn; a trace that started
in a solid reports fraction 0; the trace with the smallest fraction wins (ties keep the earlier
solid), and its solid's index is reported. Solids whose grown bounds do not overlap the swept
segment's bounds (padded by 1) are skipped, except the world.

**A point test** ("is the player's box at this point free") asks each solid's hull for the
contents at the point and fails on solid. **The contents for water** use the world's point hull
(hull 0) alone; the water jump's two tests ask every brush model's point hull and answer solid
if any is solid, otherwise the first non-empty answer, otherwise empty.

## Clip hulls

A hull is a tree of planes (`dist`, `normal`, and a type: 0, 1, 2 when the normal is exactly
+x, +y, +z, else 3) and nodes (a plane and two children: the front child for points at or in
front, `normal · p − dist ≥ 0`, the back child otherwise; a child is a node index, or a negative
contents number for a leaf). The format stores three: hull 0 for points, hull 1 for the
player's box (−16, −16, −24)..(16, 16, 32), hull 2 for large boxes (−32, −32, −24)..(32, 32, 64).

**The six-plane box hull** for a box (mins, maxs): planes +x at maxs.x, +x at mins.x, +y at
maxs.y, +y at mins.y, +z at maxs.z, +z at mins.z; node i uses plane i; for even i the front child
is empty and the back child the next node; for odd i the back child is empty and the front the
next node; the last node's other child is solid. (So a point is solid when mins ≤ p < maxs on
every axis.)

**The contents at a point**: from the first node, go to the front or back child by the sign
rule until a leaf.

**The line trace** through a hull, from p1 to p2, finds the first point where the segment
passes from a non-solid leaf into a solid one. It is one recursive walk, and every number in it
is a 32-bit float. A call has a node, the segment's two ends p1 and p2, and their two
fractions p1f and p2f of the whole trace (0 and 1 at the top). It answers "go on" or "stop".
The trace's result starts as fraction 1, end point = the trace's end, "all solid" set and the
other flags clear.

- **A leaf**: an empty leaf clears "all solid" and sets "in open"; a liquid leaf clears "all
  solid" and sets "in water"; a solid leaf sets "started in solid". All three answer "go on".
- **A node**: compute the signed distances `t1`, `t2` of p1 and p2 from its plane (`p[type] −
  dist` for an axial plane, `normal · p − dist` otherwise). Both ≥ 0: the front child with the
  same segment, and its answer is this call's. Both < 0: the same with the back child.
- **A straddle** (one end on each side). **The split point already stands 1/32 unit back on p1's
  side:** `frac = (t1 − 1/32) / (t1 − t2)` when `t1 ≥ 0`, `(t1 + 1/32) / (t1 − t2)` when `t1 < 0`,
  clamped to [0, 1]; `mid = p1 + frac·(p2 − p1)` per axis and `midf = p1f + (p2f − p1f)·frac`.
  That one `mid` is both where the near part ends and where the far part STARTS: the far part
  is never begun on the plane itself. So where a split's back-off moves `mid` along the segment
  by `(1/32) / |t1 − t2| × length`, which is more than 1/32 when the segment meets the plane
  at a slant, the far part starts that much short of the plane.
  1. The near child (the front when `t1 ≥ 0`, else the back) with p1 → mid (p1f → midf). If it
     answered "stop", answer "stop".
  2. The contents at `mid`, walked down from the FAR child (not from the hull's first node).
     If not solid: the far child with mid → p2 (midf → p2f), and its answer is this call's.
  3. Otherwise, if "all solid" is still set (no empty or liquid leaf yet), answer "stop" and
     change nothing.
  4. **The impact.** The plane is this node's, facing p1's side (normal and dist as stored
     when `t1 ≥ 0`, both negated otherwise). Then, while the contents at `mid` walked down from
     the hull's FIRST node are solid, `frac −= 0.1` and `mid`, `midf` are worked out again from
     this call's p1, p2 (if `frac` drops below 0, the current `mid` and `midf` are kept and the
     loop ends). The result's fraction is `midf` and its end point `mid`. Answer "stop".
- **Start in solid**: the first leaf reached was solid. "Started in solid" is set; when no
  empty or liquid leaf was ever reached, "all solid" stays set too, and the fraction stays 1
  with the end point at the trace's end (the player move treats any start in solid as fraction
  0). A trace that starts in solid and then leaves it reports "started in solid" with what it
  met after.
- **The caller's offset**: the trace runs in the hull's own space (the start and end minus the
  offset). Only when the fraction is not 1 is the offset added back to the end point; at
  fraction 1 the end point is still the trace's end as given.
- Reaching an empty leaf marks "in open", a liquid leaf "in water".

## Test vectors

`vectors/pmove.txt` and `vectors/entity.txt` are the measurements, and `vectors/hulltrace.txt` holds single traces through the same worlds' hulls (one line each: the hull, 0 for a point and 1 for the player's box; the start and end in world space; then the fraction, end point, plane normal and dist, the all-solid, started-in-solid, in-open and in-water flags, whether the world was hit, and how many 0.1 steps back the impact took). The worlds in them are
written as **brushes**: convex solids, each a list of planes `normal dist` (a point is inside
when `normal · p ≤ dist` for every plane) and a contents (`solid`, `water`, `slime`, `lava`).
Every brush lists its six axial planes (its bounding box) as well as any slanted one, so
growing a brush by a box is only offsetting each plane.

**The hulls the measurements ran on** were built from the brushes this way, and ours must be
built the same way for the comparison to hold:

- Hull 0 holds every brush, planes as written. Hull 1 holds the solid brushes only, each plane
  moved out by the player box: `dist' = dist − Σ_i normal_i × (normal_i > 0 ? mins_i : maxs_i)`
  (summed in float in axis order, mins (−16, −16, −24), maxs (16, 16, 32)).
- The tree is a chain, brush by brush, plane by plane, in the order written: each plane is one
  node; its front child is the first node of the next brush in the hull (or empty after the
  last brush); its back child is the next plane of the same brush, or the brush's contents
  after its last plane. Node and plane indices count up from 0 in that order. Plane types as in
  § Clip hulls.
- A `physent box` line is another solid in the move's list (index 1, 2, … in order): a box at
  that origin with those mins and maxs.

**The scenarios** (`scenario` blocks) start the player at the given state, with the given
settings, and run the commands in order, each line giving the command and the state after it:
origin, velocity, on ground and the ground's index (−1 off the ground), water level and type,
jump held, water jump time, the slide's blocked bits (the move's return: the walk/air,
water or fly move's slide result, 0 when it did not slide) and the touch list. A command over
50 ms is printed as its halves, as the server runs it. They cover: running from rest and
stopping, mixed command lengths with a chopped one, the held and re-pressed jump, bunny
hopping, strafe jumping with and without the bunny speed cap, another player's box, the dead,
flying and both spectators, unsticking from the floor, a fall and a landing, a wall slide and
a jump into a wall, stairs (with and without the air step), a 30° ramp up and down, the jump
on a 10° ramp under both `pm_ktjump` values, `pm_slidefix` down a ramp, sliding off and
climbing a 50° ramp, running off a ledge and the edge friction, sinking, swimming up and
jumping, the water jump out onto a bank, walking into a pool, and jumping in slime and lava.

`vectors/entity.txt` holds point-sized thrown entities (`gameloop.md` § Entity physics) against
the world's hull 0, one line a physics frame.

**Tolerance.** The recordings are 32-bit floats printed to 9 significant digits. A faithful
float implementation following this note is expected to agree to within 0.01 unit in position
and 0.05 unit/s in velocity on every line, with the discrete fields (on ground, water level,
jump held) equal; the parity lab reports the worst line of each scenario and fails above the
tolerance.
