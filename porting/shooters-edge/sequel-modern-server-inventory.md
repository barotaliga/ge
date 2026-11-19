# Sequel modern server inventory: the settings, commands and features of a modern sequel server

WORKING AREA: deleted when porting ends. A clean-room description, in our words, of the settings, operator commands, protocol and file features, server info keys and out-of-band answers of a modern sequel server (the AQtion fork of Q2PRO, github.com/actionquake/q2pro, branch `aqtion`, commit `6400f7e97309554b2b22fe4b2a1ec4803be7ee50`, committed 2026-11-16) and of the game library it ships, read from that project and commit by a reader session. It holds no code and no internal identifiers; every fact is marked read (R) or derived (D), and points the source does not settle are said to be open. It is read beside `sequel-server-inventory.md` and `sequel-net.md`, which describe the stock server; "as stock" here means the same as there.

Read from github.com/actionquake/q2pro, default branch `aqtion`, commit `6400f7e97309554b2b22fe4b2a1ec4803be7ee50` (committed 2026-11-16). The product calls itself AQtion; its dedicated program is `q2proded`. Its build defaults the base folder to `baseaq` and the `game` setting to `action`. Nothing was run: every fact is **R** (read in the source or the project's own documentation) or **D** (derived by combining what was read). No fact is **M**. "Stock" means the stock sequel server and library described in `sequel-server-inventory.md` and `sequel-net.md`; "as stock" means the behaviour is the same.

Flag letters: **SI** server info, **UI** user info, **L** latched (waits for the next `map`), **NS** no-set (command line only), **A** archive, **RO** read-only, **P** private (never shown in server info, not saved), **X** listed in the extended rules answer (`rulesx`, question 3), **—** none. Times are seconds unless stated; rates are bytes per second.

---

## 1. Every setting the dedicated server registers

### 1.0 What decides which settings exist (R)

- The build has switches. Their defaults: the anticheat interface off, the multi-view recorder and relay on, save games on, variable frame rate off, packet duplication off. The project's published builds (its CI) turn on the anticheat interface, packet duplication and variable frame rate. A setting below marked "build" exists only when that switch is on.
- `developer`, `showpackets`, `showdrop`, `sv_debug`, `sv_pad_packets`, `com_debug_break` and the network log settings exist only in debug builds; a release server has none of them.

### 1.1 How settings behave (changed from stock)

- **Registering an existing setting** adds the new flags and keeps the first default. So a game library that registers `hostname`, `maxclients` or `dmflags` with another default does not change what `reset` restores: the engine's default wins. (Stock: the later default wins.) (R)
- A setting the operator created before the program registered it is set back to its default if it turns out read-only, no-set (after startup), cheat-protected while `cheats` is 0, or an info setting with an illegal value. (R)
- **Info values** may not hold a backslash, a double quote or a semicolon (`Info cvars should not contain '\', ';' or '"' characters.`) and must be under 64 characters (`Info cvars should be less than 64 characters long.`). (R)
- **Refusals**: `<name> is read-only.`; `<name> is cheat protected.` (for `timescale`, `fixedtime`, `timedemo` while `cheats` is 0); `<name> may be set from command line only.`; and, for a latched setting while a server runs, `<name> will be changed for next game.` (R)
- **`set <name> <value> u|s`** changes only the user-info or server-info flag, and still obeys read-only, no-set and latching. Stock's flag form replaced all flags and so bypassed them; here it does not. (R) `setu`, `sets` and `seta` (adds archive) are short forms. (R)
- **Typing a name alone** prints `"<name>" is "<value>"`, followed by the default and the latched value when they differ from it. (R)
- Other setting commands: `cvarlist` (with filter letters), `toggle`, `inc`, `dec`, `mul`, `div`, `reset`, `resetall`. (R)
- **Macros** an operator can use in commands: `$com_date`, `$com_time`, `$com_uptime`, `$com_uptime_long`, `$random` (one digit, 0 to 9), `$com_maplist`, `$net_uprate`. (R)

### 1.2 Startup order (changed from stock)

1. The system's default config, then the command line's early `+set` words. (R)
2. The file system starts. (R)
3. Configs, each only if present: `default.cfg`, `config.cfg` (`q2config.cfg` on Windows), `autoexec.cfg` (first from the base folder, then again from the game folder when `game` is set), `postexec.cfg`. A missing file is silent; another read error prints a warning. `fs_autoexec` (default 1) decides whether they run again when `game` changes. (R)
4. The early `+set` words again. (R)
5. The other `+` commands. When there are none, the alias `dedicated_start` runs if it is defined. (R)
6. The dedicated server opens its network sockets even with no map loaded, so rcon works before the first map. (Stock: no sockets until a map.) (R)
7. `postinit.cfg`. (R)

### 1.3 Game rules and identity

| Setting | Default | Flags | Effect | vs stock | Mark |
| --- | --- | --- | --- | --- | --- |
| `protocol` | `34` | SI, RO | Rewritten at every map: `36` when the protocol extensions are on (question 3), else `34`. | changed (rewritten by map) | R |
| `deathmatch` | `1` | SI, L | Game mode. | changed (stock 0) | R |
| `coop` | `0` | L | Coop mode. | changed (no longer server info) | R |
| `skill` | `1` | L | Difficulty; latched in the engine itself. | changed (stock engine has no flag; its library latches it) | R |
| `cheats` | `0` | SI, L | As stock; also gates `timescale`, `fixedtime`, `timedemo`. | same | R |
| `dmflags` | `16` | SI | As stock. | same | R |
| `fraglimit` | `0` | SI | As stock. | same | R |
| `timelimit` | `0` | SI | As stock. | same | R |
| `maxclients` | `8` | SI, L | Player slots. When a game starts: deathmatch with 1 or fewer becomes 8; coop with 1 or fewer becomes 4; otherwise 1. Clamped to 1 to 256; 1 to 255 when the multi-view recorder is on. | changed (stock 1, other clamps) | R |
| `sv_reserved_slots` | `0` | L | Slots kept for holders of the reserved password. Clamped to 0 to `maxclients` minus 1. Set to 1 automatically when multi-view recording is on and it is 0 (the recorder's dummy client uses it). | new | R |
| `hostname` | `noname` | SI, A | Server name. | same as stock engine | R |
| `game` | `action` | SI, L | Game folder; never archived. | changed (default) | R |
| `version` | `q2proded <version> <date> <os> <cpu>` | SI, UI, RO | Version text. | changed (text) | R |
| `dedicated` | `1` | RO | | same | R |
| `sv_running`, `sv_paused` | | RO | State reports. | new | R |
| `timescale` `fixedtime` `timedemo` | `1` `0` `0` | cheat-protected | Diagnostics; refused while `cheats` is 0. | changed (stock has no protection) | R |

### 1.4 Timeouts and time checks

| Setting | Default | Flags | Effect | vs stock | Mark |
| --- | --- | --- | --- | --- | --- |
| `timeout` | `90` | — | Seconds of silence before a drop. Clamped to 0 to 24 days. | changed (stock 125) | R |
| `zombietime` | `2` | — | As stock. | same | R |
| `sv_ghostime` | `6` | — | Seconds a slot that passed the challenge may stay silent before it is freed. Also the delay before a client whose packets come back unreachable is dropped. | new | R |
| `sv_idlekick` | `0` | — | Seconds without activity before a drop with reason `idling`; 0 is off. Activity is any change of buttons or move, or a `say`/`say_team`. | new | R |
| `sv_enforcetime` | `1` | — | Move-time budget: each client may spend 1800 ms of moves per 16 base frames (1.6 s); moves over budget are ignored without a message. | changed (stock 0, other budget) | R |
| `sv_timescale_time` | `16` | — | Window in seconds for the time-skew check. | new | R |
| `sv_timescale_warn` | `0` | — | Above 1.0, a client whose move time divided by real time over the window exceeds it is logged: `<name>[<addr>]: detected time skew: <x.xxx>`. | new | R |
| `sv_timescale_kick` | `0` | — | Above 1.0, the same ratio over it drops the client: `time skew too high`. | new | R |

### 1.5 Frame rate, frames and entities

| Setting | Default | Flags | Effect | vs stock | Mark |
| --- | --- | --- | --- | --- | --- |
| `sv_fps` (build: variable frame rate) | `10` | L | Server frame rate. Used only when the game library says it can run faster; otherwise forced to 10. The divisor is fps/10 clamped 1 to 6; the frame lasts 100 ms divided by the divisor as a whole number of ms: 100, 50, 33, 25, 20, 16 ms. The setting is rewritten to divisor times 10. | new | R |
| | | | So `sv_fps 60` runs 16 ms frames, about 62.5 frames per second, and `30` runs 33 ms frames, about 30.3. | | D |
| `sv_allow_nodelta` | `1` | — | With 0, a client that asks for more than 64 full (non-delta) frames is dropped: `too many nodelta frames`. | new | R |
| `sv_max_packet_entities` | `0` | — | Most entities in one frame; 0 means 128 for old-protocol clients and 512 with the extended limits. The project's documentation says the default is 128; the source says 0. | new | R |
| `sv_trunc_packet_entities` | `1` | — | For old-channel clients, a frame too big for a packet is cut short instead of failing. | new | R |
| `sv_prioritize_entities` | `0` | — | When a frame is over the cap: players, monsters and brush models first, gibs and entities without a model last, then by distance; the cap is applied, then the kept ones are put back in number order. | new | R |
| `sv_novis` | `0` | — | 1 sends every entity, skipping area and visibility culling. | new | R |
| `sv_features` | engine bits | RO | The engine's feature bits, read by the game library. | new | R |
| | 64527 with variable frame rate, 62479 without | | | | D |
| `g_features`, `g_view_predict`, `g_view_low`, `g_view_high` | set by the library | RO | The library's feature bits and view heights for prediction. The fork's library sets prediction on with heights 22 (standing) and 8 (crouching). Sent to protocol 36 and 38 clients as server settings (question 3). | new | R |

### 1.6 Movement

| Setting | Default | Flags | Effect | vs stock | Mark |
| --- | --- | --- | --- | --- | --- |
| `sv_airaccelerate` | `0` | L | As stock: deathmatch only, written into the map's air-acceleration value at each map. | same | R |
| `sv_qwmod` | `0` | L | For protocol 36 and 38 clients, the alternative move: top speed 320 (normal 300), friction 4 (normal 6), water friction 4 (normal 1), water speed 0.7 of normal (normal 0.5), and air acceleration on. | new | R |
| `sv_strafejump_hack` | `1` | L | Removes the landing pause that blocks a jump after landing faster than 200 units/s (200 ms after more than 400, else 144 ms). 1 applies to protocol 35, 36 and 38 clients; 2 or more to all. | new | R |
| `sv_waterjump_hack` | `1` | L | Removes the special surfacing speed when jump is held under water. 1 applies to protocol 36 and 38 clients; 2 or more to all. | new | R |
| (no setting) | | | Protocol 35 and later: spectators fly twice as fast. Protocol 36 and 38: the spectator fly fix, with fly friction 4 (normal 9). | new | R |

### 1.7 Rates and pings

| Setting | Default | Flags | Effect | vs stock | Mark |
| --- | --- | --- | --- | --- | --- |
| `sv_min_rate` | `15000` | L | Lowest rate a client may ask for; at least 1500 and not above `sv_max_rate`. | new | R |
| `sv_max_rate` | `60000` | L | Highest rate; at least 1500. | new | R |
| `sv_lan_force_rate` | `0` | L | 1 gives LAN addresses unlimited rate. | new | R |
| (user info `rate`) | | | Clamped to the two settings above. A client that sends no rate gets 5000, not clamped. Loopback is unlimited. | changed | R |
| | | | 5000 is below the default minimum, so a client without a rate key runs slower than one that sends any value. | | D |
| (user info `msg`) | | | Message level, clamped 0 to 256. | as stock | R |
| `sv_calcpings_method` | `2` | — | 0 no ping, 1 the average latency of the last 16 frames, 2 their minimum. | new | R |
| `sv_packetdup_hack` (build: packet duplication) | `0` | — | Most duplicate packets a client may ask for with its `packetdup <n>` command. Answers: `Packetdup of <n> is not allowed on this server.` and `Server is sending <n> duplicate packet(s) to you.` | new | R |

Ping statistics (average, minimum, maximum) and moves per second are updated every 10 s. (R)

### 1.8 Access, passwords and limits

| Setting | Default | Flags | Effect | vs stock | Mark |
| --- | --- | --- | --- | --- | --- |
| `sv_password` | `` | P | Password every player must give (user info `password`). Its holders may also use reserved slots. | new | R |
| `sv_reserved_password` | `` | P | Password for the reserved slots. | new | R |
| `sv_locked` | `0` | — | 1 refuses new connections: `Server is locked.` | new | R |
| `sv_iplimit` | `3` | — | Most connections per IPv4 address or per IPv6 /48 network; slots being freed count half; link-local IPv6 is exempt. Also limits recorder viewers over TCP. | new | R |
| `sv_redirect_address` | `` | — | When the server is full, new clients are sent to this address. A leading `!` sends every client that lacks the reserved password, full or not (not in the project's documentation). | new | R |
| `sv_force_reconnect` | `` | L | An address the client must reconnect to before it is admitted (question 3). | new | R |
| `sv_auth_limit` | `1` | — | Failed password attempts allowed per second; a good attempt gives its token back. | new | R |
| `sv_rcon_limit` | `1` | — | Failed rcon attempts allowed; a good one gives its token back. | new | R |
| `sv_namechange_limit` | `5/min` | — | Name changes allowed (`You can't change your name too often.`). | new | R |
| `sv_status_limit` | `15` | — | `status` and extended queries answered. | new | R |
| `sv_allow_unconnected_cmds` | `0` | — | With 0, commands from a client not yet in the game are not passed to the game library. | new | R |
| `lrcon_password` | `` | P | Limited rcon: passes only commands that start with a prefix added by `addlrconcmd` (question 2). | new | R |
| `rcon_password` | `` | P | As stock. | same | R |

**Rate-limit syntax** (the four limit settings above): `limit[/period[sec|min|hour]][*burst]`. The period defaults to 1 second and the burst to 5; a limit of 0 means unlimited. Errors: `Period too large: <n>`, `Limit too large: <n>`, `Burst too large: <n>`. (R)

### 1.9 Names, status and masters

| Setting | Default | Flags | Effect | vs stock | Mark |
| --- | --- | --- | --- | --- | --- |
| `sv_show_name_changes` | `0` | — | 1 announces name changes of players in the game, 2 of everyone: `<old> changed name to <new>`. The dedicated console always logs `<old>[<addr>] changed name to <new>`. | new | R |
| `sv_enhanced_setplayer` | `0` | — | 1 lets commands that take a player match part of a name, without case. | new | R |
| `sv_status_show` | `2` | — | 0 no `status` answer, 1 server info only, 2 server info and players. | new | R |
| `sv_status_ext` | `0` | — | 1 answers the extended queries `statusx` and `rulesx` (question 3). | new | R |
| `sv_uptime` | `0` | — | Adds an `uptime` key to `status` and heartbeat answers only: 1 short form (`MM.SS`, `H:MM.SS`, `D+H:MM.SS`), 2 long form (`N days, N hours, N mins, N secs`). | new | R |
| `public` | `0` | L | Sends heartbeats to the masters named by `setmaster`. There is no built-in master. | changed (latched; no default master) | R |

### 1.10 Maps

| Setting | Default | Flags | Effect | vs stock | Mark |
| --- | --- | --- | --- | --- | --- |
| `sv_allow_map` | `0` | — | 0 refuses `map` while a game runs (question 2); 1 allows it; 2 makes `map` behave as `gamemap`. (Clients default to 1.) | new | R |
| `sv_recycle` | `0` | — | 1 turns the next `gamemap` into a full `map`; 2 makes it quit with a reconnect to clients. Reset at each game start. | new | R |
| `sv_changemapcmd` | `` | — | Command run after each map loads. | new | R |
| `sv_cinematics` | `1` | — | Cinematic levels; the exact effect of 0 was not traced. | new | R |
| `sv_noreload` | `0` | — | As stock. | same | R |
| `sv_load_ent` | `1` | L | Allows the map override file (question 3). It does not gate the separate entity file, though the documentation says it gates both. | new | R |
| `map_override_path` | `` | — | Folder for entity files and map override files (question 3). | new | R |
| `map_visibility_patch` | `1` | — | Repairs the visibility data of three stock deathmatch maps (`q2dm1`, `q2dm3`, `q2dm8`). | new | R |
| `map_noareas` | `0` | — | Effect not traced (the name says area portals are ignored). | new | R |

### 1.11 Network and downloads

| Setting | Default | Flags | Effect | vs stock | Mark |
| --- | --- | --- | --- | --- | --- |
| `net_ip` | `` | — | Address to bind for IPv4. Replaces stock `ip`. Changing it restarts the sockets. | changed (name, not no-set) | R |
| `net_ip6` | `` | — | Address to bind for IPv6. | new | R |
| `net_port` | `27910` | — | Port. Replaces stock `port`. Changing it restarts the sockets. | changed (name) | R |
| `net_enable_ipv6` | 1 if the system has IPv6, else 0 | — | 2 also listens on IPv6. | new | R |
| `net_ignore_icmp` | `0` | — | 1 ignores unreachable notices (so the `sv_ghostime` drop of question 3.6 does not happen; D). | new | R |
| `net_maxmsglen` | `1390` | — | Largest packet the server accepts a client asking for; 0 no limit, otherwise clamped 512 to 4086. | new | R |
| `sv_max_download_size` | `8388608` | — | Largest file sent, in bytes; 0 means the 64 MiB internal limit. | new | R |
| `sv_downloadserver` | the project's asset URL | — | HTTP download address given to clients. In this fork an empty value still sends the built-in URL. | new | R |
| `allow_download` | `0` | — | Master switch. | changed (stock 1) | R |
| `allow_download_players` | `1` | — | | changed (stock 0) | R |
| `allow_download_models` | `1` | — | Also covers `sprites/`. | same, wider | R |
| `allow_download_sounds` | `1` | — | | same | R |
| `allow_download_maps` | `1` | — | Sends maps from inside packs too. The documentation's difference between 1 and 2 is not in the source: any nonzero value sends them. | changed | R |
| `allow_download_textures` | `1` | — | `textures/` and `env/`. | new | R |
| `allow_download_pics` | `1` | — | | new | R |
| `allow_download_others` | `0` | — | Anything else; not archived. | new | R |

### 1.12 Logging, console and files

| Setting | Default | Flags | Effect | vs stock | Mark |
| --- | --- | --- | --- | --- | --- |
| `logfile` | `0` | — | 1 writes a new log, 2 appends. | changed (stock 1, other values, other file name) | R |
| `logfile_flush` | `0` | — | 1 flushes each line, 2 writes unbuffered. | new | R |
| `logfile_name` | `console` | — | Written to `logs/<name>.log`. | new | R |
| `logfile_prefix` | `[%Y-%m-%d %H:%M] ` | — | Time stamp before each line; an `@` becomes a letter for the line's kind (T, D, W, E, N, A). | new | R |
| `console_prefix` | `` | — | Prefix on console lines. | new | R |
| `com_date_format` | `%Y-%m-%d` | — | For `$com_date`. | new | R |
| `com_time_format` | `%H:%M` | — | For `$com_time`. | new | R |
| `com_fatal_error` | `0` | — | Effect not traced. | new | R |
| `com_completion_mode` | `1` | — | Tab completion. | new | R |
| `com_completion_treshold` | `50` | — | Tab completion list size. | new | R |
| `sys_console` | 2 on a terminal, else 1 | NS | Console mode. | new | R |
| `sys_history` | | — | Command history size. | new | R |
| `sys_forcegamelib` | `` | NS | Game library path tried first. | new | R |
| `sys_allow_unsafe_savegames` | `0` | NS | Effect not traced. | new | R |
| `basedir` | `.` | NS | | same | R |
| `homedir` | `~/.q2pro` in system-wide builds, else `` | NS | | new | R |
| `libdir` | `.` | NS | | new | R |
| `fs_autoexec` | `1` | — | Rerun configs when `game` changes. | new | R |
| `fs_debug` | `0` | — | | new | R |

**Stock settings that are gone:** `ip`, `port`, `ip_hostport`, `hostport`, `cl_maxfps`, `showclamp`, `sv_entfile`, the `sv_optimize_*` group, `paused`, `colorterminal`, `cddir`, `multicast`, `modder`. (R)

### 1.13 Multi-view recorder (new)

| Setting | Default | Flags | Effect | Mark |
| --- | --- | --- | --- | --- |
| `sv_mvd_enable` | `0` | L | 1 records locally; 2 also accepts viewers over TCP on the game port. If the TCP port cannot be opened it falls back to 1 with `Error opening server TCP port.` or `Server TCP port already in use.` | R |
| `sv_mvd_maxclients` | `8` | L | TCP viewers, clamped 1 to 256. | R |
| `sv_mvd_bufsize` | `2` | L | Send buffer in 32 KiB units, clamped 1 to 4. | R |
| `sv_mvd_password` | `` | P | Viewer password. | R |
| `sv_mvd_maxsize` | `0` | — | Recording limit in kB (at most 2000000): `Stopping MVD recording, maximum size reached.` | R |
| `sv_mvd_maxtime` | `0` | — | Limit in minutes: `... maximum duration reached.` | R |
| `sv_mvd_maxmaps` | `1` | — | Map changes before a recording stops: `... maximum number of level changes reached.` | R |
| `sv_mvd_noblend` | `0` | L | Leave screen blends out. | R |
| `sv_mvd_nogun` | `0` | L | Leave the held weapon out. | R |
| `sv_mvd_nomsgs` | `1` | L | Leave text messages out. | R |
| `sv_mvd_begincmd` | `wait 50; putaway; wait 10; help;` | — | Commands the dummy client runs on entering. | R |
| `sv_mvd_scorecmd` | `putaway; wait 10; help;` | — | Run after 9 s without a scoreboard update. The dummy's `wait` lasts 100 ms. | R |
| `sv_mvd_autorecord` | `0` | L | Not documented. Lets `record <name>` and `stop` sent to the dummy client by the game start and stop a local recording to `demos/<name>.mvd2` (`Auto-recording local MVD to <path>`, `Stopped local MVD auto-recording.`). Forces `sv_mvd_suspend_time` to 0. | R |
| `sv_mvd_capture_flags` | `5` | — | Not documented. Bit 1 records only players fully in the game; bit 2 never records invisible ones; bit 4 always records visible ones. Spectators, frozen players and players without prediction are never recorded. | R |
| `sv_mvd_disconnect_time` | `15` | — | Minutes. | R |
| `sv_mvd_suspend_time` | `5` | — | Minutes; exact trigger not traced. | R |
| `sv_mvd_allow_stufftext` | `0` | L | Not documented. Above 0, text the game sends to the dummy to run is run. At 0 or above, the viewer string-command flag is removed. | R |
| `sv_mvd_spawn_dummy` | `1` | — | 0 never, 1 when the game supports recorder spectators, 2 always. Errors `No slot for dummy MVD client`, `Dummy MVD client rejected by game: <reason>`. | R |

### 1.14 Relay (new; the same program can act as a relay)

| Setting | Default | Effect | Mark |
| --- | --- | --- | --- |
| `mvd_shownet` | `0` | Debug print. | R |
| `mvd_timeout` | `90` | Seconds. | R |
| `mvd_suspend_time` | `5` | Minutes. | R |
| `mvd_wait_delay` | `20` | Seconds; exact effect not traced. | R |
| `mvd_wait_percent` | `50` | Per cent; exact effect not traced. | R |
| `mvd_buffer_size` | `8` | | R |
| `mvd_username` | `unnamed` | Name sent upstream. | R |
| `mvd_password` | `` | Upstream password. | R |
| `mvd_snaps` | `10` | | R |
| `mvd_admin_password` | `` | `!mvdadmin <password>` makes a relay spectator an admin. | R |
| `mvd_part_filter` | `0` | Join/part messages: 0 all shown, negative all hidden; admins always see them. | R |
| `flood_msgs`, `flood_persecond`, `flood_waitdelay` | `4`, `4`, `10` | Relay spectators' chat only. | R |
| `flood_mute` | `0` | 1: `[MVD] Spectators may not talk on this server.` | R |
| `mvd_filter_version` | `0` | Rewrites `!version` in chat to stop automatic replies. | R |
| `mvd_default_map` | `q2dm1` (L) | Map shown while waiting. | R |
| `mvd_stats_score` | `0` | | R |
| `mvd_stats_hack` | `0` | Bit mask of the dummy's status values copied to chasers. | R |
| `mvd_freeze_hack` | `1` | The dummy frozen means intermission. | R |
| `mvd_chase_prefix` | `xv 0 yb -64` | Layout prefix for the chase text. | R |

Relay spectator commands: `!mvdadmin`, `fwd`, `say`, `say_team`, `follow`/`chase`, `autofollow`/`autochase` (add, rm, set, list, on, off), `observe`/`spectate`/`obs`/`spec`, `inven`/`menu`, `invnext`, `invprev`, `invuse`, `help`/`score`, `oldscore(s)`/`lastscore(s)`, `putaway`, `channels`, `clients`/`players`, `join`, `leave`, `commands`. Their chat lines start `[MVD] <name>: `. (R)

### 1.15 Anticheat (build: anticheat; on in published builds)

| Setting | Default | Flags | Effect | Mark |
| --- | --- | --- | --- | --- |
| `sv_anticheat_required` | `0` | L | 0 off; 1 optional (required only from addresses on the `addacrequirement` list); 2 required for all. | R |
| `sv_anticheat_server_address` | the anticheat service's host | L | | R |
| `sv_anticheat_error_action` | `0` | — | When the service is down: 0 let players in, 1 refuse them. | R |
| `sv_anticheat_message` | text pointing to the anticheat website | — | Shown to refused players. | R |
| `sv_anticheat_badfile_action` | `0` | — | On a modified file: 0 kick, 1 tell the player, 2 tell everyone. | R |
| `sv_anticheat_badfile_message` | `` | — | | R |
| `sv_anticheat_badfile_max` | `0` | — | | R |
| `sv_anticheat_show_violation_reason` | `0` | — | | R |
| `sv_anticheat_client_disconnect_action` | `0` | — | 1 kicks a player whose anticheat disconnects. | R |
| `sv_anticheat_disable_play` | `0` | — | | R |

The service is pinged every 60 s with a 15 s timeout; reconnection waits 30 s, then 30 s longer each time. Messages include `ANTICHEAT VIOLATION: ...`, `<name> was kicked for anticheat violation` (with the reason in brackets when shown), `This server is unable to take new connections right now. Please try again later.`, `<name> is not using anticheat.`, `<name> is exempt from using anticheat.` (R)

---

## 2. Operator console commands

### Players

- **`kick <id>`**, **`kickban <id>`**. A number is a slot. Errors: `Bad client slot number: <n>`, `Client slot <n> is not active.`, `Userid '<id>' is not on the server.`; with partial matching on (`sv_enhanced_setplayer`), `No clients matching '<id>' found.` and `'<id>' matches multiple clients.`. A kick tells everyone `<name> was kicked`. `kickban` also bans the address, with mask /32 for IPv4 and /64 for IPv6. (R)
- **`status [d|l|p|s|t|v]`**. The default table's columns: number, score, ping, name, last message, address, rate, protocol, frames per second. The ping column shows `ZMBI` (being freed), `ASGN` (assigned), `DNLD` (downloading), `HTTP`, `CNCT` (connecting) or `PRIM` (being sent the map's data), otherwise the ping capped at 9999. The letters choose other views. A table of bots follows when there are bots, then the recorder's status. `No UDP clients.` when empty; `Usage: status [d|l|p|s|t|v]` on a bad letter. (R)
- **`dumpuser <id>`**: the user info, then the client's version, protocol major and minor, packet size, compression, channel kind, ping, move rate, update rate, round trip minimum/average/maximum, packet loss each way, time scale and connection time. (R)
- **`stuff <id> <text>`**, **`stuffall <text>`**: make clients run text. **`stuffcvar <id> <name>...`**: asks the client for settings; answers print as `<name>[<addr>]: "<var>" is "<value>"`. **`printall <text>`**: prints to everyone. (R)
- **`pickclient <addr:port>`**: asks a waiting client at that address to connect (sends it `passive_connect`). Errors `Single player server running.`, `Bad client address: <addr>`, `Please specify client port explicitly.` (R)
- **`say <text>`** on the dedicated server sends `console: <text>`. (R)

### Server and maps

- **`serverinfo`**, **`heartbeat`** (heartbeat at the next frame). (R)
- **`map <name> [force]`**. With `sv_allow_map` 0 and a game running, it is refused with `Using 'map' will cause full server restart. Use 'gamemap' for changing maps.` and a hint shown once, unless latched settings are waiting, `force` is given, a recycle is pending, or no game has started. `sv_allow_map 2` turns it into `gamemap`. Errors: `Refusing to process oversize level string.`, `Couldn't load <path>: <error>`. Name prefixes: `!` quits the server if the map is missing; `*` ends the unit; `$<spot>` names the spawn point; `+` chains the next level. (R)
- **`gamemap <name>`**: as stock. **`demomap`** exists on clients only. (R)
- **`dumpents <file>`**: writes the map's entity text to `entdumps/<file>.ent`. `No map loaded.`, `Dumped entity string to <path>`. (R)
- **`killserver`** (`Server was killed.`), **`sv <command>`** (passes to the game library; `No game loaded.`). (R)
- **`quit [reason]`** (default reason `Server quit`), **`recycle [reason]`** (as quit, but clients are told to reconnect instead of disconnect). (R)
- **`save`**, **`load`**, **`autosave`**: saves, refused in deathmatch (`Can't savegame in a deathmatch.`); `Game does not support enhanced savegames.`; `Game saved.` (R)
- **Removed from stock:** `serverrecord`, `listmaps`, `gamemode`. (R)

### Masters

- **`setmaster <addr>...`**: up to 8 masters, port 27900 when none is given. Sets `public` to 1 and forces a heartbeat. `Too many masters.`, `Couldn't resolve master: <addr>`, `Master server at <addr>.`. There is no built-in master. Names are resolved again at map changes: good ones every 24 h, failed ones every 3 h. (R)
- **`listmasters`**: number, name, last message, address; `There are no masters.` (R)

### Address lists

- **`addban` / `delban` / `listbans`** and **`addblackhole` / `delblackhole` / `listblackholes`**. An entry is `address[/mask] [comment]`. Delete by address, by number from 1, or `all`. A ban refuses connects; a blackhole ignores every packet from the address. Messages: `Entry <addr> already exists.`, `Address list is empty.`, `No such index: <n>`, `No such entry: <addr>`, `Please specify a mask after '/'.`, `Bad address: <addr>`, `Bad mask: <n> bits`. The listing's columns: number, address/mask, hits, last hit (day, month, hour:minute, or `never`), comment. (R)
- **`addgtvhost` / `delgtvhost` / `listgtvhosts`** (recorder viewers allowed without the password) and **`addgtvban` / `delgtvban` / `listgtvbans`** (viewers refused), same form. (R)
- **`addacexception` / `delacexception` / `listacexceptions`** and **`addacrequirement` / `delacrequirement` / `listacrequirements`** (anticheat builds), same form. (R)

### Commands sent on connect, and limited rcon

- **`addstuffcmd connect|begin <text>`**, **`delstuffcmd connect|begin <n|text|all>`**, **`liststuffcmds connect|begin`**: text clients run when they connect or enter the game. Messages `Unknown stuffcmd list: <name>`, `Stuffcmd already exists: <text>`, `No stuffcmds registered.`, and no-such-entry messages. (R)
- **`addlrconcmd <prefix>`**, **`dellrconcmd`**, **`listlrconcmds`**: commands allowed through `lrcon_password`. A refused command answers `This command is not permitted.`; the console logs `Limited rcon from <addr>:` and the command, or `Invalid limited rcon from <addr>:`. Limited rcon commands are not macro-expanded. (R)

### Filters on what clients do

- **`addfiltercmd <command> [ignore|log|print|stuff|kick] [comment]`**, **`delfiltercmd`**, **`listfiltercmds`**: a client command the server refuses. `print` and `stuff` take the text to show or run. The log line: `<name>[<addr>]: issued banned command: <command>`. (R)
- **`addcvarban <setting> <match> [log|print|stuff|kick] [comment]`**, **`delcvarban`** (`Removed <n> cvarban(s).`), **`listcvarbans`**, **`checkcvarbans`** (runs the check again; whether on every client was not traced): a client setting value the server will not accept, checked by asking the client. **`adduserinfoban` / `deluserinfoban` / `listuserinfobans`** do the same on user info keys (refusal at connect: question 5). (R)
- **Match forms**: a leading `!` negates; `*` any non-empty value; `=`, `<`, `>` compare as numbers; `~` contains; `#` takes the rest literally; anything else is an exact match. Text comparisons ignore case. The log line: `<name>[<addr>]: matched cvarban: "<var>" is "<value>"`. (R)

### Recorder and relay

- **`mvdrecord [-z] <name>`**: records to `demos/<name>.mvd2`, gzip-compressed with `-z`. Messages `No server running.`, `Missing filename argument.`, `MVD recording is disabled on this server.`, `Already recording a local MVD.`, `Recording local MVD to <path>`. **`mvdstop`**: `Not recording a local MVD.`, `Stopped local MVD recording.` **`mvdstuff <text>`**: runs text as the dummy client; `Can't 'mvdstuff', dummy MVD client is not active`. (R)
- Relay: **`mvdplay [-l <loops>] [-n <name>] [-r <channel>] <file>...`**, **`mvdconnect [-n <name>] [-u <user>] [-p <password>] <address>`**, **`mvdisconnect [-a]`**, **`mvdkill`**, **`mvdspawn`**, **`mvdchannels`**, **`mvdservers`**, **`mvdcontrol`**, **`mvdpause`**, **`mvdskip`**, **`mvdseek`**. (R)

### Anticheat (anticheat builds)

`svaclist`, `svacinfo`, `svacupdate`, `svacinvalidate`. (R)

### General console

`cmdlist`, `macrolist`, `exec`, `echo`, `alias`, `unalias`, `wait`, `text`, `complete`, `trigger`, `untrigger`, `if`; files: `path`, `fdir`, `dir`, `fs_stats`, `whereis`, `link`, `unlink`, `softlink`, `softunlink`, `fs_restart`; other: `z_stats`, `lasterror`, `bsplist`, `net_restart`, `net_stats`, `showip`, `dns`, `listsounds`. (R)

---

## 3. Features beyond single settings

### 3.1 Protocols and negotiation

- **Accepted protocols**: 34 (stock), 35, 36 and 38. The challenge answer advertises them: `challenge <n> p=34,35,36,38`. 26 to 33 are refused with `You need Quake 2 version 3.19 or higher.`, anything else with `Unsupported protocol version <n>.` (R)
- **Connect arguments**: protocol, qport, challenge, user info; for 35 and later then the largest packet the client takes (0 means 4086; below 0 or above 4086 refused with `Invalid maximum message length.`; capped by `net_maxmsglen` except on loopback; at least 512). Protocol 34 always gets 1390. Protocol 35 then sends a minor version, clamped 1903 to 1905. Protocol 36 sends the channel kind (0 or 1, default 1; another value `Invalid netchan type.`), compression support (default yes) and a minor clamped 1015 to 1026 (1016 becomes 1015). Protocol 38 is as 36 with the minor clamped 3011 to 3017. (R)
- **Extended limits** (the game library's `g_protocol_extensions`, question 4): `protocol` reads 36. Then only clients of protocol 36 with minor 1024 or later (1025 for the newer game interface) are admitted; others get `This is a protocol limit removing enhanced server.` then `Your client version is not compatible. Make sure you are running the latest Q2PRO client version.` (R) So protocols 34, 35 and 38 are all refused (D).
- **Qport**: protocol 34 reads a 16-bit qport at byte 8 of the packet; the others an 8-bit qport at byte 8, or match on the source port when the qport is 0. (R)
- **The server's data message** after the stock fields: for 35, a byte 0, a 16-bit minor, a byte 0, and a byte for the strafe-jump fix. For 36 and 38, a 16-bit minor, a byte server state, then for minor 1024 and later (36) or 3017 and later (38) a 16-bit flag word: bit 0 strafe-jump fix, bit 1 the alternative move (`sv_qwmod`), bit 2 water-jump fix, bit 3 extended limits, bit 4 second extension set; for older minors three separate bytes instead. (R)
- **Message numbers** the protocol defines (server to client): compressed packet 21, compressed download 22, game state 23, server setting 24, configstring stream 25, baseline stream 26, game HUD update 29, extended message 30, user statistic 31, setting sync 32. Client to server: client setting 5, move without delta 10, batched moves 11, user info change 12, setting sync reply 50. (R)
- **Server settings message** (to 36 and 38 clients): a 32-bit index then a 32-bit value. Indices: 0 player updates, 1 frame rate, 2 view height low, 3 view height high. (R)
- **Client settings message**: a 16-bit index then a 16-bit value. 0 no gun, 1 no blend, 2 recording, 3 player updates, 4 frame rate (0 highest), 10 no gibs (the server stops sending them), 11 no footsteps (the server clears footstep events), 12 no prediction, 13 no flares. (R)
- **Per packet limits**: 8 string commands, 8 user info updates, 32 moves per frame, 4 frames. A string command of 1024 bytes or more drops the client (`oversize stringcmd`). (R)

### 3.2 Compression

- A compressed packet is byte 21, a 16-bit compressed length, a 16-bit original length (both little-endian), then raw deflate data. Used when the client supports it and the message is at least half the largest packet. (R)
- Compressed downloads stream deflate data straight from compressed packs to clients of protocol 36 minor 1021 or later, or 38. (R)

### 3.3 Frame rate

See `sv_fps` (question 1.5). Clients may ask for fewer updates with the client setting messages. The rate check holds back a frame when the sum of the last 10 messages' sizes is over the client's rate (scaled by the frame divisor). (R)

### 3.4 Clients' version and forced reconnect

- At a client's first `new`, the server makes it reply with its `version` (and an anticheat token in anticheat builds). A client that has not replied by `begin` is dropped silently (`failed version probe`). The console logs `<name>[<addr>]: <version>`. (R)
- With `sv_force_reconnect` set, a client is given random settings and told to reconnect to that address; the values it brings back are checked, and a mismatch drops it silently (`failed to reconnect`). (R)

### 3.5 Commands the server handles for clients

`new`, `begin`, `nextserver`, `disconnect`, `info`, `sinfo`, `download`, `nextdl`, `stopdl`, `nogamedata` (stops frames being sent), `lag [player]`, `packetdup <n>`, `aclist`, `acinfo`, `cvarsync`. (R)

### 3.6 Drop reasons

- A client in the game is announced to all as `<name> was dropped: <reason>`; a reason starting with `?` reads `<name> <reason>`, one starting with `!` is not announced. (R)
- Reasons: outgoing sequence too big, timed out, connection reset by peer, too many nodelta frames, idling, time skew too high, multiple clc_move commands in packet, too many frames in packet, read past end of message, too many usercmds in frame, unknown command byte, empty userinfo, malformed userinfo, malformed name, oversize userinfo (key or value), oversize stringcmd, reconnected, disconnected, was kicked. (R)
- A client whose packets come back unreachable is dropped after `sv_ghostime`. A smaller path size found on the way is logged `Fixing up maxmsglen`. (R)

### 3.7 Downloads

Chunk size is the room left in a packet minus 4 bytes. No game frames are sent while a client downloads. A resumed download of a different size answers `File size differs from server.` and `Please delete the corresponding .tmp file from your system.` (R)

### 3.8 Names

An empty name answers `You can't have an empty name.`; too many changes, `You can't change your name too often.` (R)

### 3.9 Masters and heartbeats

Only a dedicated, public server sends them. The text is `heartbeat` and a newline followed by the status text. One master is sent per frame in turn; the round repeats every 300 s, and the first round goes 5 s after the game starts. `heartbeat` and `setmaster` cause one at the next frame. On shutdown a public server sends `shutdown` to each master. (R)

### 3.10 Rcon

Limited by `sv_rcon_limit` for bad attempts. A bad password answers `Bad rcon_password.` and the console logs `Invalid rcon from <addr>:` and the command (without the password); a good one logs `Rcon from <addr>:`. Works before any map. Full rcon commands are macro-expanded. (R)

### 3.11 Recorder viewers over TCP (relays)

- On the game port. Each side first sends the four bytes `MVD2`. Then each message is a 16-bit little-endian length and an operation byte; a viewer's message is at most 256 bytes. (R)
- The protocol word is 0xED04. The viewer's hello carries flags (1 deflate, 2 string commands), a 32-bit value, a name and a password. (R)
- Server operations 0 to 9: hello, pong, stream start, stream stop, stream data, error, bad request, no access, disconnect, reconnect. Viewer operations 0 to 4: hello, ping, stream start, stream stop, string command. (R)
- Access: the allow list first, then the ban list; with no password everyone else is allowed, otherwise the password must match. Console lines `Accepted MVD client`, `TCP client [<addr>] rejected: too many connections` or `no free slots`. (R)

### 3.12 Recording file

A multi-view recording starts with `MVD2`, then blocks of a 16-bit little-endian length and data, ending with a block of length 0. It may be gzip-compressed. (R)

### 3.13 Game HUD (protocol 38, minor 3012 and later)

Up to 200 elements per client (64 below minor 3014). Kinds: image, fill, text, number. Flags: in use, blink, 3D position, hidden. Sent as changes each frame, ending with byte 255. The field layout was not traced. (R; layout open)

### 3.14 Setting sync (protocol 38, minor 3013 and later)

The server sends the extended message (30) with sub-kind 32, a count byte, then name and value strings, at most 32 entries. The client answers with message 50, an index byte and a value of at most 63 characters. The fork's library syncs `cl_antilag 1`, `cl_indicators 1`, `cl_xerp 0`, `cl_spectatorhud 1`, `cl_spectatorkillfeed 0`. The client command `cvarsync` answers `cvarsync: set <name> to <value>`. (R)

### 3.15 Map overrides

- **Entity file**, used whenever `map_override_path` is set: first `<path>/<map>@<crc>.ent`, where the crc is the 4-digit hexadecimal CRC-16 of the map's own entity text without its last byte, then `<path>/<map>.ent`. It must be at least 2 bytes. (R)
- **Override file**, used when `sv_load_ent` is 1: `<path>/<map>.bsp.override`. It begins with a 32-bit set of bits (1 name, 2 checksum, 4 entities); then, if the name bit is set, a 64-byte name padded with zero bytes; if checksum, a 32-bit checksum; if entities, a 32-bit length then the text. On success `Loaded <path>`. (R)

### 3.16 Anti-lag

The engine has no anti-lag of its own. The game library rewinds hit tests (question 4). (R)

### 3.17 Status answers and filters

See question 5 for the answers, and question 2 for bans, blackholes, command filters and setting bans. (R)

---

## 4. The game library

- **One library ships**: the Action Quake 2 "TNG" library (it reports `TNG 2.83 git`) with two kinds of bots, and an optional web-request module. A library derived from the stock deathmatch and coop rules is in the source tree but is not built; it registers the stock set plus `g_protocol_extensions` (0, latched) and settings for a store's user ids. (R)
- **It forces `deathmatch` to 1.** It does not register `spectator_password`, `maxspectators`, `sv_maplist`, `skill`, `flood_msgs`, `flood_persecond`, `flood_waitdelay` or the `coop_*` group. Chat flood control is `flood_threshold` (4): `You can't talk for <n> seconds.` (R)
- **`password`** is checked, with `none` meaning none: `Password required or incorrect.` Its address filter refuses with `Banned.` or `Temporary banned for <n> games.` (R)
- **Defaults it gives that the engine ignores** (the engine's registration wins, question 1.1): `hostname unnamed`, `dmflags 0`. (R)
- **Votes** come from `action.ini` (setting `ininame`), defaults: map vote minimum 1, need 0, pass 51 %; kick vote minimum 4, need 0, pass 75 %, temporary ban 1; settings vote minimum 1, need 0, pass 51 %; scramble vote minimum 4, need 0, pass 75 %. (R)
- **Anti-lag** (`sv_antilag 1`): hit tests are rewound by the shooter's ping, at most 250 ms, over a history of 64 frames. `sv_antilag_interp` adds one frame. A player may opt out. (R)
- **Server commands** (`sv <command>`): `addip`, `removeip`, `listip`, `writeip`, `addsb`, `removesb`, `listsb`, `writesb`, `nextmap`, `reloadmotd`, `stuffcmd`, `ircraw`, `softmap`, `map_restart`, `resetscores`, `t1score` to `t3score`, `t1name` to `t3name`, `t1skin` to `t3skin`, `t1skin_index` to `t3skin_index`, `softquit`, `slap`, `scramble`, `botdebug`, `shownodes`, `addbot`, `addbots`, `removebot`, `savenodes`, `initnodes`, `loadnodes`, `bots`. Unknown: `Unknown server command "<cmd>"`. (R)

### 4.1 Its settings (name, default, flags)

All new relative to the stock library unless the note says otherwise. Effects are those the project's documentation states or the name makes plain; most effects were not traced (so this question is partly answered). (R for names, defaults and flags)

**Shared with stock (changed or same):** `deathmatch 1` L (changed: forced); `coop 0` L, X; `maxclients 8` SI, L; `maxentities 1024` L (same); `dmflags 0` SI; `fraglimit 0` SI; `timelimit 0` SI; `cheats 0` SI, L; `password` UI; `needpass 0` SI (bits as stock without the spectator bit); `hostname unnamed` SI; `gamename`, `gamedate` SI, NS; `sv_rollspeed 200`, `sv_rollangle 2`, `sv_maxvelocity 2000`, `sv_gravity 800` (same); `gun_x`, `gun_y`, `gun_z 0`; `run_pitch 0.002`, `run_roll 0.005`, `bob_up 0.005`, `bob_pitch 0.002`, `bob_roll 0.002` (same); `g_select_empty 0` A (same).

**Identity and reports:** `actionversion` (`TNG <version>`) SI, NS; `maptime 0:00` SI, NS; `t1`, `t2`, `t3 0` SI, NS (team scores); `gm dm` SI (mode name); `gmf 0` SI (mode flags); `server_id`; `stat_logs 0` SI; `stat_apikey none`; `stat_url` (the project's statistics address); `motd_time 2`; `skipmotd 0`; `printrules 0`; `timedmsgs 0`; `msgflags 0`; `g_highscores_dir highscores`; `g_highscores_countbots 0`; `stats_endmap 1`; `stats_afterround 0`; `use_killcounts 0` X.

**Modes:** `teamplay 0` L, X; `ctf 0` L, X; `ctf_mode 0` L, X; `ctf_forcejoin`; `ctf_dropflag 1`; `ctf_respawn 4`; `ctf_model male` L; `ctf_dyn_respawn 0`; `ctf_rewards 1` X; `capturelimit 0` SI; `dom 0` L, X; `esp 0` L, X; `esp_atl 0`; `esp_punish 0`; `esp_etv_halftime 0` L; `esp_showleader 1`; `esp_showtarget 1`; `esp_leaderequip 1`; `esp_leaderenhance 0`; `esp_enhancedslippers 0`; `esp_matchmode 0`; `esp_respawn_uvtime 10`; `esp_debug 0`; `teamdm 0` L, X; `teamdm_respawn 2`; `use_tourney 0` L, X; `tourney_lca 0`; `use_3teams 0` L, X; `jump 0` L, X; `training_mode 0` L, X; `darkmatch 0` L; `day_cycle 10`; `dm_choose 0` L, X; `dm_shield 0` X; `use_classic 0`.

**Rounds and teams:** `roundlimit 0` SI; `roundtimelimit 0` SI; `round_begin 15` X; `limchasecam 0` L, X; `maxteamkills 0`; `twbanrounds 2`; `tkbanrounds 2`; `punishkills 1`; `ff_afterround 1`; `eventeams 0` X; `use_balancer 0` X; `auto_join 0`; `auto_equip 0`; `auto_menu 0`; `force_skin`; `uvtime 40`; `spectator_hud 1` L, X; `use_ghosts 0` L, X; `use_oldspawns 0` L; `respawn_effect 0`; `wave_time 5`.

**Match mode:** `matchmode 0` SI, L; `mm_forceteamtalk 0`; `mm_adminpwd 0`; `mm_allowlock 1` L; `mm_allowcount 3` L; `mm_pausetime 2` L; `mm_timeoutcount 2` L; `mm_timeouttime 60` L; `mm_captain_teamname 0` X; `mm_carryover 0` L; `use_forfeit 0`; `forfeit_abandon_time 60`; `warmup 0` L, X; `warmup_bots 0` L; `warmup_unready 0`.

**Weapons and items:** `weapons 1` SI, L; `items 1` SI, L; `wp_flags 511`; `itm_flags 63`; `dmweapon` (the pistol); `allweapon 0` X; `allitem 0` X; `allow_hoarding 0` L; `item_respawnmode 0` L; `item_respawn 59`; `weapon_respawn 74`; `ammo_respawn 30`; `g_spawn_items 0` L; `item_kit_mode 0` L, X; `knifelimit 40`; `knife_catch 0` X; `tgren 0` SI; `use_gren_bonk 0` X; `grenade_drop 0` X; `lca_grenade 0` X; `flashgren 1`; `flashradius 300`; `flashtime 100`; `hc_single 1` L, X; `hc_boost 0` L, X; `hc_boost_percent 100`; `hc_silencer 0`; `gun_dualmk23_enhance 0` X; `use_buggy_bandolier 0`; `use_grapple 0` X; `ir 1`; `new_irvision 0`; `use_newirvision 1` X; `use_flashlight 0`; `zoom_comp 1` X; `use_punch 1` X; `medkit_drop 0`; `medkit_time 30`; `medkit_instant 0`; `medkit_max 3`; `medkit_value 25`; `use_pickup 0`.

**Body and world:** `sv_gib 1`; `sv_killgib 0` X; `sv_limp_highping 70` X; `slopefix 1`; `silentwalk 0`; `llsound 1` X; `loud_guns 0`; `sync_guns 1`; `shelloff 1`; `shelllimit 30`; `shelllife 1.2`; `bholelimit 0`; `bholelife 20`; `splatlimit 0`; `splatlife 25`; `breakableglass 0` X; `glassfragmentlimit 30`; `use_buggy_ent_hitbox 1`.

**Lag and prediction:** `sv_antilag 1` SI; `sv_antilag_interp 0` SI; `use_xerp 1` X; `force_cl_xerp 0` X; `use_indicators 1` X.

**HUD and score:** `nohud 0` L; `hud_team_icon 0`; `hud_items_cycle 20`; `noscore 0` L; `hud_noscore 0` L; `use_newscore 0`; `scoreboard`; `use_rewards 1`; `use_warnings 1`.

**Chat and radio:** `use_voice 1`; `radiolog 0`; `radio_max 3`; `radio_time 2`; `radio_ban 15`; `radio_repeat_time 1`; `radio_repeat 2`; `hearall 0`; `deadtalk 0`; `flood_threshold 4`; `filterban 1`; `silenceban 1`; `ppl_idletime 15`; `sv_idleremove 0` X; `sv_crlf 0` L.

**Votes and rotation:** `use_kickvote 1`; `use_mapvote 1`; `use_scramblevote 1` (a second registration defaults it to 0; the first wins, question 1.1); `use_cvote 0`; `mv_public 0`; `vk_public 0`; `mapvote_waittime 8`; `mapvote_next 1`; `mapvote_next_limit 0`; `scramblevote_min 4`; `scramblevote_need 0`; `scramblevote_pass 75`; `actionmaps 1`; `vrot 0`; `rrot 0`; `empty_rotate 0`; `empty_exec`; `ininame action.ini`; `maplistname maplist.ini`; `configlistname configlist.ini`; `motdname motd.txt`; `use_randoms 0` SI, L.

**Limited rcon (the library's own):** `lrcon_config lrcon.cfg`; `lrcon_claimer_name`; `lrcon_claimer_ip`.

**Recording:** `use_mvd2 0` X (the documentation: 1 records in match mode, 2 in every mode).

**Client checks:** `check_time 3`; `video_check 0`; `video_max_3dfx 1.5`; `video_max_3dfxam 1.5`; `video_max_opengl 3.0`; `video_force_restart 0` L; `video_check_lockpvs 0`; `video_check_glclear 0`; `video_checktime 15`; `gl_shaders 0`.

**Protocol:** `g_protocol_extensions 0` L, X (question 3.1); `sv_fps` SI (mirrors the engine's); `sv_features`.

**Web and chat bridges:** `sv_curl_enable 0`; `sv_discord_announce_enable 0`; `sv_curl_stat_enable 0`; `sv_aws_access_key disabled`; `sv_aws_secret_key disabled`; `sv_curl_discord_info_url disabled`; `sv_curl_discord_pickup_url disabled`; `server_ip`; `server_port`; `sv_last_announce_time 0`; `sv_last_announce_interval 1800`; `server_announce_url disabled`; `cl_discord 0`; `cl_discord_id 0`; `cl_discord_discriminator 0`; `cl_discord_username`; `cl_discord_avatar 0`; `ircserver`, `ircport 6667`, `ircchannel`, `ircuser`, `ircpasswd`, `irctopic` (build-time defaults); `ircbot 0`; `ircstatus`; `ircop`; `ircmlevel 6`; `ircdebug 0`; `ircadminpwd off`.

**Bots:** `bots 0` SI; `botdir bots`; `ltk_skill 5`; `ltk_jumpy 1`; `ltk_showpath 0`; `ltk_chat 1`; `ltk_routing 0`; `ltk_botfile botdata`; `ltk_loadbots 1`; `bot_enable 0` L, X; `bot_skill 7`; `bot_skill_threshold 0`; `bot_remember 15`; `bot_reaction 0.5`; `bot_showpath 0`; `bot_maxteam 0`; `bot_playercount 0` X; `bot_rush 0`; `bot_randvoice 5`; `bot_randskill 10`; `bot_randname 1`; `bot_chat 0`; `bot_personality 0` L; `bot_ragequit 0`; `bot_countashuman 0` X; `bot_debug 0`; `bot_count_min 0`; `bot_count_max 0`; `bot_rotate 0`; `bot_reportasclient 0` L, X; `bot_reportpings 0` X; `bot_navautogen 0`; `bot_randteamskin 0`; `warmup_bots 0` L.

**Other:** `steamid 0` NS; `net_port 27910` NS (the library reads it; the engine's registration has no no-set flag, question 1.1); `dedicated` NS; `use_rewards`, `use_warnings` above.

---

## 5. Server info keys and out-of-band answers

### 5.1 Server info keys

- **Added by the engine:** `port` (the `net_port` value), `protocol` (34 or 36), `anticheat` (when the service is ready, anticheat builds), `uptime` (only in `status` and heartbeat answers, with `sv_uptime`). (R)
- **Removed:** `coop` is no longer server info. (R)
- **Added by the game library:** `actionversion`, `maptime`, `capturelimit`, `roundlimit`, `roundtimelimit`, `use_randoms`, `matchmode`, `weapons`, `items`, `tgren`, `t1`, `t2`, `t3`, `stat_logs`, `sv_antilag`, `sv_antilag_interp`, `gm`, `gmf`, `bots`, `sv_fps`, `needpass`, `gamename`, `gamedate`, plus `fraglimit`, `timelimit`, `dmflags`, `maxclients`, `cheats`, `hostname`. (R)
- In `status` answers, `maxclients` is shown minus the reserved slots. (R)

### 5.2 Out-of-band answers

- Packets from blackholed addresses are ignored, all kinds. Before a game starts only `rcon` is answered. Words recognised: `ping`, `ack`, `status`, `statusx`, `rulesx`, `info`, `getchallenge`, `connect`, `rcon`. (R)
- **`ping`** answers `ack`. (R)
- **`info <version>`**: ignored when `maxclients` is 1; answered only for versions 34 to 38. The answer is `info` and a newline, then the host name right-aligned in 16 characters, the map in 8, then players/slots with the reserved slots subtracted. (R)
- **`status`**: subject to `sv_status_show` and `sv_status_limit`. The answer is `print` and a newline, the server info (with the optional `uptime`), a newline, then one line per player `<frags> <ping> "<name>"`, bots included. At most 1384 bytes. (R)
- **`statusx [n]`** (with `sv_status_ext 1`): the data are `[SERVER_INFO]`, then lines `hostname\<v>`, `mapname\<v>`, `clients\<v>`, `bots\<v>`, `maxclients\<v>`, `gamename\<v>`, `fraglimit\<v>`, `timelimit\<v>`, `dmflags\<v>`, `teamplay\<v>`; then `[PLAYERS]` and a line per player (bots marked ` (BOT)`); then `[END]`. Cut into chunks of 1200 bytes, at most 32, kept for 5 s. `n` of -1 answers `statusxmeta <chunks> <total bytes>`; otherwise `statusxdata <n>/<count> <size>`, a newline and the chunk. `n` defaults to 0; out of range gets no answer. (R)
- **`rulesx [n]`**: data `[EXTENDED_RULES]`, then `name\value` for every setting marked for the extended rules (X in questions 1 and 4), then `[END]`; answered as `rulesxmeta` and `rulesxdata` in the same way. The project's documentation calls it `rulesext`; the source answers `rulesx`. Both extended queries share the `status` limit. (R)
- **`getchallenge`**: 1024 challenges are kept, the oldest replaced. The value is a random 31-bit number, used once (loopback needs none). Answer `challenge <n> p=34,35,36,38`. (R)
- **`connect` refusals**, each sent as `print` and a newline then the text:
  - `Unsupported protocol version <n>.`; `You need Quake 2 version 3.19 or higher.`
  - `Bad challenge.`; `No challenge for address.`
  - Ban: the ban's comment, or `Your IP address is banned from this server.`, then a newline and `Connection refused.`
  - `Server is locked.`
  - `Too many connections from your IP address.`; `Too many connections from your IPv6 network.`
  - `Invalid maximum message length.`; `Invalid netchan type.`
  - The extended-limits refusal (question 3.1).
  - `Empty userinfo string.`; `Malformed userinfo string.`; `Please set your name before connecting.`; `Oversize userinfo string.`
  - `Please set your password before connecting.`; `Invalid password.`
  - User info ban: the comment, or `Userinfo banned.`, then a newline and `Connection refused.`
  - `Server and reserved slots are full.`; `Server is full.`
  - Redirect: the client is sent a pretend `client_connect`, then `Server is full. Redirecting you to <addr>...` and told to run `connect <addr>`.
  - Game library refusal: its reason, a newline and `Connection refused.`, or `Connection refused.` alone. (R)
- **Accepted**: `client_connect`, then ` nc=0` or ` nc=1` (36 and 38 only), ` ac=1` (anticheat), ` dlserver=<url>`, ` map=<map>`. (R)
- **User info the server rewrites**: the name is cut to 15 characters with the high bit of each character cleared. Unless the game library asks for the extra user info (this one does not): the `ip` key is forced to the client's address; an IPv6 client's is shown as `198.51.100.<n>:<port>`, `n` folded from its /48 network; `mvdspec` is removed; and `password` is removed when `sv_password` or the reserved password is set. (R)
- **`rcon`**: question 3.10. (R)

---

## 6. What a public deathmatch operator would typically set (judgement)

All D: chosen from what the settings do, not read as advice in the source.

- **Identity and reach:** `hostname`, `net_port` (and `net_ip` on a multi-address host), `public 1` with `setmaster <master>` (there is no built-in master), `sv_uptime 1` and `sv_status_ext 1` for server browsers.
- **Rules:** `game action` (or the mod folder), `maxclients`, `timelimit`, `fraglimit`, `dmflags`, and the library's mode settings (`teamplay`, `ctf`, `dom`, `matchmode`, `roundlimit`, `roundtimelimit`); a map rotation through `action.ini`.
- **Admin:** `rcon_password`; `lrcon_password` with `addlrconcmd` lines for trusted helpers; `sv_reserved_slots` with `sv_reserved_password`; `sv_password` only for a private game; `sv_allow_map 0` (the default) so admins use `gamemap`.
- **Speed and rates:** `sv_fps` (in builds that have it; 40 or 60 are common, meaning 25 ms or 16 ms frames), `sv_min_rate` and `sv_max_rate` raised for modern connections, `sv_antilag 1`.
- **Downloads:** `allow_download 1` and `sv_downloadserver` pointing at an HTTP copy of the content.
- **Abuse:** `sv_iplimit`, `sv_idlekick`, `sv_namechange_limit`, `sv_timescale_warn` and `sv_timescale_kick` (for example 1.05 and 1.15), the ban and blackhole lists, `addfiltercmd` and `addcvarban` rules, `sv_show_name_changes 1`, and `sv_anticheat_required` in builds that have it.
- **Records:** `logfile 2` with `logfile_name`, and `sv_mvd_enable` with `sv_mvd_autorecord` or `use_mvd2` for match recordings.
