# Ground Zero: map entities, game modes and campaign (facts)

This is a WORKING-AREA facts page for Ground Zero's (the second Quake II mission pack, game
folder `rogue`) map entities, game modes and campaign. A clean-room reader read the pack's game
library as released under the GPL v2 (the yquake2 "Ground Zero for Yamagi Quake II" release,
HEAD of November 2026), and compared it with Quake II's own game library from the same project
to tell what the pack adds or changes. It holds facts only, in the reader's own words: no code.
Each fact is marked **read** (stated directly by the source), **derived** (follows from what
the source does, worked out by the reader) or **measured** (none here: nothing was run).

Units: distances in map units, times in seconds, one server frame is 0.1 s. "Speed key" means
the value a mapper types in the entity's `speed` key.

Two cautions about the source release. It is a community bug-fix release of the pack, so a few
behaviours may differ from the 1998 retail library; where the release itself says it changed
something, that is noted. Several behaviours also differ between this release and the base
library of the same project only because the two ports drifted apart (not because the pack
changed them); those are left out of question 6 unless they matter to a mapper.

---

## 1. `func_plat2`

Spawnflags (read): 1 `PLAT_LOW_TRIGGER`, 2 `PLAT2_TOGGLE`, 4 `PLAT2_TOP`, 8
`PLAT2_TRIGGER_TOP`, 16 `PLAT2_TRIGGER_BOTTOM`, 32 `BOX_LIFT`.

Keys and defaults (read):
- `speed`: default internal 20 units per frame (200 units/s); a key value is multiplied by 0.1,
  so the key is in units/s. Same rule as `func_plat`.
- `accel` and `decel`: default internal 5 each; a key value is multiplied by 0.1. Because speed
  and accel differ, the plat always uses the frame-stepped accelerating mover, in which speed,
  accel and decel are read per 0.1 s frame: the default accel is 5 units/frame per frame, which
  is 500 units/s² (derived). A key of 50 reproduces the default accel (derived).
- In deathmatch, speed, accel and decel are all doubled after the defaults are applied (read).
  (`func_plat` does not do this.)
- `dmg`: default 2 (damage when blocked).
- `lip`: no default (0). `func_plat` defaults it to 8 (read).
- `height`: if set, the plat travels `height` minus `lip`; otherwise it travels its own model
  height minus `lip` (read). (`func_plat` with `height` travels exactly `height`, ignoring lip.)
- `wait` is copied but never read; the waits are fixed (below) (read/derived).
- Sounds are fixed: `plats/pt1_strt.wav` (start), `plats/pt1_mid.wav` (looped while moving),
  `plats/pt1_end.wav` (stop) (read). There is no `sounds` key.
- The top position is where the plat is built; the bottom is below it by the travel (read).

Start position (read):
- Without a `targetname`: it starts at the bottom, unless `PLAT2_TOP` is set, in which case it
  starts at the top. Its trigger is spawned at once.
- With a `targetname`: it starts at the top and does nothing until used. The first use spawns
  its trigger, makes it an ordinary plat2, and sends it down at once (read). When the trigger is
  built at that moment the spawn-time `lip` is no longer known, so the trigger's height uses a
  lip of 0 (derived).

The trigger box (read): the same box `func_plat` builds (the plat's horizontal extent shrunk by
25 on each side, from 8 above the plat's top down by the travel plus lip; with
`PLAT_LOW_TRIGGER` only 8 tall at the bottom of that range; at least 1 unit wide), then grown
by 10 on each horizontal side. Only living players and living monsters operate it (read).

What makes it move (read):
- A touch of the trigger, or a use while stopped (a use is ignored while the plat is moving
  up or down, and ignored by the same 2 s lockout as a touch). A use acts as if the activator
  had touched the trigger.
- Lockout: nothing happens while it is moving, or within 2 s of the last move time.
- On a touch, the plat decides which end the toucher wants:
  - Plat at the bottom: the toucher wants the top if the toucher's origin is above the middle
   height of the trigger box; else the bottom.
  - Plat at the top, normal lift: the toucher wants the bottom if the toucher's origin is below
   the top of the trigger box; else the top.
  - Plat at the top, `BOX_LIFT`: the toucher wants the bottom if the toucher's origin is below
   the middle height of the trigger box.
  - If the wanted end is not where the plat is, the plat was "called": it leaves after 0.1 s.
   If the toucher is already at the plat's end (riding it), it leaves after 0.3 s in
   deathmatch or 0.5 s otherwise. Either way it goes to the other end.
- Arriving at an end, it plays the stop sound, then:
  - If it had been called: unless `PLAT2_TOGGLE`, it returns to the end it came from after
   5 s. Its last move time is set back by 1 s (deathmatch) or 2 s (otherwise), so the 2 s
   lockout lasts 1 s or 0 s after arrival.
  - If not called and it reached the top: unless `PLAT2_TOP` or `PLAT2_TOGGLE` is set, it goes
   back down after 2 s.
  - If not called and it reached the bottom: if `PLAT2_TOP` is set and `PLAT2_TOGGLE` is not,
   it goes back up after 2 s.
  - Otherwise it stays.
  - In every case it fires its targets on arrival at either end (read). The
   `PLAT2_TRIGGER_TOP` and `PLAT2_TRIGGER_BOTTOM` flags are never tested, so targets fire at
   both ends whatever they say (derived).
- So `PLAT2_TOGGLE` means "stays where it was sent", and `PLAT2_TOP` makes the top the resting
  end (read, as the editor note says).

Blocked (read): an entity that is neither a monster nor a player takes 100000 crush damage and,
if still there, is turned into an explosion. A dead monster or player takes 100 crush damage
first (gibbing it); then the blocker takes `dmg` crush damage and the plat reverses.

Monsters (read): while rising, the plat lays a "bad area" (a region monsters avoid) covering the
plat's footprint from its bottom to 64 above it; it is removed when the plat reaches the bottom.
A monster that wants to reach a player above or below it, and is standing on or about to step
onto any entity whose classname begins `func_plat`, uses that plat when it is stopped at the
end that helps (the probe looks up to 384 below the monster's next step).

## 2. `hint_path`

Spawnflags (read): 1 `END`. Keys: `targetname`, `target`, `wait`.

Spawning (read): removed in deathmatch. One with neither `targetname` nor `target` is removed
with the developer message `unlinked hint_path at (x y z)`. It is a 16-unit cube trigger (from
-8 to +8 on each axis), invisible.

Linking chains at level start (read; done only outside deathmatch):
- A chain starts at a `hint_path` with `END` set and a `target` but no `targetname`. An `END`
  node with both `target` and `targetname` is rejected with the message `Hint path at (x y z)
  marked as endpoint with both target (...) and targetname (...)`.
- At most 100 chains; starts beyond that are ignored.
- From each start, the chain follows `target` to the node whose `targetname` matches, and on
  until a node has no `target`. The far end is normally also marked `END` (the editor note
  asks for it; the linking does not require it).
- If a `target` matches more than one node the chain is cut at its start (it becomes a single
  node) with the message `Forked hint path at (x y z) detected for chain N, target ...`. A node
  already on a chain ends the walk with `Circular hint path at (x y z) detected for chain N,
  targetname ...`, also cutting the chain at its start.

What a monster does (read):
- A monster that has an enemy but has lost sight of it for 5 s, and has not checked hint paths
  in the last 10 s, looks for one. Monsters told to stand ground and `monster_turret` never do.
- It keeps the nodes within 512 of itself that it can see, and the nodes within 512 of its
  enemy that the enemy can see. Only chains that have nodes in both sets count.
- It walks to the nearest node it can see on such a chain (the entry), and aims for a node on
  the same chain near the enemy (the goal). Because of the way the comparison is written, the
  goal picked is the last qualifying node in the chain list, not the nearest one (derived).
- Touching each node, it steps to the next node along the chain toward the goal (either
  direction). If a node has `wait`, the monster freezes for that many seconds on touching it
  (meant for riding a plat).
- It leaves the path on reaching the goal, if it cannot work out the direction, or as soon as
  it gets an enemy by sight or sound. On leaving, if its enemy is still valid it attacks if
  visible or hunts it; otherwise it forgets the enemy and stands (with a very long pause so it
  does not wander).
- In co-op, a monster on a hint path does not count a heard sound as a reason to break off
  (read).

## 3. `info_player_coop_lava`

Read: removed unless co-op. Box -16 -16 -24 to 16 16 32. Used only on the maps named `rmine2`
and `rmine2p` (by name), instead of `info_player_coop`, for every co-op player (player 0 too).

How a spot is chosen (read): the game looks at every `func_door` with spawnflag 2 whose centre
is in water, slime or lava (that is, the rising "smart water", question 6) and takes the
highest top of them; the lava line is that top plus 64. Of the first 64
`info_player_coop_lava` spots, it takes the lowest one whose origin is not below the lava line
and that has no player within 32. If none qualifies (or there is no such liquid door, or no
such spot), it falls back to the ordinary start (`info_player_start`, then the co-op fallback
search).

## 4. Specific entities

**`misc_nuke_core`** (read): box -16 -16 -16 to 16 16 16. Model
`models/objects/core/tris.md2`. Starts visible; each use toggles it invisible / visible. It is
not solid and has no other behaviour.

**`target_anger`** (read): keys `target` (the monsters to anger) and `killtarget` (the one
entity they get angry at). Removed with `target_anger without target!` or `target_anger without
killtarget!` if either is missing. On use: finds the first entity named by `killtarget`; that
entity is marked as a monster and a "good guy" (so monsters will fight it) and its health is
set to 300. Then every entity named by `target` that can be used gets it as its enemy, is
flagged as angered by a target, and goes straight to attacking it. If one of those has health
below 0 the whole use stops there. Invisible; does nothing else.

**`target_blacklight`** (read): removed in deathmatch. No size. Model
`models/items/spawngro2/tris.md2`, frame 1, with the tracker effects (the dark pulsing light
and its trail). Every 0.1 s it turns to random angles (0 to 359 on each axis). Not usable.

**`target_orb`** (read): removed in deathmatch. No size. Same model, frame 2, with the
translucent-sphere effect. Every 0.1 s random angles. Not usable.

**`target_killplayers`** (read): on use, every player in the game takes 100000 damage that
ignores protection (death message as telefrag); then every living, damageable entity that any
player can see takes damage equal to its health, also ignoring protection. Invisible.

**`target_steam`** (read): point entity. Keys:
- `speed`: particle speed, default 75 (the editor note says 50; the code's default is 75).
- `count`: number of particles, default 32; kept to 0 to 255.
- `sounds`: palette colour, default 8 (steam); kept to 0 to 255. The colours used run from this
  index to index plus 6. The note suggests 6 to 9 whites, 224 sparks, 176 blue water, 80 brown
  water, 208 slime, 232 blood.
- `wait`: seconds the effect runs; turned into milliseconds at start. If unset, the first use
  sets it from the user's `wait` times 1000 (so a `func_timer` that fires it each second makes
  a 1 s jet), or 1000 ms when there is no user.
- `angle`/`angles`: direction when there is no `target`.
- `target`: aim at the centre of that entity; then the entity starts 1 s after spawn (so its
  target exists) and the direction is recomputed toward the target's centre on each use. A
  missing target prints `target_steam at (x y z): NAME is a bad target`.

What a use sends (read): the steam temporary entity (number 40 in the pack's list) to every
client that can see the origin. Long form, when the duration is over 100 ms: a 16-bit effect id
(a counter shared by all steam entities that wraps after about 20000), the particle count byte, the
origin (the entity's own origin, not offset), the direction, the colour byte, the speed as a
16-bit value, and the duration in milliseconds as a 32-bit value. Short form, for 100 ms or
less: the id is sent as -1, then count, origin, direction, colour, speed, and no duration
(read). How the client draws the two forms is client-side and was not read.

**`trigger_disguise`** (read): brush trigger. Spawnflags 1 `TOGGLE`, 2 `START_ON`, 4 `REMOVE`.
It starts active only with `START_ON`. Every use toggles it active / inactive, whatever the
`TOGGLE` flag says (the flag is never tested) (derived). While active, a player touching it is
given the disguise, or with `REMOVE` loses it. It gives and takes nothing else: no items, no
health.

What the disguise does (read):
- Monsters do not pick a disguised player as the player they look for, do not wake when a
  disguised player triggers them, and a trigger-spawned monster ignores a disguised enemy.
- Noises a disguised player makes are ignored, except weapon noise: firing makes the player
  a "violator" for 5 frames (0.5 s), and monsters checking for targets then treat that player
  as seen.
- When a monster finds a disguised player as its enemy, the disguise is removed.
- The player is drawn with the disguise render flag (the client then draws the player with its
  disguise skin) (read for the flag, the skin is client-side).
- The disguise is carried across levels with god mode, notarget and power armour (read).
- The console command `disguise` gives it too (read).

## 5. Every spawn name the pack's library adds

Read from the spawn table and the item list, compared with the base game's:

Map entities
- `func_plat2`: the smarter lift (question 1).
- `func_door_secret2`: a second secret door. Spawnflags 1 `open_once` (it then never returns,
  the editor note wrongly says "not implemented"), 2 `1st_left`, 4 `1st_down`, 8 `no_shoot`
  (never tested), 16 `always_shoot`, 32 `slide_right`, 64 `slide_forward`. Must face 0, 90,
  180 or 270 (otherwise `Secret door not at 0,90,180,270!`). It moves one depth (its size along
  the facing) backward, or forward with `slide_forward`, and one width (its size across) left,
  or right with `slide_right`; by default the sideways move comes first, with `1st_down` the
  to-and-fro move first. Order on use (or when shot): first move, wait 1 s, second move, wait
  `wait` (default 5), unless `open_once`, move back to the end of the first move, wait 1 s,
  move home. Speed, accel and decel 50 (a steady 50 units/s). `dmg` default 2 when blocked
  (only the team master deals it). Shootable (health 1) when it has no `targetname`, or with
  `always_shoot`; it becomes shootable again when home. A touching living player sees
  `message` centred, at most every 2 s. Sounds `doors/dr1_strt.wav`, `doors/dr1_mid.wav`,
  `doors/dr1_end.wav`. All read.
- `func_force_wall`: a vertical particle wall that is solid when on. Spawnflag 1 `start_on`.
  `style` is the particle colour, default 208 (green; the note lists 240 red, 241 blue, 224
  orange). The particle line runs along the brush's longer horizontal side at its top, through
  its middle. While on, every 0.1 s it sends the force-wall temporary entity (number 37) with the
  two end points and the colour byte, to clients that can see its centre. Use toggles it: on
  makes it solid and kills anything inside it (telefrag), off makes it non-solid and stops the
  particles. Invisible brush. With `start_on` it spawns solid but without particles; the first
  use keeps it on and starts the particles, the second turns it off (derived from the flag
  handling).
- `hint_path`: monster route marker (question 2).
- `info_player_coop_lava`: co-op start above the lava (question 3).
- `info_teleport_destination`: destination marker for `trigger_teleport`, box -16 -16 -24 to
  16 16 32; does nothing itself.
- `trigger_teleport`: brush teleporter. Spawnflags 1 `player_only`, 2 `silent`, 4 `ctf_only`, 8
  `start_on`; only players are ever teleported, and `silent` and `ctf_only` are unused. `target`
  names the destination (any entity with that `targetname`). Without a `targetname` it is
  always on. With a `targetname` it starts off (or on with `start_on`) and each use toggles it.
  On a touch: the teleport temporary entity (number 48) at the player's old place, the player
  placed at the destination plus 10 up, velocity cleared, movement held for 160 ms, the
  teleport event on the player, view turned to the destination's angles, and anything at the
  destination killed. `wait` defaults to 0.2 but is not used. Missing destination: `Teleport
  Destination not found!`.
- `trigger_disguise`: question 4.
- `misc_nuke_core`, `target_anger`, `target_blacklight`, `target_killplayers`, `target_orb`,
  `target_steam`: question 4.
- `turret_invisible_brain`: an invisible driver for a `turret_breach`. Editor note: `target` the
  breach, `killtarget` the thing to attack, `delay` between shots (default ramps with skill); it
  does not search for targets; if targeted, it can be turned on once and off once, then it is
  disabled; when activated later it waits 3 s to acquire before firing (read from the note; the
  driving behaviour itself was not studied).
- Deathmatch-ball entities: `dm_dball_ball`, `dm_dball_ball_start`, `dm_dball_goal`,
  `dm_dball_speed_change`, `dm_dball_team1_start`, `dm_dball_team2_start` (question 8).
- `dm_tag_token`: the tag game's token (question 7).

Monsters
- `monster_stalker`: the wall- and ceiling-crawling spider. Spawnflags Ambush 1, Trigger_Spawn
  2, Sight 4, `OnRoof` 8 (starts on the ceiling). Box -28 -28 -18 to 28 28 18.
- `monster_turret`: the wall turret. Spawnflags Ambush 1, Trigger_Spawn 2, Sight 4, then the
  weapon: `Blaster` 8, `MachineGun` 16, `Rocket` 32, `Heatbeam` 64, and `WallUnit` 128. Blaster
  is the default. A wall unit moves 32 units out in the direction it faces when activated.
- `monster_daedalus`: the improved hover (icarus): health 450, mass 225, skin 2, strafes more.
- `monster_carrier`: the big flying carrier boss (box -56 -56 -44 to 56 56 44).
- `monster_widow` and `monster_widow2`: the two stages of the final boss (boxes -40 -40 0 to 40
  40 144 and -70 -70 0 to 70 70 144).
- `monster_medic_commander`: a medic variant (health 600, gib at -130, mass 600; the plain
  medic has 300) that can also call in reinforcements (spawning monsters).
- `monster_kamikaze`: a suicide flyer (the flyer's model and sounds; removed in deathmatch).
- The soldiers (`monster_soldier_light`, `monster_soldier`, `monster_soldier_ss`) gain a 4th
  spawnflag, 8 `Blind`: "monster will just stand there until triggered"; soldiers can also
  blind-fire at the spot where they last saw their enemy.

Items placeable on maps (new item classnames, with the name a player sees)
- `weapon_etf_rifle` "ETF Rifle" (ammo "Flechettes"), `weapon_proxlauncher` "Prox Launcher"
  (ammo "Prox"), `weapon_plasmabeam` "Plasma Beam" (ammo "Cells"), `weapon_disintegrator`
  "Disruptor" (ammo "Rounds"), `weapon_chainfist` "Chainfist" (melee).
- `ammo_flechettes` "Flechettes" (50), `ammo_prox` "Prox" (5), `ammo_tesla` "Tesla" (5; also a
  thrown weapon), `ammo_disruptor` "Rounds" (15), `ammo_nuke` "A-M Bomb" (a usable bomb).
- `item_double` "Double Damage", `item_ir_goggles` "IR Goggles", `item_compass` "compass",
  `item_sphere_vengeance` "vengeance sphere", `item_sphere_hunter` "hunter sphere",
  `item_sphere_defender` "defender sphere", `item_doppleganger` "Doppleganger".
- Keys: `key_nuke_container` "Antimatter Pod" and `key_nuke` "Antimatter Bomb" (both with the
  model `models/weapons/g_nuke/tris.md2`).
- Spelling aliases (read): a map's `weapon_nailgun` becomes the ETF rifle, `ammo_nails` becomes
  flechettes, `weapon_heatbeam` becomes the plasma beam.
- The other mission pack's items are swapped when a map places them (read): `ammo_magslug`
  becomes flechettes, `ammo_trap` the prox launcher, `weapon_boomer` the ETF rifle,
  `weapon_phalanx` the plasma beam, and `item_quadfire` a sphere chosen at random each time it
  spawns (hunter 20 %, vengeance 40 %, defender 40 %).

Not found in this library: any `misc_` entry other than `misc_nuke_core`, any other new
`info_` entry, any new `func_` entry other than the three above, and any new entity keys
(the set of keys a map may use is the base game's, without the base port's `music`
worldspawn key).

## 6. Changes to base entities

Spawn-time filtering (read; see question 10 for co-op): in co-op the "not in co-op" spawnflag
(4096) is honoured, and an entity with all three of "not easy", "not medium" and "not hard"
(256, 512, 1024) spawns in co-op at every skill. The base game ignores both rules.

`func_plat` (read): monsters may use it whenever it is stopped (a monster's use sends it to the
other end); a dead monster or player blocking it takes 100 crush damage first; while rising it
lays a 64-unit "bad area" for monsters, removed at the bottom.

`func_door` (read): no new spawnflags on the plain door. There is no "eat monsters" behaviour
in this library: a blocking monster or player takes `dmg` and the door reverses, exactly as in
the base game (read: not found).

"Smart water" (read): a `func_water` with spawnflag 2. When used and its centre is in water,
slime or lava, instead of a normal move it rises after the lowest living player: each frame it
aims at the lowest player's feet and moves straight up at the distance to them divided by
`accel` (units/s), kept between 5 and `speed`; if the distance is below `accel` it creeps at 5.
`accel` defaults to 20; `speed` is the usual `func_water` key (default 25). `health`, if set,
is the height at which its top stops (it then stays up). It fires its targets and opens its
area portals when it starts rising. A blocker that is neither monster nor player takes 100000
damage (as lava) and is exploded if still there; a monster or player takes 100 lava damage
each time it blocks. Once smart, its touch message is cleared.

`func_door_rotating` (read): new spawnflag 8192 `INACTIVE`: it starts not shootable and
ignores touches until used; the first use makes it a normal door (shootable if it has
`health`, otherwise its touch field appears a frame later). After that first use it can no
longer be used by triggers (derived: its use is cleared).

`func_explosive` (read): new spawnflag 8 `INACTIVE`: when it has a `targetname`, it spawns
solid but cannot be damaged until used; the first use makes it explodable (health 100 if
unset); later uses make it explode. An exploding member of a team is first taken out of its
team's chain.

`misc_explobox` (read): the barrel now reacts to liquids each frame (it is immune to slime and
does not drown; lava and other liquid effects apply).

`func_train` (read):
- A `path_corner` may now carry `speed`, `accel` and `decel`: on leaving that corner the train
  takes the corner's speed, and accel and decel from the corner or equal to that speed. When a
  corner's accel or decel differ from its speed, the leg uses the frame-stepped accelerating
  mover, which reads speed, accel and decel as units per 0.1 s frame (derived).
- Train teams: any entity sharing the train's `team` key moves with the train, by the same
  displacement each leg, at the train's speed and accelerations (the train is made the team's
  master at level start, and its team members become pushers).
- With `TOGGLE`, a corner with a negative `wait` stops the train and clears its current corner,
  so the next use starts it again toward the corner's target (derived).

`func_rotating` (read): new spawnflag 8192 `ACCEL`. Speed (default 100, degrees/s) is reached by
adding `accel` (default 1) every 0.1 s, and when stopped it slows by `decel` (default 1) every
0.1 s; accel and decel are capped at the speed. The rotator now fires its targets when it
starts (on reaching full speed with `ACCEL`) and when it stops (on reaching zero with `ACCEL`).
The editor list of its flags is: 1 START_ON, 2 REVERSE, 4 X_AXIS, 8 Y_AXIS, 16 TOUCH_PAIN, 32
STOP, 64 ANIMATED, 128 ANIMATED_FAST, then the skill and mode flags, then 8192 ACCEL.

`trigger_multiple` (read): new spawnflag 8 `TOGGLE`: it starts inactive; each use switches it
on or off (the use does not fire it). The existing 4 `TRIGGERED` still means "inactive until
the first use". (Flags 1 MONSTER, 2 NOT_PLAYER as in the base game.)

`trigger_push` (read): spawnflags 1 `PUSH_ONCE`, 2 `START_OFF`, 4 `SILENT`. A pusher with a
`targetname` toggles on and off with each use; with `START_OFF` it starts off. `START_OFF`
without a `targetname` prints `trigger_push is START_OFF but not targeted.` and the brush turns
into a solid pusher wall with no push. `SILENT` stops the wind sound. Speed default 1000 as
before.

`trigger_gravity` (read): spawnflags 1 `TOGGLE` (use switches it on and off), 2 `START_OFF`
(starts off, implies toggle). `gravity` is now read as a fraction (1.0 is the level's gravity),
not rounded to a whole number.

`trigger_hurt` (read): same flags as the base game (1 START_OFF, 2 TOGGLE, 4 SILENT, 8
NO_PROTECTION, 16 SLOW). This release's version does not hurt things already inside when it is
switched on; the base port's does (a port difference).

`target_earthquake` (read): new spawnflag 1 `SILENT`: no `world/quake.wav`. Otherwise as
before: `speed` severity default 200, `count` seconds default 5; every frame each player on the
ground is thrown with up to 150 units/s random sideways push and an upward speed of speed times
100 over the player's mass; the sound repeats every 0.5 s.

`target_laser` (read): new spawnflag 128 `WINDOWSTOP`: the beam is traced against shot-blocking
contents, so it stops at windows. The beam also no longer stops at damageable non-monster
objects (it passes through them after hurting them).

`target_crosslevel_trigger` (read): sets its bits and removes itself; in this library it does
not fire its own targets.

Other new behaviour touching maps (read): every spawned entity is visible to the IR goggles,
monsters can jump up to or drop down from ledges toward their enemy; the pack's player-only teleporter is
`trigger_teleport` (question 5).

## 7. The tag game (`gamerules 2`)

Chosen by `gamerules` 2 or the word `tag`, in deathmatch only (question 8) (read).

The token (read): item `dm_tag_token`, shown as "Tag Token", model
`models/items/tagtoken/tris.md2`, icon `i_tagtoken`, pickup sound `items/pkup.wav`; spins and
leaves the tag trail. A `dm_tag_token` placed on a map is removed unless the game is tag
deathmatch. If the map has none, one is made at level start at a point chosen the way a
deathmatch spawn point is chosen. It cannot be given by `give` commands.

Holding it (read): touching it makes the toucher the holder (with one token in inventory) and
gives the "kill-it bonus": if hurt, health up by 200, capped at the player's maximum; and a Body
Armor as if picked up. The holder is drawn with the tag trail effect every frame. On the
scoreboard the holder's row shows the picture `tag3` instead of the usual `tag1` (yourself) or
`tag2` (your killer). The token sits in the holder's inventory with its icon; there is no other
tag status-bar field and no message text (no centre-print, no broadcast) about the token (read).

Damage (read): a hit where neither the victim nor the attacker is the holder does 3/4 of its
damage (rounded down). A hit that rounds to 0 does nothing at all, not even knockback
(derived). Hits involving the holder are full.

Scoring (read; the frag tally is the usual score):
- Holder kills another player (a positive kill): 3 points instead of 1. Every 5th such kill
  (the counter resets to 0 after the 5th, and whenever the token changes hands or drops) gives
  the holder a Quad Damage, used at once.
- Someone other than the holder kills the holder: 5 points instead of 1. If the attacker is
  alive and the kill was not by a hunter sphere or a doppleganger (explosion, vengeance or
  hunter), the token passes straight to the attacker, who gets the kill-it bonus. Otherwise
  the token is dropped.
- Everything else scores as usual (a friendly-fire kill -1; a suicide or death by the world
  -1 to the dead player). A friendly-fire kill of the holder by a non-holder still counts as
  "killed the holder": 5 points and the token (derived).

Dropping (read): when the holder dies (and the token did not pass directly) or disconnects, the
token is thrown: spawned 24 forward and 16 below the holder's origin (kept out of walls), box
-15 to 15 on each axis, tossed forward at 100 units/s and up at 300 units/s, glowing, spinning,
with the tag trail. It can be picked up after 1 s. If nobody takes it within 30 s (3 s if it is
in lava or slime) it moves to the deathmatch spawn point farthest from players (trying again
every 1 s if there is none) (read). It relocates once (derived).

When the token passes directly to a killer, the killer's inventory count is not raised, so the
new holder holds the token without an inventory icon until he next drops and picks it up
(derived). When a dying holder drops it, his inventory was already cleared, so his count goes
below zero until he respawns (derived).

End of the match (read): the usual fraglimit and timelimit; tag adds no rule of its own.

## 8. The `gamerules` framework

Read:
- `gamerules` is a latched cvar, default 0. It is read once when the game library starts.
  Outside deathmatch it is forced to 0.
- Value 2, or the text `tag` (any case): the tag game (question 7).
- Any other value: forced to 0, plain deathmatch.
- The pack defines a second game number, 3 for the deathmatch ball ("deathball"), and the
  library still contains the whole deathball game, but in this release nothing selects it:
  `gamerules 3` is forced back to 0. The deathball map entities spawn in any deathmatch but do
  nothing without the deathball rules (derived: the ball can still be shot and pushed, since it
  is a damageable body; its goal and speed fields only act on the ball).
- A game mode can change: game start, setup after the map is spawned, client begin, spawn point
  choice, player death, scoring, per-frame player effects, the scoreboard picture, disconnect,
  damage, knockback, and the end-of-match check. Tag uses start, setup, death, scoring, effects,
  picture, disconnect and damage.

What the dormant deathball game does (read, for completeness):
- On start it adds the dmflags "no mines", "no nukes", "no double-damage stacking", "no
  friendly fire" and "skin teams", sets stop speed to 0, and registers `dball_team1_skin`
  (default `male/ctf_r`), `dball_team2_skin` (default `male/ctf_b`) and `goallimit` (default 0).
- Joining players are put on the smaller team by skin (ties go to team 1). Spawn points:
  `dm_dball_team1_start` / `dm_dball_team2_start` by skin, else `info_player_deathmatch`; the
  one farthest from players, 9 up. Teleporter destinations are made non-solid.
- `dm_dball_ball`: model `models/objects/dball/tris.md2`, box -32 to 32, mass 50, health
  50000 that refills on every hit. Damage to the ball is cut to 1, and player-on-player damage
  is halved. Knockback on the ball by weapon: blaster ×3, shotgun ×3/8, super shotgun ×1/3,
  hyperblaster ×4, grenades, prox, tracker and disintegrator ×1/2, rocket splash and machinegun
  ×3/2, railgun and heat beam ×1/3; rocket direct 70 and BFG 90 when the weapon gives none. A
  moving ball that hits a player crushes for one tenth of its speed (damage and knockback).
- `dm_dball_goal` (brush; spawnflags 1 TEAM1, 2 TEAM2: the team that benefits): `wait` is the
  score, default 10. When the ball enters: the team's goal total rises by `wait`; every player
  on that team gets `wait`, and the last player who hit the ball gets `wait` plus 5 (and loses
  that much if he is on the other team). The ball vanishes with the goal splash (temporary
  entity 49) and reappears after 2 s at a random `dm_dball_ball_start`, killing anything there.
- `dm_dball_speed_change` (brush; spawnflag 1 ONEWAY): multiplies the ball's velocity by `speed`
  (default 2) at most every `delay` (default 0.2 s); with ONEWAY only when the ball moves within
  about 37 degrees of the brush's angle.
- With a goal limit, the match ends at it: `Team 1 Wins.` or `Team 2 Wins.`.

Other deathmatch options the pack adds (read): dmflags 131072 no mines (prox and tesla),
262144 no double-damage stacking, 524288 no nukes, 1048576 no spheres; the cvar
`randomrespawn` (default 0) makes a respawning deathmatch item come back as a random item of
its own class (health as health 60 %, large health 30 %, adrenaline 9 %, mega 1 %; armour as
jacket 60 %, combat 30 %, body 10 %; small health, shards and power armour stay as they are),
`strong_mines` (default 0); `huntercam` (default 1).

## 9. The campaign

What the source settles (read):
- The unit start configs (`runit` files) are not in the library or the release's tree; the
  order of the five units is not stated in the source. **Open.**
- The release ships entity fixes for seven maps, which show part of the flow (read from those
  map entity files, the level names a player sees in quotes):
  - `rmine1` "Lower Mines" (next map `rlava1`) ends a unit: it changes to the cinematic `reu1_`
   then the new unit starting at `rware1`.
  - `rware2` "Waterfront Storage" (next `rbase1`) links to `rbase1`, `rbase2` and `rware1`.
  - `rbase1` "Logistics Complex" (next `rbase2`) links to `rbase2` and `rware2`.
  - `rsewer1` "Waste Processing" (next `rsewer2`); it has the message `Deactivate green
   security\nlasers.` (a two-line help or trigger message).
  - `rsewer2` "Waste Disposal" (next `rhangar2`) links to `rhangar2` and `rsewer1`.
  - `rhangar2` "Maintenance Hangars" (next `rammo1`) links to `rhangar1` and `rsewer2`.
  - `rammo1` "Munitions Plant" (next `rammo2`) links to `rammo2` and ends a unit: it changes to
   the cinematic `reu4_` then `rboss` as a new unit.
  - So: the unit after the one ending at `rmine1` starts at `rware1`; the unit ending at
   `rammo1` leads to `rboss`; the other unit ends (`reu2_`, `reu3_`, the final cinematic) are
   not in the source. **Partly open.**
- Map-specific code by name: `rboss` gives every player (single player and co-op, never
  deathmatch) the "Antimatter Bomb" key, selected, on entering; `rmine2` / `rmine2p` use the
  lava co-op starts (question 3); `rhangar2` has three co-op spawn corrections (a rotating door
  named `t265` is kept in co-op, and a `trigger_always` aimed at it and the wall brush model 15
  are removed in co-op).
- The help computer is unchanged from the base game: the same layout (skill, level name, two
  help lines, kills, goals and secrets), the same skill words `easy`, `medium`, `hard`,
  `hard+`. The pack adds no help lines of its own in the library (the lines come from the
  maps' `target_help`).
- Cross-level triggers work as in the base game (eight bits kept across the unit,
  `target_crosslevel_target` waits `delay`, default 1 s, then fires if all its bits are set),
  except that `target_crosslevel_trigger` does not fire its own targets (question 6). Keys
  and power cubes are taken from co-op players at a unit change, as in the base game.
- The end of the game (the final cinematic's name): not in the source. **Open.**

## 10. Co-op differences

Read:
- Spawn filtering: entities flagged "not in co-op" (4096) are removed in co-op, and entities
  flagged not-easy, not-medium and not-hard together appear in co-op at every skill (single
  player removes them at every skill). The cvar `coop_baseq2` (default 0, latched) turns this
  off and filters by the base game's rules (skill only).
- Starts: the lava starts on `rmine2` / `rmine2p` (question 3); otherwise the base game's co-op
  starts.
- `rhangar2` co-op corrections (question 9).
- Bosses scale with players: the carrier gets 500 times the skill level more health in co-op,
  the widow has 2000 plus 1000 per skill level, plus 500 per skill level in co-op, and its number of
  helper slots is raised in co-op by the skill level for each player beyond the first, capped at
  6. The
  widow in co-op looks at all players' powerups (invulnerability, then quad, then double
  damage) to choose how to respond. The carrier and the medic commander pick a random visible
  player as a target in co-op.
- The tesla does not shock players outside deathmatch (in co-op and single player).
- Monsters following a hint path ignore heard sounds in co-op (question 2).
- Everything else co-op does (scores for monster kills, items that stay, keys, elevators that
  wait `coop_elevator_delay`, default 1.0 s, before rising) is the same as the base port's.
