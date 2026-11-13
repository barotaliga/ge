# Server inventory: every setting the classic server program registers

WORKING AREA: deleted when porting ends. A clean-room description, in our words, of every setting (console variable) the classic server program (MVDSV) registers, the server info keys it sets itself, and the commands that set setting-like values. Written from its source by a reader session; the building session reads only this. No code, no internal identifiers.

Read from MVDSV at commit `4e63928` (github.com/QW-Group/mvdsv, default branch, committed 2026-11-11). Nothing here was measured by running the program: every fact is marked **R** (read directly) or **D** (derived by combining what was read). Where the source does not settle a point, the text says so.

Terms used throughout:
- **Setting** is a console variable an admin types (`name value`) or puts in `server.cfg`. **Server info** is the key/value string every client and server browser receives. **Read-only** means the admin cannot change it; only the program writes it.
- **Latched at map load** means the program reads the value only when a map starts, so a change takes effect on the next `map`.
- **Game started** (used by several settings) is true when a demo is being recorded to a file, or when the server info key `status` is anything other than `Standby` (compared without case). A server whose mod never sets `status` therefore always counts as "game started" (D).
- Sizes: KiB = 1024 bytes. Rates are bytes per second.

---

## How settings behave in general (applies to every row)

- Names are matched without regard to case (`sv_demoDir`, `sv_demodir` and `SV_DEMODIR` are the same setting). (R)
- Typing a setting's name alone prints its current value. Typing it with arguments joins all arguments with single spaces and sets that. (R)
- There are only three flags: **server info** (mirrored into the server info string), **read-only**, and **user-created** (made by `set` for a name nothing registered). There is no "archive", "latched" or "hidden" flag; nothing is saved back to a config file automatically. (R)
- A server info setting set to exactly `0` is stored as the empty string, so it reads back as empty and its key disappears from the server info. This applies to `deathmatch` too: `deathmatch 0` removes the key (the program has a special case meant to keep `deathmatch 0`, but the empty substitution happens first, so the special case never sees `0`). (R, the consequence D)
- When a server info setting changes while a map is running, the new key and value are sent at once to every connected client. Keys are limited by a 512-character server info string. (R)
- Startup order: built-in defaults, then `server.cfg` is executed, then the command-line `+commands`, then the first map. Just before `server.cfg` runs, every non-read-only setting is set again to its own value so that any setting with a side effect (a server info key, a directory created, a check) applies its default. (R)
- `cvarlist` (also `cvardump`) lists settings sorted by name, with an `s` in front of server info settings; an optional glob pattern filters by name. (R)
- A mod can create new settings at run time (see question 4). (R)

## Which builds register what

The Linux dedicated server as built by the project's own build files registers **191 settings** when the HTTP library (curl) is found at build time, and 187 without it. (D, from counting registrations in that configuration.)

| Present when | Settings |
| --- | --- |
| Every build | all rows below except those named in this table |
| Unix builds only (Linux, BSD, macOS) | `sys_extrasleep` |
| Windows builds only | `sys_sleep` (not counted in the 191) |
| Only when the build found curl ("web integration") | `sv_www_address`, `sv_www_authkey`, `sv_www_checkin_period`, `sv_login_web` |
| Defined in the shared source but **never registered by the dedicated server** | `qport` (a client-only setting), `sv_highchars` (registration is commented out) |

All protocol-extension settings (`sv_bigcoords`, `pext_ezquake_verfortrans`, `sv_csqc_progname`, `sv_csqcdebug`, `sv_downloadchunksperframe`, `sv_voip*`, `sv_debug_weapons`, `sv_pext_mvdsv_serversideweapon`) are compiled in by default in every build of this commit. (R)

---

## Question 1: every setting, by subsystem

Column "Flags": **SI** = server info, **RO** = read-only, **—** = none. "Src" is R or D.

### 1.1 Network and connections

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `timeout` | `65` | — | Seconds without any packet from a connected client before it is dropped; everyone sees `<name> timed out`. Bots are never timed out. | R |
| `zombietime` | `2` | — | Seconds a slot stays reserved after a client disconnects normally (so the last reliable messages can be resent) before it can be reused. | R |
| `sv_reconnectlimit` | `0` | — | Seconds. A new connection from the same address (and same port or same client port id) as an existing slot is refused if that slot has been connected for less than this; the server console prints `<address>:reconnect rejected: too soon`. 0 disables the check. | R |
| `maxclients` | `24` | SI | Player slots. Values above 32 are cut to 32. Players + spectators + VIP spectators may not exceed 32: when they would, `maxspectators` is reduced first, then `maxvip_spectators`. The check runs on every connection attempt. | R |
| `maxspectators` | `8` | SI | Spectator slots, cut as above. | R |
| `maxvip_spectators` | `0` | — | Extra spectator slots usable only by VIP spectators (see 1.5). Not in server info. | R |
| `sv_forcespec_onfull` | `2` | — | What happens when a player tries to join a full server but spectator slots are free. 0: refused (`server is full`). 1: the player is let in as a spectator unless their userinfo `svf` has bit 2 set. 2: let in as a spectator only if their userinfo `svf` has bit 1 set. In both 1 and 2 the spectator password must be empty or `none` (or the client must be a VIP); the client sees `server is full: connecting as spectator`. | R |
| `sv_getrealip` | `1` | — | Real-address check. 0 off. 1: while connecting, the client is told to send a small packet carrying a random number straight to the server's address, so the server learns the client's real address behind a proxy; up to 10 tries, and after 3 seconds or 10 tries the client is let in anyway. 2: as 1, but a client that never proves its address is told `Failed to validate client's IP.` and dropped. When the server's own address and the client's address are on different kinds of network (one private, one public) and `sv_serverip` is empty, the check is skipped with a console warning. | R |
| `sv_serverip` | `` (empty) | — | The address (`ip` or `ip:port`) clients are told to send the real-address packet to. Empty means the server's own detected address. | R |
| `sv_local_addr` | detected | RO | The server's own UDP address as detected at startup, or empty if no socket could be opened. | R |
| `sv_maxping` | `0` | — | Milliseconds. A player whose ping is above this when entering the game, or when typing `join`, is told their ping is too high and is made a spectator. Once a client has passed the check it is not checked again. 0 disables. | R |
| `sv_minping` | `0` | — | Milliseconds. The server delays its handling of each player's packets so that their ping rises to at least this value: the delay grows or shrinks by 1 ms per packet and is bounded to 0..1 s. Spectators and a paused game are not delayed. | R |
| `sv_enable_cmd_minping` | `1` | — | 1 lets players change `sv_minping` with the client command `minping <0..300>` while no game has started. 0 refuses that command. | R |
| `sv_mapcheck` | `1` | — | 1: a client whose map checksum matches neither of the server's two checksums for the map is told `Map model file does not match ...` and dropped. 0: not checked. | R |
| `showpackets` | `0` | — | Nonzero prints a console line for every packet sent and received (sequence numbers and sizes). Diagnostic. | R |
| `showdrop` | `0` | — | Nonzero prints a console line for every out-of-order or dropped packet. Diagnostic. | R |
| `sv_pext_replacementdeltas` | `0` | — | 1 advertises the "replacement deltas" entity encoding to every connecting client. 0 advertises it only when the current map's entity capacity is above 2048 (where it is required). Takes effect for new connections; existing connections keep their encoding. A client that accepts replacement deltas but not the extended temporary effects is told so and dropped. | R |
| `sv_bigcoords` | `` (empty) | SI | Latched at map load. Nonzero: coordinates go on the wire as 4-byte floats and angles as 2 bytes, for everyone. Clients without the float-coordinate extension are told to enable it or upgrade; players among them are dropped, and spectators get no further into the connection (a `[ServeMe]` spectator excepted). Packed nail updates are not used under it. Empty or 0: classic 2-byte coordinates and 1-byte angles. | R |
| `pext_ezquake_verfortrans` | `7814` | — | A client identifying itself as ezQuake with a build number below this has its transparency (alpha) extension switched off, and is warned. Build numbers that do not parse also switch it off. | R |
| `sv_pext_mvdsv_serversideweapon` | `1` | — | Latched at map load. With 1, server-side weapon selection is offered to clients, but only when the mod reports a name containing `KTX` in `qwm_name`. 0 never offers it. | R |
| `sv_extlimits` | `2` | — | Latched at map load. Caps the map's entity capacity: 0 always caps at 512; 1 never caps; 2 caps at 512 unless the map is in the extended BSP format (`sv_bspversion` 2). | R |
| `sv_maxedicts` | `2048` | — | Latched at map load. Entity capacity for the next map, an integer from 512 to 32768; any other value is refused with a message. The final capacity is the smallest of this, the mod's own limit (for a compiled module; 512 for an old module interface) and the `sv_extlimits` cap. A capacity above 2048 requires the replacement-delta encoding. | R |
| `sv_nailhack` | `1` | — | Nonzero: nails and super-nails are sent as ordinary entities. 0: they are sent in the compact "packed nails" message (never under `sv_bigcoords`). Note the sense: the default 1 turns the compact form off. | R |
| `sv_serveme_fix` | `1` | RO | Always 1 (cannot be changed). A spectator named exactly `[ServeMe]` is not sent broadcasts, entities or player information. | R |

### 1.2 Rates, downloads and uploads

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `sv_maxrate` | `0` | — | Cap on each client's userinfo `rate`. 0 = no cap. Whatever the cap, a rate is raised to at least 500 and lowered to at most 300000. A client with no `rate` key is treated as asking for 99999; a `rate` of 0 means 2500. Changing the setting re-applies to all connected clients within one frame. | R |
| `sv_maxdownloadrate` | `0` | — | Cap on the userinfo `drate` used while a client downloads. 0 means downloads are capped by `sv_maxrate` instead. Same 500..300000 bounds. | R |
| `allow_download` | `1` | — | Master switch for downloads. 0 refuses all (except to a client that used `techlogin`). | R |
| `allow_download_skins` | `1` | — | Allows files under `skins/`. | R |
| `allow_download_models` | `1` | — | Allows files under `progs/`. | R |
| `allow_download_sounds` | `1` | — | Allows files under `sound/`. | R |
| `allow_download_maps` | `1` | — | Allows files under `maps/`. | R |
| `allow_download_pakmaps` | `1` | — | With 0, a map that lives inside a pack archive is refused even when `allow_download_maps` is 1. | R |
| `allow_download_demos` | `1` | — | Allows `demos/<name>` (looked for in `sv_demoDir`, then `sv_demoDirAlt`) and `demonum/<n>` (a recent demo by number; a negative number or a string of N dots means the N-th most recent). | R |
| `allow_download_other` | `0` | — | Allows any other path that has a directory part. | R |
| `download_map_url` | `` | — | When non-empty, a client starting a map download is shown `Download this map faster:` followed by this text with the map's file name (for example `dm2.bsp`) appended. | R |
| `sv_downloadchunksperframe` | `30` | — | For clients using chunked downloads: most 1024-byte chunks sent to one client per server frame, bounded to 1..30. | R |
| `sv_maxuploadsize` | `1048576` | — | Bytes. Upper size of a screenshot a client uploads after an admin's `snap`; a larger upload is cancelled. Other uploads are not limited by it. | R |

Download rules that no setting changes (R): absolute paths, paths with `..`, paths starting with `.`, file names ending in `.log`, and files with no directory part are always refused; `csprogs.dat` is always allowed when `allow_download` is on.

### 1.3 Physics and movement

The ten movement numbers (`sv_gravity` to `sv_waterfriction`) are sent to every client when it connects, for client-side prediction; a change reaches clients only on their next connection or map change (D).

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `sv_gravity` | `800` | — | Downward acceleration, units/s². | R |
| `sv_stopspeed` | `100` | — | Below this speed, friction is computed as if the player moved at this speed, so slow movement stops quickly. | R |
| `sv_maxspeed` | `320` | — | Player wish-speed cap, units/s; also each client's starting personal maximum when it spawns. | R |
| `sv_spectatormaxspeed` | `500` | — | Spectator free-fly speed cap. | R |
| `sv_accelerate` | `10` | — | Ground acceleration factor. The server's player move also uses this value, not `sv_airaccelerate`, for acceleration in the air. | R |
| `sv_airaccelerate` | `10` | — | Sent to clients with the other movement numbers; the server's own player move does not read it. | R |
| `sv_wateraccelerate` | `10` | — | Water acceleration factor. | R |
| `sv_friction` | `4` | — | Ground friction factor. | R |
| `sv_waterfriction` | `4` | — | Water friction factor. | R |
| `sv_maxvelocity` | `2000` | — | Entities moved by the server's own physics (not the player move) have their total speed (the length of the velocity, not each axis) scaled down to this. | R |
| `sv_mintic` | `0.013` | — | Seconds. The server's entity physics does not run a step until at least this much game time has passed. | R |
| `sv_maxtic` | `0.1` | — | Seconds. Longest step entity physics takes; longer gaps are clamped. | R |
| `maxfps` | `77` | SI | Packets per second clients may send (clients read the key). The server uses it as the step rate for bots and for the anti-lag prediction offset; values outside 20..1000 are treated as 77 for those uses. | R |
| `sv_maxpitch` | `80` | — | Highest look-down angle, bounded to 0..89.9. Published as the server info key `maxpitch` only when it differs from 80. | R |
| `sv_minpitch` | `-70` | — | Highest look-up angle, bounded to -89.9..0. Published as `minpitch` only when it differs from -70. | R |
| `pm_ktjump` | `1` | SI | Jump boost: on a jump the upward speed below 270 is blended toward 270 by this fraction (values above 1 act as 1). 0 or empty: off. | R |
| `pm_bunnyspeedcap` | `` | SI | When above 0: air acceleration may not raise horizontal speed above `maxspeed × this` (speed already above it is kept, not cut). Empty or 0: no cap. | R |
| `pm_slidefix` | `` | SI | Nonzero: on the ground, upward speed is kept down to at most 0 and gravity is applied, so players slide down slopes smoothly. Empty: classic behaviour (vertical speed zeroed). | R |
| `pm_airstep` | `` | SI | Nonzero: a player in the air can step up ledges as on the ground. Setting it also sets `pm_pground` to match. Players can change it with the client command `airstep 0|1` while no game has started (spectators cannot); the change is announced to everyone. | R |
| `pm_pground` | `` | SI, RO | Follows `pm_airstep` (1 when it is on, empty otherwise). Changes how "on ground" is decided between moves. | R |
| `pm_rampjump` | `` | SI | Nonzero: changes jumping off ramps (the jump is taken from the ramp's surface). Reset to 0 at every map load, then set from a value stored in the map file's physics-normals data if the map has it. | R |
| `sv_safestrafe` | `0` | SI | N > 0: when a player reverses sideways movement direction (or starts moving sideways after stopping too briefly), their sideways input is forced to zero for N movement commands in total. Counters the instant direction flips of certain keyboards. Spectators are not affected. | R |
| `sv_antilag` | `` | SI | 0 or empty off. 1: the server records each player's last 128 positions and can test hits against where players were when the shooter saw them; only traces the mod asks to be rewound use it. 2: every line trace the mod makes is rewound, and projectiles too when `sv_antilag_projectiles` is set. | R |
| `sv_antilag_no_pred` | `` | SI | Nonzero: rewinds to exactly the frame the client saw. Empty: rewinds a little further, by one packet interval at `maxfps` or 20 ms, whichever is smaller. | R |
| `sv_antilag_projectiles` | `` | SI | With `sv_antilag 2`, nonzero also moves projectiles against rewound player positions. | R |
| `sv_speedcheck` | `1` | — | Anti speed-hack: a movement command may not claim more milliseconds than real time allows, plus up to 500 ms banked. 0 off. Bots are exempt. | R |

### 1.4 Chat, names and anti-abuse

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `sv_spectalk` | `1` | — | 1: spectators' `say` reaches players. 0: it reaches only spectators and is shown as `[SPEC] <name>: <text>`. A spectator's `say_team` always goes only to spectators with that prefix. Applies only when the mod does not handle chat itself. Also gates spectators' voice to players. | R |
| `sv_sayteam_to_spec` | `1` | — | 1: a player's `say_team` also reaches spectators who are following a teammate (or, when free-flying, are on the same team), but only for messages that carry the carriage-return marker clients add for team macros. 0: never. | R |
| `sv_unfake` | `1` | — | 1: carriage returns in a public `say` are replaced by `#`, so nobody can fake a message line. Team messages are left alone. | R |
| `sv_default_name` | `unnamed` | — | The name given to a client whose name is empty, is `console`, contains `&c` or `&r`, or contains a backslash, carriage return, line feed, `$`, `#`, `"` or `;`. Leading and trailing spaces are removed first. A duplicate name becomes `(N)<first 10 characters>`. | R |
| `sv_kicktop` | `1` | — | 1: a client that changes `topcolor` more than 6 times within 8 seconds is kicked (`<name> was kicked for topcolor spam`). | R |
| `sv_kickuserinfospamtime` | `3` | — | Window in seconds for the userinfo flood check. 0 disables the check. | R |
| `sv_kickuserinfospamcount` | `300` | — | More `setinfo` changes than this within the window kicks the client (`<name> was kicked for userinfo spam`). 0 disables. | R |
| `sv_specprint` | `0` | — | Bit field: 1 centre prints, 2 private prints, 4 commands sent by the mod to a player are also copied to spectators following that player. A spectator must also opt in with the same bit in userinfo `sp` (the server value is the default for it). The copy of commands checks the followed player's `sp`, not the spectator's (D: probably unintended). | R |
| `sv_voip` | `1` | — | 0 ignores all voice packets from clients. | R |
| `sv_voip_echo` | `0` | — | 1 sends a speaker's voice back to themselves (testing). | R |
| `sv_voip_record` | `0` | — | Voice in recorded demos: 0 none, 1 everyone's, 2 only spectators'. | R |
| `pausable` | `0` | — | 1 lets players use `pause`; spectators never can. The mod may still veto. | R |

Chat flood protection itself is not a setting: see `floodprot` and `floodprotmsg` in question 4.

### 1.5 Passwords, spectators and VIPs

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `password` | `` | — | Password to join as a player. Empty or `none` means no password. A client failing it sees `server requires a password`. VIPs skip it. | R |
| `spectator_password` | `` | — | Password to join as a spectator (sent in userinfo `spectator`). Empty or `none` means none. A client failing it sees `requires a spectator password`. | R |
| `vip_password` | `` | — | A space-separated list of passwords; any one makes the client a VIP (also accepted in userinfo `password`). Empty or `none` means none. A VIP skips both passwords and may use the VIP spectator slots. | R |
| `vip_values` | `` | — | A space-separated list of numbers matched by position to `vip_password`; the matching number becomes the client's VIP level (shown in userinfo `*VIP`). Empty: the level is the password's position, counting from 1. A VIP level is also granted by address with `vip_addip`. | R |

Whenever any of the three passwords changes, the server info key `needpass` is updated (question 3). (R)

### 1.6 Demos (recording)

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `sv_demoDir` | `demos` | — | Folder, inside the game directory, demos are written to and listed from. Cannot be set empty; a value containing `/..` is refused. | R |
| `sv_demoDirAlt` | `` | — | A second folder searched for demo downloads and web uploads. Same `/..` rule; may be empty. | R |
| `sv_demoPrefix` | `` | — | Text put in front of every demo name made by `record` and `easyrecord`. | R |
| `sv_demoSuffix` | `` | — | Text put after every demo name (before `.mvd`). | R |
| `sv_demoExtraNames` | `0` | — | For `easyrecord` names in team games: 0 gives `<n>on<m>_<team1>_vs_<team2>_<map>`; 1 gives `<n>on<m>_[<team1>]_<players>_vs_[<team2>]_<players>_<map>`. Duels are `duel_<p1>_vs_<p2>_<map>`, free-for-all `ffa_<map>(<count>)`. A name that already exists gets `_01`, `_02`, ... | R |
| `sv_demoRegexp` | `\.mvd(\.(gz|bz2|rar|zip))?$` | — | Case-insensitive regular expression that decides which files in the demo folder are demos (for lists, numbers, removal and the name-clash check). | R |
| `sv_demofps` | `77` | — | Most demo frames written per second while spawned clients are present and the game is not paused. 0 means 20; values below 4 act as 4. | R |
| `sv_demoIdlefps` | `10` | — | Demo frames per second with nobody spawned or while paused, bounded to 4..30. | R |
| `sv_demopings` | `3` | — | Seconds between ping tables written into the demo. 0 never. | R |
| `sv_demoMaxSize` | `20480` | — | KiB. A demo file larger than this is ended. 0 no limit. QTV streams are not limited. | R |
| `sv_demoMaxDirSize` | `102400` | — | KiB. Checked when a recording starts: if the demo folder is larger, either old files are deleted (see next row) or recording is refused with `Insufficient directory space, increase sv_demoMaxDirSize`. 0 no limit. | R |
| `sv_demoClearOld` | `0` | — | How many of the oldest demos to delete when the folder is over `sv_demoMaxDirSize` (twice as many files when `sv_demotxt` is on, to take each demo's text file too). 0 never delete. | R |
| `sv_demotxt` | `1` | — | 0 no text file. 1 writes `<demo>.txt` beside each demo with the teams and players, at the start and again at the end. 2 only makes sure an empty `<demo>.txt` exists (existing contents kept), for a mod to fill. | R |
| `sv_demoUseCache` | `0` | — | 1 buffers each demo file in memory of `sv_demoCacheSize` KiB before writing. The console announcement says `memory` instead of `disk`. | R |
| `sv_demoCacheSize` | `0`, set to `16384` at start | RO | KiB of the memory buffer: the command-line value `-democache <KiB>`, at least 16384. | R |
| `sv_silentrecord` | `0` | — | 1 suppresses the chat announcements `Server starts recording ...`, `Server recording completed` and `Server recording canceled, demo removed`. Error and size-limit messages are still shown. | R |
| `sv_onRecordFinish` | `` | — | When a demo finishes normally, runs the external script `./<first word>.qws` from the server's working directory with the demo folder, the demo name without extension, and any further words of the setting as arguments. | R |
| `sv_onDemoRemove` | `` | — | When an admin removes a demo, runs the external script `./<value>.qws` with the demo folder and the demo name as arguments. | R |
| `sv_demo_format` | `auto` | — | Latched at map load. `auto`: replacement-delta demo format when the map's entity capacity is above 2048, else the legacy format. `rd`: always replacement-delta. `legacy`: legacy, and if the capacity is above 2048 recordings are closed (`Closing recording output: selected MVD format is unsupported.`). Any other value is refused (`sv_demo_format must be auto, legacy or rd`). | R |
| `sv_demo_rd_predinfo` | `0` | — | Latched at map load. In replacement-delta demos, 1 keeps each player's movement input, velocity and command duration; 0 blanks them. | R |
| `sv_mvd_csqc` | `0` | — | 1 records the client-side-program entity stream and extra stats into demos and QTV when such a program is active. Off by default because common players and proxies cannot parse it. | R |
| `serverdemo` | `` | SI, RO | The file name of the demo being recorded, empty when none. | R |
| `extralogname` | `unset` | — | Set by `easyrecord` to `<demo folder>/<demo name>.xml`. The server itself never reads it; a mod may. | R |

### 1.7 QTV (broadcast to proxies)

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `qtv_streamport` | `0` | — | TCP port on which proxies (and TCP game connections) are accepted, 0..65534. 0 closes it. A failed open is retried about every 10 seconds; changing the port closes all current streams. | R |
| `qtv_maxstreams` | `1` | — | Most proxy streams at once. 0 unlimited. A few extra connections are tolerated beyond it; past the limit plus 3 the proxy is sent an error that the server limits proxies and is disconnected. | R |
| `qtv_password` | `` | — | Password proxies must give. Empty: none. Accepted methods: plain, CRC-16 (CCITT), MD4 and SHA3-512 challenge-response; the strongest one both sides offer is used. A failure sends `Bad password.` | R |
| `qtv_pendingtimeout` | `5` | — | Seconds a proxy has to finish the handshake. | R |
| `qtv_streamtimeout` | `45` | — | Seconds a stream may go without being able to send before it is closed (twice this for the initial header). | R |
| `qtv_stream_max_queue_size` | `16384` | — | KiB queued per stream, an integer from 1 to 2097151. Lowering it drops streams whose queue is already larger. Other values are refused with a message. | R |
| `qtv_sayenabled` | `0` | — | 1 lets proxy viewers' chat reach players at any time. 0: once a game has started (and `status` is not `Countdown`), proxy chat reaches only spectators. | R |

### 1.8 Logs

Log files are named `<sv_logdir>/<prefix><port>_<NNNN>.log`, NNNN from 0000 to 0999. The console, error, rcon, telnet and player logs append to the newest existing file; frag logs always start a new one. The commands that start and stop each log are in question 4. (R)

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `sv_logdir` | `.` | — | Folder for all log files (relative to the working directory). Created when set; a value containing `..` is refused. | R |
| `sv_maxlogsize` | `0` | — | Bytes. When a log file grows past it, logging continues in a new numbered file. 0 no limit. | R |
| `qconsole_log_say` | `0` | — | Level of the console log: 0 writes console output only; 1 or more also writes chat lines and similar level-1 lines. | R |
| `frag_log_type` | `0` | — | Frag log line format: 0 `\<killer>\<victim>\`; 1 `\frag\<killer>\<victim>\<killer team>\<victim team>\<Y-M-D h:m:s>\`, and with 1 a `\newmap\<map>\...\<date>\` line is written on each map change. | R |
| `telnet_log_level` | `0` | — | Level of the telnet log. Nothing in this program writes to the telnet log, so it has no effect (D). | R |

### 1.9 Remote control, bans and logins

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `rcon_password` | `` | — | Password for remote commands. Empty disables ordinary rcon. Such rcon may not start with `rm`, `rmdir`, `ls`, `chmod`, `sv_admininfo`, `if`, `localcommand`, `sv_crypt_rcon`, `sv_timestamplen`, `sys_command_line` or any word starting with `log` (only the first word after the password is checked, after expansion of `$` variables); it gets `Command not valid.`. The master password (question 4) has no such limits. Banned addresses cannot use rcon. | R |
| `sv_crypt_rcon` | `1` | — | 1: the password is never sent; instead the client sends a SHA-1 digest (40 hex digits) of the word `rcon`, a space, the password, a 16-hex-digit timestamp, a space, and each command word followed by a space, then the timestamp (the 8 bytes of the time value, least significant byte first, each byte as two hex digits, high digit first). 0: the plain password is sent, and is replaced by `*` in logs and console output. | R |
| `sv_timestamplen` | `60` | — | Seconds of allowed clock difference for the encrypted rcon timestamp, either way. 0 skips the time check. | R |
| `sv_rconlim` | `10` | — | Most rcon attempts per second, counted across all senders together; extra attempts in that second are refused silently, and a warning with the attempted rate is printed. 0 or less: no limit. | R |
| `filterban` | `1` | — | 1: addresses in the ban list are refused. 0: only addresses in the ban list are allowed. | R |
| `sv_admininfo` | `` | — | Text published as server info key `*admin`; empty removes the key. Ordinary rcon cannot change it. | R |
| `sv_login` | `0` | — | Account logins. 0 none. 1 players must log in, spectators not. 2 everyone must. Clients see `Enter login & password:` (or `Enter username:` with web logins), and are dropped with `Login timeout expired` after 60 seconds. An account is blocked after 10 failed attempts in a row. A client whose real address has an address-type account is logged in automatically. | R |
| `sv_login_web` | `1` | — | Optional build. 0: accounts from the local `accounts` file only. 1: logins checked by the web service (`sv_www_address`); local accounts can still block. 2: web login and a local account both required. | R |
| `sv_hashpasswords` | `1` | — | 1: `acc_create` stores account passwords as SHA-1 hex digests and logins are compared that way. 0: stored and compared as plain text. | R |
| `sv_forcenick` | `0` | — | 1: a logged-in client's name is forced to its login (or the account's alias), and a later name change is refused with `You can't change your name while logged in on this server.` | R |
| `sv_registrationinfo` | `` | — | Text shown to a client just before the login prompt. | R |

### 1.10 Master servers, the web service and inter-server broadcast

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `sv_broadcast_enabled` | `1` | — | Inter-server chat. A player who types `say .qw <message>` before a game has started sends it to every server the masters list (at most once every 30 seconds). Incoming broadcasts are shown to everyone, or once a game has started only to spectators (to players too when the mod is KTX with its spectator-talk option on). Also published as server info key `broadcast`. 0 refuses sending and ignores incoming broadcasts. | R |
| `sv_broadcast_sender_validation_enabled` | `1` | — | 1: an incoming broadcast is accepted only from an address on the list fetched from the masters (refreshed every 600 seconds while no game has started, or with `updatebroadcasts`). 0 accepts any sender. Incoming broadcasts are also rate-limited to 10 per address per 60 seconds. | R |
| `hostport` | `` | SI | The public `host:port` a server browser should show; also sent as the sender address in outgoing broadcasts. | R |
| `sv_www_address` | `` | — | Optional build. Base address of the web service used for web logins, demo uploads and the periodic check-in. Empty disables all of them (`Remote logins not supported on this server`). | R |
| `sv_www_authkey` | `` | — | Optional build. Key sent with every request to the web service. | R |
| `sv_www_checkin_period` | `60` | — | Optional build. Seconds between check-ins with the web service, never less than 60; no check-in while a game is running. | R |

Master servers themselves are set by the `setmaster` command (question 4), not by a setting.

### 1.11 The mod (game program) and loading it

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `sv_progsname` | `qwprogs` | — | Base name of the mod, read on each map load. For a bytecode program the server tries `<name>.dat`, then `qwprogs.dat`, then `spprogs.dat`. For a compiled module it looks for `<name>.qvm` or a native library `<name>.so`. | R |
| `sv_progtype` | `0` | — | Read on each map load. 0: try `<name>.qvm` run by the interpreter, else the bytecode `.dat`. 1: try a native library in the game folder, else `<name>.qvm` compiled, else `.dat`. 2: `<name>.qvm` interpreted. 3: `<name>.qvm` compiled to machine code (interpreted where no compiler exists for the CPU). Also set by the command-line option `-progtype <0..3>` (out-of-range values become 0). | R |
| `sv_pr2references` | `0` | — | Reset to 0 by the server before each compiled module starts; a module of interface version 15 or later sets it to 1 to use 64-bit-safe references. A 64-bit server refuses a native module that does not. | R |
| `vm_rtChecks` | `1` | — | Bit field of run-time checks compiled into a module translated to machine code: 1 program-stack overflow, 2 operand-stack overflow, 4 jump-target range, 8 memory-access bounds. Has no effect on the interpreter or native modules. | R |
| `sv_mod_extensions` | `2` | RO | A fixed number mods may read to learn the server's mod-extension level. The server itself does not consult it. | R |
| `qwm_name`, `qwm_fullname`, `qwm_version`, `qwm_buildnum`, `qwm_platform`, `qwm_builddate`, `qwm_homepage` (7 settings) | all empty | — | Placeholders for the mod to describe itself. The server reads `qwm_name`: when it contains `KTX`, server-side weapon selection and the KTX spectator-talk rule for broadcasts apply. | R |
| `sv_csqc_progname` | `csprogs.dat` | — | Latched at map load. The client-side program file sent to clients that support it, used only when a compiled module mod is active. Its checksum, size and name are published (question 3). | R |
| `sv_csqcdebug` | `0` | — | 0 off. 1 prints diagnostics about the client-side-program entity stream. 2 also prints datagram size accounting and uses a sized form of the entity message. | R |
| `sv_forcenqprogs` | `0` | — | Meant to force loading the single-player `progs.dat`. That support is compiled out of this program, so the setting has no effect (D). | R |
| `sv_cheats` | `0` | — | Latched at map load. 1 allows the cheat commands `god`, `give`, `noclip`, `fly` and publishes `*cheats ON`. Also turned on by the command-line option `-cheats` and, for one map, by `devmap`. | R |
| `deathmatch` | `3` | SI | Game mode number for the mod. Nonzero also makes the server drop map entities flagged "not in deathmatch". Set to 0 at map load when `coop` is on. | R |
| `teamplay` | `0` | SI | Team mode number for the mod. The server also uses it: with 0, a player's `say_team` reaches only themselves and the spectators `sv_sayteam_to_spec` lets through (outside coop), and `easyrecord` uses team naming when it is 1 or more. | R |
| `coop` | `0` | — | Cooperative mode for the mod; when nonzero the server forces `deathmatch 0` at map load, and players' `say_team` reaches everyone. | R |
| `skill` | `1` | — | Difficulty for single-player mods. At map load it is rounded to the nearest integer, raised to at least 0 and written back; entities are filtered by it with 3 as the highest level. | R |
| `fraglimit` | `0` | SI | Published for the mod and browsers; the server does not act on it. | R |
| `timelimit` | `0` | SI | Published for the mod and browsers; the server does not act on it. | R |
| `samelevel` | `1` | — | Kept for mods that read it; the server does not act on it. | R |
| `registered` | `1` | RO | Always 1, so mods behave as with the full game. | R |
| `sv_debug_weapons` | `0` | — | Latched at map load. 1 asks supporting clients to send their weapon-choice reasoning, which is written as hidden blocks into demos. | R |
| `sv_debug_antilag` | `0` | — | Latched at map load. 1 asks supporting clients to send their predicted positions, and writes those with the server's rewound positions into demos. | R |
| `sv_debug_usercmd` | `0` | — | 1 writes every client's movement commands into demos as hidden blocks. | R |
| `sv_mod_msg_file` | `` | — | Path (relative to the working directory) of a text file of up to 512 rules that turn the mod's obituary messages into lines of the mod frag log. Each line holds five fields separated by `#`: a message type number, a weapon number, the player count (1 or 2), a flag to swap killer and victim, and a regular expression. Empty or unreadable: built-in rules (unreadable also prints a warning). | R |

### 1.12 Maps and visibility

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `sv_loadentfiles` | `1` | — | 1: at map load, entities come from `maps/<sv_loadentfiles_dir>/<map>.ent` if it exists, else `maps/<map>.ent`, else the map file itself. 0: always the map file. | R |
| `sv_loadentfiles_dir` | `` | — | Sub-folder of `maps/` searched first for `.ent` files. | R |
| `sv_calcphs` | `2` | — | Latched at map load. 0: no "potentially hearable set" (sound culling by area). 1: always build it. 2: build it only when the map's visibility table is under 1 MiB once expanded. | R |
| `sv_phs` | `1` | — | 0: sounds are sent to everyone instead of only those who could hear them. | R |
| `sv_reliable_sound` | `0` | — | 1: sounds are sent on the reliable channel to every client whose userinfo `rsnd` is anything other than `0` (a client without the key counts as wanting it). 0: sounds unreliable unless the mod asks otherwise. | R, D |
| `sv_bspversion` | `1` | RO | Format of the current map: 1 classic (or Half-Life), 2 extended (large-map format). | R |
| `halflifebsp` | `0` | RO | 1 when the current map is in the Half-Life format. | R |
| `watervis` | `0` | SI | Published for clients (whether they may see through water); the server does not act on it. | R |
| `sv_paused` | `0` | RO | 1 while the game is paused, 0 otherwise. | R |

### 1.13 File system

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `fs_cache` | `1` | — | 1: file lookups go through a name index, rebuilt when the search path changes. 0: every lookup walks the search path. | R |

### 1.14 System and process

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `sys_select_timeout` | `10000` | — | Microseconds the main loop waits for network input before running a frame anyway; 1000 to 1000000, other values refused with a warning. | R |
| `sys_simulation` | `0` | — | Nonzero: the main loop does not wait for network input at all and runs frames back to back. | R |
| `sv_idlesleep` | `0` | — | Milliseconds slept each frame while no client is connected, to save processor time. 0 off. | R |
| `sys_extrasleep` | `0` | — | Unix builds. Microseconds slept after every frame, to simulate a bad connection on purpose. | R |
| `sys_sleep` | `8` | — | Windows builds only. Milliseconds slept per frame; forced to 0 in some conditions. | R |
| `sys_nostdout` | `0` | — | 1 stops console output (and the error line on a fatal error) from going to standard output. | R |
| `sys_restart_on_error` | `0` | — | 1: after a fatal error the program starts itself again with the same command line instead of exiting. | R |
| `sys_command_line` | the command line | RO | The full command line the program was started with. Ordinary rcon cannot read it. | R |
| `developer` | `0` | — | Nonzero prints developer messages to the console (and loads a compiled module's debug symbols). | R |
| `sv_use_dns` | `0` | — | 1: `status` shows clients' host names (looked up, which can stall) instead of addresses. | R |
| `sv_allowlastscores` | `1` | — | 1 answers the connectionless queries `lastscores` and `laststats` (scores and stats of recent games). 0 ignores them. | R |

### 1.15 Server identity (shown to browsers and clients)

| Setting | Default | Flags | Values and effect | Src |
| --- | --- | --- | --- | --- |
| `hostname` | `unnamed` | SI | Server name shown in browsers. | R |
| `countrycode` | `` | SI | Country code for browsers (for example `se`). Published only. | R |
| `city` | `` | SI | City name. Published only. | R |
| `coords` | `` | SI | `latitude,longitude`. Published only. | R |
| `version` | `MVDSV 1.20-dev` | RO | Program name and version. | R |
| `qws_name` | `MVDSV` | RO | Program short name. | R |
| `qws_fullname` | `MVDSV: MultiView Demo SerVer` | RO | Program full name. | R |
| `qws_version` | `1.20-dev` | RO | Program version. | R |
| `qws_buildnum` | the source revision, or `unknown` | RO | Build identifier. | R |
| `qws_platform` | one letter | RO | `l` Linux, `w` Windows, `f` FreeBSD, `o` OpenBSD, `n` NetBSD, `d` DragonFly, `s` SunOS, `m` macOS, `u` other. | R |
| `qws_builddate` | build date | RO | Date the program was built. | R |
| `qws_homepage` | the project's address | RO | Home page address. | R |

---

## Question 2: settings that are compatibility-only or have no effect

| Setting | Why | Src |
| --- | --- | --- |
| `telnet_log_level` | No part of the program writes to the telnet log; the `logtelnet` command opens an empty file. | D |
| `sv_forcenqprogs` | Single-player progs support is compiled out. | R |
| `samelevel`, `fraglimit`, `timelimit`, `watervis`, `countrycode`, `city`, `coords` | Registered and published for mods, clients and browsers; the server never acts on them. | R |
| `registered` | Fixed at 1 for old mods. | R |
| `sv_mod_extensions` | Fixed value for mods to read. | R |
| `qwm_*` (7) | Placeholders filled by the mod; only `qwm_name` is read. | R |
| `sv_airaccelerate` | Only sent to clients; the server's own player move uses `sv_accelerate` in the air. | R |
| `extralogname` | Written by `easyrecord`, never read by the server. | R |
| `sv_serveme_fix` | Read-only and always on. | R |
| `pm_pground` | Read-only; follows `pm_airstep`. | R |
| `halflifebsp`, `sv_bspversion`, `sv_paused`, `serverdemo`, `sv_local_addr`, `sys_command_line`, `sv_demoCacheSize`, `version`, `qws_*` | Read-only reports of program state. | R |
| `vm_rtChecks` | Only matters for modules translated to machine code. | R |
| `sys_sleep` | Windows only. | R |
| `qport`, `sv_highchars` | Not registered by the dedicated server at all. | R |
| Settings a single-player mod would expect (`gamecfg`, `scratch1`..`scratch4`, `saved1`..`saved4`, `savedgamecfg`, `temp1`) | Created only for single-player progs, which this build cannot load, so never present. | R |

---

## Question 3: server info keys the program sets itself

| Key | Set when | Meaning | Src |
| --- | --- | --- | --- |
| `*version` | Startup | `MVDSV 1.20-dev`. | R |
| `*z_ext` | Startup | `511`: the bit set of protocol extensions the server supports (1 player move types, 2 newer move types, 4 view height, 8 server time, 16 pitch limits from `maxpitch`/`minpitch`, 32 `join`/`observe` commands, 64 on-ground flag in player updates, 128 visible weapons, 256 solid flag in player updates). | R |
| `*gamedir` | Command line `-game <dir>` (or `+gamedir`), or the `gamedir`/`sv_gamedir` commands | The game folder clients should use. Absent means `qw`. | R |
| `*progs` | Each mod load | Bytecode or `.qvm` mod: its CRC as a decimal number. Native library mod: the library extension (`so` on Linux). | R |
| `*qvm` | Native library mod loaded | The library extension (`so` on Linux). | R |
| `*csprogs`, `*csprogssize`, `*csprogsname` | Each map load | Client-side program checksum (`0x` hex), size (`0x` hex) and file name; all empty when none is in use. | R |
| `*cheats` | Map load, or `-cheats` | `ON` while cheats are allowed, removed otherwise. | R |
| `*admin` | `sv_admininfo` changes | Its text; removed when empty. | R |
| `map` | Each map load | The map name. | R |
| `needpass` | One of the three passwords changes | Bits: 1 player password, 2 spectator password, 4 VIP password; removed when none is set. | R |
| `maxpitch`, `minpitch` | `sv_maxpitch`/`sv_minpitch` change | The limit, present only when not the default (80 and -70). | R |
| `broadcast` | `sv_broadcast_enabled` changes | `1` when inter-server broadcast is on; removed when 0. | R |
| `epoch` | A demo recording starts | The start time in seconds since 1970. Stored without notifying already-connected clients; new clients and the demo see it. | R |
| Every SI setting in question 1 | When it changes | `maxfps`, `fraglimit`, `timelimit`, `teamplay`, `maxclients`, `maxspectators`, `deathmatch`, `watervis`, `serverdemo`, `hostname`, `hostport`, `countrycode`, `city`, `coords`, `sv_bigcoords`, `sv_antilag`, `sv_antilag_no_pred`, `sv_antilag_projectiles`, `pm_ktjump`, `pm_bunnyspeedcap`, `pm_slidefix`, `pm_airstep`, `pm_pground`, `pm_rampjump`, `sv_safestrafe`: 25 keys. | R |

The key `status` is not set by the program, but it reads it (see "game started" above). Keys starting with `*` cannot be set with the `serverinfo` command. (R)

---

## Question 4: commands that set setting-like values

Server console (and rcon) commands:

| Command | Arguments | What it does | Src |
| --- | --- | --- | --- |
| `set` | `<name> <value>` (exactly two) | Sets a setting; when no setting has that name, creates a user setting (refused if the name is a command). | R |
| `toggle` | `<name>` | Sets the setting to 1 if its value is 0, else to 0. | R |
| `inc` | `<name> [amount]` | Adds the amount (default 1) to the setting. | R |
| `cvarlist`, `cvardump` | `[pattern]` | Lists settings (see the start). | R |
| `serverinfo` | none, `<key>`, or `<key> <value>` | No argument: prints the server info with its length out of 512. One: prints that key or `No such key <key>`. Two: sets the key (`0` becomes empty, which removes it); if it is a server info setting, the setting changes too. `*` keys are refused (`Star variables cannot be changed.`). | R |
| `localinfo` | none, `<key>`, or `<key> <value>` | The same for the server-only local info store (up to 10000 keys, 32768 characters), which only the mod reads; the mod is told of each change. `*` keys refused. | R |
| `setmaster` | `<address> ...` (up to 8) or `none` | Replaces the master server list (default port 27000), pings each and sends a heartbeat at once. `none`, or an address that does not resolve, leaves no masters. Heartbeats go every 300 seconds. | R |
| `heartbeat` | none | Sends a heartbeat to the masters now. | R |
| `updatebroadcasts` | none | Fetches the list of servers for inter-server broadcast now. | R |
| `floodprot` | `<messages> <seconds> <silence>` | Chat flood protection: more than N `say`s within S seconds silences the speaker for T seconds. All three must be positive; N at most 10. Defaults 4, 4 and 10. No arguments prints the current values. | R |
| `floodprotmsg` | `<text>` | Message shown to a silenced speaker after `FloodProt: `. No argument prints it. | R |
| `master_rcon_password` | `<password>` | A second rcon password without the command restrictions. Accepted only while the startup configuration runs (before the first map); later attempts are ignored. | R |
| `gamedir` | `<dir>` | Switches the game folder and sets `*gamedir`. A single folder name only (no `/`, `\`, `:` or `..`). | R |
| `sv_gamedir` | `<dir>` | Sets only `*gamedir` (what clients are told), not the folder used. | R |
| `addip` | `<address> [ban|safe] [time]` | Adds an address filter. Missing parts of the address match anything (a whole subnet). `safe` protects that exact entry from bans made by players' ban commands. Time is `+<seconds>` from now or an absolute time in seconds since 1970. | R |
| `removeip`, `listip`, `writeip` | `<address>` / none / none | Remove an entry (it must be written exactly as added), list the entries, save them as `addip` lines to `listip.cfg` in the game folder. | R |
| `vip_addip`, `vip_removeip`, `vip_listip`, `vip_writeip` | as above | The same for addresses that make a client a VIP; saved to `vip_ip.cfg`. | R |
| `logfile`, `logerrors`, `logrcon`, `logtelnet`, `fraglogfile`, `logplayers`, `modfraglogfile` | none | Each turns its log (file prefixes `qconsole_`, `qerror_`, `rcon_`, `qtelnet_`, `frag_`, `player_`, `modfrag_`) on, or off if it is already on. | R |
| `acc_create`, `acc_remove`, `acc_list`, `acc_block`, `acc_unblock` | `<login> [password]`, ... | Manage login accounts in the `accounts` file of the game folder (at most 1000). | R |
| `devmap` | `<map>` | Like `map`, with cheats allowed for that map. | R |

Command-line options that set values: `-port <n>` (UDP port, default 27500), `-ip <address>`, `-game <dir>`, `-basedir <dir>`, `-cheats`, `-democache <KiB>`, `-progtype <0..3>`. (R)

Client commands players type that change server values: `minping <0..300>` (sets `sv_minping`, see 1.1), `airstep 0|1` (sets `pm_airstep`, see 1.3), `pause` (see `pausable`), `rate <n>` (their own rate). Client userinfo keys the settings above read: `rate`, `drate`, `dupe` (0..3 extra copies of each packet), `msg` (message level), `sp` (spectator copies, see `sv_specprint`), `svf` (see `sv_forcespec_onfull`), `rsnd` (see `sv_reliable_sound`), `spectator` and `password`. (R)

A mod creates settings with its "register setting" call; it returns false if the name exists. In the bytecode interface that call passes the name as the value too, so a new setting starts with its own name as its value (D: a defect). (R)

---

## Open points (the source read here does not settle them)

- The exact meaning of `pm_rampjump` and `pm_pground` inside the player move beyond "jump from the ramp's surface" and "how on-ground is decided": the move code was only skimmed (`movement.md` is the place for that).
- How the `.qws` script's arguments are split before the script is run was not read in detail.
- Whether `zombietime` counts from the moment of the drop or from an earlier connection clock was not settled.
