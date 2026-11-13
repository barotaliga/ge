# The sequel's single-player campaign, specified

How the sequel's single-player campaign holds together, in our words (README.md, the rules), for
M18v7d2: the help computer and its counters, what a player carries from level to level and from
unit to unit, the cross-level bits, revisited levels, saves and the autosave, the remaining
single-player map entities, co-op's differences and the end of the game. **Where these facts came
from**: a clean-room reader session that read the sequel's GPL game library (and, for level
files, saves and cinematics, the server and client of the same release) and answered written
questions (`clean-room` skill); the builder reads this note, never the source. A fact marked
**(verify)** was worked out from the rules read, not measured: a lab checks it before code depends
on it. **(open)** marks what the source does not settle. Facts marked read are stated by the
source; derived ones follow from it.

Units: map units, seconds, the 0.1 s server frame. Class names, keys, spawnflag names, commands,
cvars, sound and picture paths and save file names are what a map author or player sees. Already
in other notes: the deathmatch rules, items, damage and § Levels (`sequel-rules.md`), the movers
and triggers of M18v7c (`sequel-movers.md`), the demo's monsters (`sequel-monsters.md`).

**Settles** the open point in `sequel-movers.md` § target_changelevel on the map string's other
forms (a cinematic or picture as the next map, a `+` chain): § The end of the game below.

## The help computer

- **target_help** (read): a point entity, removed in deathmatch. Key `message` (required: without
  it a console note names the class and place and the entity is removed). Spawnflag 1 `help1`.
  Each use replaces one of the two game-wide help messages with its `message` (at most 511
  characters kept): with `help1` set, the first (upper) message, otherwise the second (lower)
  one. It then bumps a game-wide "help changed" counter. The entity stays and can be used again.
- **Noticing the change** (read): at the end of every server frame each player compares the
  game-wide counter with the value they last saw; on a difference the player is marked "help
  unread" (count 1).
- **The beep** (read): while a player's unread count is 1, 2 or 3, on every frame whose number is
  a multiple of 64 (every 6.4 s) the sound `misc/pc_up.wav` plays on that player (voice channel,
  full volume, static attenuation) and the count goes up by one. So at most three beeps, 6.4 s
  apart, the first on the next frame whose number is a multiple of 64 (0 to 6.3 s after the
  change) (derived).
- **The blinking icon** (read): while the player has unread help (any count, beeps done or not),
  the status bar's help-icon slot shows the picture `i_help` on frames whose number has bit 8 set
  and nothing of it otherwise: 0.8 s on, 0.8 s off (a 1.6 s period) (derived from the frame
  test). When the help is not unread, the slot shows the current weapon's icon when the player's
  `hand` is 2 (centre) or the field of view is over 91, else nothing.
- **What the screen shows** (read), drawn on the layout layer over the background picture `help`
  (positions in the layout language's virtual coordinates, the format's own):
  - the skill at (202, 12): `easy` for skill 0, `medium` for 1, `hard` for 2, `hard+` for any
    other value;
  - the level's name centred at y 24: the worldspawn's `message`, or the map's file name when
    the worldspawn has none;
  - the first help message centred at y 54, the second at y 110 (both in the highlighted text);
  - column heads "kills", "goals", "secrets" at (50, 164), and under them at (50, 172) three
    found/total pairs: kills (each number right-aligned in three places), goals and secrets.
- **How each counter counts** (read):
  - **kills, total**: one per monster that starts on the level (each monster's spawn, not in
    deathmatch, unless it is a "good guy": misc_insane, misc_deadsoldier and the actor are not
    counted), plus one per turret_driver. A monster made later (target_spawner, or a monster a
    medic raises again) is counted again when it spawns (derived).
  - **kills, found**: one each time a counted kind of monster that is not already dead is
    killed, whoever kills it. In co-op a player who kills one also scores 1.
  - **goals**: the total is the number of target_goal entities spawned; found goes up by one per
    goal used. When found reaches the total, the CD track is set to 0 (music stops).
  - **secrets**: the total is the number of target_secret entities spawned; found goes up by one
    per secret used.
  - The screen is a snapshot taken when it is opened; it is not refreshed while open (only the
    deathmatch scoreboard is resent, every 32 frames). Reopening shows new numbers (derived).
- **The `help` command** (read): in deathmatch it acts exactly as `score`. Otherwise it closes
  the inventory and the scoreboard; if the help screen is open and no new help arrived since the
  player last noticed one, it closes the help screen; otherwise it opens it, clears the unread
  mark (stopping the blink and any remaining beeps) and sends the screen. Because the "noticed"
  value is brought up to date at the end of every frame, in practice `help` toggles the screen
  (derived). `players`, `say`, `say_team`, `score` and `help` are accepted during an intermission; the other
  game commands are not.
- **F1** (read): the release's sample `config.cfg` binds F1 to `cmd help`; the game library
  itself binds no key.
- **When the screen closes** (read): on `help` again (above), `score` (in single player `score`
  only closes the help screen and inventory and does nothing else), `inven` (closes it, then
  toggles the inventory), `putaway` (closes help, scores and inventory). The client sends
  `putaway` when ESC is pressed while any layout is shown. It also closes whenever the player is
  put into the world: a respawn and every level change (the player's per-level state is wiped)
  (derived). A save holds the open state and a load restores it (derived).
- **The single-player end of a unit** (read): the game library sends no layout of its own at a
  single-player intermission; the player sees the intermission view, with the help screen only if
  they open it (deathmatch and co-op show the scoreboard instead).

## Units and cross-level state

- **The player's carried state** (read). A player's persistent record lives in the game-wide
  state and survives every level change of a game: user info, name, `hand`, health, maximum
  health, the god-mode, no-target and power-armour-on flags, the selected item, the whole
  inventory (weapons, ammo, armour items, power screen or shield, keys, held powerups not yet
  used), the six ammo maxima (bullets 200, shells 100, rockets 50, grenades 50, cells 200, slugs
  50 at a new game; raised by bandolier and pack), the current and the previous weapon, the co-op
  power-cube bits, the co-op score and the help-unread state. Armour is carried because it is an
  inventory count. Health above the maximum is cut to the maximum as the level is left.
- **Not carried** (read): everything kept per life or per level: running powerup timers (quad,
  invulnerability, rebreather, environment suit), a held grenade, view kicks, the open help,
  inventory or score screens. The player is put in the world fresh at the new level's start
  with the carried record; if the carried health is 0 or less the record is reset to a new
  game's (100 health, maximum 100, the blaster only, the maxima above).
- **A new game** (read): the console `map` command restarts the server and reloads the game
  library, so the help messages, cross-level bits and player records start empty (derived from
  the restart). `gamemap` (what an exit uses) continues the game.
- **target_crosslevel_trigger** (read): a point entity, not drawn. Spawnflags `trigger1` to
  `trigger8` (1, 2, 4, 8, 16, 32, 64, 128). Used, it sets its flags in the game-wide cross-level
  bits and removes itself. Its own `message`, `target`, `killtarget` and `delay` are not acted on,
  although the editor's note says they work.
- **target_crosslevel_target** (read): a point entity, not drawn. Spawnflags as above; key
  `delay` (default 1, also when 0). It checks once, at level time = `delay` after the level was
  spawned: if every one of its flags is set in the cross-level bits, it uses its targets with
  itself as the activator and removes itself; otherwise it does nothing more on this visit (a bit
  set later on the same level does not wake it). Its targets go through the usual delay rule with
  its own `delay`, so they fire `delay` after the check: with the default, the check is at level
  time 1.0 s (the 10th frame, counting the two frames run before any player can join) and the
  targets fire at 2.0 s (derived). Its `message` reaches nobody (the activator is not a player),
  though the talk sound plays at the entity (derived). The skill and deathmatch spawnflags (256
  and up) are stripped before spawn, so they do not upset the match (read).
- **When the bits are cleared** (read): only by a target_changelevel whose `map` contains `*`,
  when it is used (in any mode). Nothing else clears them within a game; a save holds them.
- **The help messages** (read): game-wide, kept across every level and unit for the whole game
  (never cleared at a unit change); a save holds them. The kills, goals and secrets counters are
  the level's own: a fresh level starts at zero; a revisited level brings its counters back with
  its saved state.
- **Single player level change** (read, with sequel-rules.md § Levels): a changelevel without
  `*` goes on the next frame with no intermission; with `*` an intermission is shown and any
  button after 5 s leaves. Co-op is the same; only deathmatch always shows the intermission.
- **The keys at a unit change** (read): stripped only in co-op. In single player keys are
  carried into the next unit like any item.
- **The next map's spawn point** (read): the server splits the next map string: `*` at the start
  marks a new unit, the text after `$` names the spawn point, the text after `+` is a follow-on
  map (§ The end of the game). The game keeps the spawn point name for the whole level (co-op respawns use it).
  Single player (and co-op's first player): the first `info_player_start` whose `targetname`
  equals the name (letter case ignored); with no name, the first `info_player_start` without a
  `targetname`, or failing that the first `info_player_start` of any name. When a name was given
  and no start has it, or the map has no `info_player_start`, the game stops with an error that
  names the missing spawn point (the server drops to the console). The player appears 10 units
  above the spot's origin (9 for the spot, 1 to clear the ground), turned to the spot's yaw, with
  pitch and roll 0.
- **Re-entering a level within a unit** (read; the save files are the server's of the same
  release): when a level is left with `gamemap` to a map without a leading `*`, the server writes
  that level's whole state to the game's `save/current/` folder: the configstrings and the area
  portals' open state, and the game's level record (time, counters, intermission state and the
  rest) with every entity in use, except the players (they are marked absent first, so a return
  spawns them at a spawn point rather than in their old bodies). A leading `*` instead deletes
  every saved level of the unit. Entering a map that has a saved file (not in deathmatch, and
  unless the cvar `sv_noreload` is set; default 0): the map is spawned fresh and run two frames,
  then the saved state replaces it all (entities, configstrings, portals, level record), so
  killed monsters stay dead, opened doors stay open, items taken stay taken, fired triggers stay
  gone, and level time goes on from where it was. The level then runs 100 frames (10 s) with no
  player present before the player arrives. The player arrives fresh at the spawn point the
  changelevel names, with the carried record.
- **Cross-level targets on a revisit** (read): every target_crosslevel_target restored from the
  saved level (one that has not fired) is re-armed to check at the restored time + its `delay`,
  so a bit set on another level wakes it during the 10 s run before the player arrives when
  `delay` is under 10 s; its targets fire `delay` later, inside that run if 2 × `delay` ≤ 10 s
  (derived).

## Saving and loading

- **Where** (read): each save is a folder `save/<name>/` under the game folder. The playing state
  is kept in `save/current/`; a save copies `current` to the named folder, a load copies the named
  folder to `current`.
- **What a save holds** (read):
  - `server.ssv`: a 32-character comment (the time and date and the level's name for a save
    made by hand, or "ENTERING" and the level's name for an autosave), the map command that
    started the level (the whole `gamemap` string, with its `*`, `$` and `+` parts), and every
    latched cvar with its value (skill, coop, deathmatch, maxclients and the rest).
  - `game.ssv`: a stamp of the game library's build date (a save from another build is refused
    with an error saying it is from an older version), the game-wide state (both help messages
    and their change counter, the spawn point name, the cross-level bits, the player and entity
    limits, whether this is an autosave) and every player slot's full record.
  - one `<map>.sav` and `<map>.sv2` per level of the current unit already visited and left, as
    left, and (for a save by hand) the current level as it is now, players included.
- **The `savegame <name>` command** (read): refused with a console message when no game is
  running, in deathmatch, when the name is `current`, and in a one-player game while the player
  is dead. A name containing `..`, `/` or `\` prints a complaint but the save still goes ahead.
  Allowed in co-op. The client's menu offers 15 slots: `save0` is the autosave and is not offered
  for saving.
- **Autosave** (read): after every level change made with `gamemap` (an exit, and the console
  `map`, which uses it), unless the server is dedicated: the server writes `server.ssv` and
  `game.ssv` as an autosave and copies `current` to `save0`. It happens after the new level has
  been spawned and settled but before the player has joined, so it holds no file for the new
  level unless that level was visited before (then the file is from when it was last left). An
  autosave is also made when the "map" is a cinematic or a picture (derived).
- **The `loadgame <name>` command** (read): refused with "no such savegame" when the folder has
  no `server.ssv`. Otherwise: the folder is copied to `current`; the latched cvars are forced to
  the saved values; the server restarts and the game library starts again; the game-wide state
  and the player records are read back; then the saved map command is run as a load:
- **The order of events after a load** (read):
  1. The map is spawned fresh from its entity text (every spawn function runs) and two frames
     run (level time 0.1 and 0.2 s), with no player present.
  2. If `current` holds a file for this map, it replaces everything from step 1: configstrings,
     area portals, the level record and every entity, the players' bodies included for a save by
     hand. The 100-frame run of a revisit is skipped. Every player slot is marked not connected,
     and every target_crosslevel_target restored is re-armed to check at the restored time + its
     `delay`.
  3. Each player reconnects. A player whose body is in the restored level takes it over with
     all its state (only the view angles are reconciled). A player with no body (an autosave, or
     a level never saved) is put at the spawn point with the carried record when the save is an
     autosave, and with a new game's record otherwise.
- **Do triggers fire again on a load** (derived): entities that fired and removed themselves
  (trigger_once, a used target_secret, a fired cross-level target) are not in the saved level, so
  they do not fire again; delayed uses pending at the save are restored and fire on time;
  unfired cross-level targets check again (step 2 above). trigger_always runs at spawn in step 1 and
  schedules its targets at least 0.2 s later, so with a delay of 0.2 s or less they fire in
  step 1's second frame, before the saved level replaces the entities: what they do to entities
  is thrown away, but what they do to game-wide state (a target_help's message, a cross-level
  bit) stays (derived, verify). The same happens on every revisit within a unit.
- **Death in single player** (read): one second after death any button runs the client's
  `menu_loadgame` command, which opens the load-game menu. Nothing is reloaded on its own.

## The remaining single-player map entities

- **trigger_key** (read): a point entity used by another (a trigger's `target` names it). Keys
  `item` (an item class name such as `key_data_cd`; required) and `target` (required). Without
  either, or with an unknown item, a console note names the problem and the entity is inert. On a
  use by a non-player: nothing. A player lacking the item: a centred message telling them they
  need that item (by its pickup name) and the sound `misc/keytry.wav`, at most once per 5 s
  (uses within 5 s are silent); the entity stays ready. A player holding it: the sound
  `misc/keyuse.wav`, the item is used up (single player: one taken from that player; co-op: that
  key taken from every player; a co-op power cube: the user's lowest-numbered cube, and that cube
  from every player who holds it, one each), its targets fire with the player as activator, and
  the entity never answers again. Any item class works, not only keys (derived).
- **trigger_counter** (read): has no touch (a use only). Key `count` (default 2), spawnflag 1
  `nomessage`. Each use counts one down. While some remain: unless `nomessage`, a centred message
  to the activator saying how many more are needed, with `misc/talk1.wav`. On the last: unless
  `nomessage`, a centred "sequence completed" message with the same sound; then its targets fire
  with that activator and it is removed on the next frame. Uses after that do nothing.
- **trigger_monsterjump** (read): a brush trigger. Keys `speed` (default 200, the horizontal
  speed) and `height` (default 200, the upward speed); `angle` gives the direction (an angle of 0
  counts as 360, so it still points along +x). Only live monsters that neither fly nor swim are
  affected. On every touch their horizontal velocity is set to the direction × `speed`, even in
  the air (so a jump clears a lip); if they stand on the ground they leave it with an upward speed
  of `height`.
- **trigger_gravity** (read): a brush trigger. Key `gravity` is required (without it a console
  note and removal). The value is read as a whole number, so a fraction is cut toward zero (0.5
  gives 0, no gravity). On every touch the toucher's gravity factor is set to it; nothing puts it
  back when the toucher leaves. The player's own move uses only `sv_gravity`, so players are not
  affected; monsters, gibs, grenades, items and other tossed things are (derived from the read
  move).
- **func_water** (read): a door-like mover made of water. Keys `angle` (up or down), `speed`
  (default 25), `wait` (default −1; −1 means toggle: each use moves it to the other end), `lip`
  (default 0), `sounds` (0 none; 1 water and 2 lava both play `world/mov_watr.wav` at the start
  and `world/stp_watr.wav` at the end). Spawnflag 1 `START_OPEN` (starts at the far end and works
  in reverse). The move distance is the brush's extent along the direction minus `lip`. It must
  be targeted. After spawn its class name reads `func_door`, so it is treated as a door
  everywhere a door is (its targets skip area portals, as a door's do). The plain move
  (sequel-movers.md).
- **func_explosive** (read): removed in deathmatch. Spawnflags 1 `Trigger_Spawn`, 2 `ANIMATED`, 4
  `ANIMATED_FAST`. Keys `health` (default 100), `mass` (default 75), `dmg` (default 0),
  `targetname`, `target`. With `Trigger_Spawn` it starts hidden and not solid; a use makes it
  appear (killing anything in its box) and it can then be shot. Without `Trigger_Spawn` and with
  a `targetname`, a use explodes it and it cannot be shot; with neither it is shootable. When it
  goes (killed or used): its centre becomes its place; if `dmg` is set, radius damage of `dmg`
  within `dmg` + 40 units (the killer, or the user, as attacker); then debris: when `mass` ≥ 100,
  min(mass ÷ 100, 8) large pieces (whole division), and min(mass ÷ 25, 16) small ones (the default
  75 makes 3 small, no large), each placed at random within a quarter of the brush's size either
  way of the centre. A piece flies at the brush's own velocity (150 u/s away from what shot it;
  zero when used) plus a random push (each horizontal axis up to ±100, upward 0 to 200) scaled
  by 1 for large pieces and 2 for small ones; it spins up to 600°/s on each axis, bounces, is not
  solid, is removed by any damage, and vanishes after 5 to 10 s. Then its targets fire (the
  attacker as activator), and it leaves an explosion effect if `dmg` was set, else simply
  disappears.
- **func_object** (read): spawnflags 1 `TRIGGER_SPAWN`, 2 `ANIMATED`, 4 `ANIMATED_FAST`; key `dmg`
  (default 100). Its box is 1 unit smaller than the brush on every side. With no spawnflags at
  all it is solid and, 0.2 s after spawn, becomes a tossed body: it falls when nothing holds it
  up. With any spawnflag (even only an animation flag) it starts hidden and not solid until used;
  the use makes it solid and visible, kills anything in its box, and lets it fall. While it
  falls, landing on something that can take damage, on a contact plane that faces straight up,
  deals `dmg` (knockback 1, a crush death).
- **func_conveyor** (read): a solid brush. Key `speed` (default 100); spawnflags 1 `START_ON`, 2
  `TOGGLE`. A use switches the stored speed between 0 and its value; without `TOGGLE` the value
  is forgotten after the first use, so a conveyor turned off cannot be turned on again (derived).
  Nothing reads that speed: the push comes from the current flags on the brush's surface, which
  move a player standing on it at a fixed 100 u/s whatever `speed` or the switch say, and the
  game's own physics for monsters and items read no currents.
- **func_killbox** (read): a brush, not drawn. Each use kills everything inside the brush's
  bounding box that a player could collide with: 100 000 damage that ignores armour, god mode
  and every other protection, with the telefrag death. Reusable.
- **target_laser** (read): spawnflags 1 `START_ON`, 2 `RED`, 4 `GREEN`, 8 `BLUE`, 16 `YELLOW`, 32
  `ORANGE` (the first set in that order wins; none gives the colour bytes all 0), 64 `FAT`. Keys
  `dmg` (default 1), `target` or `angle`. It sets itself up 1 s after spawn: with a `target` it
  aims, every frame, at the centre of that entity's box (a missing target prints a console note);
  without one it points along its angle. The beam is 2048 units long, drawn 4 units wide (16 with
  `FAT`), translucent. Its colour is four palette indexes, of which the client draws one at random each frame (at 0.3
  opacity): red 242, 242,
  240, 240; green 208, 209, 210, 211; blue 243, 243, 241, 241; yellow 220, 221, 222, 223; orange
  224, 225, 226, 227. While on, every frame (10 times a second) it damages each monster and player
  along it by `dmg` (energy damage, knockback 1, the laser death; attacker: whoever last used it,
  or the laser itself), passing through them, and stops at the first other thing, which it also
  damages if it can. Only one monster is immune (monster_boss2). When it is switched on, 8 sparks
  of its colour fly from where it stops; when the beam turns to follow a moving target, 4. Each
  use toggles it (a use records the user as the attacker).
- **target_lightramp** (read): removed in deathmatch. Spawnflag 1 `TOGGLE`. Keys `message`: two
  different lower-case letters a to z, the starting and ending light level (anything else: a
  console note and removal); `speed`: the ramp's length in seconds (0 divides by zero, so it must
  be set) (derived); `target` (required): a `light` entity (others named print a note; if none is
  a light at the first use the ramp is removed). A use restarts the ramp: the light's style
  becomes the start letter at once, then every 0.1 s the letter at start + elapsed frames × (end −
  start) ÷ (speed ÷ 0.1), cut toward zero, until `speed` seconds have passed. With `TOGGLE`, once a
  ramp completes, start and end swap for the next use; without it every use ramps the same way.
  When several lights share the target name, the ramp drives the last one found. The cut toward
  zero may leave the last letter one short of the end when the time sum rounds below (derived,
  verify).
- **target_splash** (read): keys `sounds` (the splash kind: 1 sparks, 2 blue water, 3 brown water,
  4 slime, 5 lava, 6 blood), `count` (default 32, the number of particles), `dmg` (default 0),
  `angle` (the splash direction). Each use sends the splash to those who can see the place; with
  `dmg`, radius damage of `dmg` within `dmg` + 40 (the splash death).
- **target_spawner** (read): key `target` holds the class name to make (an item or any other
  class); `angle` the facing; `speed` (default 0). Each use makes a new entity of that class at its
  place with its angles, runs that class's spawn (an unknown class prints a console note and the
  entity stays empty), kills anything in the new entity's box, and, when `speed` is set, gives it
  a velocity of `speed` along the angle (for gibs). The skill and deathmatch spawnflags are not
  applied to what it makes. Monsters made this way add to the level's kill total (derived).
- **target_blaster** (read): spawnflags 1 `NOTRAIL`, 2 `NOEFFECTS` (both ignored: every bolt has
  the plain blaster glow). Keys `dmg` (default 15), `speed` (default 1000), `angle`. Each use fires
  a bolt from its place along its angle and plays `weapons/laser2.wav` on it. The bolt is an
  energy bolt (knockback 1) that lives 2 s and is removed by the sky; it is marked as a
  hyperblaster bolt, so a death by it reads as a hyperblaster death (derived).
- **target_earthquake** (read): keys `speed` (default 200, the severity), `count` (default 5, the
  length in seconds). An untargeted one prints a console note but still works. A use starts it on
  the next frame and it runs until level time ≥ use + `count`. While it runs, the sound
  `world/quake.wav` plays at its place with no fall-off (heard everywhere) on the first frame and
  again once more than 0.5 s has passed (the test is strict, so in practice every 0.6 s; derived,
  verify), and every frame each player standing on the ground is lifted off it: each
  horizontal speed gets a random amount up to ±150 added, the upward speed is set to `speed` ×
  100 ÷ mass (100 u/s for a 200-mass player at the default). Only players are shaken, though the
  editor's note says monsters too. A use during a quake restarts its length.
- **target_temp_entity** (read): key `style`, the temporary-effect type number of the protocol.
  Each use sends that effect with only the entity's place, to those who can see it. Types whose
  message carries more than a place are not supported by it (derived).
- **target_character** (read): a brush (solid, a pusher, drawn). Key `count`: its position in the
  string, from 1; `team`: the same as its target_string's. It starts showing frame 12 (blank).
- **target_string** (read): a point entity. Keys `message` (default empty), `team`. Each use sets
  every character entity on its team: a digit shows frames 0 to 9, `-` frame 10, `:` frame 11,
  any other character or a position past the end of the message frame 12 (blank). Characters
  with `count` 0 are left alone. func_clock drives one.
- **misc_explobox** (read): removed in deathmatch. A barrel model with the box (−16, −16, 0) to
  (16, 16, 40), solid, falling under gravity (dropped to the floor 0.2 s after spawn). Keys
  `mass` (default 400), `health` (default 10), `dmg` (default 150); the editor's note gives other
  defaults (mass 100, health 80) that the game does not use. **The push**: when an entity standing
  on the ground (not on the barrel) touches it, the barrel steps away from it by 20 × (toucher's
  mass ÷ barrel's mass) × 0.1 units, 1 unit per touch for a 200-mass player at the default.
  **Killed**: it stops taking damage and explodes 0.2 s later: radius damage `dmg` within `dmg` +
  40 (the barrel death; the killer as attacker), then debris thrown from its centre: 2 large
  pieces placed at random up to its whole size either way, scaled 1.5 × dmg ÷ 200; 4 pieces at its
  bottom corners scaled 1.75 × dmg ÷ 200; 8 small pieces placed at random, scaled 2 × dmg ÷ 200
  in whole numbers (1 at the default); the debris otherwise as func_explosive's. The explosion
  effect is the grounded kind when it rests on the ground, else the airborne kind. Barrels in
  reach set each other off.
- **misc_deadsoldier** (read): removed in deathmatch. The dead-body model, box (−16, −16, 0) to
  (16, 16, 16), solid, not counted. Spawnflags pick the pose (the first set wins): 2 `ON_STOMACH`
  frame 1, 4 `BACK_DECAP` frame 2, 8 `FETAL_POS` frame 3, 16 `SIT_DECAP` frame 4, 32 `IMPALED`
  frame 5; otherwise (1 `ON_BACK`) frame 0. It takes damage (health from `health`, default 0, dropping
  further with every hit); when health reaches −80 or below it gibs: `misc/udeath.wav`, 4 small meat gibs,
  and the body becomes a head gib.
- **misc_insane** (read): removed in deathmatch. A prisoner: a "good guy" monster (not counted,
  never picks an enemy, never attacks; a player or another good guy hurting it does not anger
  it). Health 100, gibs at −50, mass 300, box (−16, −16, −24) to (16, 16, 32), skin chosen at
  random among 0 to 2. Spawnflags 1 `Ambush` and 2 `Trigger_Spawn` (as for monsters), 4 `CRAWL`
  (crawls instead of walking and running), 8 `CRUCIFIED` (hangs in place: box (−16, 0, 0) to (16,
  8, 32), no knockback, does not fall; pain makes it struggle), 16 `STAND_GROUND` (does not walk;
  with `CRAWL` it stays down), 32 `ALWAYS_STAND` (never drops to the floor). Otherwise it stands
  (calm or raving, half and half), now and then drops to the floor and gets up, and walks or runs
  in a calm or a raving gait chosen half and half. Pain: the player's pain sounds by health (under
  25, 50, 75 or more), at most every 3 s; no pain animation at skill 3. Death: the player's death
  sounds; at −50 or below it gibs (`misc/udeath.wav`, 2 bones, 4 meat, a head). Its sounds use the
  idle fall-off.
- **misc_viper** and **misc_strogg_ship** (read): fly-by ships. Key `target` (required: the first
  path_corner; without it a console note and removal), `speed` (default 300). Hidden until used,
  then shown and run along their path as a func_train does (plain move, speed for the move's
  speed, acceleration and deceleration); later uses act as a train's. Not solid; box (−16, −16, 0)
  to (16, 16, 32). They differ only in the model.
- **misc_viper_bomb** (read; not asked, it goes with the viper): hidden until used; key `dmg`
  (default 1000). A use shows it falling with a rocket trail, thrown at the first misc_viper's
  current speed along its path direction, its nose turning from level to straight down over its first second of fall while it rolls 10°
  more each frame. On touching anything:
  its targets fire, radius damage `dmg` within `dmg` + 40 (the bomb death), and an explosion.
- **misc_bigviper** (read): a large still ship model, solid, box (−176, −120, −24) to (176, 120,
  72). Nothing else.
- **misc_blackhole** (read): a translucent model, not solid, looping its frames 0 to 18 at 10 Hz;
  a use removes it.
- **misc_eastertank**, **misc_easterchick**, **misc_easterchick2** (read): still, solid poses that
  loop a stretch of a monster's frames at 10 Hz: the tank's frames 254 to 292, box (−32, −32,
  −16) to (32, 32, 32); the chick's 208 to 246, and 248 to 286 for the second; box (−32, −32, 0)
  to (32, 32, 32).
- **turret_breach, turret_base, turret_driver** (read):
  - **turret_breach** (a brush, solid pusher): turns in pitch and yaw. Keys `speed` (default 50,
    degrees per second on each axis, so at most 5° a frame), `dmg` (default 10, the damage per
    frame to what blocks the turret, knockback 10, a crush death; the driver is the attacker),
    `angle` (the starting yaw), `target` (an info_notnull at the muzzle; its offset is taken 0.1 s
    after spawn and the info_notnull removed; without it a console note), `minpitch` (default
    −30), `maxpitch` (default 30), `minyaw` (default 0), `maxyaw` (default 360). A 0 in `minpitch`,
    `maxpitch` or `maxyaw` means the default, so they cannot be set to 0. The aimed pitch is held
    between the limits; an aimed yaw outside [minyaw, maxyaw] goes to the nearer limit.
  - **turret_base** (a brush, solid pusher): must be on the breach's `team`; turns in yaw with it.
  - **turret_driver** (removed in deathmatch): an infantry model, box (−16, −16, −24) to (16, 16,
    32), health 100, gibs at 0, mass 200, eye 24 above its origin, no knockback, counted in the
    kill total. It must not be on the turret's team; its `target` names the breach. Key `item`
    is read (a console note when unknown). It rides the breach, kept at its offset and snapped to
    1/8 unit. It looks for players as a monster does; while it sees one it aims the breach at the
    player's eye; after seeing the player continuously for (3 − skill) seconds (3, 2, 1, 0) it
    fires, then waits (3 − skill) + 1 s before the next shot. Losing sight stops the aim. A shot
    is a rocket from the muzzle along the breach's facing: damage 100 + a random amount under 50,
    speed 550 + 50 × skill, radius damage of the same amount within 150 units, the sound
    `weapons/rocklf1a.wav`, the driver as attacker. Without a driver the turret never fires. When
    the driver dies the gun levels (pitch 0) and the driver dies as infantry does. Because the
    driver is a pusher, its death skips the monster death step: its `item` is not dropped and its
    death targets do not fire (derived, verify).

## Co-op

- **Players** (read): co-op allows 2 to 4 players (the server forces `maxclients` to 4 when it
  is 1 or less or over 4). Spectating is deathmatch only. When deathmatch and co-op are both set,
  co-op is turned off.
- **Spawn points** (read): the first player uses the single-player rule (§ Units and cross-level state). Player n (n = 1 to
  3) uses the n-th `info_player_coop` whose `targetname` equals the level's spawn point name (a
  co-op spot without a name matches an empty spawn point name); when there are not that many,
  the single-player rule applies, so players can share a start and the later one kills whoever
  stands there (derived, verify). `info_player_coop` is removed outside co-op. Some shipped maps
  have their co-op spots renamed at spawn to the name of a named start within 384 units, and one
  shipped map gets three co-op spots made at spawn (fixes for those maps).
- **Entities** (read): the not-in-co-op spawnflag (4096) is not honoured: such entities spawn in
  co-op too. The skill flags apply as in single player.
- **Items** (read): every weapon (blaster to BFG10K), the rebreather, the environment suit and
  every key stay in the world when taken. A player may take a weapon only when not holding it
  (and gets its ammo); a rebreather or suit only when holding none; a key only when not holding
  it. These items cannot be dropped in co-op. Each power cube on a level gets its own bit in spawn
  order; a player can take each cube once (derived: only the first 8 cubes of a level get a bit
  that the check sees; a ninth or later cube can be taken again and again, verify).
- **Keys used** (read): see trigger_key (§ The remaining single-player map entities): a key is taken from every player; a power cube
  from every holder of that cube.
- **Respawning** (read): a dead co-op player respawns after 1 s on any button, at a co-op spawn
  point, leaving a body behind, with a teleport effect, as in deathmatch. Their record goes back
  to what it was when they entered the level, except that the keys they held when they died are
  kept (the help-unread state also carries over). Score: each monster a player kills gives them 1;
  it is carried from level to level; deaths do not change it.
- **Level change** (read): a co-op exit works while players are dead (the single-player "first
  player alive" test is skipped) and nobody waits for anyone: without `*` all players go at once
  on the next frame; with `*` dead players are respawned, everyone is moved to the intermission
  with the scoreboard, all keys are stripped, and the first player to press a button after 5 s
  takes everyone on.
- **Friendly fire** (read): on. Damage between co-op players is cut only when `dmflags` has a
  team flag (model teams or skin teams), the two are on the same team, and no-friendly-fire is
  set. Every death caused by a player is announced to all, marked as friendly fire; scores do not
  change.
- **Monsters** (read): each frame one player (the next living, not no-target one, in turn) is the
  one monsters may notice; a hunting monster that sees another player switches to them; a monster
  hit by a second player while it can still see its enemy remembers the second for later.
- **Commands** (read): `help` shows the help computer (as in single player); `score` shows the
  scoreboard. Saving is allowed.

## The end of the game

- **The game library** (read): names no final map and has no victory screen of its own. A
  target_changelevel hands its `map` string whole to the server (after the `*` and intermission
  rules of § Units and cross-level state); the game reads nothing else in it.
- **The server's map string** (read; the server of the same release): `[*]<map>[$<spawn>][+<next>]`
  in that order of parsing: the `+` part is kept as a follow-on, the `$` part is the spawn point,
  a leading `*` is dropped (after it has cleared the unit's saved levels). A name ending `.cin`
  plays a cinematic (the file under `video/`), `.pcx` shows a still picture (under `pics/`), `.dm2`
  plays a recorded demo; anything else is a map `maps/<name>.bsp`. Example: `*inter.cin+jail`
  clears the saved levels, plays the cinematic, then goes to the map.
- **During a cinematic or picture** (read): no map is loaded and no entity is spawned (the game
  sees an empty entity list); the players' carried records survive into the next map (derived).
  The cinematic runs at 14 frames a second; a button pressed more than 1 s after it starts ends
  it; a cinematic that cannot be found ends at once. At its end the client asks the server for
  the next one: the `+` part is run as `gamemap <next>`; with no `+` part the server shuts down
  (back to the console or menu).
- **A picture as the end** (read): in single player the server ignores the request to go on while
  a picture is shown, so the picture stays until the player leaves through the menu or console.
  In co-op, `victory.pcx` is the one name the server treats specially: its follow-on is forced to
  `gamemap *base1`, so a button press starts the campaign again at its first map as a new unit.
  That `victory.pcx` is the full game's final picture is inferred from that special case; which
  map's changelevel names it is map data the source does not hold (open).
