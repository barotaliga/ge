# Scourge of Armagon's map things: what thirteen of them do (clean-room facts)

A clean-room reader's answers on thirteen of the pack's map things, the ones the things lab (`se-campaign --mod=hipnotic --things`) could not settle by a plain use or touch. Each fact is marked read, derived or open. Questions: what activates each, what it does that a server sees, and the cheapest check.


Read in: the mission pack 1 ("Scourge of Armagon") game program as published in id Software's
2022 GPL re-release of the game code; the base game's program of the same release only where a
shared behaviour (target firing, triggers, damage) is involved.

Marks: [read] stated directly by the source's behaviour; [derived] follows from the source plus
the standard engine's documented rules (named where used); [open] the source does not settle it.

Shared facts used by several answers:

- S1 [read] Firing targets: when a thing "fires its targets", every entity whose `targetname`
  equals its `target` is used, in entity order, and each one sees the firing thing as the
  entity that used it. If the firing thing has a nonzero `delay`, nothing fires now: a helper
  entity of classname `DelayedUse` is spawned and fires the same `target`, `killtarget` and
  `message` after `delay` seconds, and the targets then see that helper, not the original, as
  their user. A `message` is centre-printed only when the activator is a player (with
  `misc/talk.wav` unless the thing has a `noise`). `killtarget` removes every entity of that
  name; when no entity (or no further entity) carries that name, the firing stops there and
  `target` is NOT fired at all (a base behaviour kept in the pack).
- S2 [read] A brush trigger set up the standard way is solid-as-trigger, invisible (no model
  sent), not moving, and its push direction comes from its `angle`/`angles` only when those are
  nonzero: `angle` -1 means straight up, -2 straight down, any other value the forward
  direction of the angles (pitch counts). With no angle the direction stays zero.
- S3 [derived] Standard engine rules relied on: an entity whose solidity is "not solid" is never
  touched; touch functions run for triggers each time a moving player or monster is linked
  while overlapping; an entity whose next think time is zero or below never thinks again,
  entities are spawned at server time 1.0 s; the particle builtin sends an `svc_particle`
  message (origin, direction, count, colour), not a temp entity.

---

## 1. `path_follow` and 2. `path_follow2`

(a) Activation [read]: touch only, and only by an entity flagged as a monster. A player
touching it does nothing; the pack's player decoy (classname `monster_decoy`) is ignored
explicitly. Neither has a use: using either by name does nothing. Conditions on the toucher:
  - a per-monster cool-down of 2 s: a monster that was redirected by any path_follow in the
  last 2 s is ignored [read],
  - a line test from the monster's eyes to its current enemy's eyes, ignoring nothing: when that
  line is clear (the enemy is in plain view) the touch is ignored, so a monster that can see
  its enemy keeps fighting [read]. A monster with no enemy tests a line toward the world's
  origin point; in almost any map that line is blocked, so the touch proceeds [derived].

(b) Effect on the monster [read], in order:
  1. if it had an enemy, that enemy is kept as its previous enemy, its enemy is cleared, and it
   drops into its walking behaviour (it is "tamed"),
  2. its goal and its path target are both set to the first entity whose `targetname` equals
   the path_follow's `target`, and it turns its ideal facing toward that entity,
  3. its 2 s cool-down starts.
  If no entity has that name: a monster that had an enemy takes it back and resumes the hunt
  immediately [read]. A monster that had no enemy: when the server's current "client to look
  for" exists, the monster stands still indefinitely (pause of 999999 s) [read]; when there is
  no such client, it is made to "find" an empty enemy, which looks like a reversed test in the
  source and has no sensible observable result [derived; behaviour beyond that open].
  After step 2 the monster walks to that target with the normal monster walking; reaching a
  `path_corner` continues along the corner chain the base way (each corner's `target` is the
  next, a corner's `delay` is a pause there in seconds, a corner without a `target` or with an
  unknown one ends the walk with the monster standing for good). A walking tamed monster can
  still re-acquire a player by sight through its normal search [derived].
  No sound, no temp entity, no particle is sent by these things themselves [read].

  Who walks them: monsters only. What the map needs: monsters that reach the trigger, and an
  entity named by its `target`, in practice a `path_corner` chain [read]. Without monsters on
  the map these things never do anything [derived].

(c) Nothing on its own: no think [read].

(d) Keys [read]: `target` (the path entity to walk to; no default; unset or unknown gives the
fallback above). `path_follow` is a brush entity (its size is its brush). `path_follow2` is a
point entity with a fixed box of 16 units a side (plus or minus 8 on each axis) around its
origin. `targetname` is not required and not used by them.

Cheapest observable: put a monster with no enemy into the trigger and check that its goal/path
target becomes the named `path_corner` (it turns toward and walks to it). The scripted player
touch must show no effect at all.

## 3. `func_particlefield`

(a) Activation [read]: used by name, once per use; never on from spawn, no toggle, no timer.
Spawnflag 1 (`USE_COUNT`): the use is ignored unless the user's reported counter value equals
the field's `cnt`. Only a `func_counter` reports a value; any other user reports 0, so with
`USE_COUNT` set and `cnt` 0 (the default) any non-counter user still fires it [derived].

(b) Effect of one accepted use, in order [read]:
  1. a window of 0.25 s opens (see touch damage below),
  2. when `noise` is set, that sample is started on the field, voice channel, full volume,
   normal attenuation; this happens whether or not anyone can see it,
  3. only if the server's "client to look for" is in view of the field (the engine's rotating
   potentially-visible client check) are particles sent; otherwise nothing more happens,
  4. particles: one flat sheet through the middle of the brush, a grid of points 16 units apart,
   each point one `svc_particle` with zero direction, colour `color`, count `count` [read,
   message kind derived from S3].
  The sheet's plane [read]: take the brush size minus 16 on each axis. If both horizontal sizes
  exceed the height, the sheet is horizontal (XY) at mid height. If X exceeds height but Y does
  not, the sheet is vertical in XZ at the middle Y. If X does not exceed the height and Y
  exceeds X, the sheet is YZ at the middle X. Otherwise XZ at the middle Y. Grid points start 8
  units in from the brush's lower corner on each sheet axis and step by 16; they run up to 7.9
  units past the brush's upper corner, the XZ sheet including that end and the XY and YZ
  sheets stopping strictly before it (so XZ sheets can get one more row and column) [read].
  The whole grid is sent in the same server frame: "one brief flash" per use, not every frame.
  Touch damage [read in intent, derived in effect]: a touch is meant to deal `dmg` to the
  toucher within 0.25 s of a use, at most once per 0.5 s; but the field is made not solid, so by
  S3 it is never touched and the damage never happens [derived].

(c) Nothing on its own [read].

(d) Keys [read]: `color` palette index, default 192 (yellow); `count` particles per grid point,
default 2; `noise` sample path, default none (do not use a looping sample); `dmg` touch damage,
default 0 (unreachable, see above); `cnt` the counter value to fire on with `USE_COUNT`, default
0; spawnflag 1 `USE_COUNT`. It is a brush entity whose brush is not drawn; it has no `target`.

Cheapest observable: with `noise` set, the sound start on use (it does not depend on
visibility); otherwise the burst of `svc_particle` messages in the frame of the use.

## 4. `info_startendtext`

(a) Activation [read]: used by name; no condition, no touch, no think.

(b) Effect [read]: it puts the server into intermission mode and immediately advances it one
step, exactly as a player pressing a button at the end of a level would; it does not move
players to an intermission camera, does not change their view height and sends no
intermission (scoreboard) message itself.
  - Deathmatch: an immediate level change to the next map (the `samelevel` cvar keeps the same
  map).
  - Otherwise, on `maps/hip1m4.bsp` and `maps/hip2m5.bsp`: a CD track message (track 6, loop
  track 3) then a finale message carrying the localized text key `$qc_finale_hip1` or
  `$qc_finale_hip2`. On `maps/hipend.bsp`: CD track 2 (loop 3), finale text
  `$qc_finale_hipend`, and in a campaign game the achievement `ACH_COMPLETE_HIPEND` (plus
  `ACH_COMPLETE_HIPEND_NIGHTMARE` on skill 3). The text is shown by the client's finale
  screen (the `svc_finale` message: centred typed-out text), not a centre print.
  - On any other map: an immediate level change to the next map, i.e. the map the last touched
  `trigger_changelevel` named; on a map where none was touched that name is empty [read],
  what the engine does with an empty name is open.
  Afterwards [read]: from 1 s later, any player pressing attack, jump or use advances again.
  The second advance, on a registered game: if all four episode runes are held, the finale
  text `$qc_finale_all_runes`; else on hip1m4 `$qc_finale_hip1m4`, on hip2m5
  `$qc_finale_hip2m5`, on hipend `$qc_finale_hipend2` with exit by button disabled and, once the
  client reports the finale finished, after 5 more seconds the credits and a disconnect in
  single player or a change to map `start` in co-op; elsewhere the next map. On the shareware
  game the second advance shows the sell screen. A client connecting during this also
  advances it.

(c) Nothing on its own [read].

(d) Keys [read]: only `targetname`. Point entity, box 16 units a side.

Cheapest observable: on a map not named above, a level-change request right after the use (or
in deathmatch). On a lab map with a custom name, the `svc_finale` path is not reachable.

## 5. `func_counter`

(a) Activation [read]: used by name, or started on its own 0.1 s after spawn with spawnflag 64
(`START_ON`). The counter has an off state and an on (counting) state.

(b) Behaviour [read]:
  Switching on (a use while off, or `START_ON`): any pending "stop at the end" request is
  cleared; with `RESET` (8) the count and the reported value return to 0; the next use is set
  to "switch off" when `TOGGLE` (1) is set, otherwise further uses are ignored for good; then
  the first tick happens after `delay` seconds when `delay` is set, otherwise at once, in the
  same frame as the use.
  Each tick: the internal count goes up by one; the reported value becomes that count, or with
  `RANDOM` (16) a random whole number from 1 to `count` (uniform); the counter fires its
  targets (S1); the next tick is scheduled `wait` seconds later. With `STEP` (4) the counter
  then switches itself off at once, so each use gives exactly one tick (a relay that counts).
  Then, when the count has reached `count`, the count returns to 0, and unless `LOOP` (2) is set
  (or a stop was requested): with `TOGGLE` it switches off (a later use starts it again),
  without `TOGGLE` the counter is removed from the map. With `LOOP` and no stop request it keeps
  counting 1, 2, ... `count`, 1, 2, ... forever.
  Switching off (a use while on, `TOGGLE` only): with `FINISHCOUNT` (32) and a count not at 0,
  the counter keeps ticking and stops at the end of the cycle; otherwise ticking stops at once.
  Activator for the targets [read]: on a tick that happens in the frame of the use, it is the
  entity that used the counter (for example a trigger or relay, not the player behind it); on
  timer ticks it is whatever the global activator holds, normally the world entity [derived].
  So a counter's `message` is in practice never centre-printed [derived].
  Interaction with `delay` [derived]: the counter's own `delay` is also applied by the standard
  target firing (S1) to every tick, so each tick's targets fire `delay` seconds late through a
  `DelayedUse` helper, and they see that helper as their user: `func_oncount` and
  `USE_COUNT` particle fields then read the value 0 and never match.
  Edge [read]: with `STEP` and `FINISHCOUNT` together, the self-switch-off after a tick with a
  nonzero count only records a stop request, so the counter keeps ticking on its timer until
  the cycle ends.
  No sound, no temp entity of its own.

(c) Once started (not `STEP`) it runs on its own every `wait` seconds until it finishes or is
switched off [read]. With `START_ON` it starts without any use [read].

(d) Keys [read]: `count` the number of ticks (or the top of the loop), floored, values 0 or
below become 10; `wait` seconds between ticks, default 1 (0 becomes 1); `delay` seconds before
the first tick (and see the interaction above), default 0; `target`, `killtarget`, `message`.
Spawnflags: 1 `TOGGLE`, 2 `LOOP`, 4 `STEP`, 8 `RESET`, 16 `RANDOM`, 32 `FINISHCOUNT`, 64
`START_ON`. Point entity, box 32 units a side from its origin.

Cheapest observable: a counter with `count` 3 and its default `wait` targeting a
`func_oncount` with `count` 3 (or any visible target): uses of the target at 0 s, 1 s and 2 s
after the use, then the counter entity disappears (not `TOGGLE`, not `LOOP`).

## 6. `func_oncount`

(a) Activation [read]: used by name, meant to be the target of a `func_counter`.

(b) Effect [read]: on each use it asks the entity that used it for its counter value; that is
the counter's reported value (the tick number, or the random value with `RANDOM`) when the user
is a `func_counter`, and 0 for any other user. When the value equals its own `count` it fires
its targets (S1), with the counter as activator. It fires EVERY time the value matches (each
loop of a looping counter, each matching random draw); it is never removed and never disarms.
Its own `delay` delays its firing the standard way (S1). A use by anything other than a
counter never matches, since `count` is at least 1 [derived].

(c) Nothing on its own [read].

(d) Keys [read]: `count` the value to fire on, floored, 0 or below becomes 1; `delay`,
`target`, `killtarget`, `message`. Point entity, box 16 units a side from its origin.

Cheapest observable: its targets fire once when the counter ticks to `count`, and not on the
other ticks. Note: the scripted lab's direct use of an oncount by the player does nothing.

## 7. `func_spawn_small`

(a) Activation [read]: used by name. Same behaviour as `func_spawn`; only its editor box
differs (32 by 32 by 64 units, from -24 to 40 in height, instead of 64 by 64 by 88).

(b) What it spawns [read]: whatever entity its `spawnfunction` names (any spawn function of the
game program: a monster such as `monster_ogre`, or an item), set up at map load as a hidden
copy of the spawner itself, so it gets the spawner's origin, angles, `target`, spawnflags and
every other key (for a monster, spawnflag 1 is ambush). The hidden copy is not solid, has no
model, and is kept idle by a 1 s heartbeat; deathmatch is treated as off while it is set up so
monsters are kept in deathmatch too.
  When no `spawnfunction` is set, one monster is chosen at MAP LOAD, not at use: dog
  (`monster_dog`) 50 %, ogre (`monster_ogre`) 30 %, fiend (`monster_demon1`) 12 %, zombie
  (`monster_zombie`) 5 %, shambler (`monster_shambler`) 3 %. All five hidden copies are created
  and the four not chosen stay on the map as idle hidden entities for good [read].
  On use, in order [read]:
  1. with `spawnmulti` 1, a fresh copy of the hidden entity is made; otherwise the hidden entity
   itself is used,
  2. its model, solidity, size and real think are restored and it is placed at its origin (the
   spawner's origin),
  3. unless `spawnsilent` is nonzero: a `TE_TELEPORT` temp entity at that origin at once, and
   0.2 s later one of `misc/r_tele1.wav` to `misc/r_tele5.wav` (uniform) from that point,
   voice channel, normal attenuation,
  4. if it is a monster and `spawnmulti` is nonzero, the total-monsters count goes up by one and
   an `svc_updatestat` for total monsters is broadcast,
  5. without `spawnmulti`, the spawner removes itself (a second use does nothing).
  The restored monster's own start-up think (dropping to the floor, finding its path) runs at
  the hidden copy's next heartbeat, so within 1 s after the use [derived]. With `spawnmulti`
  each use makes another one. Monster counting [read]: without `spawnmulti` the monster is
  counted in the level's total at load; with it, it is counted only when spawned; the random
  case counts one monster at load whatever `spawnmulti` says.
  The pack's horn of conjuring changes this while it is active (copies are always made, the
  spawn is charmed to the horn's user and given a dim light) [read]; not part of a plain lab.
  Edge [derived]: a `spawnmulti` other than 0 or 1 reuses the single hidden entity every time
  without removing the spawner, so later uses only re-place the same monster.
  Edge [derived]: `spawnfunction` set without `spawnclassname` stops the map with an object
  error at load.

(c) Nothing visible on its own; the hidden copies exist from load [read].

(d) Keys [read]: `spawnfunction` (the name of a spawn function, resolved by the engine as a
function name; default: the random monster), `spawnclassname` (the classname the spawned thing
gets; required when `spawnfunction` is set, should equal it), `spawnsilent` (1 for no teleport
effect; default 0), `spawnmulti` (1 for repeated spawns; default 0), plus any key of the spawned
thing. Editor spawnflags: big/ambush (1), megahealth (2), passed through.

Cheapest observable: an entity of the `spawnclassname` appearing at the spawner's origin with a
`TE_TELEPORT` at that point in the frame of the use (`spawnsilent` 0).

## 8. `trap_gravity_well`

(a) Activation [read]: on its own; it has no use (using it does nothing). It thinks every
0.1 s from 0.1 s after spawn.

(b) Each think [read]: it gathers targets within `distance` of its origin (the engine's radius
search, measured to each entity's box centre [derived]) that are players, or players and
monsters with spawnflag 1, that are not flagged notarget, have health above 0, are not at that
moment held by a tesla coil's or the pack's hammer's bolt, and are in clear sight: a line from
the well's origin to the target's eyes not blocked by the world (monsters do not block it) and
not crossing from open air into liquid. At most `count` targets are taken; `count` is not
defaulted and 0 means no limit, although the editor text says 2 [read]. For each target, the
unit vector from the target's origin toward the well's origin, times `speed`, is ADDED to its
velocity: 210 units/s per think by default, so about 2100 units/s per second of steady pull,
the same at any distance inside the reach, in full 3D (it can lift) [read; rate derived]. With
spawnflag 2 (`UNDERWATER`) a target whose wetsuit is active gets 0.6 of the pull (the editor
text says half) [read]. No sound, no temp entity.
  Touch [read]: the well is a trigger box 32 units a side (plus or minus 16) around its origin.
  Anything that touches it and can take damage takes `dmg` (default 10000, a gib), from the
  well as attacker; the well then ignores touches for 0.2 s (one shared timer for all
  touchers). Missiles are never pulled (not players or monsters) and are not hurt [derived].

(c) Yes, runs on its own from spawn [read].

(d) Keys [read]: `distance` reach in units, default 600; `count` most targets per think,
default unlimited (see above); `speed` pull added per 0.1 s, default 210; `dmg` touch damage,
default 10000. Spawnflags: 1 targetenemies (monsters too), 2 `UNDERWATER`.

Cheapest observable: a player in sight within 600 units gains about 210 units/s of velocity
toward the well within 0.1 s, with no use at all.

## 9. `trap_shooter`

(a) Activation [read]: on its own, by a timer, from spawn; it is on and stays on. A use fires
one extra shot at once and does NOT toggle (the toggling version is
`trap_switched_shooter`). The first timer shot is at the absolute server time
`nextthink` plus `wait` (not relative to spawn): with both keys at their defaults that is 1 s,
which is about the first frame since spawning happens at time 1.0 [derived, S3]. Then a shot
every `wait` seconds forever.

(b) Each shot, by spawnflags (the first flag that matches wins, in this order) [read]:
  - laser (2): a laser bolt, model `progs/laser.mdl`, dim light, 600 units/s, removed after
  5 s; sound `enforcer/enfire.wav` from the shooter on the voice channel unless silent; on
  hitting something with health: blood and 15 damage; else `TE_GUNSHOT` at the impact; and
  `enforcer/enfstop.wav` at the impact unless silent (the bolt carries the shooter's flags).
  - lavaball (4): a fly-missile, model `progs/lavarock.mdl`, size 8 units, 300 units/s, spinning,
  removed after 5 s; sound `misc/spike.wav` unless silent; explodes like a rocket on touch
  (below).
  - rocket (8): a rocket, model `progs/missile.mdl`, 1000 units/s from 8 units in front of the
  shooter, removed after 5 s; sound `weapons/sgun1.wav` on the weapon channel ALWAYS (silent
  does not stop it) and again on the voice channel unless silent. Explosion on touch (also
  for the lavaball): 100 to 120 damage to a toucher with health (half on a shambler),
  120-point radius damage to others, `TE_EXPLOSION`; touching sky just removes it.
  - otherwise a nail: model `progs/spike.mdl`, 500 units/s, removed after 6 s; sound
  `weapons/spike2.wav` unless silent. With superspike (1) it hits for 18 and sends
  `TE_SUPERSPIKE` on a wall; without, 9 and `TE_SPIKE`. Hitting something that bleeds sends
  blood particles instead of the temp entity.
  The shot leaves from the shooter's origin along its direction from `angle`/`angles` (S2
  rules: -1 up, -2 down). The shooter has no model and is not solid.
  Silence [derived]: the source writes the not-silent test in a form whose meaning depends on
  the compiler's precedence for "not" against bitwise "and"; with the classic compiler's
  precedence it reads as intended ("play unless the silent flag is set").

(c) Yes, it fires on its own every `wait` seconds from the start [read].

(d) Keys [read]: `wait` seconds between shots, default 1; `nextthink` absolute time offset of
the first shot, default 0; `angle`/`angles` the direction; `targetname` for the extra shot.
Spawnflags: 1 superspike, 2 laser, 4 lavaball, 8 rocket, 16 silent.

Cheapest observable: the sound start `weapons/spike2.wav` (default flags) once per second, or
one projectile entity of model `progs/spike.mdl` per `wait`.

## 10. `trap_tesla_coil`

(a) Activation [read]: a timer of its own, gated by its `state`: with `state` 1 it is live from
spawn (first think 0 to 1 s after spawn, uniform); with `state` 0 (the default) it is off and
only polls every 0.25 s. A use flips `state`. Turning it ON by a use also sets its next think
time to a field that is never set for this class, i.e. zero; by the engine rule in S3 the coil
then never thinks again [derived]. So: a coil placed with `state` 1 works on its own; a use
turns it off (it keeps polling); a second use leaves it "on" but frozen. A coil placed off and
then used never fires [derived]. This looks unintended; worth confirming black-box.

(b) The cycle while live [read]:
  1. every 0.25 s it looks for targets with the same search as the gravity well (players, plus
   monsters with spawnflag 1; within `distance`; in clear sight from its origin; alive; not
   notarget; not already held by a bolt; at most `count`, which is not defaulted, so 0 means
   no limit although the editor text says 2),
  2. on finding any: `misc/tesla.wav` (any free channel, full volume, normal attenuation, only
   when `wait` is above 0), then a charge of `wait` seconds,
  3. it searches again; for each target found now: `hipweap/mjolhit.wav` and one bolt per
   target. Nobody found now means no zap this cycle,
  4. each bolt acts at once and then every 0.1 s: a line from the coil's origin to the target's
   origin (monsters do not block it); a `TE_LIGHTNING2` temp entity from the coil's origin to
   the line's end; damage `dmg` to the first damageable entity on the line, and to up to two
   more different entities met by two side lines offset from it, each hit also sending a
   particle burst (colour 225, count four times `dmg`, direction up 100) at the hit point.
   The attacker is the world (no knockback). A target wearing an active wetsuit takes no
   bolt damage.
  5. a bolt ends when `duration` has passed (default: never), when the world blocks the line,
   when the target's health is 0 or below, or when it is more than `distance` plus 10 units
   away,
  6. while any bolt is alive the coil does not search again; about 0.3 s after the last bolt
   ends (at most 1.3 s after the zap if bolts end at once) it goes back to step 1, and the
   next zap needs another full charge [read; timing derived].
  Obituary type: electrocuted.

(c) With `state` 1 yes, from spawn [read]. With `state` 0 it does nothing until used, and see
(a) for what a use then does.

(d) Keys [read]: `wait` charge time, default 2 s; `dmg` per bolt tick, default 2 plus 5 times
the `skill` cvar (2, 7, 12, 17); `duration` bolt life, default continuous; `distance` reach,
default 600; `state` 0 off (default) or 1 on; `count` targets per zap, default unlimited (see
above). Spawnflag 1 targetenemies (monsters too). Point entity, not solid, no model.

Cheapest observable: a coil with `state` 1 and a player in sight within 600 units: the sound
start `misc/tesla.wav`, then `TE_LIGHTNING2` messages every 0.1 s from 2 s later.

## 11. `trigger_damagethreshold`

(a) Activation [read]: damage only. It is a solid brush (pushable-type, so bullets and rockets
hit it) that can take damage with `health` set to its key (default 60). A use does nothing (it
has no use); a touch does nothing.

(b) Effect [read]: any hit that leaves it with health above 0 calls its pain response, which
resets its health to the full `health`; damage does NOT add up across hits. A single hit whose
damage (after the attacker's quad multiplier of 4) is at least `health` brings it to 0 or
below and fires: its health is reset to full, it is made immune to damage while it fires its
targets (S1) with the damaging attacker (the player who fired) as activator, then it can take
damage again; without spawnflag 1 (`MULTI_USE`) it removes itself afterwards. Usual pairing:
`func_breakawaywall` as the target, which removes itself when used. No sound, no temp entity
of its own. Rocket splash on it counts like any damage: 120 minus half the distance to its
centre [derived].
  Spawnflag 2 (`INVISIBLE`): its model name is cleared, so it is not sent to clients but stays
  solid and damageable [derived from the engine skipping entities without a model name].

(c) Nothing on its own [read].

(d) Keys [read]: `health` the threshold, default 60; `target`, `killtarget`, `message`,
`delay`. Spawnflags: 1 `MULTI_USE`, 2 `INVISIBLE`.

Cheapest observable: one rocket hitting it directly (100 to 120 damage) fires its target; a
shotgun blast (or any hit under 60) does not, however often.

## 12. `trigger_decoy_use`

(a) Activation [read]: touch only, and only by an entity of classname `monster_decoy`. Using
it by name does nothing (it has no use). A player touching it does nothing. Removed at load in
deathmatch.

(b) What the decoy is [read]: a stand-in for the player in the pack's finale cutscenes, a
walking monster with the player's model `progs/player.mdl`, the player's box and colours,
health 3000000, that walks a `path_corner` route at 12 units per frame with player footstep
sounds (`misc/foot1.wav` to `misc/foot7.wav`, half volume). It is created either by an
`effect_finale` cutscene (from a path corner it names or at the player's position) or placed
as `monster_decoy` (single player and co-op only; not counted as a monster).
  Effect when the decoy touches [read]: once only: its touch is disabled, it removes itself
  0.1 s later, and it fires its targets (S1). It sets NO field on any monster; it does not
  touch monsters at all. The activator is not set by it, so it is whatever the global
  activator last held [derived]. Separately, `path_follow` ignores the decoy [read].

(c) Nothing on its own [read].

(d) Keys [read]: `target`, `killtarget`, `message`, `delay`; it is a brush trigger (S2).

Cheapest observable: none reachable by the scripted player: neither the use nor the player's
touch does anything. With a `monster_decoy` walking a path through it, its target fires once
and the trigger disappears 0.1 s later.

## 13. `trigger_waterfall`

(a) Activation [read]: touch, by players only (monsters and items are ignored); every touch,
no cool-down, no use (using it by name does nothing).

(b) Effect per touch [read]: the player's velocity gains the trigger's direction times `speed`
(default 50 units/s), and its X and Y velocity each gain an independent uniform random amount
between minus half `count` and plus half `count` (default plus or minus 50 units/s). The
direction comes from `angle`/`angles` by the S2 rules (-1 up, -2 down, otherwise the forward
direction of the angles including pitch). With no angle set the direction is zero and only
the random sideways jitter remains [derived from S2]. Because a player inside is touched each
time its movement is linked, the push repeats every player physics frame, so its strength per
second depends on the frame rate [derived]. No sound, no temp entity.

(c) Nothing on its own [read].

(d) Keys [read]: `angle`/`angles` the direction, default none; `speed` push per touch, default
50; `count` total width of the random XY spread, default 100. Brush trigger (S2).

Cheapest observable: a player standing inside a trigger with `angle` -2 gets about 50 units/s
more downward velocity on the first frame in it than a player outside.
