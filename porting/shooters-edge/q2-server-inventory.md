# Quake II server inventory: every setting Quake II's stock server and game library register

WORKING AREA: deleted when porting ends. A clean-room description, in our words, of every setting (console variable) Quake II's stock server (its dedicated build and the shared code that build runs) and Quake II's stock game library (the deathmatch, coop and single-player rules) register, the server info keys they set themselves, the operator's commands, and the startup order. Written from the source by a reader session; the building session reads only this. No code, no internal identifiers.

Read from Quake II's stock server and game library (github.com/yquake2/yquake2, default branch, release 8.71pre) at commit `a1d7e2b` (committed 2026-11-16). The competitive Quake II server could not be read (question 7). Nothing here was measured by running the program: every fact is marked **R** (read directly) or **D** (derived by combining what was read). Where the source does not settle a point, the text says so.

What this note does not repeat (read those first): the setting record, the flag numbers and the exact rules of get, set and force-set as the game library sees them, and the game-mode and `maxclients` clamps (`q2-game-library.md` §§ 8 and 18); the out-of-band requests and their answers (`info`, `status`, `ping`, `rcon`, `getchallenge`, `connect`), the connect checks and their refusal texts, the user info keys, `timeout`, `zombietime`, `sv_enforcetime`, `rate`, downloads, spectators and the level-change messages (`q2-net.md` §§ 1 to 4, 8, 11, 16 to 19).

Terms used throughout:
- **Setting** is a console variable an operator types (`name value`) or puts in a config file. **Server info** is the key/value string the `status` query and the heartbeat carry. **User info** marks a setting the local client would send in its own user info; on a dedicated server it changes only which characters the value may hold.
- **Latched**: a change typed while a map runs waits ("will be changed for next game") and takes effect at the next `map` command (a full restart), not at `gamemap` or a level end. With no map running it applies at once. **No-set**: refused from the console (`<name> is write protected.`); it can be given only on the command line (`+set`) or by the program itself. **Archive**: marked to be saved in the player's `config.cfg`; only the client writes that file, so on the dedicated build the flag has no effect (D).
- Flag column: **SI** server info, **UI** user info, **L** latched, **NS** no-set, **A** archive, **—** none.
- Times are in seconds unless stated. Rates are bytes per second.

---

## How settings behave in general (applies to every row)

- **Names are matched with case** for settings: `Hostname` is not `hostname` (a typed `Hostname x` is not found and is silently dropped on the dedicated build). Command names and alias names are matched without case. (R; the silent drop D from the dedicated build having no "unknown command" message)
- **Typing a name alone** prints `"<name>" is "<value>"`. **Typing it with arguments sets only the first argument**; later words are ignored, so a value with spaces needs double quotes. (R)
- **An unknown word typed on the dedicated build is ignored without any message**: it is not a command, not an alias, not a setting, and the dedicated build has no client to forward it to. (R)
- **`set <name> <value>`** sets an existing setting by the ordinary rules, or creates a new one with no flags. **`set <name> <value> <a|u|s>`** creates or overwrites it with exactly that one flag (archive, user info or server info), REPLACING its previous flags; any other letter prints `flags can only be 'a', 'u' or 's'`. Because this form replaces flags, it also bypasses no-set and latching: `set protocol 35 s` changes the protocol key, and `set maxclients 16 s` applies at once and drops the latch (D, from the replace-flags rule). `set name value s` is how an operator adds a custom server info key (an admin name, a website). (R)
- **Values in server info or user info settings** may not contain a backslash, a double quote or a semicolon: `invalid info cvar value` is printed and the old value kept. A key or value of 64 characters or more is refused by the server info builder (`Keys and values must be < 64 characters.`), and the whole server info string is capped at 511 characters (`Info string length exceeded`; the key that overflowed is left out). (R)
- **The value as a number** is the leading number of the text (text that is not a number reads as 0). (R)
- **Registering a setting that already exists** keeps its value and adds the new flags; the new registration's default replaces the remembered default. So when the engine and the game library both register a name with different defaults, `reset` uses the game library's (D; examples in question 3). (R)
- **Deprecated names**: a name on the program's replacement list (old renderer and sound names only; none is a server setting) is quietly rewritten to its new name with `cvar <old> is deprecated, use <new> instead`. (R)
- **`game` set to `baseq2`** is stored as the empty string (the base game). (R)
- **Helper commands**: `reset <name>` prints `<name>: <default>` and sets the default; `resetall` resets every setting except no-set ones and `game`; `toggle <name>` flips 0 and 1 (other values: `can't toggle`), or with a list of values steps to the next one in the list (matched without case; `can't cycle` when the current value is not in the list); `inc <name> [n]` and `dec <name> [n]` add or subtract n (default 1), refusing a value that is not a plain decimal (`can't inc`), and write the result with six decimals; `cvarlist` lists every setting, sorted by name, each line prefixed by `*` (archive), `U` (user info), `S` (server info) and `-` (no-set) or `L` (latched), then a count `<n> cvars`. `vstr <name>` runs a setting's value as commands. (R)
- **Aliases** (`alias <name> <commands>`) are run like commands; an alias that calls itself is stopped after 16 nested expansions (`ALIAS_LOOP_COUNT`). Alias names are at most 31 characters. (R)

## Which builds register what

The dedicated-only build (the program's separate server executable) and the full client build run with `+set dedicated 1` share the server code; the client build also registers its client, renderer and sound settings, which this note leaves out. The dedicated-only build registers **49 engine settings** at startup on Linux (D, counted from its registrations), plus `multicast` when an IPv6 socket opens and the four the program creates as it runs (`mapname`, `gamedir`, `nextserver`, `nextdemo`). The stock game library registers **44** names when it starts, **35** of them new (D); it reads `cl_gun` and `game` later as well. Windows builds register a different network set (§ 1.6). (R)

---

## Question 1: every setting the stock server registers, by subsystem

### 1.1 Network

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `port` | `27910` | NS | UDP port the server listens on, for IPv4 and IPv6 alike. Read when the sockets open (at the first map of a multiplayer game). | R |
| `ip` | `localhost` | NS | Address to bind. Empty or `localhost` (any case) means every interface (`0.0.0.0` and the IPv6 unspecified address); anything else is resolved and bound as given. | R |
| `multicast` | `NULL` | NS | Linux and other Unix builds. Name of the network interface for IPv6 multicast; the literal `NULL` means none. When set, the server joins the IPv6 multicast group `ff12:0:0:0:0:0:0:666`, which clients use to find LAN servers over IPv6. | R |
| `qport` | a number from the clock | NS | The client half's connection id (`q2-net.md` § 2); registered by the shared network code on the dedicated build too, where nothing uses it. | R |
| `showpackets` | `0` | — | Nonzero prints a line for every connection packet sent and received (sequence numbers, sizes). Diagnostic. | R |
| `showdrop` | `0` | — | Nonzero prints a line for every out-of-order, duplicate or dropped connection packet. Diagnostic. | R |
| `public` | `0` | — | Nonzero makes a dedicated server send heartbeats to the master servers (question 4). Off by default: the stock server is private until `public 1` or `setmaster`. | R |
| `rcon_password` | `` | — | Password for remote commands. Empty refuses every rcon (`Bad rcon_password.`). Compared with case; the command after it is rebuilt from its words joined by single spaces, so quotes in an rcon command are lost (D). Every attempt, good or bad, is printed on the server console with the sender's address and the whole line, password included. | R |
| `sv_downloadserver` | `` | — | When set, the `client_connect` answer carries ` dlserver=<value>`, pointing clients at an HTTP server for files (`q2-net.md` § 1). The stock server serves nothing over HTTP itself. | R |

### 1.2 Server: identity, mode and timing

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `hostname` | `noname` | SI, A | The server's name, shown by `info` (right-aligned in 16 characters) and in server info. Applies at once. | R |
| `maxclients` | `1` | SI, L | Client slots. Clamped when a game starts: deathmatch at most 1 becomes 8 and above 256 becomes 256; coop outside 2 to 4 becomes 4; single player becomes 1 (`q2-game-library.md` § 18). The game library re-registers it with default `4` (question 3). | R |
| `deathmatch` | `0` | SI, L | Deathmatch mode. Read when a game starts. Wins over `coop` and `singleplayer`; a dedicated server with none of the three set gets `deathmatch 1` (`Gamemode ambiguity: Chose: <mode>, ignored: <others>` when more than one was set). | R |
| `coop` | `0` | SI, L | Cooperative mode (2 to 4 players). Wins over `singleplayer`. | R |
| `singleplayer` | `0` | SI, L | Single-player mode. On a dedicated server it still opens the network sockets, with one slot. | R |
| `skill` | `1` | — | Difficulty. The game library re-registers it latched (§ 2.4), so once a map has run, a change waits for the next `map`. | R |
| `dmflags` | `16` | SI | The deathmatch rule bits (§ 2.2). Read by the game library whenever a rule is applied, so a change applies at once. The game library re-registers it with default `0`. | R |
| `fraglimit` | `0` | SI | Frags that end a deathmatch level (§ 2.1). | R |
| `timelimit` | `0` | SI | Minutes that end a deathmatch level (§ 2.1). | R |
| `cheats` | `0` | SI, L | 1 allows the cheat commands in deathmatch and coop (§ 2.1). | R |
| `protocol` | `34` | SI, NS | The protocol number, published for browsers. | R |
| `timeout` | `125` | — | Seconds of silence before a client is dropped (`q2-net.md` § 8). | R |
| `zombietime` | `2` | — | Seconds a dropped slot keeps resending before it is free (`q2-net.md` § 8). | R |
| `sv_enforcetime` | `0` | — | 1 drops moves past each client's time budget (`q2-net.md` § 16). | R |
| `sv_airaccelerate` | `0` | L | Air acceleration for the player move, deathmatch only: copied into the map's air-acceleration configstring and the server's move at every map spawn (also `gamemap`), but as a latched setting a change typed during play reaches it only after the next `map`. Outside deathmatch the air acceleration is always 0. 0 is the classic move (almost no air control); 1 or more gives air control. | R |
| `paused` | `0` | — | Pause. With one client slot the world stops. With more slots the world keeps running but players' moves are not run (they stand frozen while monsters, projectiles and timers go on) (D, from the two checks). A server playing a demo stops reading it while paused. Reset to 0 when a demo map starts. There is no `pause` command on the dedicated build; it is set directly. | R |
| `timedemo` | `0` | — | Nonzero makes the server run its 10 Hz frames as fast as it can instead of waiting for real time. Diagnostic. | R |
| `showclamp` | `0` | — | Nonzero prints `sv highclamp` or `sv lowclamp` when the server's clock is pulled back toward the 100 ms frame. Diagnostic. | R |
| `sv_noreload` | `0` | — | Outside deathmatch, 0 restores a level's saved state when a player returns to a level already visited in this unit; 1 always starts the level fresh. | R |
| `sv_entfile` | `1` | A | 1: when a map loads, the server looks for a replacement entity list `maps/<name>@<hhhh>.ent` (hhhh the four lowercase hex digits of the CRC-16 of the map's entity text), then `maps/<name>.ent`, in the search path, and uses it instead of the map's own entities (`.ent file <path> loaded.`). A file of 1 byte or less, or one too large, is skipped. 0: always the map's own entities. | R |
| `sv_optimize_sp_loadtime` | `31` | — | Bit field of signon speed-ups used in single player (below). | R |
| `sv_optimize_mp_loadtime` | `7` | — | The same bits used in coop and deathmatch. | R |
| `mapname` | (set by the server) | SI, NS | The current map's name, written after each map spawn. | R |
| `nextserver` | (set by the server) | — | The command to run after a cinematic or picture ends (from `map a+b`, the next demo, or the coop victory screen). Cleared after use. | R |
| `nextdemo` | (set by the program) | — | The client's demo-loop hand-off: copied into `nextserver` by the next `map` that has no `+`, then emptied. | R |
| `map_noareas` | `0` | — | Diagnostic: nonzero treats every area as connected, so closed doors no longer hide what is behind them (more entities sent, sounds heard through doors). | R |

The signon speed-up bits (R): **1** fills each signon packet's reliable message up to the full message size (about 1400 bytes) instead of stopping at half; **2** sends to clients still in the signon on every pass of the main loop rather than once per 100 ms frame; **4** skips flushing every client's pending messages just before a level change loads the new map; **8** sends the status-bar layout as one long configstring and skips the indices it spans; **16** after the last configstring, uses the rest of the same packet for the entity baselines instead of asking the client to request them. They are read each time they are used, so a change applies at once (D). Default: all five in single player, the first three in coop and deathmatch.

### 1.3 Downloads

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `allow_download` | `1` | A | Master switch for downloads (`q2-net.md` § 11). | R |
| `allow_download_players` | `0` | A | Paths under `players/`. | R |
| `allow_download_models` | `1` | A | Paths under `models/`. | R |
| `allow_download_sounds` | `1` | A | Paths under `sound/`. | R |
| `allow_download_maps` | `1` | A | Paths under `maps/`. A map inside a pack is never sent. | R |

### 1.4 File system

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `game` | `` | SI, L | The mod folder. Empty (or `baseq2`) is the base game. A value with `/`, `\`, `..` or equal to `.` is refused (`Gamedir should be a single filename, not a path.`). A change applies at the next `map`: the search path is rebuilt, the four startup configs are run again from the new folder (question 6), and the game library is looked for there. | R |
| `gamedir` | (set by the program) | SI, NS | The mod folder in use, empty for the base game (so the key is absent). | R |
| `basedir` | `.` | NS | Deprecated way to give the data folder; any other value prints `+set basedir is deprecated, use -datadir instead` and is used as `-datadir`. | R |
| `cddir` | `` | NS | An extra data folder searched last (meant for a CD). | R |
| `fs_debug` | `0` | — | 1 prints every file lookup and where it was found. | R |

The search path itself (the user's folder, the executable's folder, the data folder; the mod's folder before `baseq2` in each) is in `q2-game-library.md` § 1. Command-line options that are not settings: `-datadir <folder>` (must exist; the folder that holds `baseq2/`), `-cfgdir <name>` (the user folder's name under the home directory, default `.yq2` on Linux), `-portable` (write configs, saves and logs beside the executable instead of the home folder). (R)

### 1.5 Common and console

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `dedicated` | `1` (dedicated build), `0` (client build) | NS | Marks a dedicated server: heartbeats, the console `say`, the game library's chat echo on the server console. On the client build it can be given only on the command line. | R |
| `developer` | `0` | — | Nonzero shows developer-level console lines (packet traces, refused moves, signon details). | R |
| `modder` | `0` | — | Nonzero shows the lines meant for mod authors (also shown with `developer`). | R |
| `logfile` | `1` | A | Console log `qconsole.log` in the write folder. 0 off; 1 written (truncated when first opened); 2 as 1 and flushed after every line; 3 appended and flushed. The file opens at the first console line after the setting exists, so lines printed earlier in startup are not in it (D). | R |
| `colorterminal` | `1` | A | 1 colours console output on a terminal whose `TERM` names colour support. | R |
| `timescale` | `1` | — | Scales the time passed to the server each loop. Diagnostic. | R |
| `fixedtime` | `0` | — | Nonzero gives every loop exactly this many microseconds. Diagnostic. | R |
| `cl_maxfps` | `-1` | A | Registered by the shared frame code. On the dedicated build a positive value caps how many times per second the server's frame function is entered; the default leaves it uncapped (the loop sleeps 0.85 ms between passes and the server sleeps until its next 100 ms frame). (D, from the loop) | R |
| `version` | `<release> <architecture> <build date> <OS>` | SI, NS | The build's version string. | R |

### 1.6 Windows builds only

`ip` (`localhost`, NS), `port` and the older `ip_hostport`, `hostport` (all `0` unless set, NS; the first one set nonzero is the server port, else `port`), `ip_clientport`, `clientport` (the client's port, NS), `ipx_hostport`, `ipx_clientport` (IPX ports, NS), `noudp`, `noipx` (`0`, NS; 1 disables that protocol), `net_shownet` (`0`, diagnostic), `multicast` as above. (R)

---

## Question 2: every setting the stock game library registers

The library registers these when it starts (at the first `map`, and again at every `map`, which reloads it). Settings already registered by the engine keep the engine's value and flags and gain the library's flags (R). Rows marked "engine" are the engine's registration with the library's added flags or default.

### 2.1 Deathmatch rules and limits

| Setting | Library default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `deathmatch`, `coop` | `0` | engine, + L | As § 1.2. | R |
| `maxclients` | `4` | engine (SI, L) | As § 1.2; the library sizes its client arrays from it and, if below 1, force-sets it to 1. | R |
| `maxentities` | `1024` | L | Entity slots the library allocates. Raised to `maxclients` + 1 when below that (force-set). No upper bound is enforced (question 8). | R |
| `fraglimit` | `0` | engine (SI) | In deathmatch, checked each frame (outside an intermission): as soon as any player in the game reaches it, everyone sees `Fraglimit hit.` and the level ends. 0 off. | R |
| `timelimit` | `0` | engine (SI) | In deathmatch, minutes of level time; when reached everyone sees `Timelimit hit.` and the level ends. Checked before `fraglimit`. Fractions work (D). 0 off. | R |
| `dmflags` | `0` | engine (SI) | The rule bits (§ 2.2). The engine's default 16 stays in force; `reset dmflags` sets 0 (D). | R |
| `sv_maplist` | `` | — | The deathmatch rotation: map names separated by spaces, commas or line breaks. At a level end the current map's name is looked for in it (without case); the next name is used, wrapping to the first; a list holding only the current map repeats it. When the current map is not in the list, or the list is empty, the map's own next map is used: its `nextmap` world key, else the map's first exit, else the same map. Overridden by dmflag 32. | R |
| `cheats` | `0` | engine (SI, L) | With 0, in deathmatch or coop, the client commands `give`, `god`, `notarget`, `noclip`, `teleport`, `spawnentity` and `spawnonstart` answer `You must run the server with '+set cheats 1' to enable this command.` Single player always allows them. | R |
| `gamename` | `baseq2` | SI, L | The game library's name, published for browsers. | R |
| `gamedate` | the library's build date | SI, L | The library's build date, published. | R |
| `dedicated` | `0` | engine (NS) | Read: on a dedicated server every chat line is also printed on the server console. | R |

**The level end.** The level ends into an intermission (players frozen at the intermission camera); in deathmatch it is left only when, more than 5 s after it began, any player presses any button; then the library runs `gamemap "<next map>"` (a level change, not a restart). With no player pressing a button the server stays in the intermission indefinitely (D, question 8). Outside deathmatch, a level exit that does not start a new unit skips the intermission entirely. (R)

### 2.2 `dmflags`, bit by bit

All bits act only in deathmatch unless stated; every bit is read when the rule is applied, so a change applies at once, except the ones that remove items, which act when a map spawns (D). (R)

| Bit | Value | Effect | Src |
| --- | --- | --- | --- |
| No health | 1 | Health items (every size), adrenaline and the ancient head are removed when the map spawns. | R |
| No powerups | 2 | The five powerups (quad damage, invulnerability, silencer, rebreather, environment suit) are removed when the map spawns. | R |
| Weapons stay | 4 | A picked-up weapon stays for others instead of respawning after 30 s, and a player who already owns it normally cannot take it again (but see question 8 for `coop_pickup_weapons`). Players cannot drop weapons. | R |
| No falling damage | 8 | No damage from falls. | R |
| Instant powerups | 16 | Powerups marked for instant use (quad damage, invulnerability, silencer, rebreather, environment suit) are used at the moment of pickup instead of going to the inventory. A dropped quad damage is always used at pickup, whatever this bit. The engine's default. | R |
| Same level | 32 | At a level end, stay on the same map (wins over `sv_maplist`). | R |
| Skin teams | 64 | Teams by skin: players whose skin name (after the `/`) matches are teammates. Enables team chat (`say_team`); without either team bit `say_team` is ordinary chat. | R |
| Model teams | 128 | Teams by model (the part before the `/`). Wins over skin teams when both are set. | R |
| No friendly fire | 256 | With a team bit (or in coop), teammates take no damage from each other (knockback still applies); without it, team damage is dealt and marked as friendly fire in the obituary. Damage to oneself is unaffected. | R |
| Spawn farthest | 512 | Spawn at the deathmatch spot farthest from every player. Without it: a random spot, excluding the two spots nearest to any player when there are more than two. | R |
| Force respawn | 1024 | A dead player respawns 1 s after death without pressing anything. Without it, attack (deathmatch) or any button respawns after that second. | R |
| No armor | 2048 | Armor and power armor items are removed when the map spawns. | R |
| Allow exit | 4096 | Exits work in deathmatch. Without it, touching an exit deals ten times the toucher's maximum health and kills them. | R |
| Infinite ammo | 8192 | Shots and throws use no ammunition; a weapon pickup gives 1000 of its ammo; ammo items and the BFG are removed from the map. | R |
| Quad drop | 16384 | A player dying with more than 1 s of quad damage left drops it (with the dropped weapon, spread apart). | R |
| Fixed FOV | 32768 | Every player's field of view is 90, whatever their `fov`. | R |
| (mission-pack bits) | 65536, 131072, 262144, 524288, 1048576 | Named in the shared header for the expansion packs' libraries (quad-fire drop, no mines, no stacked double damage, no nukes, no spheres); the stock library never reads them. | R |

### 2.3 Passwords, spectators and the ban list

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `password` | `` | UI | Player password; empty or `none` means none. The connect and the spectator-to-player switch compare `none` and the password with case (`q2-net.md` §§ 2, 17). As a user info setting it cannot hold a backslash, a double quote or a semicolon. | R |
| `spectator_password` | `` | UI | Spectator password, same rules; the client sends it as its user info `spectator` value. | R |
| `needpass` | `0` | SI | Set by the library, not the operator: bit 1 when `password` is set and not `none`, bit 2 when `spectator_password` is, so 0 to 3. Recomputed at the end of every frame in which either password changed (and on the first frame after the library starts), so it lags one 100 ms frame and is not updated while no map runs (D). Here `none` is compared without case. | R |
| `maxspectators` | `4` | SI | Most spectators at once, deathmatch only (`Server spectator limit is full.`). | R |
| `filterban` | `1` | — | 1: addresses in the filter list are refused (`Banned.`). 0: only addresses in the list may connect. | R |

**The filter list** (game commands, question 5): up to 1024 entries, each four numbers. A typed address may stop early (`addip 192.246.40` covers that whole range); **each octet given as 0 is a wildcard too**, so `addip 10.0.0.1` matches every address of the form 10.x.x.1 (D, from how the mask is built). The list holds IPv4 only; the check reads the client's address text digit by digit, so an IPv6 client is matched against an unrelated number (D). The list lives in memory only: it is empty at each start of the library (every `map`) unless a config re-adds it (D).

### 2.4 Single player and coop

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `skill` | `1` | engine, + L | 0 easy, 1 medium, 2 hard, 3 hard plus. At each map spawn it is rounded down and clamped to 0 to 3 (force-set when changed). Outside deathmatch it removes map entities flagged for other skills and tunes monsters (reaction, aim, turret speed; on easy, monsters often skip a melee attack and players take half damage, at least 1). | R |
| `coop_pickup_weapons` | `1` | A | In coop, weapons stay for everyone. 1: a player who already owns a weapon may still take one copy that nobody has taken yet (for its ammo); 0: owners can never take it again. | R |
| `coop_elevator_delay` | `1.0` | A | In coop, seconds a platform waits after being stepped on before it moves, so others can board. 0 or less: it moves at once. | R |
| `g_fix_triggered` | `0` | — | 1: a monster flagged to appear only when triggered, but which nothing can trigger (no target name), spawns normally instead of never appearing. A warning line is printed either way. | R |
| `g_commanderbody_nogod` | `0` | A | 1: the commander's body (a map object) can be shot and gibbed; 0: it is invulnerable. | R |
| `g_monsterfootsteps` | `0` | A | 1: walking monsters of several kinds make footstep sounds. | R |

### 2.5 Movement, view and weapons

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `sv_gravity` | `800` | — | Gravity, units/s²: for the library's entity physics and copied into every player's move each frame, so it applies at once. | R |
| `sv_maxvelocity` | `2000` | — | Entities moved by the library's physics have their total speed (the length of the velocity) scaled down to this. The player move is not affected. | R |
| `sv_rollangle` | `2` | — | Degrees of view roll when strafing at full side speed. | R |
| `sv_rollspeed` | `200` | — | Side speed at which the roll reaches `sv_rollangle`. | R |
| `run_pitch` | `0.002` | — | View pitch per unit of forward speed while running. | R |
| `run_roll` | `0.005` | — | View roll per unit of side speed while running. | R |
| `bob_up` | `0.005` | — | View bob height per unit of speed. | R |
| `bob_pitch` | `0.002` | — | View bob pitch. | R |
| `bob_roll` | `0.002` | — | View bob roll. | R |
| `gun_x`, `gun_y`, `gun_z` | `0` | — | Development offsets added to every player's gun position (right, forward, up). | R |
| `g_footsteps` | `1` | A | Player footstep sounds: 0 none; 1 on the ground and moving faster than 225 units/s horizontally (the classic rule); 2 on the ground at any speed; 3 or more always, even in the air. | R |
| `g_select_empty` | `0` | A | 0: switching to a weapon with no ammo is refused (`No <ammo> for <weapon>.`); 1 allows it. | R |
| `aimfix` | `0` | A | 1: shots and projectiles are aimed from the muzzle toward the point the crosshair looks at (found by a line of up to 8192 units from the eye), and the super shotgun's two halves start from the gun with the kick redone. 0: classic aim, from an offset point straight ahead. | R |
| `g_machinegun_norecoil` | `0` | A | 1: the machine gun's view does not climb while firing. Deathmatch never climbs anyway. | R |
| `g_quick_weap` | `1` | A | 1: `weapnext` and `weapprev` step from the weapon already chosen but not yet raised, so repeated presses skip ahead, and the chosen weapon's icon and name show for 0.9 s. | R |
| `g_swap_speed` | `1` | A | Weapon lower and raise animations advance this many frames per 100 ms frame; values at or below 1, and 65535 or more, mean 1. | R |
| `cl_gun` | `2` | — | A client setting the library reads on the server: with 2, a centre-handed or wide-angle (field of view above 91) player's help icon slot stays empty instead of showing the weapon icon. On a dedicated server it keeps its default (D). | R |

### 2.6 Chat flood

Deathmatch and coop only; covers `say`, `say_team` and any unknown client command that is spoken as chat (R).

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `flood_msgs` | `4` | — | Messages allowed within `flood_persecond`. Above 10 it is force-set to 10 (`flood_msgs lowered to max: 10`). 0 or less disables flood protection. | R |
| `flood_persecond` | `4` | — | The window, in seconds of level time. | R |
| `flood_waitdelay` | `10` | — | Seconds a flooder is muted: `Flood protection: You can't talk for <n> seconds.`, then each try `You can't talk for <n> more seconds`. | R |

Chat lines are cut to 150 characters including the name prefix (`<name>: ` or `(<name>): ` for team chat). (R)

---

## Question 3: present but without effect, or for compatibility only

On the dedicated build (D, from where each is read):
- **Archive flags** do nothing (no config is written). `hostname`, `allow_download*`, `sv_entfile` and the library's archive settings must be put in a config the operator runs.
- **`qport`**, **`cl_gun`** (holds its default), **`cl_maxfps`** (only caps the loop), `colorterminal` (cosmetic).
- **User info flag on `password` and `spectator_password`**: no client to send them; it only restricts their characters.
- **`gamename`, `gamedate`, `protocol`, `version`, `gamedir`, `mapname`** are informational keys; changing the no-set ones is refused, and the latched library ones only rename the key's value.
- **Mode-only settings** in the wrong mode: `sv_airaccelerate` and `dmflags`, `fraglimit`, `timelimit`, `sv_maplist`, `maxspectators` outside deathmatch; `coop_*` outside coop; `skill`, `sv_noreload`, `g_fix_triggered`, `g_monsterfootsteps`, `g_commanderbody_nogod` in deathmatch (no monsters); `g_machinegun_norecoil` in deathmatch; `cheats` in single player.
- **`singleplayer`** exists only to choose the mode; **`basedir`** is a deprecated alias for `-datadir`; **`cddir`** is legacy.
- **Diagnostics with no effect on play**: `showpackets`, `showdrop`, `showclamp`, `fs_debug`, `developer`, `modder`, `map_noareas` (affects visibility, a debugging aid), `timedemo`, `timescale`, `fixedtime`, `gun_x/y/z` (affect every player's gun, but only cosmetically).
- **Defaults overwritten by the second registration**: `dmflags` (engine 16, library 0), `maxclients` (engine 1, library 4), `dedicated` (library 0): the values in force are the engine's, but `reset` and `resetall` use the library's (R, D).
- The `dmflags` bits from 65536 up are read only by the expansion libraries (§ 2.2).

## Question 4: server info keys and the query answers

**Keys the program and library put in server info** (R), each present only while its value is not empty: `cheats`, `coop`, `deathmatch`, `dmflags`, `fraglimit`, `game` (empty for the base game, so usually absent), `gamedate`, `gamedir` (absent for the base game), `gamename`, `hostname`, `mapname`, `maxclients`, `maxspectators`, `needpass`, `protocol`, `singleplayer`, `timelimit`, `version`, plus any setting the operator creates with `set <name> <value> s`. Keys are in the order of the setting list, which is sorted by name (D), so the string starts `\cheats\0\coop\0\deathmatch\1\dmflags\16\...`. A key whose value is `0` is kept (unlike the first game's server, which blanks 0) (R).

**The answers** a browser reads (shapes in `q2-net.md` § 1): `info <protocol>` gives one line (host name, map, players over slots) and is ignored on a one-slot server; `status` gives the server info string, a newline, then `<frags> <ping> "<name>"` per connected or playing client, cut before 1384 bytes. A connected client can also type `info` to have the server info printed to it. (R)

**The heartbeat** (R, except where marked): only a dedicated server with `public` nonzero sends one. It is the text `heartbeat`, a newline, then exactly the `status` answer's text, sent as an out-of-band packet to every master in the list. The list's first slot is always the original publisher's master at 192.246.40.37, port 27900; `setmaster` fills up to 7 more. A heartbeat goes every 300 s. The server's clock restarts at 0 at every map spawn, which also restarts the 300 s count, so each level gets its first heartbeat 300 s after it starts; the first level of a game gets its first after about 200 s (D, from the start offset the program sets and the clock reset). `heartbeat` and `setmaster` force one on the next frame. When the server shuts down (`killserver`, `quit`, a restart by `map`), each master gets `shutdown`. No answer from a master is expected; a master's `ack` is printed (`Ping acknowledge from <address>`).

## Question 5: operator commands

Engine commands (R):

| Command | What it does |
| --- | --- |
| `map <name>` | Starts a fresh game on a map: refuses a missing `maps/<name>.bsp` (`Can't find maps/<name>.bsp`) unless the name holds a `.` or `$` or starts with `*`; wipes the current unit's saved levels; restarts the server (connected clients get `Server restarted` and reconnect, `q2-net.md` § 18), applies latched settings, reloads the game library, then loads the map. Syntax `map [*]<map>[$<spawn point>][+<next>]`: `$` names the spawn point, `+` queues another map after a cinematic, and a name ending `.cin`, `.dm2`, `.pcx`, `.tga`, `.jpg` or `.png` plays a cinematic, a server-side demo or a picture. |
| `gamemap <name>` | Changes level without a restart (the clients follow with `changing` and `reconnect`); latched settings stay pending. Outside a new unit (`*`), saves the level being left into the current unit; corrects the map name's case to the file's. Used by the library at every level end. |
| `demomap <file.dm2>` | Plays a demo as the server's map (an attract loop). |
| `gamemode <dm/coop/sp/?>` | Sets the three mode settings together for the next `map`; `?` prints the modes set. |
| `listmaps` | Lists every `maps/*.bsp` in the search path. |
| `status` | Prints the map and a table: slot, score, ping (`CNCT` while in the signon, `ZMBI` for a zombie, capped at 9999), name, milliseconds since the last packet, address, qport. `No server running.` before the first map. |
| `kick <slot or name>` | A number (any argument starting with a digit) is a slot; otherwise the exact name, with case. Broadcasts `<name> was kicked` (if in the game), tells the client `You were kicked from the game`, drops it. Errors: `Bad client slot: <n>`, `Client <n> is not active`, `Userid <name> is not on the server`. |
| `dumpuser <slot or name>` | Prints that client's user info. |
| `serverinfo` | Prints the server info string, a key and value per line. |
| `say <text>` | Dedicated only: sends `console: <text>` as chat to every client in the game. |
| `sv <command> ...` | Passes the rest to the game library (`No game loaded.` without one). |
| `serverrecord <name>` | Records every entity and message (no per-player view) to `demos/<name>.dm2` in the write folder; refused in a cinematic or demo, with `..`, `/` or `\` in the name, or while already recording. `serverstop` ends it. |
| `save <dir>`, `load <dir>` | Single player and coop saves; `save` is refused in deathmatch, to `current`, or while the single player is dead. |
| `setmaster <address> ...` | Dedicated only. Replaces masters 2 to 8 with the given addresses (port 27900 when none is given), sets `public 1`, pings each, and forces a heartbeat. When an address is refused (`Bad address: ...`), the later ones are stored one slot off and an empty slot is pinged at port 27900 (D). |
| `heartbeat` | Forces a heartbeat on the next frame. |
| `killserver` | Ends the game: clients get `Server was killed.` and are disconnected, the masters get `shutdown`, the sockets close. The dedicated server then idles until a `map`. |
| `quit` | Dedicated only: shuts down and exits. |
| `exec <file>`, `alias`, `vstr`, `echo`, `wait`, `cmdlist`, `path`, `dir`, `link`, `z_stats`, `set`, `reset`, `resetall`, `toggle`, `inc`, `dec`, `cvarlist` | The console's own commands. `exec` reads the file from the search path (`couldn't exec <file>`, `execing <file>.`). |

The game library's `sv` commands (R): `sv addip <address>` (adds to the filter list; `IP filter list is full` past 1024; a malformed address prints `Bad filter address: <text>` and leaves an unusable entry shown as 255.255.255.255), `sv removeip <address>` (removes only an entry typed the same way: `Removed.` or `Didn't find <address>.`), `sv listip` (`Filter list:` then one address per line), `sv writeip` (writes `set filterban <n>` and one `sv addip` per entry to `<game folder>/listip.cfg`, the folder taken relative to the server's working directory, not the write folder, so `exec listip.cfg` finds it only when that folder is on the search path (D)), `sv test`. Anything else: `Unknown server command "<word>"`.

## Question 6: the startup order

(R, with the consequences marked D)

1. The command line is scanned for `-datadir`, `-cfgdir`, `-portable` and the help flags.
2. Every `+set <name> <value>` on the command line is run (first pass), so `game`, `basedir` and `cddir` are known before the file system starts.
3. The file system starts (`basedir`, `cddir`, `game`, `fs_debug` registered); the search path is built, with the mod when `game` is set.
4. `default.cfg`, `yq2.cfg`, `config.cfg` and `autoexec.cfg` are run, each from the search path (the mod's copy first), each skipped with `couldn't exec` when missing. Then every command-line `+set` is run again (second pass), so the command line wins over these configs.
5. The common settings are registered (`developer`, `logfile`, `version`, `dedicated`, ...), then the network ones (`qport`, `showpackets`, `showdrop`), then the server's (§§ 1.1 to 1.3).
6. The other `+` commands on the command line are run in order (each runs to the next `+`, or to a word starting with `-` after a space): typically `+exec server.cfg +map q2dm1`. With none, a dedicated server runs the command `dedicated_start`, which the program does not define: unless a config defined an alias of that name, nothing happens and the server waits with no map and no open port (D).
7. The first `map` starts the game: latched values apply, the mode is chosen, the slots are made, the UDP sockets open (`port`, `ip`), the game library loads and registers its settings, the map spawns.

Consequences:
- **Settings registered after step 4 do not exist while the startup configs run.** A config line `maxclients 8` (without `set`) in `autoexec.cfg` is silently ignored, while `set maxclients 8` creates it and the later registration keeps the value (D). The same holds for the game library's settings in any config run before the first `map`: use `set`.
- **No-set settings** (`port`, `ip`, `dedicated`, `basedir`, `cddir`, `multicast`, `protocol`) must be given as `+set` on the command line (or with `set ... s/u/a`, which bypasses the protection).
- **Latched settings** (`maxclients`, `deathmatch`, `coop`, `singleplayer`, `cheats`, `game`, `sv_airaccelerate`, and the library's `skill`, `maxentities`) apply at once before the first map, and later only at a `map`.
- **Changing `game`** re-runs the four configs of step 4 when it takes effect, but not the command line's `+set`, so a mod's configs can override command-line values (D).
- The server has no `server.cfg` of its own; operators run it with `+exec server.cfg`.

## Question 7: settings the competitive Quake II server adds

**Open.** Its repository could not be fetched in this session (the session's network access to it was refused), so nothing is stated about it here. A later reader should answer this question from that server's source; until then no setting of it may be assumed.

## Question 8: open points the source does not settle

- **Intermission with nobody pressing.** A deathmatch level end waits for a button press more than 5 s into the intermission; nothing else ends it. Whether an empty or idle server is meant to stay there indefinitely (and stop rotating maps) is not settled; the code has no timeout (D that it stays).
- **Weapons stay and `coop_pickup_weapons` in deathmatch.** The "already owned" test lets a player take a weapon they own when `coop_pickup_weapons` is nonzero and no coop player has taken it, and only coop marks it taken. With dmflag 4 in deathmatch and the default `coop_pickup_weapons 1`, an owner can therefore keep collecting ammo from a staying weapon (D). Whether that is intended, and whether the classic release behaved so, is not settled here.
- **`maxentities` above 1024.** The library allocates any number, and the engine does not check it against the protocol's entity number range; what happens past 1024 entities is not settled by the source.
- **The heartbeat's first send.** The program's own comment intends an immediate first heartbeat, but the clock reset at map spawn makes it about 200 s (D); which one a port should keep is a decision, not a fact.
- **`set ... s` on protected settings.** Bypassing no-set and latching through the flag form looks unintended, but nothing in the source says so.
- **IPv6 clients and the filter list** (§ 2.3): the result of matching an IPv6 address text against the IPv4 list is undefined by intent.
- **The competitive Quake II server**: everything in question 7.
