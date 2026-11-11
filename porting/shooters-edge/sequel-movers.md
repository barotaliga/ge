# The sequel's movers and triggers, specified

How the sequel's map movers and triggers move and answer, in our words (README.md, the rules), for
M18v7c: the doors, buttons, plats, trains, rotators, walls, timers, triggers, teleporters and
targets on the sequel's maps, beyond the inventory in `sequel-rules.md` § The demo's map entities.
**Where these facts came from**: a clean-room reader session that read the sequel's GPL game
library and answered written questions (`clean-room` skill); the builder reads this note, never
the source. A fact marked **(verify)** was worked out from the rules read (the frame-by-frame
examples by hand arithmetic, single precision where the original keeps it), not measured: a lab
checks it before code depends on it. **(open)** marks what the game library does not settle.

Units: map units, seconds, the 0.1 s server frame. Within a frame a mover's push comes first and
its think after it, and the think runs only when the whole team's push succeeded
(`sequel-rules.md` § Running an entity). Class names, keys, spawnflag names, sound paths and
console notes are what a map author or player sees.

**Corrections to `sequel-rules.md`**: § Levels says a deathmatch exit without allow-exit kills its
toucher; it damages the entity that used it, which is the trigger, so it does nothing in practice
(§ target_changelevel below). The inventory's movers are not all accelerated: doors, rotating
doors, buttons, trains and func_water use the plain move unless a map sets `accel` or `decel`.
Only func_plat accelerates by default (§ The plain move). func_button's default wait is 3 s.

## The plain move

- **When it is chosen**: a linear mover (door, button, plat, train, water, secret door)
  uses the plain move when its speed, accel and decel are all equal, and the accelerated move
  otherwise. func_door, func_door_rotating and func_button default accel and decel to the speed,
  so they are plain unless the map sets a different `accel` or `decel`. func_train, func_water and
  func_door_secret always set all three equal (plain). func_plat defaults to speed 20, accel 5,
  decel 5 (internal units, § 2), so it is accelerated.
- **When it starts**: the move is set up when the mover is told to go. When that happens
  inside the team master's own think during the frame loop, the plain move begins at once (its
  velocity is set this frame, so the first push is the next frame). Otherwise (a touch during a
  player's command, a use from another entity's think, a trigger) the begin is scheduled for the
  next frame's think, so the first push is two frames after the frame before the touch: one frame
  of latency more (verify).
- **The velocity**: direction × speed (units/s), constant, so each frame pushes speed × 0.1
  units (then rounded to eighths by the push).
- **The whole frames**: the number of whole frames is the floor of (distance ÷ speed) ÷ 0.1
  (the division by the speed done in single precision). The remaining distance after those frames
  is distance − frames × speed × 0.1. When speed × 0.1 is already at least the distance, there are
  no whole frames and the move goes straight to the last frame.
- **The last partial frame**: at the end of the whole frames (after that frame's push) the
  last step runs. When the remainder is exactly 0, the velocity is zeroed and the "arrived" step
  runs in that same frame. Otherwise the velocity becomes remainder ÷ 0.1 for one frame, and the
  next frame pushes the remainder and then zeroes the velocity and runs "arrived". So the
  move takes ceil(distance ÷ (speed × 0.1)) pushes and "arrived" runs in the frame of the last push
  (verify). Examples (verify): 120 units at 100 units/s: 12 pushes of 10; 56 at 100: five of 10
  and one of 6, arrived on the 6th; 28 at 40: six of 4 and one of 4; 96 at 25: 38 of 2.5 and one
  of 1, arrived on the 39th.
- **No snap**: the mover is never set to the destination. Its place is the sum of the
  pushes, each rounded to eighths (§ Running an entity in sequel-rules.md). A remainder that is
  not a multiple of 0.125 ends off the destination: 0.3 units at 100 units/s is pushed as 0.25
  (verify).
- **Blocked**: a blocked frame pushes nothing and delays every team member's pending think
  by 0.1 s, so the schedule slides by one frame per blocked frame and the move resumes where it
  stopped.
- **The angular plain move** (func_door_rotating): always plain, accel and decel unused.
  The travel time is the angle to go (the length of the angle difference, degrees) ÷ speed
  (degrees/s). With less than 0.1 s to go, it goes straight to the last frame. Otherwise the turning
  speed is angle ÷ travel time for floor(travel time ÷ 0.1) frames, and the last frame turns by
  the angle still left, measured from the mover's actual angles, so a turn ends exactly on its
  destination (angles are not rounded to eighths). Because the travel time is held in single
  precision, 90 degrees at 100 degrees/s gives 8 whole frames of 10 degrees and a last frame of 10
  degrees: 9 frames in all either way (verify).

## The accelerated move

- **Units**: the accelerated move works in units per frame. Its speed is the distance one
  frame covers, its accel and decel the change of that per frame; the velocity it sets is the
  current per-frame speed × 10 (units/s). func_plat alone scales its map keys by 0.1, so a plat's
  `speed` key is in units/s, and its `accel` and `decel` keys are in units/s gained or lost per
  frame (10 × the key in units/s²). Plat defaults (no key): speed 20 units/frame (200 units/s),
  accel and decel 5 units/frame per frame (50 units/s per frame, 500 units/s²); the same as keys of
  200, 50 and 50 (verify). Any other mover that reaches the accelerated move (a door or button
  with its own `accel` or `decel`) uses its keys unscaled, so its `speed` is read as units per
  frame: a door with speed 100 and accel 50 runs at up to 1000 units/s (verify).
- **The first frame**: the accelerated move always starts on the next frame's think with
  a current speed of 0, so the first push is the frame after that.
- **Planning**, on the first think (and on any think where the current speed is 0):
- Planning: the target speed is the speed.
- Planning: when the distance is less than the accel, the current speed is set to the whole distance and
  nothing else is planned (the deceleration distance kept from the mover's previous move stays).
- Planning: otherwise the distance to reach a speed v at rate r is taken as v × (v ÷ r + 1) ÷ 2 (the sum
  of r, 2r, ... up to v by frames); the acceleration distance uses the accel and the
  deceleration distance the decel.
- Planning: when the two together exceed the distance, a lower top speed v is found so that the two
  distances add up to the whole: with f = (accel + decel) ÷ (accel × decel), v solves
  f × v² ÷ 2 + v = distance, v = (√(4 + 8 × f × distance) − 2) ÷ (2 × f); the deceleration
  distance is then taken for v.
- **Each frame**: the remaining distance is first reduced by the current per-frame speed
  (the push that just happened, unrounded). Then:
- **decelerating**: once the remaining distance is at most the deceleration distance and
  strictly below it, a pending "next speed" (below) is taken if there is one, else the speed
  drops by the decel, never below the decel itself (a speed at or below the decel is kept).
- **crossing at full speed**: at the top speed, when this frame would cross into the
  deceleration distance, the frame keeps the top speed and the next frame's speed is set to
  top − decel × (the part of the frame past the crossing ÷ the frame's length).
- **accelerating**: below the speed, the speed rises by the accel, capped at the speed (the
  configured speed, not the lowered top speed); when that frame would cross into the
  deceleration distance, its speed is blended from the average of the old and top speeds before
  the crossing and the top speed after it, and the next frame's speed set as in the crossing
  case.
- **the end**: when the remaining distance is at most the current speed, the last frame pushes
  the remaining distance exactly (velocity remainder ÷ 0.1), and the frame after runs "arrived"
  with the velocity zeroed (as in § 1). With a remainder of exactly 0, "arrived" runs at once.
- **A short distance** (verify): a distance below the accel is covered in one push of the whole
  distance. A distance too short for the speed (default plat: below 100 units) climbs by the accel
  to the lowered top speed and descends by the decel. 60 units at 20/5/5: pushes 5, 10, 15, 15,
  10, 5. 100 units: 5, 10, 15, 20, 20, 15, 10, 5.
- **Worked example: the default plat (speed 20, accel 5, decel 5 in internal units, i.e. no keys,
  or keys 200, 50, 50) over 128 units** (verify). Acceleration and deceleration distances 50 each.
  Frame 1 is the first think, 0.1 s after the plat is told to go; the position is after that
  frame's push:

  | Frame | Time after the go (s) | Push | Position | Velocity set for the next frame (units/s) |
  | --- | --- | --- | --- | --- |
  | 1 | 0.1 | 0 | 0 | 50 |
  | 2 | 0.2 | 5 | 5 | 100 |
  | 3 | 0.3 | 10 | 15 | 150 |
  | 4 | 0.4 | 15 | 30 | 200 |
  | 5 | 0.5 | 20 | 50 | 200 |
  | 6 | 0.6 | 20 | 70 | 200 |
  | 7 | 0.7 | 20 | 90 | 170 |
  | 8 | 0.8 | 17 | 107 | 120 |
  | 9 | 0.9 | 12 | 119 | 70 |
  | 10 | 1.0 | 7 | 126 | 20 (the last-frame remainder) |
  | 11 | 1.1 | 2 | 128 | 0, arrived |

  The top-speed frame at 6 crosses the 50-unit deceleration mark mid-frame, so frame 7 runs at 17
  (20 − 5 × 0.6) rather than 15.
- **The same keys taken literally (map keys 20, 5, 5, scaled to 2, 0.5, 0.5)** (verify): pushes
  0.5, 1, 1.5, then 2 a frame, then 1.5, 1, 0.5; 128 units arrive on frame 68 (6.8 s).

## func_door

- **Direction**: from `angle` (a yaw in degrees, pitch and roll 0) or `angles` (pitch, yaw,
  roll): the forward direction of those angles (a positive pitch tips it downward). The special
  values `angle` −1 (straight up) and −2 (straight down). The angles are then cleared (the door is
  not drawn turned).
- **Distance and positions**: distance = |dir x| × size x + |dir y| × size y + |dir z| ×
  size z − `lip` (lip default 8); size is the brush model's extent. The closed position is where
  the map put it, the open one closed + distance × direction.
- **Spawnflags**: 1 START_OPEN (the two positions swap: the door spawns at the open place
  and "opening" moves it to the closed one); 2 REVERSE (no effect on a sliding door); 4 CRUSHER,
  8 NOMONSTER, 16 ANIMATED (the texture animates through all frames); 32 TOGGLE; 64 ANIMATED_FAST
  on a sliding door (fast texture animation), X_AXIS on a rotating one; 128 Y_AXIS (rotating
  only).
- **Defaults**: speed 100 (doubled in deathmatch, § below), accel and decel = speed, wait 3
  (a `wait` of 0 becomes 3; negative stays open), lip 8, dmg 2.
- **States and order**: closed (bottom), opening (up), open (top), closing (down). Used:
  each team member goes up (one at a time down the team chain, master first); starting up plays
  the start sound, sets the move loop, moves, **fires the door's own targets** (with the user as
  activator) and opens its area portals. At the top: the end sound, the loop off; then, unless
  TOGGLE, closing is scheduled `wait` seconds later (none when wait is negative). Closing: start
  sound, loop, and a door with `health` gets its health back and can be shot again. At the bottom:
  end sound, loop off, area portals closed.
- **Used again**: while opening, nothing. While open (not TOGGLE), the close is pushed back
  to `wait` seconds from now. While closing, it reopens (start sound, targets fired again, area
  portals opened). With TOGGLE, a use while opening or open closes the team, and a use while
  closed or closing opens it; a TOGGLE door never closes on its own. A use on a team slave does
  nothing.
- **Touch**:
- no `targetname` and no `health`: on the first frame after spawn the team master spawns a
  trigger field (not drawn) around the whole team: the box of all members' absolute bounds,
  widened by 60 units on each side in x and y (not z). Touched by a living player or monster
  (a monster not with NOMONSTER), at most once per second per field, it uses the door (the
  toucher is the activator).
- `health`: no field; the door must be shot open (below).
- `targetname` and a `message` (and no `health`): touching the door brush prints the message
  centred to that player, at most once every 5 s per door, with the sound `misc/talk1.wav`.
  once the door has been used, its message and touch are removed for good (on every team
  member), so the message shows only until it fires.
- `targetname` alone: opened only by a use.
- **Damage with `health`**: the door takes damage. When its health reaches 0, every team
  member gets its health back and stops taking damage, and the team master is used (the attacker
  is the activator). It takes damage again when it starts closing.
- **Blocked**: by a thing neither a player nor a monster (an item, a corpse from the body
  queue, a missile, debris): 100000 damage to it and then it is removed with an explosion effect
  (explosion 1 sent to those who can see it), whatever the damage did. By a player or a monster
  (alive or dead): `dmg` damage (crush, knockback 1); then a CRUSHER door does nothing more and
  keeps pressing, so it damages again every blocked frame (every 0.1 s); a non-crusher door with
  a `wait` of 0 or more reverses its whole team (closing reopens, anything else closes); with a
  negative wait it does not reverse and damages every frame.
- **Sounds**: `sounds` other than 1: `doors/dr1_strt.wav` at the start of a move,
  `doors/dr1_mid.wav` as the looping sound during it, `doors/dr1_end.wav` at its end; started and
  ended by the team master only (slaves are silent), on the voice channel with static attenuation.
  `sounds` 1 is silent; 2, 3 and 4 (named light, medium, heavy for the map author) all give the
  same set.
- **A team**: the members share one trigger field (the master's) and are used together.
  On the first frame the master finds the smallest distance any member moves, divides it by the
  master's speed to get one travel time, and sets each member's speed to its own distance ÷ that
  time (accel and decel scaled by the same ratio, or set to the new speed when they equalled the
  old speed), so all arrive together. The pushes are made from the master's slot and are undone
  together when any member is blocked.
- **Deathmatch doubling**: only the class func_door doubles its speed in deathmatch (the
  map's `speed` or the default 100, before accel and decel default to it, so the move stays
  plain). Rotating doors, buttons, plats, trains, func_water and secret doors are not doubled.
- **Area portals**: a door's `target` may name `func_areaportal`s, opened when it starts
  opening and closed when it reaches the closed place. A START_OPEN door with a trigger field
  opens them at spawn; a START_OPEN door with a `targetname` or `health` does not (verify).

## func_door_rotating

- **Axis**: by default it turns about the vertical axis (its yaw changes). X_AXIS (64)
  turns its roll (about the forward x axis), Y_AXIS (128) its pitch (about the side y axis).
  Angles grow positively; REVERSE (2) turns the other way. It needs an origin brush, which sets
  the point it turns about.
- **Angle**: `distance` degrees, default 90 when absent (with a console note "<class> at
  <place> with no distance set"). START_OPEN (1) swaps the start and end angles.
- **Speed**: `speed` degrees/s, default 100, never doubled in deathmatch; `accel` and
  `decel` are unused (§ 1, the angular move); `wait` 3, `dmg` 2. No `lip`.
- **Differences from func_door**: the same states, uses, toggle, team, trigger field
  (no `targetname`, no `health`), damage and blocked behaviour, sounds and target firing. Its
  message touch is set whenever it has both a `targetname` and a `message`, even when it also has
  `health` (the sliding door's only without `health`). ANIMATED (16) animates its texture; it has
  no ANIMATED_FAST. A rider on it is carried round its origin and a player's view yaw turns with
  it (sequel-rules.md § Running an entity).

## func_button

- **Move**: direction from `angle`/`angles` as the door; distance = size along it − `lip`
  (default 4); speed 40 units/s (accel and decel default to it: plain move). It is a stop-type
  mover: it carries what stands on it but anything else in its way blocks it (§ 13).
- **Pressing**: touch by a living player only (not monsters), and only on a button with
  neither `health` nor `targetname`. A use presses any button (the user is the activator). With
  `health` it takes damage and is pressed when its health reaches 0 (the attacker is the
  activator; health restored, damage off until it returns). A press while moving in or in is
  ignored; a press while returning starts it moving in again.
- **Firing**: when it reaches the in position: its targets fire (delay, message,
  killtarget as for any use), its brush frame becomes 1 (the texture's alternate frame), and its
  texture animation changes from frames 0–1 (out) to frames 2–3 (in).
- **Wait and return**: `wait` default 3 s (0 becomes 3; the editor's note of 1 s is not
  what the game does); after it the button moves back out (frame 0, damage back on with
  `health`), and at the out position it can be pressed again and its animation is 0–1. A negative
  wait leaves it in for good.
- **Sounds**: `switches/butn2.wav` when a press starts (voice channel, static attenuation.
  not for a team slave), unless `sounds` is 1. Values 2–5 give the same sound. Nothing on the
  return.

## func_plat

- **Positions**: the plat is drawn raised. Top = where the map put it; bottom = top −
  `height` when the key is set, else top − (its height in z − `lip`), lip default 8. Its angles are
  cleared.
- **At the start**: without a `targetname` it moves at once to the bottom and rests there.
  With a `targetname` it stays at the top in the "going up" state, so its trigger does nothing
  until a use sends it down; it then works as a normal plat.
- **Use**: a use sends the plat down only when it has never moved (it has no pending
  action yet). After its first move, uses do nothing (verify), so a targeted plat is released
  once.
- **The trigger**: spawned at the plat's spawn, fixed in the world (it does not ride the
  plat), in the map's coordinates of the plat as drawn (raised). x and y: the plat's extent shrunk
  by 25 units on each side (when that leaves nothing on an axis, a 1-unit slab at the centre of
  that axis). z: from 8 units above the raised plat's top down by (travel + lip), where travel is
  top − bottom and lip the key (default 8). With the default lip and no `height`, that bottom is
  the lowered plat's top surface (verify). LOW_TRIGGER (spawnflag 1): only the lowest 8 units of
  that span.
- **Touching it**: only living players. At the bottom: the plat goes up. At the top: going
  down is put off to 1 s from now. Going up or going down: nothing (a player stepping on a
  descending plat does not send it back up).
- **The top**: on arriving it waits a fixed 3 s and goes down (the `wait` key is read but
  unused). A player still standing on it is inside the trigger, and every command touches it, so
  the plat stays up until 1 s after the last touch (verify). With LOW_TRIGGER the trigger is only
  at the bottom, so a rider at the top does not hold it and it goes down after 3 s (verify).
- **Blocked**: by a non-player non-monster: 100000 damage and removal with an explosion
  effect (as the door). By a player or monster: `dmg` (default 2) crush damage, then it reverses
  (up becomes down, down becomes up).
- **Speeds**: § 2; defaults speed 20, accel 5, decel 5 internal; `speed`, `accel`, `decel`
  keys × 0.1.
- **Sounds**: always `plats/pt1_strt.wav` at the start of a move, `plats/pt1_mid.wav` as the
  loop, `plats/pt1_end.wav` at the end (the `sounds` key is ignored); the team master only.

## func_train

- **First corner**: on the frame after it spawns (0.1 s) the train picks the entity its
  `target` names (a random one when several share the name) and places itself so that **its mins
  (the lowest corner of its box) sits at the corner's origin**: origin = corner − mins. Its target
  becomes that corner's `target`. Without a `target` it never moves (console note "func_train
  without a target at <place>").
- **Starting**: a train without a `targetname` starts by itself (START_ON is set for it); a
  train with a `targetname` starts only with START_ON (1). Starting means a move to the next
  corner on the following frame, with the train as its own activator.
- **Corner to corner**: each leg picks the next corner by name (random among duplicates),
  and moves the train's mins to it at the train's `speed` (default 100 units/s), plain move. The
  corner's `wait` is taken; the corner's `speed` is **not** read (no override). On arriving: the
  corner's `pathtarget` fires (as targets, with the train's activator); if that removed the
  train, nothing more. Then the corner's wait: 0 goes on to the next corner in the same frame (no
  pause); positive waits that many seconds; negative stops the train (with TOGGLE (2) it is
  stopped already pointed at the following corner and can be restarted; without TOGGLE it can
  never be restarted). With a non-zero wait the move's loop sound stops.
- **Teleport corners**: a `path_corner` with spawnflag 1 (TELEPORT): when the train's next
  corner is one, the train jumps to it at once (mins at the corner, no interpolation, a "teleport"
  event for clients) and goes on to the corner after it in the same step. Two teleport corners in
  a row stop the train with a console note.
- **Use**: when moving or waiting with START_ON: a TOGGLE train stops where it is (velocity
  zero, schedule cleared); a non-TOGGLE train ignores the use. When stopped: it resumes toward the
  corner it was heading for, from where it is; a train that never had one starts to the next corner.
- **Blocked**: by a non-player non-monster: 100000 damage and removal with an explosion
  effect. By a player or monster: `dmg` (default 100; the editor's note of 2 is not what the game
  does) crush damage at most once every 0.5 s; the train never reverses or stops its schedule, it
  presses until the obstacle goes (the schedule slides one frame per blocked frame). BLOCK_STOPS
  (4) sets dmg to 0, so it does no damage and simply waits.
- **Sound**: `noise` names a loop played while moving; there is no start or end sound.

## func_rotating, func_wall, func_timer

- **func_rotating**: spawnflags 1 START_ON, 2 REVERSE, 4 X_AXIS, 8 Y_AXIS, 16 TOUCH_PAIN,
  32 STOP, 64 ANIMATED, 128 ANIMATED_FAST. Turns about the vertical axis by default; X_AXIS turns
  its roll, Y_AXIS its pitch (as the rotating door's 64 and 128, but on bits 4 and 8). `speed`
  degrees/s, default 100; `dmg` default 2. Use toggles: spinning stops at once (no slowing) and
  touch pain is removed; still starts at full speed at once and, with TOUCH_PAIN, touching it while
  it spins does `dmg` crush damage per touch. START_ON spins it from spawn. Blocked: `dmg` crush
  damage every blocked frame, never reverses. STOP makes it a stop-type mover: it carries only
  what stands on it and is blocked by anything else in its way, instead of pushing it. It plays no
  sound.
- **func_wall**: spawnflags 1 TRIGGER_SPAWN, 2 TOGGLE, 4 START_ON, 8 ANIMATED, 16
  ANIMATED_FAST. With none of 1, 2, 4: a plain solid wall that ignores uses. Otherwise
  TRIGGER_SPAWN is forced on, and START_ON without TOGGLE gets TOGGLE added (console note
  "func_wall START_ON without TOGGLE"). It starts present (START_ON) or absent (not solid, not
  sent to clients). A use makes an absent wall appear, solid and drawn, and kills everything inside
  it (100000 damage that ignores god mode and invulnerability, telefrag), or makes a present wall
  vanish. Without TOGGLE the use is removed after the first one (one-shot).
- **func_timer**: spawnflag 1 START_ON. Keys `wait` (default 1 s), `random` (default 0; a
  `random` at or above `wait` is cut to wait − 0.1 with a console note), `delay`, `pausetime`. Not
  touchable, not sent to clients. A use toggles it: on, it fires after `delay` (at once when no
  delay) and then every wait + r × random seconds, r uniform in [−1, 1) drawn each time; off, the
  schedule is cleared. START_ON: the first firing at 1 + pausetime + delay + wait + r × random
  seconds after spawn, the timer its own activator. Each firing uses its targets the usual way,
  so its own `delay` also postpones every firing's effect by that much (verify).

## trigger_once and trigger_multiple

- **Who**: spawnflags 1 MONSTER, 2 NOT_PLAYER, 4 TRIGGERED. Players fire it unless
  NOT_PLAYER; monsters only with MONSTER; nothing else does. Dead players and monsters touch no
  triggers at all.
- **Facing**: when the trigger has a non-zero `angle`/`angles`, a toucher fires it only
  when its facing direction (its angles' forward, pitch included) is within 90 degrees of that
  direction (the dot product is not negative). An angle of exactly 0 means no check, so "facing
  east" cannot be required (verify).
- **TRIGGERED**: the trigger starts not solid; the first use makes it solid and live (that
  use does not fire it); later uses fire it.
- **A use**: fires it like a touch, the user as activator, whatever the who-flags say.
- **Firing**: ignored while the trigger is waiting. Otherwise it uses its targets at once
  (with `delay`, `message`, `killtarget` and `target` as any use), the toucher or user as the
  activator. `wait` default 0.2 s for trigger_multiple; it is then dead for `wait` seconds.
- **`message`**: centre-printed to the activator only, unless the activator is a monster.
  with the trigger's sound: `sounds` 1 `misc/secret.wav`, 2 `misc/talk.wav`, 3
  `misc/trigger1.wav`; no `sounds` (or 4): `misc/talk1.wav`. No message, no sound.
- **trigger_once**: wait −1: after firing it loses its touch at once and is removed on the
  next frame. Older maps' bit 1 on a trigger_once is moved to bit 4 (TRIGGERED) at spawn with a
  console note "fixed TRIGGERED flag on <class> at <place>", so a trigger_once cannot carry the
  MONSTER flag (verify).

## trigger_push

- **Direction**: the forward direction of its `angle`/`angles` (−1 up, −2 down). With no
  angle (all zero) the direction is zero, so it sets touchers' velocity to zero (verify).
- **The push**: velocity = direction × `speed` × 10 units/s, set (not added); `speed`
  default 1000, so 10000 units/s by default. It applies to grenades from the grenade launcher
  (class `grenade`; not hand grenades) whatever their health, and to anything with health above 0
  that touches it (players, living monsters). Rockets, bolts, items and the dead are not pushed.
- **Players**: the speed before the push is overwritten with the new velocity, so the push
  causes no falling damage on the next check; the sound `misc/windfly.wav` plays on the player at
  most once every 1.5 s per player.
- **PUSH_ONCE** (1): the trigger is removed after its first touch by anything that reaches
  it, pushed or not.
- **Clamps** (open): a monster or a tossed thing is held to `sv_maxvelocity` per axis by its own
  physics (sequel-rules.md); a player's limit comes from the move and the network state, which
  the game library does not settle.

## trigger_hurt

- **Spawnflags**: 1 START_OFF (not solid), 2 TOGGLE (a use switches it on and off), 4
  SILENT, 8 NO_PROTECTION, 16 SLOW. Only a TOGGLE trigger answers a use, so START_OFF without
  TOGGLE stays off for good (verify).
- **Timing**: `dmg` default 5. It hurts anything that takes damage and touches it, and then
  hurts nothing more until 0.1 s later (SLOW: 1 s later). The hold-off belongs to the trigger, not
  to the toucher: two things in it in the same frame and only the first touch is hurt (verify).
- **The sound**: `world/electro.wav` on the hurt toucher, only on frames whose number is a
  multiple of 10 (once a second at most), unless SILENT.
- **The damage**: knockback = `dmg` but no direction (no push); the obituary "was in the
  wrong place". Armour and power armour absorb it as usual. NO_PROTECTION makes it ignore god
  mode, invulnerability and the team-damage rule; it does **not** bypass armour.

## misc_teleporter and misc_teleporter_dest

- **The pad**: misc_teleporter is a solid box from (−32, −32, −24) to (32, 32, −16) about
  its origin (the disc one stands on), the model `models/objects/dmspot/tris.md2` skin 1, the
  teleporter particle effect, and the loop `world/amb10.wav`. Without a `target` it is removed at
  spawn (console note "teleporter without a target.").
- **The touch box**: a separate trigger from (−8, −8, 8) to (8, 8, 24) about the pad's
  origin: a 16 × 16 × 16 box above the disc's centre.
- **Who**: living players only.
- **The teleport**: the destination is the first entity (in index order) whose
  `targetname` is the pad's `target`; none: a console note "Couldn't find destination" and
  nothing happens. The player is taken out of the world, placed at the destination's origin + 10
  in z (its old origin too, so it is not drawn sliding), its velocity zeroed, and held for 160 ms
  (the move's teleport hold: 20 × 8 ms). A teleport event goes on both the pad and the player (the
  splash). The view is turned to the destination's angles, the player's own angles cleared.
  Then everything at the destination whose box overlaps the player (player-solid) takes 100000
  damage that ignores god mode and invulnerability (obituary "tried to invade ... 's personal
  space"); the teleport happens whether or not they die. The player is put back in the world.
- **misc_teleporter_dest**: the same model (skin 0), also a solid box (−32, −32, −24) to
  (32, 32, −16). Its angles give the arriving view.

## Riders and the frame

- **Order**: each frame visits the entities in index order. The players' slots (1 to max
  clients) come first and do only their frame-start work; the movers push in their own (later)
  slots, a team from its master. The players' own moves are not in the frame: each command runs
  the player's move when it arrives, between frames, against the movers' current places, and
  touches triggers (and the solids it hit) after the move.
- **Riding**: a player standing on a mover has that mover as its ground entity (set by its
  move). A push moves every entity whose ground entity is the pusher by the whole move (and turns
  it with the rotation), up or down, so a rider sinks with a descending plat rather than falling.
  Other entities are moved only when their box overlaps the mover's new place, and only by a
  push-type mover; a stop-type mover (button, STOP rotating) treats them as obstacles. A pushed
  entity that was not riding loses its ground entity (it may have been pushed off an edge).
- **Losing the ride**: at the start of each frame, in the player's slot, the ground entity
  is dropped when the mover has been moved since the player's last command recorded it. So a
  player counts as a rider only when a command arrived since the mover's last push; if two frames
  pass with no command, the second frame's push does not carry the player: a mover going up still
  pushes it by overlap, one going down leaves it (verify).
- **A box that would end in a solid**: tested with its own clip mask (else the solid mask).
  It is tried back at its old place; if it fits there it is left there (not moved, the push goes
  on). If not, the whole team's push is undone (every moved thing back, in reverse order, a
  player's yaw restored), every member's pending think is put off 0.1 s, and the blocked member's
  blocked action runs on that obstacle; a mover without one just waits.
- **Not considered**: entities that are movers themselves (push or stop), stationary
  (move type none), no-clip, or not linked into the world. Gibs are not solid, so they are passed
  through, not pushed (open: that a non-solid entity is not linked is the engine's, not the game
  library's).
- **Dead bodies and items**: the blocked actions of doors, plats, trains and secret doors
  split on "player or monster" versus anything else. A dead player is still a player and a dead
  monster still a monster: they take the mover's `dmg` and a door or plat reverses as for the
  living. Anything else (an item, a corpse left in the body queue, a missile, a barrel's debris
  that is solid) takes 100000 damage and is then removed with an explosion effect. func_rotating
  makes no such split (always `dmg`).

## target_changelevel

- **How it fires**: by a use: any entity using its targets (usually a trigger_once or
  trigger_multiple whose `target` names it; a button or relay would do too). Keys: `map`. Without
  `map` it is removed at spawn (console note "target_changelevel with no map at <place>").
- **Single player**: ignored while the player (the first client) is dead.
- **Deathmatch without allow-exit** (`dmflags` 4096 clear): instead of changing level it
  deals 10 × max health damage (obituary "found a way out") to the entity that used it. That
  entity is the trigger (or a delay helper), never the player, and a trigger takes no damage, so
  in practice the exit does nothing in such a match (verify). With allow-exit: "<name> exited the
  level." to everyone, then the intermission.
- **Already in an intermission**: ignored.
- **The deathmatch limit** (§ Deathmatch in sequel-rules.md): ends the level by starting
  the intermission directly with a changelevel it makes itself; the exit rules above do not apply
  to it.
- **Map name forms**: the `map` string is passed whole to the server as the next map. A
  `*` anywhere in it marks a new unit: the unit's cross-level bits are cleared, co-op players lose
  their keys, and the intermission is shown. Without `*`, single player and co-op leave at once
  (next frame) with no intermission; deathmatch always shows it. `$` separates the spawn point
  (sequel-rules.md § Levels). Any other form (a cinematic or picture as the next "map", a `+`
  chain) is the server's (open). One shipped map's changelevel is rewritten by name at spawn to
  add a spawn point (a fix for that map).

## target_speaker, target_explosion, target_secret, target_goal

- **target_speaker**: keys `noise` (a sound path; `.wav` added when missing; without it a
  console note and the speaker does nothing), `volume` (default 1), `attenuation` (default 1; −1
  means none, heard everywhere; 2 idle, 3 static). Spawnflags 1 looped-on, 2 looped-off, 4
  reliable. A looped speaker (1 or 2) toggles its looping sound on each use (1 starts playing).
  the loop is the entity's looping sound, so `volume` and `attenuation` are not applied to it.
  Otherwise each use plays the sound once at its place on the voice channel (reliable with 4),
  with its volume and attenuation.
- **target_explosion**: keys `delay`, `dmg` (default 0). A use, after `delay` (or at once):
  an explosion effect at its place (sent to those who can hear it), radius damage `dmg` within
  `dmg` + 40 units (the user as attacker; obituary "blew up"), then its own targets fire with no
  extra delay. Reusable.
- **target_secret**: removed in deathmatch. Counted in the level's secret total at spawn.
  Keys `noise` (default `misc/secret.wav`), `message`, `target`. A use plays the noise at its place
  (voice channel, normal attenuation), counts one secret found, fires its targets (the message to
  the activator, as any use) and removes itself (once only). One shipped map gets a message set by
  position (a fix for that map).
- **target_goal**: the same as target_secret for the goal counts; when the last goal is
  found the CD track is set to 0 (the music stops).
