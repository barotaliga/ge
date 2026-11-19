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
| `zombietime` | `sv_zombietime` (2 s; `zombietime` its synonym in the sequel's layer): a dropped slot resends the disconnect and reads nothing until 2 s after the client's last packet, `ZMBI` in `status` | endpoint |
| `coop_pickup_weapons`, `coop_elevator_delay` | the same, both classic games' settings (`plugins/classic/coop.hpp`, one default each; the first game's coop plays them too since 2026-11-19): coop weapons stay for everyone, a holder takes each copy nobody has taken once when 1; a plat stepped on waits the delay, 1 s, before it rises; deathmatch's weapons stay (dmflag 4) still refuses holders, question 8 being open | classic + each game (ours) |
| `g_select_empty`, `g_quick_weap`, `g_swap_speed` | the same: an empty weapon refused ("No Shells for Shotgun."), the next/previous keys stepping from the chosen weapon with its icon shown 0.9 s, lowering and raising N frames a frame | game (ours) |
| `dmflags` 16384 (quad drop) | also the first game's `sv_dropquad` (0; either drops the quad at death) | game (ours) |
| `addip`, `removeip`, `listip`, `writeip`, `filterban`, `Banned.` | the engine's ban list (`server/bans.hpp`, its file read back at start: `ban_file_read`), `sv_banmode` with `filterban` its synonym in the sequel's layer | engine + endpoint |

| `sv_strafejump_hack`, `sv_waterjump_hack` (modern) | `sv_landjump`, `sv_waterjump` (synonyms), told per client by the move-feel message | game (sequel move) |
| `sv_reserved_slots`, `sv_reserved_password` (modern) | the engine's `sv_vipslots`, `sv_vippassword` (`server/admission.hpp`); the server password counts as a VIP's | engine |
| `sv_iplimit` (modern) | the same, the engine's (`server/admission.hpp`), the sequel's default 3 | engine |
| `sv_idlekick` (modern) | `sv_idletime` (`server/idle.hpp`) | engine |
| `sv_timescale_time`, `_warn`, `_kick` (modern) | `sv_speedwindow`, `sv_speedwarn`, `sv_speedkick` (`server/skew.hpp`) | engine |
| `sv_locked` (modern) | the same (`server/admission.hpp`), set over rcon while running | engine |
| `sv_auth_limit`, `sv_rcon_limit` (modern) | `sv_passwordrate`, `sv_rconrate` (`server/limit.hpp`, `limit[/period][*burst]`, default 1 each) | engine |
| `sv_min_rate`, `sv_max_rate`, `sv_lan_force_rate` (modern) | the engine's `sv_minrate`, `sv_maxrate` (the sequel's defaults now 15000, 60000), `sv_lanrate`; with them `timeout` 90 and `sv_speedcheck` 1 | engine |
| `sv_uptime`, `sv_status_show`, `sv_status_limit` (modern) | `sv_uptime`, `sv_status`, `sv_statusrate` (`server/status.hpp`); the extended `statusx`/`rulesx` queries not built (no browser of ours reads them) | engine |
| `lrcon_password`, `addlrconcmd`, `dellrconcmd`, `listlrconcmds` (modern) | `lrcon_password`, `lrcon_add`, `lrcon_del`, `lrcon_list` (`server/lrcon.hpp`; the modern words read too) | engine |
| `sv_namechange_limit`, `sv_show_name_changes` (modern) | `sv_namelimit` ("N S" or "N/min"), `sv_shownames` (`server/names.hpp`) | engine |

## Ranked candidates (best value for the effort first)

Re-ranked 2026-11-19 against the modern sequel server (`sequel-modern-server-inventory.md`, a
clean-room reader's note), which is now the main reference for the sequel's server. Each row
names OUR setting (short, in the first game's style: `sv_minrate`, `sv_spectalk`) and the modern
server's spelling, which is a synonym in the sequel's layer only. A modern name that is already
short and means one thing becomes ours as it is. Names agreed with Niclas 2026-11-19 (the alternative move dropped: remix physics has it).

| # | Modern setting or command | Ours (proposed) | What it adds | Home |
| --- | --- | --- | --- | --- |
| 11 | `sv_fps` | `sv_fps` (the engine's), the sequel's own rates 10 to 60 | Server frames of 100, 50, 33, 25, 20 or 16 ms. RULES LANDED 2026-11-19 (movers, shots, drops and pieces at `sv_fps`, the 10 Hz world on tenths); our clients get every frame by `*ge_ext` bit 256 | game (rules) + endpoint |
| 13 | `sv_redirect_address`, `sv_downloadserver` | `sv_redirect`, `sv_downloadurl` | Send clients elsewhere when full; a web address for downloads. `sv_redirect` LANDED 2026-11-19 (engine admission, both games); `sv_downloadurl` open | endpoint |
| 15 | `sv_allow_map`, `sv_recycle`, `sv_changemapcmd` | `sv_allowmap`, `sv_recycle`, `sv_mapcmd` | `map` refused mid-game (use `gamemap`); restart on the next map; a command after each map. `sv_mapcmd` LANDED 2026-11-19 (engine server core, every game). `sv_allowmap` and `sv_recycle` NOT TAKEN: our servers have no `map` that differs from `gamemap` (every map change keeps the process and its clients), so there is no full restart to refuse or ask for | engine |
| 16 | `addfiltercmd`, `addcvarban`, blackholes | `filtercmd_add`, `cvarban_add`, the engine's ban list | Commands a client may not send; clients whose settings break a rule; addresses ignored. `filtercmd_add` and the blackholes (`blackhole_addip`) LANDED 2026-11-19 (engine, both games); `cvarban_add` and `userinfoban_add` LANDED 2026-11-19 on Quake II's server (engine `settingban.hpp`; facts in `sequel-cvarbans.md`); QuakeWorld's stock clients expand no `$name` in a stuffed line, so its server takes none | engine |
| 17 | `sv_ghostime`, `sv_allow_nodelta`, `sv_calcpings_method` | `sv_ghosttime`, `sv_nodelta`, `sv_pingmethod` | Silent challenged slots freed; full-frame floods dropped; ping as average or minimum. LANDED 2026-11-19 (the unreachable-notice drop is not taken: our transport hears no ICMP) | endpoint |
| 18 | `sv_max_packet_entities`, `sv_prioritize_entities`, `sv_novis` | `sv_maxents`, `sv_entpriority`, `sv_novis` | Entities a frame; which ones first when over; no culling. LANDED 2026-11-19 for our own rules (a hosted library's raw frames not capped; `sv_trunc_packet_entities` not needed: our frames stop at the packet's room) | endpoint |
| 19 | `map_visibility_patch`, `map_override_path`, `sv_load_ent` | `sv_vispatch`, `sv_entpath`, `sv_entfile` | Three stock maps' visibility repaired; a map's entities replaced from a file | game (rules start) |
| 20 | `sv_mvd_enable`, `sv_mvd_autorecord`, `logfile` | the engine's TV, `sv_autorecord`, `sv_logfile` | Match recordings and the server log (built as the engine's; the sequel reads them) | engine |
| 21 | `g_machinegun_norecoil`, `aimfix` | the same | The machine gun's climb (ours has none yet) and its switch; shots from the muzzle at the crosshair's point | game (ours) |
| 22 | `sv_maxvelocity` | the plugin's `sv_maxvelocity` | The entity physics' speed cap (2000) as a setting | game (ours) |
| 23 | `sv_rollangle`, `sv_rollspeed` | the same | The strafing view roll as settings | game (ours) |
| 24 | `g_footsteps`, `g_monsterfootsteps` | the same | Footstep sounds for players and monsters | game (ours) |
| 25 | `gamemode`, `listmaps`, `say` (console) | the same | Operator conveniences | endpoint |
| 26 | `serverrecord` | the engine's TV | The server's own recording | game (wire) |
| 27 | `maxentities`, `g_fix_triggered`, `g_commanderbody_nogod` | the same | Entity slots; single-player fixes | game (ours) |

The modern server's bundled game library is a team mod's (inventory § 4), not the stock rules:
its settings are a hosted library's own and are not on this list. The modern server's `maxclients` 8 is still open (the engine's own default holds).

Not worth building: `ip`, `port` (ours are the engine's `sv_port`), `multicast`, `qport`,
`showpackets`, `showdrop`, `showclamp`, `fs_debug`, `developer`, `modder`, `logfile`,
`colorterminal`, `timescale`, `fixedtime`, `timedemo`, `cl_maxfps`, `basedir`, `cddir`,
`gun_x/y/z`, `cl_gun`, `sv_optimize_sp_loadtime`, `sv_optimize_mp_loadtime` (signon speed-ups of
one program's own), `map_noareas` (a debugging switch; our server already treats every area as
connected), the Windows-only settings, `version`, `gamename`, `gamedate`, `protocol`, `mapname`
(informational keys our server info sets itself where a browser reads them).

The competitive sequel server's additions (inventory question 7) are still open: its source could
not be fetched in the round that wrote the inventory.
