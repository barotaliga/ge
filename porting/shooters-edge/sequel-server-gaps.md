# The sequel's server settings: the gap list against its stock server (working note)

WORKING AREA: deleted when porting ends. Which settings of the sequel's stock server and stock
game library (`sequel-server-inventory.md`, written by a clean-room reader; the behaviour of each
is there) our sequel server has (`sesv -game 2`: `net/sequel_game.hpp`, `net/sequel_net_server.hpp`
over our rules `server/sequel_server.hpp`, or a hosted game library `server/sequel_native.hpp`,
which answers its own library's settings), and the ranked candidates for the next rounds. Shaped
like `server-gaps.md`, the first game's list.

Homes, in order of preference: **engine** (`engine/server`, every game's server), **plugin**
(`plugins/shooter`, any shooter), **game** (Shooter's Edge's sequel endpoint `net/sequel_*`, its
rules `server/sequel_*`).

Names and layers (`docs/porting-games.md` § One name, one meaning): the first game's server
names are ours (`server-gaps.md`, agreed with the QuakeWorld settings thread 2026-11-18); a stock
sequel name that means the same is a synonym in the sequel's layer (`Cvars::add_synonym`), never a
second setting: `maxspectators` is `sv_maxspectators`, `spectator_password` is
`sv_spectatorpassword`, `sv_enforcetime` is the plugin's `sv_speedcheck`, `allow_download*` are
`sv_download*`. A rate clamp reads the engine's `sv_minrate`/`sv_maxrate`; team chat goes through
`server/chat_rules` (`sv_sayteam_to_spec`, `sv_spectalk`); the match's phases (overtime, idle kick,
team locks, warm-up, home map) are the engine lobby's for both games.

How to run a round: take the top of "Ranked candidates", build it in the highest home it fits
from the inventory's facts alone, move its row to "Done", re-rank. Gameplay and the settings an
operator of a public server sets come first; diagnostics and look last.

With a hosted game library the library's own settings (§ 2 of the inventory: `dmflags`,
`password`, `maxspectators`, `filterban`, the flood settings, the `g_*` switches) are the
library's to read: the host answers its `cvar` calls from the server's settings, so they work as
they do on the stock server. The rows below marked **ours** are for our own rules, which play
without a library.

## Done

| Stock setting | Ours | Home |
| --- | --- | --- |
| `hostname`, `maxclients` | `sv_hostname`, `sv_maxclients` (the stock names read too) | engine |
| `deathmatch`, `coop`, `skill`, `singleplayer` | the same (game rules) | game |
| `dmflags` (every bit of § 2.2 our rules play: no health, no powerups, weapons stay, falls, instant powerups, same level, skin and model teams, friendly fire, spawn farthest, force respawn, no armour, allow exit, infinite ammo, quad drop, fixed field of view) | the same | game (ours) |
| `fraglimit`, `timelimit`, `sv_gravity`, `sv_airaccelerate` | the same | game |
| `sv_maplist` | the same, beside `sv_map` and `sv_maprotation` | game + engine |
| `password` | `sv_password` (a player's; `none` means none) | engine setting, endpoint |
| `timeout` | the endpoint's `timeout` (125 s) | game (endpoint) |
| `allow_download`, `_players`, `_models`, `_sounds`, `_maps` | `sv_download*` (the stock names map onto them) | game (endpoint) |
| `flood_msgs`, `flood_persecond`, `flood_waitdelay` | the same | game (ours) |
| `cheats` | the same | game (ours) |
| `run_pitch`, `run_roll`, `bob_up`, `bob_pitch`, `bob_roll` | the same | game (ours) |
| `sv_noreload` | the same | game (ours, saves) |
| the client's frame cut to what it sees (the engine's behaviour, not a setting) | `cut_to_view` | game (endpoint) |
| `public`, `setmaster`, `heartbeat` | `sv_rendezvous`, `sv_visibility` (the engine's own listing) | engine |
| `rcon_password`, `rcon` | `rcon_password` (the engine's); the stock texts; the password never in the log | engine setting, endpoint (`net/sequel_admin`) |
| `kick`, `status`, `dumpuser` | the same words, the stock answers (`status` reads every ping as 0 for now) | endpoint |
| `maxspectators`, `spectator_password`, `needpass` | `sv_maxspectators` (the engine's meaning: 0 spectators take players' slots, N their own N beside the players'), `sv_spectatorpassword` (sent as the `spectator` key); both stock names synonyms in the sequel's layer; `needpass` and `maxspectators` in `status`'s info; the stock refusals | engine settings, endpoint |
| `rate` (user info; 100 to 15000, none 5000) and the frame held back past it, its suppress count | the engine's `sv_minrate`, `sv_maxrate` and verdict (`server/rate.hpp`) over the sequel's bounds; the 10-frame hold in the endpoint | engine settings, endpoint |
| chat's spectators (the stock rules keep none) | the engine's chat route (`server/chat.hpp`) under our rules' `say`: `[SPEC]`, `sv_spectalk`, `sv_sayteam_to_spec` | engine + game (ours) |
| `sv_enforcetime` | `sv_speedcheck` (the sequel's meaning and default, 0: a budget of 1800 ms set every 16 frames, a move past it not run; our rules; a hosted library's runs unchecked) with `sv_enforcetime` its synonym in the sequel's layer | game (ours) |
| `coop_pickup_weapons`, `coop_elevator_delay` | the same (coop: weapons stay for everyone, a holder takes each copy nobody has taken once when 1; a plat stepped on waits the delay, 1 s, before it rises); deathmatch's weapons stay (dmflag 4) still refuses holders, question 8 being open | game (ours) |
| `addip`, `removeip`, `listip`, `writeip`, `filterban`, `Banned.` | the engine's ban list (`server/bans.hpp`, its file read back at start: `ban_file_read`), `sv_banmode` with `filterban` its synonym in the sequel's layer | engine + endpoint |

## Ranked candidates (best value for the effort first)

| # | Stock setting or command | What it adds | Home |
| --- | --- | --- | --- |
| 1 | `zombietime` | A dropped slot keeps resending its disconnect for 2 s before reuse | endpoint |
| 1 | `coop_pickup_weapons`, `coop_elevator_delay` | Coop: a weapon stays for everyone; a platform waits before it moves | game (ours) |
| 2 | `g_select_empty`, `g_quick_weap`, `g_swap_speed`, `g_machinegun_norecoil`, `aimfix` | The library's weapon handling switches (refused empty weapon, quick next/prev, faster swaps, no climb, muzzle aim) | game (ours) |
| 3 | `sv_maxvelocity` | The entity physics' speed cap (2000) as a setting | game (ours) |
| 4 | `sv_rollangle`, `sv_rollspeed` | The strafing view roll as settings | game (ours) |
| 5 | `g_footsteps`, `g_monsterfootsteps` | Footstep sounds for players (0, 1 moving fast, 2 always) and monsters | game (ours) |
| 6 | `sv_entfile` | A map's entity list replaced by `maps/<name>.ent` (or `<name>@<checksum>.ent`) | game (rules start) |
| 7 | `gamemode`, `listmaps`, `say` (console) | Operator conveniences: set the mode for the next map, list the maps, chat as the console | endpoint |
| 8 | `serverrecord` | A recording of every entity and message (we have TV and the per-player `.dm2` writer; this is the server's own) | game (wire) |
| 9 | `sv_downloadserver` | A web address for downloads in the connect answer (`dlserver=`) | endpoint |
| 11 | `maxentities`, `g_fix_triggered`, `g_commanderbody_nogod` | Entity slots; single-player fixes | game (ours) |

Not worth building: `ip`, `port` (ours are the engine's `sv_port`), `multicast`, `qport`,
`showpackets`, `showdrop`, `showclamp`, `fs_debug`, `developer`, `modder`, `logfile`,
`colorterminal`, `timescale`, `fixedtime`, `timedemo`, `cl_maxfps`, `basedir`, `cddir`,
`gun_x/y/z`, `cl_gun`, `sv_optimize_sp_loadtime`, `sv_optimize_mp_loadtime` (signon speed-ups of
one program's own), `map_noareas` (a debugging switch; our server already treats every area as
connected), the Windows-only settings, `version`, `gamename`, `gamedate`, `protocol`, `mapname`
(informational keys our server info sets itself where a browser reads them).

The competitive sequel server's additions (inventory question 7) are still open: its source could
not be fetched in the round that wrote the inventory.
