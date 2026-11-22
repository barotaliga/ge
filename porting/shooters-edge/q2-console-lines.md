# Quake II game library: server console lines (clean-room facts)

Kept from a clean-room reader's answers (2026-11-21) to the questions on the `con.*` rows of the Quake II server ledger. Facts only, no code; the implementer builds from this note.

Read from two releases, both GPL-2.0:

- **Q2PRO** (github.com/q2pro/q2pro, master, head commit 601a8df, 2025-12-11): it carries its own baseq2 game library, so it is the primary answer, as the brief asks.
- **Yamagi Quake II** (github.com/yquake2/yquake2, master, head commit a1d7e2b, 2026-11-16): read too, because the wording in our rows is Yamagi's word for word (for example "'<key>' is not a field. Value is '<value>'", "Game is starting up.", "Spawned entity at", "WARNING: <class> used itself.", "has bad movetype"). Where the two differ, both are given.

Tags: [read] = seen in the source; [derived] = follows from what was read, not run; [measured] = run here with the host C library's formatting (glibc), not the game.

Shorthand below: Q2PRO = the Q2PRO game library; YQ2 = the Yamagi Quake II game library. "The two" = both say the same.

## General facts that hold for every line

- Every line in this list goes to the server console only; no player sees it. In both releases the server prints these lines with no `developer` test of any kind: they print always, on a dedicated or a listen server. [read, in each release's server half]
- In Q2PRO the server marks these lines so that a listen server's own player does not see them in the on-screen notify area (they stay in the console only). [read]
- Every line ends with a single newline (LF). None has a trailing space, except the first half of the lightramp line described in row con.lightramp, which is printed in two pieces that join into one line. [read]
- A string that is missing (no value) where a line expects text: both releases hand it to the host C library, which prints `(null)` on Linux with glibc. [derived; `(null)` measured with glibc]

## 1. How a position (`<pos>`) is printed

- Shape in both: an opening parenthesis, the three coordinates x, y, z separated by single spaces, a closing parenthesis. No padding, no field width, no plus sign. Example: `(128 -64 24)`. [read]
- **YQ2**: each coordinate is converted to a whole number by dropping the fraction (truncation toward zero): 12.7 prints `12`, -12.7 prints `-12`, -0.3 prints `0` (never `-0`). [read; values measured]
- **Q2PRO**: each coordinate is ROUNDED to the nearest whole number with no decimal point, by the C library's default rounding: 12.7 prints `13`, -12.7 prints `-13`; an exact half goes to the even neighbour (0.5 prints `0`, 1.5 prints `2`, 2.5 prints `2`, -1.5 prints `-2`); a small negative value that rounds to zero prints `-0` (-0.3 prints `-0`). [read for the rounding format; the values measured with glibc]
- Large values print in full (10000000 prints `10000000`) in both. [measured]
- Which point is printed depends on the line: most use the entity's origin; the ships, `func_train` and the fixed-TRIGGERED line use other points (see their rows). [read]
- Not every line with numbers uses this form: YQ2's `Spawned entity at` and `listentities` print plain decimals with six places (row con.spawned), and YQ2's "bad combattarget" line truncates like the position form (question 3). [read]

## 2. The rows, one by one

In every row: "at spawn" means while the map's entities are being created on a map load, in the order they appear in the map's entity text; "one frame after spawn" means during the first of the two settling frames the server runs right after spawning, still before any client is in (so these also print during the load, after the inhibited and teams lines); "kept" means the entity stays in the world; "removed" means it is freed. Unless stated, the line prints once per offending entity per map load. [read for the order of spawn, the settle frames read in both servers; the timings derived from the think delays]

### con.no-spawn
- Text, both: `<class> doesn't have a spawn function` then newline. `<class>` is the entity's classname exactly as the map spelled it. [read]
- When: at spawn, for each entity whose classname matches no item and no spawn function (classnames compare case-sensitively here). [read]
- **Q2PRO**: the entity is then removed. **YQ2**: the entity is kept, in use but inert (no model, no think). [read]
- An entity with no classname at all prints a different line: Q2PRO and YQ2 both print `ED_CallSpawn: NULL classname` and remove it. [read]

### con.not-field
- **YQ2**: `'<key>' is not a field. Value is '<value>'` then newline, with the key and the value exactly as the map wrote them, each in single quotes. [read]
- **Q2PRO**: different wording: `ED_ParseEdict: <key> is not a field` then newline (no quotes, no value). [read]
- When: at map load, while each entity's key/value pairs are parsed (before that entity's spawn function runs), once per unknown key per entity. Keys are matched ignoring case. A key that starts with an underscore is skipped silently in both. The entity is kept; the key is ignored. [read]
- This parse happens before the skill/deathmatch filter, so keys on entities that are later inhibited still print. [read]

### con.incomplete-field
- **YQ2 only**: `ED_ParseField: entity <n>: incomplete '<key>' field` then newline. `<n>` is the entity's slot number (0 for the world); `<key>` is the field's own name as the game's table spells it (e.g. `origin`), not the spelling in the map (keys match ignoring case). [read]
- When: at map load, for a vector key (origin, angles and the like) whose value does not hold three numbers (fewer numbers, an empty value or text). The whole vector is then set to (0 0 0). [read]
- **Q2PRO**: a different line for the same case: `ED_ParseField: couldn't parse '<key>'` then newline, `<key>` as the map spelled it; the vector is also set to (0 0 0). [read]

### con.teams
- **YQ2**: `<n> teams with <m> entities.` (with a full stop) then newline. **Q2PRO**: `<n> teams with <m> entities` with NO full stop. [read] Details in question 7.

### con.inhibited
- **YQ2**: `<n> entities inhibited.` (full stop). **Q2PRO**: `<n> entities inhibited` with NO full stop. [read] Details in question 7.
- Order in both: the inhibited line prints first, after every entity has been spawned, and the teams line right after it. [read]

### con.invalid-spawnflags
- Text, both: `<class> at <pos> has invalid spawnflags set` then newline; `<class>` is the item's classname, `<pos>` its origin. [read]
- When: at spawn, for an item (any item classname: weapons, ammo, armor, health, keys, powerups) whose spawnflags are non-zero, except `key_power_cube`, which may carry flags. The skill and deathmatch bits (not easy, not medium, not hard, not deathmatch, not coop) are already cleared before this test, so only other bits trigger it. [read]
- Effect: the item's spawnflags are set to 0; the item is kept. [read]

### con.path-corner
- Text, both: `path_corner with no targetname at <pos>` then newline. [read]
- When: at spawn, a `path_corner` with no `targetname`. The entity is removed. Any mode. [read]
- Note: in both releases `point_combat` uses its own spawn and does not print this. [read]

### con.target-missing
- Text, both: `<class> at <pos> target <name> does not exist` then newline. In practice `<class>` is `point_combat` (the line belongs to it), `<pos>` the point_combat's origin, `<name>` its `target`. [read]
- When: during play, when a monster heading for that point_combat reaches (touches) it and the point_combat's `target` names no entity. It is always preceded, on the line before, by `G_PickTarget: target <name> not found`. The monster then keeps the point_combat as its goal; the point_combat's target is cleared, so it prints once. [read]

### con.viewthing
- Text, both: `viewthing spawned` then newline. At spawn, for every `viewthing`; kept. [read]

### con.func-wall
- Text, both: `func_wall START_ON without TOGGLE` then newline (no position). [read]
- When: at spawn, a `func_wall` that has spawnflag 4 (start on) but not 2 (toggle). The game then adds the toggle flag; kept. [read]

### con.viper
- Texts, both: `misc_viper without a target at <pos>` for `misc_viper`; `<class> without a target at <pos>` for `misc_strogg_ship` (so it reads `misc_strogg_ship without a target at <pos>`). [read]
- When: at spawn, the ship has no `target`; it is removed. [read]
- `<pos>` here is the entity's lower bounding corner in the world, not its origin. At that moment the entity has not been placed in the world yet, so that corner is still all zeros and the line reads `(0 0 0)`. [read for the point used; the zeros derived]

### con.no-target-count
- Texts, both: `<class> with no target at <pos>` and `<class> with no count at <pos>`, `<pos>` the origin. For `func_clock` (the row's subject) they read `func_clock with no target at <pos>` / `func_clock with no count at <pos>`. [read]
- When: at spawn. "no target": `func_clock` without a `target`. "no count": `func_clock` with spawnflag 2 (counting down) and no `count`. The clock is removed in both cases. [read]
- The same "with no target" wording is also printed by `target_lightramp` (row con.lightramp) and, in Q2PRO only, `misc_actor` (question 3). [read]

### con.teleporter
- `teleporter without a target.` (with a full stop) then newline: at spawn, a `misc_teleporter` without a `target`; removed. Both. [read]
- `Couldn't find destination` (no full stop) then newline: during play, every time a player touches the teleporter's pad and no entity has a `targetname` equal to its target (the pad is kept and does nothing, so it prints on each touching frame). Only players trigger the check. Both. [read; "each frame of contact" derived from touch handling]

### con.turret
- `<class> at <pos> needs a target` then newline: one frame after spawn, a `turret_breach` with no `target`. Kept. Both. [read]
- If the breach has a target that names nothing: Q2PRO prints only `G_PickTarget: target <name> not found` and carries on; YQ2 prints the same line and does not guard what follows (the source does not settle what happens next; a crash is likely). [read; the YQ2 outcome derived]
- `<class> at <pos> has bad item: <item>` then newline: at spawn, a `turret_driver` whose `item` key names no item; kept, with no item. Both. [read] The same wording is printed by every monster with a bad `item` key (question 3).

### con.no-distance
- Text, both: `<class> at <pos> with no distance set` then newline. [read]
- Only `func_door_rotating` prints it (not `func_button`). When: at spawn, no `distance` key (or 0). The distance then defaults to 90 degrees; kept. [read]

### con.train
- `train_next: bad target <t>`: during play, when a train reaches a path corner (or starts) and the next corner's name `<t>` matches no entity. The line before it is `G_PickTarget: target <t> not found`. The train stops there. Both. [read]
- `connected teleport path_corners, see <class> at <pos>`: during play, when a train passes a teleporting path corner (spawnflag 1) and the very next corner is also a teleporting one. `<class>` and `<pos>` are that second corner's classname (normally `path_corner`) and origin. The train stops. Both. [read]
- `train_find: no target`: printed by the train's first look-up one frame after spawn when it has no target. That look-up is only started for trains (and ships) that have a target, so maps do not reach it. Both. [read; unreachability derived]
- `train_find: target <t> not found`: one frame after spawn, the train's first corner `<t>` names nothing; preceded by `G_PickTarget: target <t> not found`. The train stays where it is. Both. Also printed for `misc_viper` and `misc_strogg_ship`, which use the same look-up. [read]
- `func_train without a target at <pos>`: at spawn, a `func_train` with no `target`. Kept, never moves. `<pos>` is the train's lower world bounding corner, read after the train has been placed: that is its brush model's minimum plus its origin, minus 1 on each axis (the server widens every placed box by 1). [read; the minus 1 read in both servers]

### con.elevator
- `elevator used with no pathtarget`: during play, every time a `trigger_elevator` is used by an entity that has no `pathtarget`. Both. [read]
- `elevator used with bad pathtarget: <t>`: the same, when the user's `pathtarget` `<t>` names nothing; preceded by `G_PickTarget: target <t> not found`. Both. [read]
- In both, a use while the train is still moving is ignored silently. YQ2 also ignores silently a use when its train is missing or not a `func_train`. [read]
- `trigger_elevator has no target`, `trigger_elevator unable to find target <t>`, `trigger_elevator target <t> is not a train`: one frame after spawn, from the trigger's set-up; the second is preceded by `G_PickTarget: target <t> not found`. After any of them the trigger is kept but never responds. Both. [read]

### con.timer
- Text, both: `func_timer at <pos> has random >= wait` then newline (the characters `>=` literally). [read]
- When: at spawn, `random` is at least `wait` (after `wait` has defaulted to 1 when unset). `random` is then lowered to `wait` minus one frame (0.1 s); kept. [read]

### con.combattarget
- Text, both: `<class> at <pos>, combattarget <t> not found` then newline. `<class>` and `<pos>`: the monster. [read]
- When: during play, at the moment a monster first acquires an enemy and has a `combattarget` that names nothing. Preceded by `G_PickTarget: target <t> not found`. The monster then simply hunts its enemy. [read]

### con.picktarget
- `G_PickTarget called with NULL targetname` and `G_PickTarget: target <t> not found`. Both. Details in question 8.

### con.usetargets
- `Think_Delay with no activator`, both. [read]
- **YQ2**: `WARNING: <class> used itself.` with the entity's classname. **Q2PRO**: `WARNING: Entity used itself.` (the literal word "Entity", capital E, no classname). [read]
- `entity was removed while using killtargets` and `entity was removed while using targets`, both. [read] Details in question 8.

### con.speaker
- Text, both: `target_speaker with no noise set at <pos>` then newline. At spawn, a `target_speaker` with no `noise` key. Kept (silent, inert). [read]

### con.no-message
- Text, both: `<class> with no message at <pos>`, i.e. `target_help with no message at <pos>`. [read]
- When: at spawn, outside deathmatch only (in deathmatch every `target_help` is removed silently first). Removed. [read]

### con.no-map
- Text, both: `target_changelevel with no map at <pos>`. At spawn, no `map` key. Removed. [read]

### con.bad-target
- Text, both: `<class> at <pos>: <t> is a bad target` then newline, i.e. `target_laser at <pos>: <t> is a bad target`. [read]
- When: one second after spawn (when the laser starts up), the laser has a `target` that names nothing. Kept, with no target entity. No G_PickTarget line here (a plain search). [read]

### con.lightramp
- `<class> at <pos> target <t> (<class2> at <pos2>) is not a light`: printed in two pieces that make one line: first `target_lightramp at <pos> ` (with a trailing space), then `target <t> (<class2> at <pos2>) is not a light` and the newline. `<class2>`/`<pos2>` are the matching entity that is not a `light`. One line per such entity. [read]
- `<class> target <t> not found at <pos>`, i.e. `target_lightramp target <t> not found at <pos>`: when no entity of class `light` matched; the ramp is then removed. [read]
- Both of these: during play, at the ramp's first use (only while it has no light yet). [read]
- `target_lightramp has bad ramp (<r>) at <pos>`: at spawn, when `message` is missing, is not exactly two characters, uses a character outside `a`..`z`, or has the two characters equal. `<r>` is the message as written (`(null)` when missing, derived). Removed. This test runs before the deathmatch removal, so it prints in deathmatch too. [read]
- `target_lightramp with no target at <pos>`: at spawn, outside deathmatch, no `target`. Removed. [read]

### con.untargeted
- Text, both: `untargeted <class> at <pos>`. For `target_earthquake` with no `targetname`, at spawn: kept (it simply can never be used). Q2PRO also prints it for `misc_actor` (removed there). [read]

### con.flood-clamp
- **YQ2 only**: `flood_msgs lowered to max: 10` then newline. When: when a player chats while the setting `flood_msgs` is above 10; the setting is then forced to 10, so it prints once per raise. [read]
- **Q2PRO**: no line; it limits silently to its own maximum. [read]

### con.spawned
- **YQ2 only**: `Spawned entity at <x> <y> <z>` then newline, each coordinate a decimal with six places, e.g. `Spawned entity at 128.000000 -64.000000 24.000000`. [read; the six places measured with glibc]
- When: the client command `spawnonstart <classname>` (allowed in single player, or with cheats on in deathmatch/coop): one line per player start of each kind (deathmatch starts first, then coop starts, then single-player starts), after spawning the entity there. [read]
- Q2PRO has no such command. [read]

### con.listentities
- **YQ2 only**: `<class>: <x> <y> <z>` with six-decimal coordinates, one line per matching entity, printed on the server console (deliberately, not to the player). Q2PRO has no such command. [read]

### con.bad-movetype
- **YQ2**: `<class>:<n> has bad movetype: <m>` then newline (no space before or after the colon between class and number), `<n>` the entity number, `<m>` the movetype number. The entity is then set to not move, so it prints once. [read]
- **Q2PRO**: a fatal error instead: `SV_Physics: bad movetype <m>` (the map ends with the server's error handling). [read]
- Neither is reachable from map data: movetype is not a key a map can set. [read]

### con.connected
- Text, both: `<name> connected` then newline, `<name>` the player's name from their user info. [read] Details in question 6.

### con.fixed-triggered
- Text, both: `fixed TRIGGERED flag on <class> at <pos>` then newline, i.e. `fixed TRIGGERED flag on trigger_once at <pos>`. [read]
- When: at spawn, a `trigger_once` with spawnflag 1: the flag is moved to 4 (triggered). Kept. [read]
- `<pos>` is meant as the centre of the trigger's box, but the box is not known yet at that moment, so it prints `(0 0 0)`. [read for the point; the zeros derived]
- It does NOT depend on `g_fix_triggered`. That YQ2 setting (default 0) belongs to a different line, `triggered <class> at <pos> has no targetname` (question 3). [read]

### con.trigger-key
- `no key item for trigger_key at <pos>`: at spawn, no `item` key. [read]
- `item <i> not found for trigger_key at <pos>`: at spawn, `item` names no item. [read]
- `<class> at <pos> has no target`, i.e. `trigger_key at <pos> has no target`: at spawn, no `target`. [read]
- In all three the entity is kept but can never be used. Both releases alike. [read]

### con.say-echo (row marked done; checked)
- On a dedicated server only, each player's chat line is echoed to the server console, as the full line the players get (the name, the message) with a newline; in Q2PRO the console marks it as chat. A listen server does not echo it. [read]

## 3. Lines the game library prints that the rows do not list

All print always (no developer setting) on the server console. [read]

Map load and setup:
- Q2PRO: `==== InitGame ====` (question 4).
- `ED_CallSpawn: NULL classname`: an entity with no classname; removed. Both. [read]
- Q2PRO: `ED_ParseField: couldn't parse '<key>'`; YQ2's counterpart is `incomplete` (row con.incomplete-field). [read]
- Items dropping to the floor, one frame later than other entities (the second settle frame): `droptofloor: <class> startsolid at <pos>` then newline, when an item starts inside solid; the item is removed. Both, same text (YQ2's names the same step). [read]
- `trigger_gravity without gravity set at <pos>`: at spawn, a `trigger_gravity` without `gravity`; removed. Both. [read]

Monsters (outside deathmatch only, where monsters exist):
- `<class> at <pos> has bad item: <item>`: at spawn, a monster's `item` key names no item; kept. Both. [read]
- YQ2 only: `triggered <class> at <pos> has no targetname`: at spawn, a monster with spawnflag 2 (triggered spawn) and no `targetname`. If the setting `g_fix_triggered` is non-zero (default 0) the triggered flag is also removed so the monster appears at once; either way the line prints. [read]
- The next four print one frame after spawn, when the monster starts (for a triggered monster in YQ2, only when it is finally triggered; in Q2PRO at the first frame): [read]
  - `<class> at <pos> has target with mixed types`: its `target` names both point_combat entities and others.
  - Bad combattarget: Q2PRO: `<class> at <pos> has a bad combattarget <t> : <class2> at <pos2>` (note the spaces around the colon). YQ2: the same words, the positions written in parentheses as whole numbers truncated toward zero (same appearance as YQ2's position form). One line per entity named `<t>` that is not a `point_combat`.
  - `<class> can't find target <t> at <pos>`: its `target` names nothing; preceded by `G_PickTarget: target <t> not found`; the monster stands.
  - `<class> in solid at <pos>`: a walking monster (not triggered) that starts on the ground inside solid, or any flying monster that starts inside solid. Swimming monsters do not test. Kept.
- Q2PRO only (YQ2 has no actor): `misc_actor`: at spawn outside deathmatch `untargeted misc_actor at <pos>` (no targetname) or `misc_actor with no target at <pos>`, removed in both cases; during play, when the actor is used and its target is missing or is not a `target_actor`: `misc_actor has bad target <t> at <pos>` (when the target names nothing, preceded by the G_PickTarget line); `target_actor with no targetname at <pos>` at spawn, kept. [read]

Saved games (YQ2 only, when loading a save written by another build): four lines that name an internal step and print a memory address or a name; these are developer aids about save compatibility, left out here. [read]

Broadcast lines that also reach the console (on a DEDICATED server only, since the server echoes every broadcast to its console with the top bit of each character cleared, i.e. coloured text printed plain): `<name> entered the game`, `<name> disconnected`, `<name> exited the level.`, `Timelimit hit.`, `Fraglimit hit.`, and every obituary (`<name> died.` and the rest). These are the game's player messages, not console-only lines. Both. [read]

Fatal errors (the map stops; the server prints them through its error path, worded by the server around them): `ED_LoadFromFile: found <token> when expecting {` (YQ2 names the spawn step instead: `SpawnEntities: found <token> when expecting {`), `ED_ParseEdict: EOF without closing brace` (YQ2 has `ED_ParseEntity: EOF without closing brace` for the key and `ED_ParseEdict: EOF without closing brace` for the value), `ED_ParseEdict: closing brace without data`, `ED_Alloc: no free edicts`, `Couldn't find spawn point <name>`, `PrecacheItem: <class> has bad precache string` (Q2PRO). [read]

## 4. con.startup

- **YQ2**, when the library starts: two lines, `Game is starting up.` then `Game is baseq2 built on <date>.`, each with a full stop and a newline. `<name>` is the fixed word `baseq2`. `<date>` is the compile date in the C compiler's form: a three-letter English month, a space, the day padded to two characters with a space (so a single-digit day gets two spaces before it, e.g. `Nov  6 2026`), a space, the four-digit year. A reproducible build sets the same form from its source date. The same date is also published as the server-info setting `gamedate`. [read; the padding is the compiler's documented date form, derived]
- **Q2PRO**, when the library starts: one line, `==== InitGame ====`. No version or date line. [read]
- Shutdown, both: `==== ShutdownGame ====` then newline. [read]
- When: the library starts when a new game begins (the first map; `map` in YQ2; a save being loaded; in Q2PRO, `map` when the server decides a full restart is due) and is shut down just before that restart and when the server stops; a plain level change (changelevel, `gamemap`) does neither. In YQ2 the server prints `-------- game initialization -------` itself between them. The exact restart conditions are the server's, not the library's. [read]

## 5. The `sv` commands

- `sv test` prints exactly `Svcmd_Test_f()` then newline on the server console (the text is literally that). Both. [read]
- An unknown word: `Unknown server command "<word>"` then newline, the word in double quotes as typed. With no word at all it prints `Unknown server command ""`. Words are matched ignoring case. Both. [read; the empty case derived]
- The other words the game knows (not asked): `addip`, `removeip`, `listip`, `writeip`. [read]

## 6. con.connected

- When a client connects (once its connection is accepted by the game, before it loads the map): `<name> connected` then newline, printed only when the server allows more than one client (any multiplayer game); not in single player. `<name>` is the name in the client's user info at that moment. Console only. Both. [read]
- Begin: no console-only line. A dedicated server shows the broadcast `<name> entered the game`: in deathmatch at every begin; in coop/single-player games with more than one client allowed, at begin when not loading into an intermission. [read]
- Disconnect: no console-only line; a dedicated server shows the broadcast `<name> disconnected`. [read]
- On a listen server the server does not echo broadcasts to its console; the host player still sees them as any player does. [read]

## 7. What `teams` and `inhibited` count

- Inhibited: during the map load, every entity other than the world that is removed before spawning because of its spawnflags: in deathmatch, those with the "not in deathmatch" flag (2048); otherwise those flagged "not on easy" (256) at skill 0, "not on medium" (512) at skill 1, "not on hard" (1024) at skill 2 or 3. The "not in coop" flag (4096) is NOT honoured in either release (coop counts as single player), so it never inhibits. Entities removed later by their own spawn (items barred by dmflags, monsters in deathmatch, target_help in deathmatch and so on) are NOT counted. Skill is first clamped to 0..3 (YQ2 also drops the fraction). [read]
- One map hack runs before the filter in both: on the map `command`, the `trigger_once` with model `*27` loses its "not on hard" flag. [read]
- Teams: counted after all entities have spawned, over every entity still in use except the world, that has a `team` key. `<n>` is the number of distinct team names; `<m>` is the number of entities in those teams. A team of one entity counts as one team with one entity. Entities removed during spawn (inhibited, or removed by their spawn function) are not counted; anything with a `team` key counts (doors, buttons, trains, items, turret parts). [read]
- Both lines print on every map load, including a changelevel and including the spawn that precedes reading a saved level; the counts can be 0 (`0 entities inhibited`). [read]

## 8. `G_PickTarget` and the use-targets warnings

- `G_PickTarget called with NULL targetname`: printed when a lookup by name is asked with no name at all. Map entities normally check for a missing name before asking, so this is rare. Both. [read]
- `G_PickTarget: target <t> not found`: printed whenever a single target is picked by name and no entity has that `targetname` (names compared ignoring case). It is the first of a pair: the caller's own line follows (train, elevator, point_combat, combattarget, monster target, actor; see those rows). When several entities share the name, one of the first eight found is chosen at random (no line). Both. [read]
- `Think_Delay with no activator`: when an entity with a `delay` fires its targets without an activator; printed at the moment the delayed firing is scheduled, not when it fires. Both. [read]
- `entity was removed while using killtargets`: during the removal of the `killtarget` entities, if the firing entity itself was among those removed; firing stops. Both. YQ2 also lowers the secret/goal totals when it removes a `target_secret` or `target_goal`. [read]
- Self-use: when an entity's `target` matches its own `targetname`, instead of using itself it prints YQ2 `WARNING: <class> used itself.` / Q2PRO `WARNING: Entity used itself.`, then carries on with the other matches. Both. [read]
- `entity was removed while using targets`: after one of the targets has been used, if the firing entity has been removed as a result; firing stops. Both. [read]
- Doors firing area portals are skipped silently in this loop. [read]
