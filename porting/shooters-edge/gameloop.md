# The game loop, specified

How the classic multiplayer game runs a match, frame by frame, as rules in our words (README.md,
the rules): the server's frame, how a player's commands reach the world and move the player,
how every other entity moves and thinks, and how a client builds its commands and predicts
what it sees. The player's own move is `movement.md`; the protocol's bytes are `protocol.md`.

**Where each fact came from**: the same research session as `movement.md` (the 1996 release,
the modern server and the modern client, read 2026-10-28), with the **modern server's defaults
as the baseline**. The thrown-entity numbers are measured (`vectors/entity.txt`); the timing
rules are stated with worked examples a test can check.

**What is ours, not theirs.** The existing programs run on the wall clock. This engine is
deterministic (`docs/architecture.md` § Determinism), so where they read the clock we read the
server's own time, advanced by the run clock (§ Our deliberate changes). Everything else below
is behaviour to match.

## The game program

The rules of a match (weapons, items, scoring, doors, what a touch does) are not the loop's:
they are the **game program**'s, which in the classic game is QC bytecode (`progs.md`) and in
this engine is any implementation of one interface. The loop calls the program at fixed
points and the program reads and writes the entities' fields. The loop needs from it:

| Hook (the QC name, for `progs.md`) | When the loop calls it | `self`, `other` |
| --- | --- | --- |
| frame start (`StartFrame`) | at the start of each physics frame, before any entity runs | world, world |
| player pre-think (`PlayerPreThink`) | before each of a player's commands is moved | the player |
| player post-think (`PlayerPostThink`) | after all of a packet's commands are moved | the player |
| an entity's think function (its `think` field) | when its `nextthink` comes due (§ Thinking) | the entity, world |
| an entity's touch function (its `touch` field) | on contact (§ Touching) | the toucher's pair, see there |
| a pusher's blocked function (its `blocked` field) | when something stops a pusher (§ Pushers) | the pusher, the blocker |
| client connect, put in server, disconnect, kill, spectator connect/think/disconnect, set new parms, set change parms | at those events (`protocol.md`, the server's connection handling) | the player |

Globals the loop sets before a call: `time` (the server's time in seconds, as the program's
32-bit float), `frametime` (the current step's length), `self`, `other`. Globals the loop
reads: `force_retouch` (a count of frames to re-run trigger touches for every entity),
`newmis` (an entity the program just spawned, § New missiles).

## Entities and the fields the loop uses

Every entity is a record of fields; entity 0 is the world, entities 1 to `maxclients` are the
players (32 slots), the rest are dynamic. The loop reads and writes these, all 32-bit floats or
vectors of three, or entity references:

`origin`, `velocity`, `angles`, `avelocity` (degrees/s), `mins`, `maxs`, `size`, `absmin`,
`absmax`, `movetype`, `solid`, `flags`, `groundentity`, `owner`, `nextthink`, `think`,
`touch`, `blocked`, `ltime` (a pusher's own clock), `modelindex`, `waterlevel`, `watertype`,
`teleport_time` (the player's water jump time), `v_angle` (the player's view), `fixangle`
(the program forces the view), `health`, `button0`, `button1`, `button2`, `impulse`,
`gravity` and `maxspeed` (per-player overrides; 0 means the defaults).

| movetype | value | Moved by |
| --- | --- | --- |
| none | 0 | nothing; only thinks |
| walk | 3 | the player move (players only) |
| step | 4 | falling when off the ground; otherwise only thinks (§ Step) |
| fly | 5 | the toss rules without gravity |
| toss | 6 | the toss rules |
| push | 7 | the pusher rules (doors, plats) |
| noclip | 8 | straight-line motion through everything |
| flymissile | 9 | the toss rules without gravity, a missile's larger clip against monsters |
| bounce | 10 | the toss rules with a 1.5 rebound |
| lock | 12 | nothing (the server holds the player) |

| solid | value | Meaning |
| --- | --- | --- |
| not | 0 | no interaction |
| trigger | 1 | touched by things moving into it, does not block |
| bbox | 2 | blocks, as its box |
| slidebox | 3 | blocks, as its box (a monster's or player's) |
| bsp | 4 | blocks, through its brush model's hulls; requires movetype push |

Flags the loop uses: `onground` (512), `item` (256: a pick-up, its trigger box grown 15 units
sideways), `monster` (32), `fly` (1), `swim` (2), `waterjump` (2048).

## The area tree

The server keeps every linked entity in a fixed tree of 31 nodes over the world's bounds, built
once a map: a node at depth 4 is a leaf; otherwise it splits along x when the bounds are
longer in x than in y (else along y) at the bounds' middle; child 0 is the upper half, child 1
the lower. Each node keeps two lists: trigger entities and solid entities.

**Linking an entity** (after anything moves it): its `absmin` and `absmax` are its `origin`
plus `mins` and `maxs`, grown by 15 sideways for items, else by 1 on every axis (movement stops
an epsilon short of touching, so boxes must overlap a little to find each other). A `not` solid
is unlinked and stays out. Otherwise it goes into the first node, walking down from the root,
whose plane its box crosses (or the leaf it reaches), appended at the end of the trigger or
solid list. Linking with "touch triggers" then runs § Touching's trigger pass.

**Listing the entities in a box** (for traces and touches) visits nodes depth first: a node's
own list in order, then, when the box reaches above the split, child 0 next with child 1 kept
for later if the box also reaches below; when it only reaches below, child 1. Entities whose
absolute box does not overlap the asked box are skipped, as are `not` solids.

The player move's solid list (`movement.md` § Traces) walks the solid lists recursively instead:
a node's list, then child 0 when the 512-unit box around the player reaches above the split,
then child 1 when it reaches below.

## Traces through the world

`trace(start, mins, maxs, end, kind, passent)` sweeps a box through the world and the solid
entities. **Against a brush model** the hull is chosen by the moving box's width: under 3 units
hull 0, up to 32 hull 1, wider hull 2; the hull's own `clip_mins` minus the moving box's mins
plus the model's origin is the offset. **Against a box entity** a six-plane hull of the entity's
box grown by the moving box (`movement.md` § Clip hulls), offset by its origin. First the world,
then every solid entity whose box meets the sweep's bounds (each axis from the lower end minus
the box's mins minus 1 to the upper end plus maxs plus 1), skipping: the passing entity
itself; anything but brush models for `nomonsters`; a box-sized mover against a point-sized
entity (points never interact); entities the mover owns, and the mover's owner. A **missile**
trace (`kind` missile) uses a 30-unit cube (±15) instead of its own box against monsters. The
result keeps "started in solid" from any solid it met, and takes the first trace that is all
solid or reaches a smaller fraction, with that entity as what was hit. A trace that started all
solid in the world stops looking further.

**The hull trace** is `movement.md` § Clip hulls' line trace, the same walk the player move
uses, so the server and the client place every end point identically. (The original
single-player server's trace is a different walk, used by the campaign host: `frame-move.md`
§ The original trace.)

**Point contents** are the world's hull 0 at the point. **Testing an entity's position** is a
zero-length trace of its box at its origin (`nomonsters` for triggers and `not` solids): inside
something when it started in solid.

## The server's frame

The server runs frames as often as it can; each frame, in order:

1. **Advance time** by the time since the last frame (unless paused): the server's time
   (`sv.time`, a double) and its real time.
2. Housekeeping: timeouts, the log, console commands, a pending map change, changed settings.
3. **Read every packet waiting**, in arrival order. A connectionless packet is answered at once.
   A client's packet runs its commands now (§ A player's packet), and marks the client as owed a
   reply.
4. **Physics** (unless paused, § Physics frames).
5. **Send**: every client owed a reply (one reply per packet received, never more) gets one now,
   unless its rate would be exceeded (then it is counted as choked and skipped until its next
   packet); a client that is not yet in the game gets only its reliable stream. A client whose
   reliable stream overflowed is dropped.
6. The demo (MVD) frame, the master heartbeat, statistics.

A player is therefore moved as soon as its packet is read, at that packet's own command
lengths; everything else moves in the physics frame. The two do not share a step length.

## Physics frames

The physics runs only when at least `sv_mintic` (default 0.013 s) of server time has passed
since it last ran: `frametime = sv.time − last_physics_time`, skipped when below `sv_mintic`,
capped at `sv_maxtic` (default 0.1 s), and the last physics time set to now. (So physics runs
at most about 77 times a second, and a slow server drops time rather than running a long
step; the very first physics frame of a map runs with 0.1 s.) Then, with the program's
`frametime` set:

1. Frame start (the program's hook), `time` = the server's time.
2. Every entity in number order, the world first, skipping free entities: if `force_retouch` is
   non-zero, relink it with touching triggers; then players (entities 1..32) are skipped (their
   commands move them); every other entity runs once by its movetype (§ Running an entity),
   followed by § New missiles.
3. `force_retouch` counts down by one when non-zero.

An entity is never run twice in one physics frame: the loop remembers the server time it last
ran each entity at and skips it when that equals now.

## Running an entity

| movetype | Rule |
| --- | --- |
| push | § Pushers |
| none, lock | think only (§ Thinking) |
| noclip | think; if it survived: `angles += avelocity × dt`, `origin += velocity × dt`, relink (no touches) |
| step | § Step |
| toss, bounce, fly, flymissile | § Tossing |

`dt` is the physics frame's `frametime` (or the new missile's 0.05).

## Thinking

An entity whose `nextthink` is above 0 and **no later than `sv.time + dt`** thinks now: things
think up to one frame early, before they move. The program sees `time = max(nextthink, sv.time)`
(never in the past), `self` the entity, `other` the world; `nextthink` is cleared before the
call. Then, if the entity was removed the run ends (it does not move); if the think set a new
`nextthink` that is still due by the same rule **and later than the time just used**, it thinks
again (a chain of short thinks catches up within the frame); a think that sets `nextthink` to
the same time or earlier waits for the next frame.

Worked examples (physics at `dt = 0.013`, `sv.time = 10.000`): `nextthink = 10.010` thinks this
frame with `time = 10.010`; `nextthink = 10.013` thinks this frame (it is due by 10.013) with
`time = 10.013`; `nextthink = 10.0131` waits; `nextthink = 9.5` thinks with `time = 10.000`.
A think at 10.010 that sets `nextthink = 10.012` thinks again in the same frame (10.012 is due
by 10.013 and later than 10.010) with `time = 10.012`; one that sets `nextthink = 10.020` waits
for the next frame; one that sets `nextthink = 10.010` (no progress) waits for the next frame.

## Tossing

For toss, bounce, fly and flymissile:

1. Think. If the entity was removed, stop.
2. Rising (vertical speed above 0): clear `onground`. On the ground: stop (it rests until
   something lifts or pushes it).
3. **Bound the velocity**: any non-number component becomes 0 (and a non-number origin
   component 0); a velocity longer than `sv_maxvelocity` (2000) is scaled to that length.
4. **Gravity**, except fly and flymissile: `velocity.z -= gravity_scale × sv_gravity × dt`,
   with the product computed as `(1.0 × gravity)` in float times `dt` in double, subtracted in
   double and rounded to float.
5. `angles += avelocity × dt`.
6. **Move**: `push = velocity × dt` (dt rounded to float first); trace from the origin to
   `origin + push` with the entity's box (kind: missile for flymissile, `nomonsters` for a
   trigger or `not` solid, normal otherwise; passing itself); move to the trace's end; relink
   with touching triggers; when something was hit, both touch (§ Touching, impact).
7. When the trace ran its full length, or the entity was removed, stop.
8. **Rebound**: clip the velocity against the plane hit with factor 1.5 for bounce and 1
   otherwise (`movement.md` § Clipping, the same 0.1 rule).
9. **Landing**: when the plane is a floor (normal z above 0.7) and either the entity is not
   bouncing or its vertical speed after the rebound is below 60, it comes to rest: `onground`
   set, `groundentity` the thing hit, velocity and angular velocity zeroed.
10. **Water**: the contents at the origin. An entity with no water type yet (just spawned)
    takes it with water level 1. Entering water or worse from empty, or leaving it, plays the
    splash sound (`misc/h2ohit1.wav`, channel 0, full volume, attenuation 1); in water the type
    is the contents and the level 1, out of it the type empty and the level is set to the
    contents number (a quirk the program may read).

`vectors/entity.txt` holds measured flights of a grenade (bounce), a rocket (flymissile), a gib
(toss) and a nail (fly) against the floor and wall of the `wall` scene, at 13 ms frames and the
grenade again at 100 ms frames. These ran the world's hull 0 only (point-sized entities, no
other solids), so they hold the rules above without touches.

## Step

The movetype of walking monsters and of things resting on the ground that must still fall:

- When none of `onground`, `fly`, `swim` is set: note whether it is falling faster than
  `sv_gravity × 0.1`; add gravity (as in § Tossing); bound the velocity; **fly-move** it for
  `dt` (below; `nomonsters` for a `not` solid, so bodies and gibs are not blocked by players);
  relink with touching triggers; when it has landed (now `onground`) after falling fast, play
  `demon/dland2.wav` on channel 0.
- Then think, then the water check of § Tossing.

**The fly-move** is the player slide (`movement.md` § Slide) for an entity, with these
differences: traces are world traces with the entity's own box; the velocity a bump clips from
is reset to the current velocity every time the entity covered some distance; a floor (normal
z above 0.7) hit on a brush model sets `onground` and `groundentity`; a wall (normal z exactly
0) is remembered as the step trace; **both touch** at every bump (§ Touching, impact) and a
removed entity ends the move; it returns 3 when trapped all solid, 7 when it ran out of
planes, and otherwise the floor (1) and step (2) bits.

## Pushers

Doors, plats and trains (movetype push, solid bsp) move on their own clock `ltime`, and are
exact: they stop at their `nextthink` instead of overshooting it.

1. `movetime = dt`, or less when the pusher's `nextthink` falls inside this frame on its own
   clock (`nextthink < ltime + dt`): then `movetime = max(nextthink − ltime, 0)`.
2. If `movetime` is non-zero: a pusher with zero velocity just adds `movetime` to `ltime`;
   otherwise it tries to **push** by `velocity × movetime`, and adds `movetime` to `ltime` only
   when the push succeeded (a blocked door's clock stops).
3. When `nextthink` lies after the old `ltime` and at or before the new one: clear `nextthink`,
   call think with `time = sv.time`; if the think moved the pusher's origin by more than 1/64
   unit, it is put back and pushed by that move instead (so a think that teleports a train
   still carries its riders).

**A push** by `move`:

1. Move the pusher and relink it (no touches).
2. Every entity in number order that is not free and whose movetype is not push, none or
   noclip: skip it when it is inside something with the pusher made non-solid for the test (it
   was already stuck). Unless it stands on the pusher (`onground` with `groundentity` the
   pusher), skip it when its absolute box does not overlap the pusher's moved box (strictly) or
   it is not inside the pusher's new position.
3. A non-walking entity loses `onground`. Remember where it was; move it by `move`. If it is
   now free, relink it and continue.
4. Otherwise put it back; if it is free there, forget it and continue.
5. Still stuck: a zero-width entity is left there (relinked); a `not` or trigger solid (a
   corpse) is flattened to a zero box and left; anything else **blocks**: the pusher goes back
   to where it started, its `blocked` function runs (self the pusher, other the blocker), every
   entity moved so far goes back, and the push fails.

## Touching

- **Triggers**: whenever an entity is relinked with touching triggers, every trigger whose box
  overlaps the entity's (from the area tree's trigger lists, in listing order), other than the
  entity itself, with a touch function and still solid trigger, has its touch run with
  `self` the trigger and `other` the entity, `time` the server's time; `self` and `other`
  are restored after.
- **Impact**: when a moving entity's trace hits an entity, the mover's touch runs (self the
  mover, other the hit) and then the hit's touch (self the hit, other the mover), each only if
  it has one and is not a `not` solid; `self` and `other` are restored after.
- **A player's touches** come from the player move's touch list (§ A player's command).

## A player's packet

A client's movement message carries three commands: the newest, the one before, and the one
before that, so one lost packet loses no input. The server counts the packets the client sent
that it never received (`dropped`) and, unless paused:

1. Clear the per-packet "already touched" set (each entity touches a player at most once per
   packet, however many commands run).
2. Run the commands that were lost: when fewer than 20 packets were dropped, repeat the
   client's last command (as stored, its buttons cleared, § 6) for each dropped packet beyond
   two; then the oldest of the three when two or more were dropped, the middle one when one or
   more were. Then the newest.
3. The post-think (§ A player's command, end).
4. Remember the newest command as the client's last, with its buttons cleared (so lag does not
   fire twice).

## A player's command

For each command, before it runs:

- **The speed check** (`sv_speedcheck`, default on): a client cannot spend more command time
  than the server's time that passed. With `elapsed` the whole milliseconds of server time
  since the last check (rounded to nearest): a command longer than `elapsed` is first paid from
  a banked surplus; if still too long it is cut to `elapsed + bank` and the bank emptied; a
  shorter command banks the difference. The bank is held between 0 and 500 ms. Bots are not
  checked.
- **Split** a command over 50 ms into halves (`movement.md` § The command); the halves skip the
  speed check.

Then, for one command:

1. The buttons go to the player's fields: bit 1 → `button0` (attack), bit 2 → `button2`
   (jump), bit 4 → `button1` (use); a non-zero impulse is stored in `impulse` (a zero one does
   not clear it).
2. **The view**: the command's pitch clamped to `sv_minpitch`..`sv_maxpitch` (−70..80); unless
   the program set `fixangle`, the view (`v_angle`) takes the command's angles. A living
   player's model faces the view: model pitch `−view pitch / 3`, yaw the view's (unless
   `fixangle`), roll 0.
3. `dt = msec × 0.001`, capped at 0.1.
4. Not a spectator: with `frametime = dt` and `time = sv.time`, run the **pre-think**. If the
   player was on the ground falling (vertical speed below 0) and the pre-think set the vertical
   speed to exactly 0 without changing the horizontal speed, the vertical speed is put back (a
   program cannot cancel a landing's physics). Then run the player's own **think** (§ Thinking,
   with this command's `dt`).
5. **Move**: the player's origin (offset by the difference between its mins and the move's
   player box), velocity, view, water jump time (`teleport_time`), on ground (`onground`) and
   jump held go into the move; the move type comes from the movetype (`movement.md` § The
   player's move state; health ≤ 0 is dead); entity gravity and max speed are the player's
   (`gravity`, `maxspeed` fields, or 1 and `sv_maxspeed`); the solid list is built (§ The area
   tree). Run the move (`movement.md`).
6. Back out: jump held, `teleport_time`, `waterlevel`, `watertype`, `onground` and
   `groundentity` (the entity of the solid stood on), origin (offset back), velocity and view.
7. Unless the player is a `not` solid: relink with touching triggers, then for each entity in
   the move's touch list not yet touched this packet and having a touch function, run its touch
   (self the entity, other the player) and mark it touched.

**After the packet's commands** (the post-think): not a spectator: note on ground and the
velocity; run the post-think with `time = sv.time`; apply the same landing guard as the
pre-think; then § New missiles. A spectator runs the spectator think hook instead.

## New missiles

When the program spawns a projectile from a player's post-think (or from an entity's think in
the physics loop), it sets `newmis` to it. Right after that hook the loop runs that entity once
by its movetype (§ Running an entity) with `dt = 0.05`, and clears `newmis`. A rocket therefore
starts 50 ms ahead of where it was spawned: a fixed part of the classic feel (the player sees
it leave the barrel, not appear inside it).

## The client's commands

The client produces commands at its **physics rate**, independent of its render rate:

- The server announces `maxfps` in its server info (default 77). The client's command rate is
  that, or lower if the player sets one, and never below 10. Each rendered frame adds its time
  to an accumulator; when the accumulator reaches one command period a command is produced and
  one period taken off (or the whole accumulator when it is over two periods, a slow machine).
- **The command's length**: a running total of command time in milliseconds gains the period's
  length × 1000; the command's `msec` is its whole part and the fraction is kept for the next
  (so 77 commands a second give lengths of 12 and 13 ms, e.g. 12, 13, 13, 13, 12, 13 …,
  averaging 12.987). A length over 250 is replaced by 100.
- **Movement axes** from the keys, each key's share of the period: held the whole time 1,
  pressed during it and still held 1 (0.5 for a look key), released during it 0, pressed and
  released within it 0.25, released and pressed again 0.75. Speeds (client settings): forward
  400, back 400, side 400, up 400, the speed key ×2 (`cl_movespeedkey`). The axes are then
  rounded down to a multiple of 4 (toward negative infinity) and clamped to ±508 — the grid the
  protocol can carry.
- **Buttons**: attack 1, jump 2, use 4, second attack 8, each set when the key was down at any
  moment of the period.
- **Angles** from the view, rounded to the protocol's 16-bit grid (nearest, 65536 steps a
  turn), so the client predicts with exactly the angles the server will use.
- **The packet**: the three newest commands, delta-coded, a checksum, the loss percentage, and
  the frame the client wants entity deltas against. The client sends at its command rate (it
  may thin to a set packet rate, never dropping more than two in a row or one carrying an
  impulse).

## The client's prediction

The client keeps every command it sent, by sequence number, with the time it was sent.

- **Its own player**: from the last frame the server acknowledged (the reply to command `S`),
  take the player's state as the server sent it: origin on the protocol's 1/8 grid (each
  coordinate times 8 truncated toward zero, unless float coordinates were negotiated), velocity
  as whole units (truncated toward zero), move type, jump held and on ground (as the extensions
  send them), and run every command after `S` up to the newest through the same player move as
  the server, with the solid list of the world, the brush models and the other players' boxes
  at their predicted positions. That is the view's position. (The move's unsticking step exists
  because the grid can put the start a hair inside a wall.)
- **When the newest command was just acknowledged** (nothing to predict) or prediction is off,
  the server's state is used as it is.
- **Smoothing** (modern client): a prediction error between 4 and 64 units seen when a frame
  arrives is not snapped; it is folded into a correction that decays at 200 units/s below 8,
  500 below 16, 800 below 32, 1400 above, scaled by a smoothing setting.
- **Other players** are predicted forward from their last state by how long ago the server last
  ran their command: the server sends, per player, the milliseconds since that player's last
  command (`msec`, capped at 255); the client takes the state's time as the frame's arrival time
  minus that, the player time as now minus the measured latency plus 0.02 s (never in the
  future), and runs that player's last command (sent with the state) for the difference in
  milliseconds (capped at 255, split as usual) through the player move.
- **Other entities** are drawn between the last two frames received, by the time since the
  newest arrived over the interval between them.

## Order of a client frame

Read every packet (parse server frames, store their states); on a command period, build a
command and send the packet; predict; draw. The client never moves anything it was not told
about except its own player and the other players' prediction.

## Our deliberate changes

These are decisions for this engine, not facts about the existing programs, and are the only
places our behaviour may differ on purpose:

- **The server's clock is the run clock.** Physics frames run on a fixed step of `sv_mintic`
  (0.013 s by default; `sv_mintic` and `sv_maxtic` keep their meaning for a frame that is late),
  and the speed check and timeouts read server time, never the wall clock. A server's run is
  a pure function of its map, settings and the commands it received in order, so it replays
  to the bit from its input log.
- **Packets are processed at a step boundary in a recorded order**, so a run is reproducible
  across machines; on a live server that order is arrival order.
- Everything else (the player move in 32-bit floats, the thinking window, the 50 ms split, the
  new missile's 0.05 s, the area tree's order) is matched, and the parity labs hold it.
