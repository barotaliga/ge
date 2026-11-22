# Quake II entity keys at map load: facts from the stock game library (Q2PRO upstream)

Source read: the game library under `src/game` (spawn parsing, item list, main init, view),
plus `src/shared` (number parsing helpers) and `src/server` (the game's debug print).
No code is reproduced here. [read] = seen in the source; [derived] = inferred from what was read.

## 1. Keys the game accepts

There are two tables. A key is looked up first in the entity-field table, then in the
temporary spawn-only table; only if it is in neither does the "is not a field" line print. [read]

### Entity fields (stored on the entity itself) [read]

| Key | Type |
| --- | --- |
| classname | string |
| model | string |
| spawnflags | integer |
| speed | float |
| accel | float |
| decel | float |
| target | string |
| targetname | string |
| pathtarget | string |
| deathtarget | string |
| killtarget | string |
| combattarget | string |
| message | string |
| team | string |
| wait | float |
| delay | float |
| random | float |
| move_origin | vector of three |
| move_angles | vector of three |
| style | integer |
| count | integer |
| health | integer |
| sounds | integer |
| light | ACCEPTED BUT IGNORED (no storage, no message) |
| dmg | integer |
| mass | integer |
| volume | float |
| attenuation | float |
| map | string |
| origin | vector of three |
| angles | vector of three (pitch yaw roll) |
| angle | angle-hack: one number becomes the yaw; pitch and roll are set to 0 |

`angle` and `angles` write the same storage, so whichever comes later in the entity wins. [derived]
`light` is the only ignored key in either table. [read]

### Temporary spawn-only fields (valid only while the spawn function runs; cleared before each entity and after the whole load) [read]

| Key | Type | Who reads it [read, by grep of uses] |
| --- | --- | --- |
| lip | integer | movers (func file) |
| distance | integer | movers (rotating door) |
| height | integer | movers (plat), triggers, actor |
| noise | string | movers, targets (speaker) |
| pausetime | float | movers |
| item | string | monsters, triggers (key), turret |
| gravity | string (kept as text; the world copies it into the sv_gravity cvar, trigger_gravity turns it into an integer) | worldspawn, trigger_gravity |
| sky | string | worldspawn |
| skyrotate | float | worldspawn |
| skyaxis | vector of three | worldspawn |
| minyaw | float | turret |
| maxyaw | float | turret |
| minpitch | float | turret |
| maxpitch | float | turret |
| nextmap | string | worldspawn |
| musictrack | string | worldspawn |

String values are copied with one escape rule: a backslash followed by `n` becomes a newline;
a backslash followed by anything else becomes a single backslash and the following character
is dropped (the pair collapses to one backslash). A trailing lone backslash is kept. [read]

A repeated key simply overwrites the earlier value (last one wins). [derived]

## 2. How a key is matched

- Case-INSENSITIVE comparison of the whole key against each table name (the library's
  case-insensitive string compare). `Origin`, `ORIGIN` all match. [read]
- Entity table is searched before the temporary table; no name appears in both. [read]
- No key is world-only at parse time. Every key in both tables is accepted on every entity,
  including the world. The sky/skyrotate/skyaxis/nextmap/musictrack/gravity keys are only
  *used* by worldspawn (gravity also by trigger_gravity), but putting them on another entity
  prints nothing. [read]
- Skipped silently: only keys whose first character is an underscore. `classname` and
  `origin` are not special in the parser: they are ordinary table entries (string, vector).
  [read]
- An empty key (zero-length) does not start with an underscore and matches nothing, so it
  would print the "is not a field" line with an empty name. [derived]
- Parse errors that abort the level (fatal error, not a message): end of data without the
  closing brace; a value token that is a closing brace ("closing brace without data"); a
  token other than an opening brace where an entity should start. [read]
- If an entity block has no pairs at all, the entity record is zeroed. [read]

## 3. Exact wording of the lines

All go through the game's debug print, which on this server is printed unconditionally to
the console (not gated by `developer`). [read]

- Unknown key: the function name of the per-entity parser, a colon and space, the key as
  written in the map, then ` is not a field`, newline. I.e. `ED_ParseEdict: <key> is not a field`. [read]
- Bad vector: the function name of the per-field setter, colon, space, `couldn't parse '`,
  the KEY (not the value) in single quotes, newline. I.e. `ED_ParseField: couldn't parse '<key>'`.
  The value itself is not printed. [read]
- Vector parse rule: a scanf-style read of three floats separated by whitespace; failure is
  "fewer than three numbers converted". So: empty value fails (no conversion); one or two
  numbers fail; a non-number before the third number fails. Trailing text after the third
  number is ignored (e.g. "1 2 3 junk" or "1 2 3 4" succeed). Leading whitespace is fine.
  Numbers with trailing letters glued on ("1x 2 3") fail at the second because the reader
  stops at the letter. [read for the rule; examples derived from scanf semantics]
- On vector failure the stored value is 0 0 0 (all three components cleared), not the
  partially parsed numbers and not the previous value. [read]
- Integer and float keys NEVER print a parse failure. Integers: base-10 string-to-long,
  clamped to the 32-bit signed range (non-Windows; Windows uses plain atoi), garbage gives 0,
  leading number prefix is taken ("12abc" -> 12). Floats: string-to-float with no end check,
  garbage gives 0, prefix taken. Angle-hack uses the float rule too, no message. [read]

## 4. Inhibited entities and ordering

Per entity, in order [read]:
1. The key/value pairs are parsed: every "is not a field" and "couldn't parse" line for that
   entity prints here, in key order, BEFORE anything is decided about the entity.
2. A one-off map hack: on the map named `command`, a trigger_once with model `*27` has its
   not-in-hard flag cleared.
3. Inhibit check (not for the world, entity 0): in deathmatch, the not-in-deathmatch flag
   (bit 11, value 2048) removes it; otherwise skill 0 with not-easy (bit 8, 256), skill 1 with
   not-medium (bit 9, 512), skill 2 or 3 with not-hard (bit 10, 1024) remove it. The coop
   flag test is commented out (coop flag never inhibits). A removed entity is counted and
   prints nothing of its own.
4. Surviving entities have bits 8..12 cleared from spawnflags.
5. Spawn call: no classname -> `ED_CallSpawn: NULL classname` and the entity is freed;
   item classname -> item spawn, which prints `<classname> at <origin as (x y z)> has invalid
   spawnflags set` if any spawnflags remain (except key_power_cube, which may carry flags) and
   then zeroes them; unknown classname -> `<classname> doesn't have a spawn function`.

So yes: key lines ARE printed for entities later inhibited, and they always come before
that entity's "doesn't have a spawn function" / "has invalid spawnflags set" line. Because
the skill/deathmatch bits are cleared before the item spawn, an item carrying only those
bits produces no invalid-spawnflags line. [read/derived]

After all entities: `<n> entities inhibited`, then `<n> teams with <m> entities`. [read]

Skill is clamped to 0..3 at the start of the load (the cvar is force-set if it was outside). [read]

## 5. Spawn table and classname matching

Order of lookup: the item list first, then the spawn-function table. Both comparisons are
CASE-SENSITIVE exact string compares (unlike keys). [read]

Spawn-function table classnames [read]:
- item_health, item_health_small, item_health_large, item_health_mega
- info_player_start, info_player_deathmatch, info_player_coop, info_player_intermission
- func_plat, func_button, func_door, func_door_secret, func_door_rotating, func_rotating,
  func_train, func_water, func_conveyor, func_areaportal, func_clock, func_wall,
  func_object, func_timer, func_explosive, func_killbox
- trigger_always, trigger_once, trigger_multiple, trigger_relay, trigger_push, trigger_hurt,
  trigger_key, trigger_counter, trigger_elevator, trigger_gravity, trigger_monsterjump
- target_temp_entity, target_speaker, target_explosion, target_changelevel, target_secret,
  target_goal, target_splash, target_spawner, target_blaster, target_crosslevel_trigger,
  target_crosslevel_target, target_laser, target_help, target_actor, target_lightramp,
  target_earthquake, target_character, target_string
- worldspawn, viewthing
- light, light_mine1, light_mine2, info_null, func_group (same function as info_null),
  info_notnull, path_corner, point_combat
- misc_explobox, misc_banner, misc_satellite_dish, misc_actor, misc_gib_arm, misc_gib_leg,
  misc_gib_head, misc_insane, misc_deadsoldier, misc_viper, misc_viper_bomb, misc_bigviper,
  misc_strogg_ship, misc_teleporter, misc_teleporter_dest, misc_blackhole, misc_eastertank,
  misc_easterchick, misc_easterchick2
- monster_berserk, monster_gladiator, monster_gunner, monster_infantry,
  monster_soldier_light, monster_soldier, monster_soldier_ss, monster_tank,
  monster_tank_commander (same function as monster_tank), monster_medic, monster_flipper,
  monster_chick, monster_parasite, monster_flyer, monster_brain, monster_floater,
  monster_hover, monster_mutant, monster_supertank, monster_boss2, monster_boss3_stand,
  monster_makron, monster_jorg, monster_commander_body
- turret_breach, turret_base, turret_driver

Item-list classnames, also spawnable (40; the list's empty slot 0 and the generic "Health"
entry have no classname and are skipped) [read]:
item_armor_body, item_armor_combat, item_armor_jacket, item_armor_shard, item_power_screen,
item_power_shield, weapon_blaster, weapon_shotgun, weapon_supershotgun, weapon_machinegun,
weapon_chaingun, ammo_grenades, weapon_grenadelauncher, weapon_rocketlauncher,
weapon_hyperblaster, weapon_railgun, weapon_bfg, ammo_shells, ammo_bullets, ammo_cells,
ammo_rockets, ammo_slugs, item_quad, item_invulnerability, item_silencer, item_breather,
item_enviro, item_ancient_head, item_adrenaline, item_bandolier, item_pack, key_data_cd,
key_power_cube, key_pyramid, key_data_spinner, key_pass, key_blue_key, key_red_key,
key_commander_head, key_airstrike_target.

Note item_health* are in the spawn-function table, not the item list; each of their spawn
functions calls the item spawn with the generic Health item, so the same invalid-spawnflags
check (and line) applies to them too. [read: the calls; derived: the check runs]

Unknown classname: the "doesn't have a spawn function" line prints and the entity is FREED
(removed). Same for a missing classname. [read]

In deathmatch the item spawn may also free armor/powerups/health etc. by dmflags (no message). [read]

## 6. Cvars

All registered by the game library at game init (not the server), except as noted. [read]

| Cvar | Default | Flags | Readers |
| --- | --- | --- | --- |
| gun_x | "0" | none | Read every frame when building the player's view: added to the player state's gun offset along the view's right axis. [read] |
| gun_y | "0" | none | Same, along the forward axis. [read] |
| gun_z | "0" | none | Same, along the up axis, NEGATED (positive gun_z moves the gun down). [read] The source comments call these development tools. The client has its own separate cl_gun_x/y/z (default 0, no flags), applied client-side; unrelated names. [read] |
| gamename | "baseq2" (the game library's version macro, which is the game directory name) | server info + latched | Nothing in the game reads it back; it exists to appear in the server info string. [read; "nothing reads it" derived from grep] |
| gamedate | the compiler's build-date macro: month as three-letter English abbreviation, day padded with a space to two characters, four-digit year, e.g. "Nov  2 2026" or "Nov 22 2026" | server info + latched | Same: server info only. [read for the macro; format is the C standard's date macro] |
| maxentities | "1024" | latched | Read once at game init: the entity array size is the value clamped between (maxclients + 1) and the protocol's limit (1024 on the old protocol; 8192 when the server advertises protocol extensions and the game's extension cvar allows it). Also saved into and checked against save games ("Savegame has bad maxentities"). [read] |

None of these has the no-set flag. (No-set in this init is used for `dedicated`.) [read]

The MVD/GTV server mode (not the game library) sets its own server-info values gamename
"gtv" and gamedate to its build date. [read]

Nearby, for context, the same init registers: sv_rollspeed 200, sv_rollangle 2,
sv_maxvelocity 2000, sv_gravity 800 (no flags); cheats 0 (server info + latched);
maxclients 4 (server info + latched); maxspectators 4 (server info); deathmatch 0, coop 0,
skill 1 (latched); dmflags, fraglimit, timelimit 0 (server info). [read]

## 7. The view weapon's own sway: gun angles and gun offset (per-frame view work)

Computed once per server frame per client, inside the end-of-frame view step, after the view
offset and before the screen blend. The game sets two player-state vectors, gun angles
(pitch, yaw, roll, degrees) and gun offset (world units); both are rebuilt from scratch each
frame. [read]

### The shared bob inputs (computed earlier in the same frame) [read]
- Horizontal speed: the length of the velocity's two horizontal components.
- Bob step per frame: 0 when horizontal speed is below 5 (and the stored bob time is reset to
  0, restarting the cycle); when on the ground: 0.25 above speed 210, 0.125 above 100,
  otherwise 0.0625; in the air: 0 (the cycle holds).
- The client's bob time accumulates the step. A local copy of it is multiplied by 4 when the
  player is ducked (the stored value is not).
- Bob cycle: the whole-number part of that (possibly x4) bob time.
- Bob fraction: the ABSOLUTE value of the sine of (bob time times pi), so it runs 0 to 1 and
  back each whole cycle, never negative.

### Gun angles [read]
1. Bob part:
   - roll = horizontal speed x bob fraction x 0.005
   - yaw  = horizontal speed x bob fraction x 0.01
   - when the bob cycle is ODD, roll and yaw are both negated (the odd cycles are the right
     foot going forward).
   - pitch = horizontal speed x bob fraction x 0.005, never negated.
2. Lag part, for each of the three axes (pitch, yaw, roll) in that order:
   - delta = LAST frame's view angle minus THIS frame's view angle on that axis (old minus
     new, so the gun trails the turn).
   - wrap: if above 180 subtract 360; if below -180 add 360 (one step each, not a loop).
   - clamp to the range -45 to +45.
   - cross-feed: on the yaw axis only, 0.1 x delta is added to ROLL as well.
   - every axis adds 0.2 x delta to its own gun angle.
   Because roll is handled after yaw in the loop, the roll axis gets: bob roll + 0.1 x yaw
   delta + 0.2 x roll delta. [read/derived]
   The "last frame's view angles" are the player-state view angles saved at the very end of
   the previous view step. [read]
3. No clamp or limit is applied to the finished gun angles. [read]

### Gun offset [read]
- Cleared to zero, then (a commented-out line once added the vertical bob; it is not active,
  so the bob does NOT move the gun offset).
- The three view axes used are forward, right and up from the player's view angles
  (the client's aim angles, recomputed earlier in this frame).
- gun_y moves it along FORWARD (positive = forward).
- gun_x moves it along RIGHT (positive = right).
- gun_z moves it along UP but NEGATED (positive gun_z = down).
- Nothing clamps the offset afterwards. (The client's own cl_gun_x/y/z are separate.) [read]

### Dead and intermission [read unless marked]
- Intermission: the view step returns early (it sets blend alpha 0, fov 90 and the stats)
  BEFORE the speed, the bob and the gun step, so gun angles and gun offset are not
  recomputed: they keep whatever they held on the last frame before intermission. The saved
  "last view angles" are not updated either. Moving a client to intermission sets the gun
  model index to 0, so no gun is drawn anyway, and puts the view at the intermission angle.
- Dead: there is no death check in the gun step; it runs every frame as normal. [read] While
  dead the movement think forces the player-state view angles to roll 40, pitch -15, yaw =
  the killer's yaw, and stops updating the aim angles. So the lag term sees one large delta
  (wrapped and clamped to +-45) on the frame the view snaps, then zero; the bob part follows
  the corpse's horizontal speed; the offset axes stay at the last live aim. [derived from
  those reads] The death handler does not itself clear the gun model index (it removes the
  third-person weapon model and empties the inventory); the gun model goes to 0 when the
  weapon change finds no weapon. [read; whether that happens before respawn not traced]
