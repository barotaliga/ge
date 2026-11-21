# Quake II single player on the server with a game library: answers

Stock = the original server. Q2PRO differences are given after each stock answer. Every fact is
marked [read] (seen in the source) or [derived] (inference). On-disk names (server.ssv, game.ssv,
`<map>.sav`, `<map>.sv2`, the `save/current` and `save/save0` folders) are what a player sees in
the game folder.

## 1. Parsing the map argument (`map`, `gamemap`)

- The one argument is a level string of the form `[*]<name>[$<spawnpoint>][+<next>]`. [read]
- Order of parsing in stock: first the string is cut at the first `+`; the part after it is
  stored in the setting `nextserver` as the text `gamemap "<rest>"` (the rest is kept whole,
  so a chain `a.cin+b+c` becomes `gamemap "b+c"`). With no `+`, `nextserver` is set empty. [read]
- Special case: in coop, if the level (after the `+` cut) is exactly `victory.pcx` (case
  ignored), `nextserver` becomes `gamemap "*base1"`. [read]
- Then the remaining part is cut at the first `$`; what follows is the spawn point name, else
  the spawn point is empty. [read]
- Then a leading `*` is removed from the name. [read] The `*` itself is acted on only by
  `gamemap` (see 2), not by the parser. [read]
- Then the name's last four characters choose the kind of server: `.cin` a cinematic, `.dm2`
  a demo, `.pcx` a still picture, anything else a game map loaded from `maps/<name>.bsp`. The
  test is case sensitive in stock. [read]
- What reaches the game's SpawnEntities: the map name with `*`, `$...` and `+...` removed (the
  bare name, e.g. `base2`, no `maps/` or `.bsp`); the entity text of the loaded map; and the
  spawn point text after `$` (empty string when none). [read] For `.cin`/`.pcx`/`.dm2` the
  map is a dummy with no entity text, but SpawnEntities is still called with the file name
  (e.g. `ntro.cin`) and an empty entity string. [read]
- `map` in stock: if the argument has no `.` it first checks that `maps/<name>.bsp` exists and
  prints `Can't find maps/<name>.bsp` and stops if not. Note the check uses the whole argument,
  so a `$` or `+` part makes it look for a file that does not exist. [read; the last sentence
  derived] It then marks the server dead (so the current level is not saved), wipes
  `save/current`, and runs exactly what `gamemap` runs. [read]
- Q2PRO: a leading `!` means "if this map cannot be loaded, kill the server" (it puts `!` on
  the `nextserver` text it builds: `gamemap "!<rest>"`, and `gamemap "!*base1"` for victory).
  [read] Each `+`-separated part is tried in turn: a part that fails to load (or a cinematic or
  demo skipped because the setting `sv_cinematics` is 0, or a demo during multiplayer) falls
  through to the next part. [read] The extension test ignores case. [read] The map is fully
  loaded and checked BEFORE anything happens to the running level. [read] `$` is split after
  `+` per part, same result as stock. [read] An oversize argument is refused with
  `Refusing to process oversize level string.` [read] A `.dm2` under `map`/`gamemap` is turned
  into the client's local demo playback instead. [read]
- Q2PRO `map` on a running game: unless the game is not running, latched settings are pending,
  the second argument is `force`, or the setting `sv_allow_map` is 1 (default 1 on a listen
  server, 0 on a dedicated one), it refuses with `Using 'map' will cause full server restart.
  Use 'gamemap' for changing maps.` (plus a one-time hint); `sv_allow_map` 2 or more makes
  `map` behave as `gamemap`. [read]

## 2. The `gamemap` sequence (single player / coop)

Stock, in order: [read unless marked]
1. Usage check: anything but exactly one argument prints `USAGE: gamemap <map>`.
2. The folder `save/current/` under the game folder is created if missing.
3. If the argument starts with `*` (new unit): every save file in `save/current` is deleted
   (server.ssv, game.ssv, all `.sav` and all `.sv2`). The leaving level is NOT written.
4. Otherwise, if the server is running a game map (not a cinematic/picture/demo, not dead):
   - every client slot's player entity has its in-use flag remembered and then cleared;
   - the level is written (see the level file below);
   - the in-use flags are put back.
   Why (the source's own reason): a level written with the players marked unused, when
   re-entered later, makes players spawn at spawn points instead of taking over a saved body.
   The flags must come back because the game's SpawnEntities on the next map collects each
   in-use player's health, max health and flags (god, notarget, power armour; and coop score)
   into the persistent client data, and skips players that are not in use. [read; the second
   reason derived from the game's spawn code]
   Health: before the game issues `gamemap` at the end of an intermission, the stock game caps
   each in-use player's health at their max health; that is the game's doing, not the
   server's. [read]
5. The new map is started (section 3), with "not a load".
6. The text of the argument (with `*`, `$` and `+` intact) is stored as the server's map
   command for the next server file.
7. Autosave, only when the server is not dedicated (stock checks nothing else: not
   deathmatch, not the kind of map): the server file is written with the autosave flag set
   (and the game's WriteGame is called with autosave true), then the whole of `save/current`
   is copied to `save/save0` (see 6). This happens AFTER the new level is spawned, settled
   and (if kept) read and run its 100 frames, and before any client re-begins. [read; the last
   clause derived from client flow in 4]

The level file pair (written when a level is left and on `save`): [read]
- `save/current/<map>.sv2`, written by the server: the whole configstring table as a raw
  block (2080 strings of 64 bytes each in stock), followed by the area portal open/closed
  state as a raw block (1024 entries, one per possible area portal, each a 4-byte true/false).
- `save/current/<map>.sav`, written by the game's WriteLevel, given that full path. (The stock
  game writes the entity size, a function address check, the level locals and every in-use
  entity by number, ending with -1.)
- `<map>` is the bare map name (the server's current level name).

The server file `save/current/server.ssv` (stock, raw fixed-size fields): [read]
- a 32-byte comment (see 5 for its text; `ENTERING <level title>` for an autosave, where the
  title is the new level's name/message config string at that moment);
- the map command, 128 bytes;
- then for every setting flagged as latched, its name in a 128-byte field and its value in a
  128-byte field, to the end of the file. In stock that is e.g. `deathmatch`, `coop`, `skill`,
  `maxclients`, `cheats`, `game`, `maxentities`, `sv_airaccelerate`, `gamename`, `gamedate`;
  a name or value too long is skipped with `Cvar too long: <name> = <value>`.
- then the game's WriteGame is called with `save/current/game.ssv` and the autosave flag.

Q2PRO differences: [read]
- The working folder is `save/.current`, not `save/current`, and saves live only in the home
  (writable) game folder.
- The leaving level is written BEFORE the new map's game is restarted and before the new map
  spawns, but after the new map has been loaded and checked. A `*` wipes `save/.current`
  instead. A `map` that restarts the game counts as a new unit (wipe).
- Nothing is written in deathmatch, or when the game library does not declare the safe
  ("enhanced") save format (unless the command-line setting `sys_allow_unsafe_savegames` is
  set). Q2PRO also requires the running map to be a game map for the autosave.
- Autosave runs whether dedicated or not (no dedicated check), after the spawn.
- Files are tagged: the server file has a magic, a version (1), the save time as a 64-bit
  number, a save-type byte (manual, level-start autosave, or the remaster's "autosave"), the
  level title, the map command and then name/value string pairs ending with an empty name;
  latched settings flagged private are left out. The `.sv2` holds a magic, version, only the
  non-empty config strings by index, an end marker, then the portal state as a length byte
  and one bit per portal of the map.
- The game's WriteGame autosave flag is true only for the level-start autosave.

## 3. Starting a map, and a kept level

Stock order inside the spawn (both new maps and kept ones): [read]
1. Clients above "connected" are put back to "connected"; spawn count bumped; all per-level
   server state cleared.
2. Air acceleration config string: `sv_airaccelerate` in deathmatch, else 0.
3. The map is loaded; inline brush models named `*1`, `*2` ... become model config strings.
   Loading the same map again (no `flushmap`) keeps the loaded data but resets every area
   portal to closed.
4. The world's links are cleared; the state is set to loading.
5. The game's SpawnEntities is ALWAYS called (map name, entity text, spawn point), even when
   the level will be read from a save: the game's own note says it must so the server's
   baselines come out the same.
6. Two game frames are run (no clients in the game yet).
7. The state becomes the map's kind (game, cinematic, ...); entity baselines are made from
   the freshly spawned entities.
8. Check for a kept level: skipped if `sv_noreload` is non-zero, skipped in deathmatch, and
   skipped if `save/current/<map>.sav` does not exist. Otherwise: world links cleared again;
   the `.sv2` is read (the whole config string table replaced, portal state replaced and the
   area connections recomputed); the game's ReadLevel is given the `.sav` path.
   - Coming back to a level (not a load): 100 more game frames are run (ten seconds at 10 Hz),
     with the server state temporarily set to loading so config string changes are not
     broadcast; no clients are in the game during these frames.
   - On a load (section 5): no extra frames in stock.
9. The setting `mapname` is set (server info, write protected).
- Deathmatch: never reads a kept level, but stock still WRITES level files and the autosave
  on `gamemap` on a listen server (the game issues `gamemap` in every mode). [read]
- Q2PRO: the kept-level check runs right after the two settling frames, BEFORE the state
  leaves loading and before baselines are taken (so baselines come from the read state).
  [read; baselines clause derived] Frames after a read: 2 on a load, 10 seconds' worth on a
  return (100 at the default 10 Hz). Frame numbers advance with each. A missing level file on
  a normal load prints `Couldn't read level file.`; on a level-start autosave it is silent.
  `sv_noreload` and deathmatch skip it, as in stock, and so does a game without the safe
  save format. Q2PRO portal state for portals beyond the saved bits defaults to open. [read]

## 4. Clients coming back

- Game API version 3 calls (stock and Q2PRO's version-3 game): ClientConnect(entity,
  userinfo) returns accept or refuse; ClientBegin(entity); SpawnEntities(map, entities,
  spawnpoint); WriteGame(file, autosave); ReadGame(file); WriteLevel(file); ReadLevel(file);
  RunFrame(). There is no load-game argument: the game tells by the player entity's in-use
  flag and by its own autosaved flag. [read]
- Level change: the server sends `changing` before the spawn and `reconnect` after it as
  console commands; the client, still connected, asks for `new` on the same connection; the
  server sends the server data and config strings; the client then sends `begin` and the
  server calls ClientBegin only. ClientConnect is NOT called again. [read; client side read
  from the stock client]
- Load: the restart of the game (section 5) shuts the server down with a "reconnect" message,
  so the client does a full new connection (challenge, connect); the server calls
  ClientConnect, then later ClientBegin. [read]
- What the stock game does with that: ClientConnect leaves the client data alone when the
  player entity is in use (a body waiting from a load); when not in use it resets the
  per-life data, and resets the persistent data unless the game was autosaved and the player
  has a weapon. ClientBegin, when the entity is in use, keeps the body and turns the saved
  view angles into a delta so the view matches; when not in use it spawns the player at a
  spawn point (chosen by the stored spawn point name) and restores health, max health and
  flags from the persistent data. ReadLevel marks every client as not connected. [read]
- A level kept by `gamemap` was written with players unused, so returning players always
  spawn at a spawn point, carrying their persistent data. A level written by `save` has the
  players in use, so a load puts each player back in their saved body. [derived]
- After the first `begin`, commands deferred during the map load are run. [read]

## 5. `save <name>` and `load <name>`

Stock `save` refusals, in order, with their text: [read]
- not running a game map: `You must be in a game to save.`
- not exactly one argument: `USAGE: savegame <directory>`
- deathmatch non-zero: `Can't savegame in a deathmatch`
- the name is `current`: `Can't save to 'current'`
- one player only and that player's health stat 0 or below: a blank line then
  `Can't savegame while dead!`
- a name containing `..`, `/` or `\`: prints `Bad savedir.` but does NOT stop (a stock bug,
  also kept in yquake2).
- No server check for intermission or coop player count (coop saves are allowed). [read]
Stock `save` writes, in order: prints `Saving game...`; the level pair for the current map
(players left in use: bodies kept); the server file with autosave off (game WriteGame with
autosave false, which makes the stock game first collect each player's health into the
persistent data); then copies `save/current` to `save/<name>` (target wiped first); prints
`Done.` [read]
Manual comment text: hour (2 wide, space padded), `:`, two-digit minutes, a space, month and
day each 2 wide separated by `/`, two spaces, then the level title, cut to 31 characters,
e.g. `14:05 11/21  The Outer Base`. [read]

Stock `load`: [read]
- not exactly one argument: `USAGE: loadgame <directory>`; prints `Loading game...`; a bad
  name prints `Bad savedir.` without stopping; missing `save/<name>/server.ssv` prints
  `No such savegame: <full path>`.
- copies `save/<name>` into `save/current` (target wiped first);
- reads `save/current/server.ssv`: skips the comment, keeps the map command, then sets each
  stored setting immediately (forced, bypassing the latch, and dropping any pending latched
  value);
- restarts the game: shuts down any running server (clients told to reconnect), applies
  latched settings, forces the player count (1 in single player, 4 in coop when out of 1..4,
  8 in deathmatch), loads the game library afresh (its Init);
- calls the game's ReadGame with `save/current/game.ssv` (the stock game reallocates its
  entity array here and refuses a save from another build date);
- marks the server dead and starts the map from the stored map command with the "load" flag:
  the command is parsed again (so `nextserver` is rebuilt from any `+`), SpawnEntities and the
  two frames run, then the kept level is read (section 3) with no extra frames.
- Players come back by a full connect (section 4) into their saved bodies.

Q2PRO differences: [read]
- `save` refusals: `You must be in a game to save.`; `Game does not support enhanced
  savegames.`; `Can't savegame in a deathmatch.`; a game extension may decide (otherwise the
  single-player dead check, `Can't savegame while dead!`); `Usage: save <directory>`;
  `Bad savedir.` (and it stops). Write failures: `Couldn't write level file.`,
  `Couldn't write server file.`, `Couldn't wipe '<dir>' directory.`, `Couldn't write '<dir>'
  directory.`; success `Game saved.`. An `autosave` command (the remaster's) saves to `save1`.
- Save menu comment is built when listed: `ENTERING <title>`, `AUTOSAVE <title>`, or the
  date as `Mon DD HH:MM <title>` (this year) or `Mon DD  YYYY <title>`.
- `load` refuses a bad name, needs both server.ssv and game.ssv (`No such savegame: <name>`),
  wipes and refills `save/.current`, validates and loads the map from the stored command
  first, then shuts down, restores the settings through the normal (latching) path, restarts
  the game, refuses a game without the safe format, calls ReadGame, and spawns directly (no
  level-leaving write, no autosave). A level-start autosave loads as "level start" (no warning
  if the level file is absent); others load as normal (2 frames after the read).

## 6. The autosave slot

- The slot is `save/save0`. Stock writes it on every `gamemap` and every `map` (which runs
  the gamemap path) on a non-dedicated server, after the new map is up: server file with the
  autosave flag, then `save/current` copied over it. [read]
- What it contains: the server and game files written at the new level's start, plus the
  `.sav`/`.sv2` of levels kept in this unit. The new level itself has no `.sav` in it unless
  it was kept from an earlier visit. [derived]
- Loading it: the game file says "autosaved", so in the stock game ClientConnect keeps each
  player's persistent data (if they had a weapon) instead of resetting it; with no level file
  for the map, the map plays from its start and players appear at spawn points; with an old
  level file the level is restored with players unused, so again at spawn points. [derived
  from read game and server code]
- The stock game clears its autosaved flag when an intermission begins and right after
  writing the game file. [read]
- Q2PRO: same slot name; wiped then refilled; written only for a game map, not deathmatch,
  safe-format games; also on a dedicated server. [read]

## 7. The game's entities around these calls

- The server never clears or copies the game's entity memory itself; it reaches entities
  through the game's exported entity array and count each time. [read] It only flips the
  players' in-use flags around the level write (section 2). [read]
- Before the game's ReadLevel the server clears all world links; ReadLevel (in the stock
  game) wipes all entities, sets the count to players+1 and grows it to the highest stored
  number +1, and links each read entity into the world again through the server's link call.
  [read]
- Baselines in stock come from the entities SpawnEntities made, taken before the read; they
  are not rebuilt after the read. [read]
- Area portal state: written as a raw table in the `.sv2` after the config strings and read
  back into the collision model, which then recomputes which areas connect. [read] A
  same-map reload resets all portals to closed first. [read]
- After a load, the server's stored per-client entity pointers are set again when each
  client connects or asks for `new`, since ReadGame may have moved the entity array. [derived]

## 8. Where the game library finds the files

- The server passes full paths to WriteGame, ReadGame, WriteLevel and ReadLevel:
  `<game folder>/save/current/game.ssv` and `<game folder>/save/current/<map>.sav`, where the
  game folder is the base folder plus the `game` setting's folder (or `baseq2`). The game
  opens them itself; it needs neither `game` nor `basedir`. [read]
- The game only ever sees the `current` folder. The server does all copying: `save` copies
  current to the named slot, `load` copies the slot to current, the autosave copies current to
  `save0`. Each copy wipes the target first (server.ssv, game.ssv, `*.sav`, `*.sv2`), then
  copies server.ssv, game.ssv and each `.sav` with its matching `.sv2`. [read]
- Q2PRO: `<home game folder>/save/.current/...`; copying lists `.ssv`, `.sav` and `.sv2`
  files (recursively) and copies each; errors are reported and stop the command. [read]
