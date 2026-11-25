# Servers

How every multiplayer game on the engine is hosted: one server core in `engine/server` that a
game plugs its rules into, run either inside the player's own process (a LISTEN server) or as its
own headless process (a DEDICATED server), configured by layered `server.cfg` files, with the
lobby before the match and the match's phases in the engine rather than in each game, and seams
for what a server will grow later: an administrator's tools, recording and broadcasting matches,
server-side match mods, and cvars that switch on whole groups of others. Rows M53a–M53h
([`goals/M53.md`](goals/M53.md)).

This page is the design; what each row has built is in its row file. Built so far (M53a, part
one): `engine/server` with `ServerGame`, `Server` (the frame, the node, the session host and the
LAN hosting round it), the dedicated process (`server_main`: `server.cfg`, then the flags, then
`+commands`), `ge_add_server`, a sample game and the server trial, and `sesv`
running on it. M53b: `server.cfg` in layers (`engine/server/config`: the base game directory's,
each mod's lowest first, a profile's winning copy, the command line, the console), every server
value's file and line kept (`Cvars::set_watch`), the rest of the server's cvars, and
`--explain`. M53a part two: a game that speaks its own protocol is handed the port
(`ServerGame::serve`, `net_frame`), and `sesv` now serves the game's own
deathmatch over the classic protocol (`games/shooters_edge/net/qw_game.hpp`), so a classic
client connects and plays (the `se-dedicated` lab). The game's own rules are settings too: a game
names them (`ServerGame::rules`, a `GameRule` each: Shooter's Edge's `deathmatch`, `teamplay`,
`fraglimit`, `timelimit`, `samelevel`, `pausable`; the move's and the loop's settings every
shooter shares, `plugins/shooter/settings.hpp`: `sv_gravity`, `sv_maxspeed`, `sv_mintic`,
`sv_maxvelocity`, `pm_ktjump`, …; and its endpoint's: `sv_maxrate`, `sv_antilag`, `sv_voip`, `maxfps`,
`sv_download*`), the core makes each a server
cvar with an empty fallback (unset: the game's own default), every layer, `+set` and `--explain`
reach it like the core's, and `create` gets the values in `ServerConfig::rules`. A map is read from
the game folder's loose files or its packs, and when a match ends the server moves everyone to
the next map (`sv_maprotation`'s when set). M53c part one: the lobby's match flow lives in the
engine (`engine/server/match`: seats, teams, ready and break, the leader's start, warmup, the
countdown, the match), and Shooter's Edge's lobby is that plus what a team is in its game.
M53c part two: the dedicated server runs it. `sv_bots` seats the game's bots (Shooter's Edge's
are local seats of its classic endpoint: shown to every client as players, sent nothing) and
`sv_lobby 1` opens every map in warmup with the game's limits held; the players' `ready`,
`break`, `team` and `start` (and the leader's `addbot`, `removebot`) arrive as their own console
lines, a countdown run out restarts the map into the match, and the match's end brings the next
map up in warmup. With `LobbySettings::humans` set (a dedicated server's 1) bots alone never
count down, and a bot never leads while a player is seated (the `se-dedicated-lobby` lab).
Quake II's server seats `sv_bots` too, in deathmatch on its own rules: the same bot seats
(`games/shooters_edge/bots/bot_seats.hpp`, which QuakeWorld's lobby keeps its bots in), their nav
built off the map's brushes, Quake II's arsenal, their moves run after the clients' each frame, a
client never given a bot's slot, and the status listing them as bots.
The lobby and the warmup are apart: with `LobbySettings::warm_on_join` off (a game that has a
lobby screen of its own, as Shooter's Edge's window does) a player waits on that screen, off the
map, until `warmup` puts him in the warmup (`sitout` takes him back, `lobby_on_map` says where a
seat is); bots are in the warmup as they join, and the match puts every seat on the map. `start`
is refused below `min_players` seated (bots count) or `humans` players. A dedicated server's
clients have no lobby screen, so there `warm_on_join` stays on and they join the warmup at once.
M53a part four: Shooter's Edge's listen server is the same `Server` on the same game, opened in
the window's process (`ServerConfig::host` carries what the window prepared: its program, the map
it read, its lines, its player), the player a local seat whose moves the window puts in and whose
prints and effects the endpoint hands back; `sv_port` lets others join it. Running one:

```
make build
build/debug/sesv --name="Arena Night" --description="the arena" --port=27500 \
  --maxclients=16
build/debug/sesv --root=<install> --map=dm6 --port=27512 \
  +set fraglimit 20 +set timelimit 10
build/debug/sesv --root=servers --mod=arena --cfg=duel --explain=sv_
build/debug/sesv -classic q2 --root=<install> --map=q2dm1 --port=27910
```

`sesv -classic q2` is the same process over Quake II's rules on its own protocol
(`games/shooters_edge/net/q2_game.hpp`): `--root` holds Quake II's game folder, and an
unmodified client of Quake II joins it. Its levels go round `sv_maprotation` after `sv_map`
when `fraglimit` or `timelimit` ends one; every client stays connected and signs on again.

The layers are read as loose files under `--root` (`<root>/<dir>/server.cfg`); a copy inside a
pack is not read yet. `ge server cfg [<prefix>]` (M53d) builds the game's dedicated binary and
prints its own `--explain`.

## What is there today, and what is missing

| Piece | Where | What it already does |
| --- | --- | --- |
| The link, the node, channels | `engine/net` (M17a–M17c) | Sockets, peers, reliable and unreliable channels, the simulated network |
| Sessions | `engine/net/session` (M17d) | Seats, the leader, teams, ready, the settings' bytes, `lobby` and `playing`, chat, kick, reconnect by token |
| Hosting and the browser | `engine/net/hosting`, `directory` (M17r) | LAN answers, pings, the router's forward, the rendezvous's list, visibility; "the same for a listen and a dedicated server" |
| Cfg files | `engine/console/cfg` (M21a) | `start_server`: `server.cfg`, then the command line; the server's tier (`kServer`, `kServerInfo`) |
| The search path | `engine/vfs/search` (M18a) | A base game directory, then a directory per mod over it, later sources winning |
| A dedicated server | `games/shooters_edge/server` (M18f) | Shooter's Edge's own host with bot slots, input log and replay (now a `ServerGame` on the core) |
| The lobby's match flow | `engine/server/match` (M53c, lifted from Shooter's Edge's) | Warmup on the map, two teams, ready and break, the leader's start, a countdown, the match restarting the map |

What is missing is the part every game would otherwise write again: the server's frame and
process, its settings and where they come from, the match's phases, the administrator's
commands, and a way for a game's client to start and stop a server. Fall Line's pilot (M17k)
needs all of it, and Shooter's Edge has its own copy of half of it today.

## The shape

```
                game's client (window)                      dedicated process
          ┌──────────────────────────────┐         ┌──────────────────────────────┐
          │ client loop                  │         │ engine/server/main           │
          │   └─ listen: Server ────┐    │  spawn  │   └─ Server                  │
          │                         │    │ ──────▶ │                              │
          └─────────────────────────┼────┘         └──────────────┬───────────────┘
                                    ▼                             ▼
            ┌───────────────────────────────────────────────────────────────────┐
            │ engine/server: Server                                             │
            │   frame on the run clock · node · session · hosting · console     │
            │   config layers · match phases (lobby) · features (hooks table)   │
            └───────────────┬───────────────────────────────────┬───────────────┘
                            ▼                                   ▼
               ServerGame (the game's rules:            features compiled in:
               Fall Line, Shooter's Edge)               rcon, status, admin, record,
                            │                           broadcast, votes, match mod
                            ▼
               the game's own server-side mods (QC progs, M52 C++ mods)
```

- **One `Server`, two ways to run it.** A `Server` owns the node, the session host, the hosting,
  the server's console and cvars, the match's phases and the features. A listen server is a
  `Server` the client loop steps between its own frames, with the host's player in it (a local
  seat of a game's own endpoint, as Shooter's Edge's is, or on a loopback link,
  `net/local_pipe`); a dedicated server is the same `Server` stepped by
  `engine/server/main` on the run clock. Nothing in a game branches on which one it is in.
- **The game plugs in through `ServerGame`.** A plain table of function pointers and a context
  pointer (the frame calls it once a tick, so the indirection costs nothing that matters): make
  the world for a map, take a player in and out, take a player's input, step, write what each
  client sees, describe the state for the input log and the digest, and the match's events
  (warmup began, match began, match ended). The game keeps its own protocol where it has one
  (Shooter's Edge's compatible protocol answers on the same socket, M18g).
- **Each game gets its own server binary.** A game's CMake calls `ge_add_server(<game>_server
  ...)`, which links the engine's `main` with the game's `ServerGame`: `fall_line_server`,
  `sesv`. There is no one binary that loads any game at run time: a game is C++
  compiled into its own executables (G8: nothing is registered at run time), and a server runs
  the game's own code so a replay and a client's prediction agree to the bit.
- **Both ways can start the other's work.** The game's HOST page either runs a listen `Server`
  or SPAWNS the dedicated binary as its own process (desktop only: the stores forbid it), hands
  it a cfg, joins it, and can find and stop it again by its local address. The dedicated server
  in turn LAUNCHES matches: a map rotation, a mode, a mod, each switch re-reading the config
  layers below and restarting the match.
  Starting, finding and stopping are `engine/server/spawn.hpp`: the binary started detached with
  a cfg handed over (`+exec`), a record per port in the game's state folder that any of its
  programs reads later, and a ping of the session's advert at this machine's port to tell a
  server that answers from one still starting (the `server-launch` lab, on the sample game's
  `gesample`). A started server runs the console lines a launcher adds to its `--commands` file
  (`hosted_command`); `mod arena,ctf` there resets every server value, remounts the search path,
  reads the layers again with the command line kept, and restarts the match under them on the
  same port (`server_switch_mods`). `quit [reason]` ends the server after the frame (the game's
  `ServerGame::quit` says goodbye with the reason first) and `recycle [reason]` the same with the
  players told to reconnect: both exit with success, so a supervisor (a script, a service) starts
  the server again. The operator's console also has `showip`, `dns`, `net_stats`, `macrolist` and the macros
  (`$com_date`, `$com_time`, `$com_uptime`, `$com_uptime_long`, `$random`, `$com_maplist`, the
  game's maps by `ServerGame::maps`). From a game's repo, `ge server --map --bots --for` builds the
  game's dedicated binary (the one `ge_add_server` names, or `--game`) and runs it headless for
  its digest and input log; `--replay=<log>` runs a log again to the bit.

## The settings: `server.cfg` and its layers

A server's settings are cvars on the server's tier (`kServer`, most with `kServerInfo`), read
from several cfg files in a fixed order, each later layer overriding an earlier one, so a mod's
`server.cfg` changes only what it needs and inherits the rest from the base game's:

| # | Layer | Where |
| --- | --- | --- |
| 1 | The engine's defaults | each cvar's fallback, registered by `engine/server` and the features |
| 2 | The game's defaults | the fallbacks the game's `ServerGame` passes (a shooter's 72 Hz, a racer's 120) |
| 3 | The base game's `server.cfg` | `<root>/<base>/server.cfg` |
| 4 | Each mod's `server.cfg`, lowest first | `<root>/<mod>/server.cfg`, in the search path's mount order (`engine/vfs/search`) |
| 5 | A named profile | `--cfg=duel` runs `duel.cfg` from the same search path (the winning copy) |
| 6 | The command line | `+set sv_hostname "..." +map dm2` and the flags below |
| 7 | At run time | the server's console, rcon and the leader's lobby settings, never written back |

Every layer below 5 is read for EVERY game directory, not just the winning one, which is why the
search path grows a `find_all(name)` that lists each directory's copy in mount order. A file that
is not there is skipped, as today.

**Where a value came from.** Every server cvar keeps the layer and the file and line that last
set it. `ge server cfg [<prefix>]` (the binary's `--explain`) prints each value with its source and what it
overrode:

```
sv_hostname   "Arena Night"   arena/server.cfg:3   (over base/server.cfg:2 "Shooter's Edge")
sv_maxclients 16              base/server.cfg:5
sv_antilag    1               preset sv_modern 1   (arena/server.cfg:7)
```

so an agent or an owner can answer "why is this server set like this" without reading the files.

**The server's own cvars** (named with the console's prefixes, `docs/console.md`):

| Cvar | What | Flags |
| --- | --- | --- |
| `sv_hostname` | the server's name in the browser and the lobby | server info |
| `sv_description` | a line or two under the name in the browser | server info |
| `sv_motd` | the message a player sees on joining | server |
| `net_ip` | the address to listen on (empty: every interface) | server, latched |
| `net_port` | the port (the game's default when 0) | server, latched |
| `sv_maxclients` | players, 1..32 | server info, latched |
| `sv_maxspectators` | spectators beside them | server info, latched |
| `sv_password`, `sv_spectatorpassword` | a password to play, to watch; the advert says locked | server |
| `sv_tvport`, `sv_tvdelay`, `sv_tvmaxviewers` | the server's own TV (§ Spectators on the server): its port (0: none), how far behind the match it shows it (10 s), its viewers at most (32); never voted on | server, latched |
| `sv_visibility` | `lan`, `invite` or `listed` (M17r) | server |
| `sv_rendezvous` | the rendezvous to register and list with | server, latched |
| `sv_master` | the masters a community's hub reads (`docs/hub.md`), space-separated, port 27900 when none is given; the server beats to each and answers status asks on its own port, a game on its own protocol too; a game's server browser lists them from there (Shooter's Edge's `sb_hubmasters`) | server, latched |
| `sv_map`, `sv_maprotation` | the first map and the rotation | server info |
| `sv_bots` | seats the game's bots fill at the start | server, latched |
| `sv_lobby` | 1: every map warms up until everyone is ready, then the countdown and the match | server info, latched |
| `sv_countdown` | with `sv_lobby`: seconds from everyone ready to the match (5) | server |
| `sv_intermission` | with `sv_lobby`: seconds the scores stay up after a match before the next map (0: none) | server |
| `sv_autorecord` | 1: every match recorded by itself to `demos/<map>-<date>-<time>.ged` in the game's folder (§ Every match kept); never voted on | server |
| `sv_mod` | the mod's game directory, mounted over the base | server, latched |
| `sv_admin` | how to reach the server's owner, shown in its info | server info |
| `sv_timeout` | seconds a client may go unheard before it is dropped (65): a dead link, not an idle player | server |
| `sv_floodprot`, `sv_floodprotmsg` | the chat flood rule, `"N S T"`: N lines in any S seconds, else silent for T (`10 1 1`), and what a flooder is told (`engine/server/guards.hpp`) | server |
| `sv_vote` | 1: players may vote on the settings for the map (`vote <setting> <value>`, § Votes) | server |
| `sv_vote_time`, `sv_vote_wait` | seconds a vote stays open (30); seconds a player whose vote failed waits before calling another (30) | server |
| `sv_vote_allow`, `sv_vote_deny` | the settings a vote may change (empty: every one but the deny list's), and those it never may (`*` any characters; the default keeps passwords, rcon, the name, the slots, the address, the vote's own, the mod, the seed, the rotation and the frame rate out) | server |
| `sv_timeouts`, `sv_timeoutlength`, `sv_timeoutresume` | timeouts each side may call in a map (1; 0 none), how long one stops the game (60 s), the count from its end back to play (5 s) (§ Timeouts) | server |

and the flags `--port`, `--ip`, `--maxclients`, `--name`, `--description`, `--cfg`, `--mod`,
`--map`, `--bots`, `--lobby`, which are the same cvars set at layer 6. The game adds its own (`fraglimit`, a mountain's
seed) the same way.

**A game's own names for these settings** (M53i). A game whose configs, admins and mods already
use other names lists them in `ServerGame::names` (`SettingName`): each is a synonym on the
server's console (`docs/console.md` § Synonyms), so a `server.cfg` line, `+set`, rcon or a vote by
either name sets the one setting, and the two can never disagree. A name may stand for one word of
a setting that holds several. Shooter's Edge's table serves both classic games, since their names
do not clash with each other, only with ours:

| Ours | The first classic game's | Quake II's |
| --- | --- | --- |
| `sv_hostname` | `hostname` | `hostname` |
| `sv_maxclients` | `maxclients` | `maxclients` |
| `sv_maxspectators` | `maxspectators` | `maxspectators` |
| `sv_password`, `sv_spectatorpassword` | `password`, `spectator_password` | the same |
| `sv_timeout` | `timeout` | `timeout` |
| `sv_maprotation` | | `sv_maplist` |
| `sv_floodprot` | `floodprot` | `flood_msgs`, `flood_persecond`, `flood_waitdelay` (its three words) |
| `sv_floodprotmsg` | `floodprotmsg` | |
| `sv_admin` | `sv_admininfo` | |
| `sv_country`, `sv_city`, `sv_coords` | `countrycode`, `city`, `coords` | |
| `sv_publicaddr` | `hostport` | |

**Every classic server setting under our names** (M58). The classic server program's own
settings are sorted into three: ported under our names with the classic name as a synonym, covered
by something of ours that does it better (not ported), and not ported (no use here). The working
list is `porting/shooters-edge/server-inventory.md`. Ported so far, Shooter's Edge's endpoint
rules unless marked engine:

| Ours | Classic | What |
| --- | --- | --- |
| `sv_download`, `sv_download_skins`, `_models`, `_sounds`, `_maps`, `_other` | `allow_download*` | which folders a client may download |
| `sv_downloadchunks` | `sv_downloadchunksperframe` | 1 KB chunks a downloading client gets a frame, 1..30 |
| `sv_country`, `sv_city`, `sv_coords`, `sv_publicaddr` (engine) | `countrycode`, `city`, `coords`, `hostport` | where the server is and the address a browser shows, in the server info |
| `sv_watervis` | `watervis` | 1: clients are told they may see through water |
| `sv_defaultname` | `sv_default_name` | the name a client with an empty or unusable name gets (`player`) |
| `sv_unfake` | `sv_unfake` | 1: carriage returns in a public chat line become `#` |
| `sv_colorkick` | `sv_kicktop` | 1: more than 6 top colour changes in 8 s kicks |
| `sv_infoflood` (`"N S"`) | `sv_kickuserinfospamcount`, `sv_kickuserinfospamtime` (one word each) | more than N user info changes in S s kicks (`300 3`) |
| `sv_fullspectate` | `sv_forcespec_onfull` | a player joining a full server: 0 refused, 1 a spectator unless `svf` bit 2, 2 only with `svf` bit 1 |
| `sv_reconnectwait` | `sv_reconnectlimit` | seconds before a connected address may connect again; a storm gets no answer |
| `sv_iplimit` | (none) | the most seats one IPv4 address or one IPv6 /48 network may hold, a slot being freed counting a half, loopback and link-local never counted; 0 (Quake's default) no limit, 3 Quake II's (`engine/server/admission.hpp`) |
| `sv_locked` | (none) | 1: every new client refused ("Server is locked."), one coming back to the seat its address still holds let in, so a match's players can drop and return while nobody else joins; changed while the server runs by the admin line `sv_locked 0` or `1` over the remote console; 0 (the default) open (`engine/server/admission.hpp`) |
| `sv_redirect` | `sv_redirect_address` (Quake II) | an address a newcomer is sent on to instead of being refused for a full server ("Server is full. Redirecting you to ...", and their client connects there, at most three times in a row); with a leading `!` every newcomer who is not a VIP is sent there, full or not; empty (the default) nobody (`engine/server/admission.hpp`) |
| `sv_ruleset` | (none) | the competition ruleset a player must keep to (`smackdown`, Quake II's `duel`; the client sends its own as `*rsn` or `rsn`): a newcomer on another is refused a player's seat ("This server requires the smackdown ruleset. Pick it in OPTIONS or type ruleset smackdown, then join again."), a spectator never; the server info carries it as `ruleset` and the settings sheet shows it as a fixed row; empty (the default) any (`engine/server/admission.hpp`) |
| `sv_mapcmd` | `sv_changemapcmd` (Quake II) | a console line the dedicated server runs once after each map loads, the first one too (`exec map.cfg`, `say` a greeting, a setting for every map); empty (the default) none (`engine/server/server.hpp`, `server_map_loaded`) |
| `sv_status` | (none) | what an out-of-band `status` query is answered with: 2 (the default) the server's info and a line per player, 1 the info alone, 0 no answer (`engine/server/status.hpp`; Quake II's `sv_status_show`) |
| `sv_statusrate` | (none) | how often `status` is answered, `limit[/period][*burst]` as `sv_rconrate`; QuakeWorld's default 0 (no limit), Quake II's 15 (its `sv_status_limit`) (`engine/server/status.hpp`) |
| `sv_uptime` | (none) | 1 adds `uptime` to a status answer's info, short (`MM.SS`, `H:MM.SS`, `D+H:MM.SS`), 2 in words ("1 day, 2 hours, 3 mins, 4 secs"); 0 (the default) none (`engine/server/status.hpp`) |
| `sv_fps` (Quake II's rules) | `sv_fps` | the rules' frames a second from the next map, 10 (the default) to 60 by tens: doors, lifts, shots, drops and pieces step every frame, each mover's tenth spread over the frames so it stands at every tenth where it would at 10; monsters, items, the players' hands, bodies and views and the scoreboards keep ten; stock clients, demos and TV still get ten frames a second, our clients every frame (`*ge_ext` bit 256) (`games/shooters_edge/server/q2_server.cpp`). The engine's own `sv_fps` (`engine/net/net_cvars.cpp`) is not this one |
| `sv_ghosttime` (Quake II) | `sv_ghostime` | the seconds a slot taken by a connect waits for the player's first packet before it is free again, 6 the default (0: only `sv_timeout`) |
| `sv_nodelta` (Quake II) | `sv_allow_nodelta` | 1 (the default) sends whole frames whenever a player's game asks; 0 drops one that asks for more than 64 ("too many nodelta frames") |
| `sv_pingmethod` (Quake II) | `sv_calcpings_method` | the ping in the scores, `status` and status answers: 2 (the default) the least of the last 16 round trips, 1 their average, 0 none |
| `sv_maxents` (Quake II) | `sv_max_packet_entities` | the most entities one frame sends a player, 0 (the default) as many as the packet holds; past it the rest wait for a frame with room (our own rules; a hosted game library's frames are not capped) |
| `sv_entpriority` (Quake II) | `sv_prioritize_entities` | which entities a capped frame keeps: 0 (the default) the lowest numbers; 1 players, brush models and anything alive first, pieces and model-less entities last, the nearest first in each |
| `sv_novis` (Quake II) | `sv_novis` | 1 sends every player every entity, behind walls too; 0 (the default) only what the player may see or hear |
| `sv_vispatch` (Quake II) | `map_visibility_patch` | 1 (the default) mends the visibility of four stock maps (q2dm1, q2dm3, q2dm8 and one of the second mission pack's), known by the map file's checksum, so a player at a few spots is no longer hidden from one nearby; 0 the maps as they were made. Applied as each map loads |
| `sv_entpath` (Quake II) | `map_override_path` | a folder under the game's whose entity files replace a map's own entities as it loads: `<map>@<crc>.ent` (the CRC-16 of the map's own entities, in four hex digits) first, then `<map>.ent`; empty (the default) none. A file under 2 bytes or one that does not parse is refused and the map keeps its own; the log says which file was loaded |
| `sv_maxping` | `sv_maxping` | a player over this ping (ms) on entering plays as a spectator |
| `sv_minping`, `sv_minpingcmd` | `sv_minping`, `sv_enable_cmd_minping` | each player's packets held until its ping reaches this; players may set it with `minping` before a match |
| `sv_spectalk` (the mod's rule) | `sv_spectalk` | 0: during a match a spectator's `say` reaches only spectators, as `[SPEC] <name>:`; a spectator's `say_team` always does. The engine's chat route (`engine/server/chat.hpp`), when the mod leaves chat to the server |
| `sv_sayteam_to_spec` (the mod's rule) | `sv_sayteam_to_spec` | when spectators read the team reports of the team they follow: 0 never, 1 in a match, 2 outside one, 3 always; a line typed by hand stays private (`engine/server/chat.hpp`) |
| `sv_minrate` | `k_minrate` (the mod's own; no synonym) | the fewest bytes a second a client may ask for; a rate out of `sv_minrate`..`sv_maxrate` is reported and set back; Quake's defaults 0 and 0 (none), Quake II's 15000 and 60000 (its `sv_min_rate`, `sv_max_rate`) (`engine/server/rate.hpp`) |
| `sv_lanrate` | (none) | 1: a client from a private, loopback or link-local address held to neither `sv_minrate` nor `sv_maxrate`, so a LAN plays at each client's own rate; 0 (the default) every client alike (`engine/server/rate.hpp`; Quake II's `sv_lan_force_rate`) |
| `sv_safestrafe` | `sv_safestrafe` | N: a reversed sideways move is zeroed until N commands of none lie between; in the server info |
| `sv_antilagexact` | `sv_antilag_no_pred` | 1: the rewind goes to exactly the frame the shooter saw, no lead |
| `sv_antilag` | `sv_antilag` | 1: the shots the game marks are traced where the shooter saw the others (ours mark every hitscan); 2: every shot's line; in the server info |
| `sv_antilag_projectiles` | `sv_antilag_projectiles` | 1, with `sv_antilag 2`: a player's missiles in flight meet the others where he saw them too |
| `sv_antilag_max` | — | no rewind further back than this many ms (0: as far as the history goes, the default; Quake II's 250) |
| `sv_voipecho` | `sv_voip_echo` | 1: a speaker hears his own voice back |
| `sv_earshot` | — | how near (map units, 1000) a player must stand, with no wall sound cannot pass, to read `say_near` and hear a voice aimed `near` (0: neither) |
| `sv_rconcrypt` | `sv_crypt_rcon` | 1: the remote console takes only a hashed password (SHA-1 of the password, the time and the line); 0 takes either |
| `sv_rcontime` | `sv_timestamplen` | seconds a hashed line's time may be off the server's clock (0: not checked; 60) |
| `sv_rconrate` | `sv_rconlim` | how often the remote console is answered, `limit[/period][*burst]` (a period of seconds or `sec`, `min`, `hour`; 1 s and 5 unset): a bucket of tries refilling, a good password's try given back, a try past it not answered; Quake's default 10*10, Quake II's 1 (its `sv_rcon_limit`); 0 no limit (`engine/server/limit.hpp`) |
| `lrcon_password` | (none) | a helper's remote console password: it runs only the lines that start with a prefix the administrator allowed with `lrcon_add <prefix>` (`lrcon_del`, `lrcon_list`; Quake II's `addlrconcmd`, `dellrconcmd`, `listlrconcmds`), anything else "This command is not permitted."; empty (the default) none; never passed to a mod or shown (`engine/server/lrcon.hpp`) |
| `sv_passwordrate` | (none) | how often a connect with a wrong player's or spectator's password is answered, as `sv_rconrate`; Quake's default 0 (no limit), Quake II's 1 (its `sv_auth_limit`) (`engine/server/limit.hpp`) |
| `sv_banmode` | `filterban` | 1: the ban list refuses its addresses; 0: only its addresses may join. The list is the engine's (`engine/server/bans.hpp`): `addip <pattern> [ban\|safe] [+seconds\|time]`, `removeip`, `listip`, `writeip`, saved to the game folder's `listip.cfg` and read back at start |
| (the blackhole list) | (none) | addresses the server never hears: every datagram from one is dropped unread, a query's and a connect's alike. `blackhole_addip <pattern> [+seconds\|time]`, `blackhole_removeip`, `blackhole_listip`, `blackhole_writeip` (Quake II's `addblackhole`, `delblackhole`, `listblackholes`) (`engine/server/bans.hpp`) |
| (the command filters) | (none) | client commands the server refuses: `filtercmd_add <command> [ignore\|log\|print\|stuff\|kick] [text]` (`print` shows the client the text, `stuff` runs it on the client's console, `log` and `kick` write `<name>[<address>]: issued banned command: <command>`), `filtercmd_remove <command\|number\|all>`, `filtercmd_list` (Quake II's `addfiltercmd`, `delfiltercmd`, `listfiltercmds`). The match is the line's first word in any case; the words a client's signon and downloads need are never filtered (`engine/server/cmdfilter.hpp`) |
| (setting and user info bans) | (none) | Quake II's servers judge a client by a console setting's value or a user info key's: `cvarban_add <setting> <match> [log\|print\|stuff\|kick] [text]` (log when none is given), `cvarban_remove <number\|setting\|all>`, `cvarban_list`, `cvarban_check` (asks every player again), and `userinfoban_add`, `userinfoban_remove`, `userinfoban_list` over user info keys (Quake II's `addcvarban`, `delcvarban`, `listcvarbans`, `checkcvarbans`, `adduserinfoban`, `deluserinfoban`, `listuserinfobans`). The match: `!` turns it round, `*` any value, `=N` `<N` `>N` numbers, `~text` contains, `#text` exactly, else the exact value, case ignored. A setting is asked at every map by a stuffed `cmd` line the client's console expands; a user info ban that kicks refuses the connect ("<text> Connection refused.") and the others act at every map and every change. Every matching entry acts in order until a kick (`engine/server/settingban.hpp`) |
| `sv_vippassword`, `sv_vipvalues` | `vip_password`, `vip_values` | words that make a client a VIP, and the level each gives; a VIP skips the passwords and carries `*VIP <level>`. The VIP address list is `vip_addip <pattern> [level]`, `vip_removeip`, `vip_listip`, `vip_writeip` (saved to `vip_ip.cfg`) |
| `sv_maxvipspectators` | `maxvip_spectators` | spectator slots only a VIP may take |
| `sv_vipslots` | (none) | the players' last slots only a VIP may take (Quake II's `sv_reserved_slots`, whose `sv_reserved_password` is `sv_vippassword` and whose server password counts as a VIP's there); 0 (the default) none (`engine/server/admission.hpp`) |
| `sv_idletime` | (none) | seconds a player may press, move and say nothing before being dropped ("<name> was dropped for idling."); 0 (the default) never (`engine/server/idle.hpp`; the engine's `sv_idlekick` is the lobby's unready time) |
| `sv_speedwindow`, `sv_speedwarn`, `sv_speedkick` | (none) | a client's asked move time over real time per window (16 s): logged above warn, dropped above kick ("<name> was dropped for a fast clock."); both 0 (the default) off, meaningful above 1 (`engine/server/skew.hpp`; Quake II's `sv_timescale_time`, `_warn`, `_kick`) |
| `sv_namelimit`, `sv_shownames` | (none) | how often a player in the game may rename: "N S" is N renames in any S seconds (5 60 the default, "N/min" read too, 0 no limit), past which the old name is kept ("You can't change your name too often."); who hears "<old> changed name to <new>.": 0 (the default) nobody, 1 everyone of a player, 2 of a spectator too (`engine/server/names.hpp`; Quake II's `sv_namechange_limit`, `sv_show_name_changes`) |

With `sv_maxspectators` above 0 the spectators have their own slots beside `sv_maxclients`'s
players (the loop's slots are both, at most 32); at 0 a spectator takes a free player slot.

A mod reads the server it runs on by its game's names too: before the rules start, the game puts
`hostname`, `maxclients` and `maxspectators` in the rules' own table, so a bytecode or compiled
mod's `cvar("maxclients")` reads the real number, not a default of its own. A rule only one
game's mods know (Quake II's `dmflags`) is left alone by a map of the other.

## Meta-cvars: one switch for a group

Some settings are a bundle: "a modern server" means lag compensation on, a faster tick, the
newer protocol extensions, spectator chat off. A META-CVAR is a cvar whose value sets a group of
others, declared as data next to the feature that owns it:

```
meta sv_modern 0 ""                                                  // off: each member as under it
               1 "sv_fps 120; sv_maxupdaterate 120; sv_rewindmax 0.2"
               2 "sv_modern 1; sv_maxrate 0; sv_rewindmax 0.1"     // a level may include another
```

- **Explicit beats implied.** A value set at any cfg layer, the command line or the console wins
  over a meta-cvar's, whatever order they ran in; a meta-cvar fills only what nothing set. So a
  mod can say `sv_modern 1` and still keep `sv_fps 72`.
- **Turning one off restores.** The members it set go back to what the layers under it say.
- **Nesting** is a level including another meta-cvar's level; a cycle is refused when the meta is
  declared, naming the loop.
- **Visible.** `--explain` names the meta-cvar behind every value it set; `cvarlist` marks meta
  cvars; `help sv_modern` lists each level's members.

This lives in `engine/console` (`Cvars::add_meta`, the `meta` command; each cvar keeps the value
under the metas, the meta that set it and whether anything explicit did), so a client's settings
can use it too (a "competitive" switch for the HUD). The first is `sv_modern`, declared beside the
network's cvars (`engine/net/net_cvars.cpp`): 1 is `sv_fps 120; sv_maxupdaterate 120;
sv_rewindmax 0.2`, 2 includes 1 and adds `sv_maxrate 0; sv_rewindmax 0.1`. The golden fixtures
in `tests/golden/console_script.txt` (`meta-*`) hold the precedence, the restore, the nesting and
the refused loop; the `server-cfg` lab draws a mod setting `sv_modern 1` over a base that set
`sv_fps`.

A game's dedicated server declares its own levels as data (`ServerGame::metas`, a `GameMeta` row a
level), so `sv_modern` means "a modern server" in every game and each says what that is for it.
Only how the network judges play is in it, never a game's feel. Shooter's Edge's QuakeWorld: 1 is
`sv_antilag 2; sv_antilag_projectiles 1` (every shot's line and every rocket, grenade and nail
rewound to what the shooter saw), 2 adds `sv_lanrate 1`. Its Quake II: 1 is `sv_antilag 2;
sv_antilag_projectiles 1; sv_antilag_interp 1`. Held by `tests/shooters_edge_qw_game_test.cpp`;
the `se-dedicated-lobby` lab plays its matches with `--modern=1`.

## The lobby and the match's phases

Sessions (`engine/net/session`) already hold the seats, the leader, teams, ready and `lobby` /
`playing`. Above them, what every match-based game shares moves into `engine/server/match`, lifted
from the lobby Shooter's Edge lands first:

```
  warmup ──(all ready, or the leader starts)──▶ countdown ──▶ match ──(limit, or the leader ends)──▶ intermission
    ▲                                              │ break                                       │
    └──────────────────────────────────────────────┴─────────────────────────────────────────────┘
```

- **Warmup** on the map itself (players move and play, scores do not count), with `ready` and
  `break` (unready) as console commands and lobby buttons, `team <name>` and the leader's `start`
  (never with fewer than `min_players` seated). A game with a lobby screen keeps each player on it,
  off the map, until he joins the warmup (`warmup`, `sitout`); the match puts everyone on the map.
- **Countdown**, cancelled by a `break`; the **match**, which the game restarts the map for; an
  **intermission** with the scores, then back to warmup or to the next map in the rotation.
- **The game decides what each phase means** through `ServerGame`'s phase events; the engine owns
  the transitions, the commands, the timers (`sv_countdown`, `sv_intermission`) and what the
  lobby shows. A free ride (Fall Line's pilot) skips the match and stays in an endless warmup with
  the lobby's picks.
- **The core sees each phase** through `ServerGame::phase`, read after every frame: `Server::phase`
  and its changes (`phase_changes`, each at its frame and clock), and the status record's phase
  (warmup as the lobby, the countdown, the match as playing, the intermission), so a hub shows a
  server's match as it goes. The server's features act on it; the first is `sv_autorecord`.

### Every match kept

`engine/server/autorecord.hpp`. With `sv_autorecord 1` the dedicated server records each match by
itself: a recording begins at the frame after the phase turns to the match (the first its players
see of it: a map restarted for the match shows from there) and ends when the phase leaves it, to
`demos/<map>-<YYYYMMDD>-<HHMMSS>.ged` in the game's folder, written as it goes, so the warmup and
the scores after are in no file and a server stopped mid-match keeps every whole second. A game with
no phases is in its match from the start. QuakeWorld's and Quake II's servers both tell their match's phase
(`sv_lobby`), so each of their matches is a file of its own. Beside `--record` and the server's own TV, every
recording takes the same frame bytes, which the game is asked for once a frame
(`Server::tv_frame_bytes`): a game whose frame hands out what its viewers have not had yet, as
Shooter's Edge's classic game does, would otherwise give each recording only part of a frame. In
the `se-dedicated-lobby` lab Shooter's Edge's dedicated server with the lobby and three bots plays
two matches in 60 s: two files of about 19 s each, every one of 11,688 views played back equal to
the live server's.

`engine/server/hand_record.hpp`. `serverrecord <name>` on the server's console records the match
from that frame, every player's part, to `demos/<name>.ged` in the game's folder, and `serverstop`
ends it; a mod switch or the server's end closes it too. It is a stream of its own beside
`--record`, the TV and `sv_autorecord`. A name with `..`, `/`, `\` or `:` is refused, and so is a
second recording while one runs.

## Votes: the players change a setting for one map

Any player may ask everyone on the server to change a setting until the map changes
(`engine/server/vote.hpp`, every game's): `vote fraglimit 30` calls the vote, `vote yes` and
`vote no` (or `yes` and `no` alone while one is open) answer, `vote` says what is open and
`vote rules` what may be voted on.

- **More than half say yes.** The voters are the people on the server, players and spectators,
  never bots. A vote passes the moment its yes are more than half of them, fails the moment half
  say no, or runs out after `sv_vote_time`. The caller's own ballot is yes, so a player alone
  passes one at once; someone leaving takes their ballot along and the vote is weighed again
  among those left. One vote at a time; a player whose vote failed waits `sv_vote_wait`.
- **For this map.** A passed change lasts until the map changes: then every change goes back to
  what the server had (newest first, each to its value before the map's first vote on it). A
  restart of the same map (the lobby's match starting) keeps it.
- **Which settings.** A name on `sv_vote_deny` never; with `sv_vote_allow` empty every other one,
  else only those it names (`*` any characters). Past the lists the game decides: a setting it has,
  a value in its range. A game names what a change touches (`key`), so a per-player grant (the
  easy jump for one player) is voted on like a setting.
- **The game makes the change.** `vote_take` hands it the passed change and `vote_map_over` what
  to put back. Shooter's Edge writes it into the rules every map is made from: a mod's rule
  (`fraglimit`, `teamplay`) takes effect at once (a limit the warmup holds, at the match), and a
  setting the players' moves are predicted with (`sv_gravity`, `pm_speedjump`) restarts the map so
  every client is told it. The endpoint's own settings (read when the server starts) are refused.
- **Acts: a kick and another map.** A game may mark a change an ACT (`VoteCheck::act`): made once
  when it passes, never kept for the map or put back. Shooter's Edge takes `vote kick <id|name>`
  (the player removed, as the owner's `kick`; never a bot or the host's own player; he may come
  back) and `vote map <name>` (a map the server has: everyone moves there as at a level change).
  `sv_vote_deny kick map` keeps both to the owner.
- **The easy jump for one player.** Shooter's Edge takes `vote speedjump <id|name> on|off|<share>`:
  the administrator's grant (`speedjump`, disclosed in the player's user info) made for this map
  by the players. Its key is `speedjump:<user id>`, so each player is his own change; the next map
  puts back what he had (an administrator's grant comes back, a voted one goes). A player who has
  left is passed over. `sv_vote_deny speedjump` keeps it to the administrator. Every player aid
  is voted on the same way (below).
- **Midair, a match mode.** `k_midair 1` (and `k_midair_minheight` 0..4) turns a deathmatch into
  the rocket duel where only a hit on a player in the air counts: every spawn holds the rocket
  launcher with rockets that never run out, 250 health and 200 armour, nothing lies on the map, a
  rocket on another player at least the least height (64, 128, 256, 512 or 1024 units) above the
  floor kills him and scores 1, 2, 4 or 8 by how far he was above where the shooter fired, and the
  match's end tells each player's midairs. The rules are one C++ file every game's rules call
  (`plugins/shooter/midair.hpp`); Quake II's rules and Quake's arena
  (`games/shooters_edge/rules/common_midair.cpp`) play it. A vote on `k_midair` restarts the map into or out of the mode; on rules that do not
  play it the vote is refused.

## The settings sheet: the server's settings browsed in the menu

OPTIONS ▸ SERVER SETTINGS shows a player the server's settings with their values now, the ones
they can change, and how (`engine/server/sheet.hpp`, every game's). The server's answer is a
SHEET: rows of `{name, words, group, value, values, help, line, how}`, `values` in the console's
grammar (`0: no limit; 1..N: that many frags`), `line` the words that change it (`%v` the value,
`up|down` a stepper, no `%v` a toggle sent as it is) and `how` one of fixed (the owner's), direct
(changed at once by a word, a match mod's `dmm4`), vote (asked of everyone) or admin.

- **The pages.** The first holds the map and the match's rules (mode, deathmatch mode, teamplay,
  the limits, overtime, midair), always shown so the limits are on view; every other row is on a
  subpage named by its `group` (PLAYER HELP: the rocket jump helpers, the aim help and each
  player's own grant; MOVEMENT; RULES). A row is offered only when the player can change it: the
  vote's lists and the game's check decide (`sheet_how_by_vote`), and a setting the server is
  started with is never offered.
- **Votes are starred.** A row a vote decides reads `FRAG LIMIT*`, with a legend line under the
  first page. Turning a direct row sends its words at once; turning a vote row waits until the
  focus leaves it or the page is left (ESCAPE included), then asks CALL A VOTE? with the old and the
  new value: YES sends `vote <name> <value>` (the row's `line`), NO puts it back. Rows the player
  cannot change are dimmed with the reason.
- **Where the sheet comes from.** A listen server's window reads its own match through
  `ServerGame::sheet`. A client of our QuakeWorld server that agreed the `ge` set's
  `settings-sheet` bit asks with `ge_sheet` and gets svc `ge_sheet` (begin, a row a message, end,
  a few each frame so the reliable channel never overflows) and the sheet again whenever a vote
  changes a setting; `ge_sheet stop` ends it. Our Quake II server has no reply to a client's
  bits, so it OFFERS the sheet instead: a client whose `*ge_ext` carries the bit gets an empty
  sheet on svc 120 as it enters (begin, end), and only a window that saw that offer sends
  `ge_sheet` (a stock Quake II server would echo the word as chat). Its parts go as the reliable
  channel drains, so a packet stays within the 1400 bytes a Quake II client reads. Its votes
  (`games/shooters_edge/net/q2_votes.hpp`) are the engine's: every rule but the owner's
  (`deathmatch`, `coop`, `skill`, `sv_maplist`, `nextserver`), `vote map`, `vote kick`, put back
  at the next map. A stock server is read, never extended
  (`plugins/classic/server_sheet.hpp`): a QuakeWorld server's serverinfo (a key missing is 0), a
  match mod's listing (`cmd status1`, `cmd status2`, taken out of the console for a few seconds
  while it comes) with its own words (`dmm1`..`dmm5`, `fragsup`, `votemap`); a Quake II server's
  `info` and a match mod's `settings` listing, never a word the game would broadcast as chat.
- **The window's side** (`games/shooters_edge/net/qw_remote_sheet.hpp`): `serversheet
  open|refresh|changed|close` is what the page says; the window asks once when it opens, every
  half second while it is open, at once after a change, and lets the sheet go when the player
  leaves.

## Timeouts: the match stopped for a while

`engine/server/timeout.hpp`. A player in the match says `timeout` and the game stops for everyone
for `sv_timeoutlength` seconds; the time left is said at 30 and 10 seconds, then the server counts
`sv_timeoutresume` seconds back to play. The side that called it (the team in a team game, else
the player) may say `timein` to start that count at once; nobody else may, and a player's own
`pause` is refused while it lasts. Each side has `sv_timeouts` a map, and gets them back at the
next. Spectators, bots and a warmup are refused with the reason. The clock is the server's own,
which runs on through the pause; the game stops and starts its world as the book says (Shooter's
Edge pauses its rules as the server's own pause, which no player's word undoes). The status line
says `timeout=out` or `timeout=resuming` while one is on.

**Quake II's match** (`games/shooters_edge/net/q2_match.hpp`) plays the lobby, the timeouts,
overtime, prewar and the lock as QuakeWorld's server does, on the rules both share
(`games/shooters_edge/server/match_rules.hpp`). Its players' own words are taken too: `unready`
and `noready` for `notready`; `time`, `calltime`, `ctime` and `hold` call a timeout and, from the
caller, end it. Its rules add a tournament's gameplay switches: `g_fast_weap_switch`,
`g_teleporter_nofreeze`, `g_spawn_mode`, `g_removed_weapons`, `g_removed_powerups` and
`g_warmup_loadout` (each one's help says what it does). While the rules are paused the server
keeps sending what is said, so a player's next word is heard.

## Teams and captains

`engine/server/captains.hpp`. A team match's two teams over the lobby's red and blue, named by
the game, each with a captain: the first to join it, then the lowest seat left when he goes. A
captain (and the referee, on either team) locks and unlocks the team, renames it in the warmup,
invites a spectator (`invite`, taken with `accept`, even into a match under way or a locked
team), picks one straight on when the server allows it, and removes a teammate to watch.
`team` and `join` take `1`, `a`, `2`, `b` or a team's name, refused while the team is locked or
full. The book asks the game to seat or unseat a player and says every line; the shuffle deals
the seats again from a seed. Quake II's server plays it with `dmflags` 64 or 128, every player
in his team's look.

## Elections and picks

`engine/server/election.hpp`. A role a player asks the others for (a captain, a coach, an admin)
is an election: ONE is open on a server at a time whatever it is for (the role is data: its name
and a team), the people playing are the voters and answer `yes` (`no` takes it back), the
candidate's own vote counts when he is one (a coach who watches is not), and it passes on the
round-up of a percent (51 to 100) of the voters, at least two. It times out after 60 seconds, the
candidate may call it off, and he waits 30 seconds before he asks again. The game gives the role
when one passes and says the lines in its own words.

`engine/server/draft.hpp`. Two captains picking the other players in turn: each waiting player
has a number (1, 2, 3... in slot order), the captain who picks first is drawn from the seed, then
A, B, B, A, B, A...; the last one goes by himself, a late joiner takes the lowest free number,
and a captain who leaves ends it. Our QuakeWorld server's captain mode is both in QuakeWorld's
words (`games/shooters_edge/net/qw_captains.hpp`): `captain` before a match in team play, the
numbers shown as frags, a pick by the number keys, each pick taking the captain's team name and
colours, nobody firing and nobody changing team while they pick.

## Coaches

`engine/server/coach.hpp`. A spectator who wants to help one team asks for it in the warmup of a
team match (`coach <team>`); the seated players answer `yes` (`no` takes it back) and he is elected
when the yes reach 51 % of them, 2 at least, within 60 s. One coach a team, at least 3 players on
the server, one election at a time and never beside a vote; a candidate who withdraws or runs out
of time waits 30 s. A coach is in his team's team chat both ways (his lines read "(Coach <name>)",
`engine/server/chat.hpp`) and his chase camera follows only his team's players; nothing else: no
captain's word. He stops with `coach` again, by leaving or by taking a seat; he stays through the
match and the map's change. The referee names one with no vote (`coach <team> <player>`) and takes
one off (`uncoach <team>`); `coaches` lists them. Quake II's server plays it.

## The referee

`engine/server/referee.hpp`. A player who gives the server's administrator password in the game
(`referee <password>`, or `admin`) runs the match without a vote: `endmatch` (or `break` in a
match), `tl <minutes>`, `changemap <map>`, `kick`, `kickban` and `ban <player> [minutes]` (60 by
default, 720 at most), `mute <player> [minutes]` (5 by default, 60 at most) and `unmute`,
`shuffle`, `readyall` and `unreadyall`; `acommands` lists them. A wrong password is refused and
logged; a referee is never kicked, banned or muted. The book hands the game each act to do.

A game keeps its own words (`RefereeWords`): its login word, its spellings of the book's words,
the wait after a try, how a player is named, a kick's reason and ban, and every line said; a word
its table leaves out is not the book's and reaches the mod. Our QuakeWorld server's admin is the
book in QuakeWorld's words (`games/shooters_edge/net/qw_referee.hpp`): `admin <code>` against
`k_admincode` (`k_admins 0`: no admins; a wrong code holds the next try 5 seconds), `forcebreak`,
`forcemap <map>` (refused during a lobby's match and for a map the server lacks), `kick <id or
name> [reason]` (the address kept out 30 seconds), and QuakeWorld's own `forcestart` (a 10-second
count, then the unready dropped and the match counted down) and `lock` (joiners watch for 15
seconds). A mod's own program keeps its own admin: the words go to it untouched.

## Player aids: help for one chosen player

Some players need help the rest do not: a player with a disability that makes fast hand motion
hard, a player on a phone's touch controls. A server gives such help to a chosen player and to
nobody else (`engine/server/aids.hpp`, every game's), and says so to everyone.

- **The game says which aids it has** (`AidKind`: the word, the words a player reads, the
  server-only user-info key a grant is kept in, what `on` grants, whether a share 0..1 may be
  given). Shooter's Edge has three, for every classic game it plays: `speedjump` (the easy jump: the
  client strafes for him), `aimhelp` (the aim help: the server bends his hitscan shots) and `rjump`
  (the rocket jump helper, `+rjump` and `+rrjump`).
- **Off until granted.** An aid acts for a player only when it is granted to him: by a vote of the
  players (`vote aimhelp <player> on`, keyed `aimhelp:<user id>`, for the map) or by the
  administrator (`aimhelp <id|name> on|off` on the server's console or over `rcon`). A server
  setting may turn an aid on for everyone where the aid has one (`pm_speedjump`, `sv_rjump`);
  the aim help has none.
- **Disclosed.** A grant is the aid's key in the player's info (`*aimhelp 1`), which a client
  cannot set itself; everyone is told when it is given or taken back; a player who joins is told
  every grant on the server with its numbers ("Player aids on this server: bert has aim help (24
  across, 48 up and down).").
- **`users` shows them.** The players list is the engine's (`users_text`), the same in every game:
  a row a seat with the user id, the frags, the name and the seat's aids, each with how it is
  allowed (granted, or on for all) and whether it is used (`cl_speedjump 1`, `3 shots helped`).
- **The aim help** (`plugins/shooter/aim_help.hpp`): a granted player's hitscan shot (bullets,
  pellets, beams, the rail, a blade) that would pass beside, above or below another player by no
  more than the leeway is bent onto him. Each shot is bent on its own, so a weapon held down (a
  beam each frame) follows him while the aim stays within the leeway. The leeway is the server's:
  `sv_aimhelp_x` sideways and `sv_aimhelp_y` up and down, in units at the target (24 and 48; 0
  turns that axis off, so `sv_aimhelp_x 0` is the vertical help alone). A wall between, a teammate
  under `teamplay` and a line that already hits are never bent onto. The classic mods' servers bend
  the line a mod traces for its shot (`shot_help.hpp`); Quake II's server bends its own lines.
- **Missiles are led** (`aim_lead`): a rocket, a grenade or a nail he launches is flown ahead (a
  step 1/128 s, gravity for a thrown one) beside each other player foreseen the same way; when it would pass him by no more
  than the leeway at the moment it reaches him, it is sent where he will be instead: a straight
  missile at its own speed, a thrown one on the arc that lands on him after the same flight. The
  classic mods' servers lead the missile the loop has just spawned (`Server::missile_watch`); the
  Quake II's leads its own launch.
- **How a player is foreseen** (`lead_mover`) was chosen on recorded matches: the `se-aimlead`
  lab replays every player's real path in a folder of multiview recordings, fires a rocket, a
  grenade and a nail at him from where the other players stood, leads each by one rule and flies
  it against where he really went. Over 13 recordings (155,518 shots a rule) the winner takes his
  velocity as it is and his jump's arc falling to the ground under him, leads nobody slower than
  150 units/s across (`kLeadRun`) and foresees his run at most 0.6 s ahead (`kLeadReach`). A rocket
  hits 23.4% against 19.0% unled (41.6% against 24.8% from 150 to 400 units); a target in the air
  16.2% against 9.8%; a grenade 12.8% against 11.8%. Means over a window (0.1 s to 1 s), an
  exponential mean, a fading run, acceleration and the sway's centre all scored lower; the lab
  reads the air from the path alone (its steps falling by the gravity, the floor at the takeoff),
  where the server knows his footing. Past 1000 units no lead still scores higher (16.8% against
  15.3%): a player turns during a long flight.
- **The view turns with the help** (`plugins/shooter/aim_turn.hpp`): the server tells the player
  it helped whom it bent a shot onto (the game's own message, svc 122 in the `ge` extension set;
  a classic client is never sent it), and our client turns his view toward that player with a
  spring while he holds the attack, and back to his own aim after he lets go. The mouse keeps
  turning the view throughout, and the target is let go when it strays more than 64 units off
  his own aim. What the client sends is the turned view, so the server's recording and anyone
  watching see a person's aim. `cl_aimhelp_turn 0` keeps the view still; the `se-aimturn` lab
  charts it on a strafer.
- **The weapons it may bend** are the server's (`sv_aimhelp_allow`, `sv_aimhelp_deny`: weapon
  names, spaces or commas between, `*` any characters; deny wins, an empty allow list allows the
  rest), named by the weapon held: `axe sg ssg ng sng gl rl lg` in Quake, `blaster sg
  ssg mg cg gl rl hb rg bfg` in the q2. `sv_aimhelp_deny "rl lg"` keeps it off the two
  strongest; a casual server leaves both lists empty.

## Recording: the match kept, played back and relayed

`engine/server/record.hpp` and `playback.hpp`. A game whose `ServerGame` fills `tv_frame` and
`tv_keyframe` is recorded as it runs: `record_begin`, `record_frame` after each server frame,
`record_finish` at the end, into a replay file (version 2, `docs/tv.md`) with a block a second and
the whole state as a keyframe every few seconds. The dedicated server writes it with
`--record=PATH` and prints the bytes and frames at the end. The same bytes feed a TV node as they
are written, so a source with a delay, its relays and their viewers see the match live
(`docs/tv.md` § The server as a source).

A recording plays back from ANY player's view, not one client's. The game adds the watch hooks:
`watch_open` and `watch_close` for a viewer's state, `watch_keyframe` and `watch_frame` to keep it
off the stream's chunks, `watch_players` and `watch_view` for what a player saw, and
`player_view` for the same bytes off the live game. `playback_open` reads the file and refuses
another game's; `playback_seek` starts at the newest keyframe before the frame and steps from
there, `playback_step` goes one frame on, and `playback_view` gives a player's view at the frame.
The sample game's view is a bot's own cell and every other bot within 12 cells of it.

The `server-record` lab records the sample match (6 bots, 30 s), plays it back and compares all 6
views at every frame and after a seek every second against the live game's (none differ), then
broadcasts the file through a source that holds it back 2 s and a relay to 5 viewers:

| Preset | Whole | At the server's digest | Lag min / p50 / p95 / max (ms) |
| --- | --- | --- | --- |
| lan | 5 of 5 | 5 of 5 | 2010 / 2030 / 2040 / 2040 |
| broadband | 5 of 5 | 5 of 5 | 2020 / 2040 / 2060 / 2060 |
| wifi | 5 of 5 | 5 of 5 | 2030 / 2080 / 2100 / 2370 |
| mobile | 5 of 5 | 5 of 5 | 2050 / 2130 / 2160 / 2510 |
| bad | 5 of 5 | 5 of 5 | 2100 / 2230 / 2406 / 2580 |
| awful | 5 of 5 | 5 of 5 | 2200 / 2460 / 3675 / 5320 |

No block reaches a viewer before the delay.

Shooter's Edge's classic game fills the same hooks (`games/shooters_edge/net/qw_game_tv.hpp`),
so `sesv --record=PATH` keeps its matches. Its frames are the multiview stream a classic relay
already takes, written by a recorder of the server's own; its keyframe is the catch-up a relay
gives a late viewer; a player's view is his place, angles, model, frame and every stat, written
alike off the live server and off the kept stream. The `se-server-record` lab serves 4 bots on the
arena for 20 s with 8 slots: about 4.2 kB a second, every view of every slot equal at every frame
and after 20 seeks, and every viewer through the 2 s delay and a relay whole and at the server's
last views under every preset. A server with no port (offline) runs no endpoint and records an
empty match.

Quake II's game fills them too (`games/shooters_edge/net/q2_game_tv.hpp`). Its wire speaks
for one viewpoint at a time, so its recording carries every player's state whole instead: each
frame is a view for each of the 32 slots off the rules' world rows (place, angles, model, frame,
health, frags, the held weapon and the 32 stats), and a keyframe is the same bytes, so a seek
needs nothing before it. The `se-server-record` lab serves two clients on a walled hall beside the
classic arena: about 1.25 kB a second, every view equal at every frame and after every seek, and
every viewer through the delay and a relay whole under every preset. It records offline too, since
it reads the rules, not a network endpoint.

### Spectators on the server

`engine/server/spectators.hpp`. A server watched from itself, with no TV process beside it: with
`sv_tvport` set (`--tvport=N`), the dedicated server records its match and feeds the same bytes to
a TV node of its own (engine/tv) on that port, which holds every block back by `sv_tvdelay`
seconds (`--tvdelay=S`, 10 by default) and fans it out to up to `sv_tvmaxviewers` spectators: a
viewer, a relay, a browser's page. A spectator who comes late starts at the newest keyframe out, and
watches any player's view through the game's watch hooks, as a recording is watched. The delay is
what keeps a spectator from telling a player where his opponent is, so no vote may change it
(`sv_tv*` is on the default deny list). A server beating to masters lists its TV beside it, as a
TV node of the game's protocol naming the server as its source, so a hub shows a Watch on the
server's card. The status line counts the spectators (`tv=N`); at a mod switch the broadcast ends
and a new one starts with the next match on the same port; when the server stops, the TV runs on
until every spectator has the match's end, or the delay and 2 s more have gone.

A block goes out the delay after it opened, counted from when the first block came whole, so a
spectator is the delay and the second a block takes to fill behind the match. In the
`server-record` lab the sample game served for 30 s with a 2 s delay, two spectators from the
start and one halfway: every one whole and at the server's last digest under every preset, none
earlier than the delay, the one who came at 15 s starting at the keyframe of 10 s:

| Preset | Whole | At the server's digest | Lag min / p50 / max (ms) |
| --- | --- | --- | --- |
| lan | 3 of 3 | 3 of 3 | 3000 / 3016 / 5116 |
| wifi | 3 of 3 | 3 of 3 | 3016 / 3033 / 5233 |
| bad | 3 of 3 | 3 of 3 | 3083 / 3116 / 6516 |
| awful | 3 of 3 | 3 of 3 | 3183 / 3250 / 6566 |

The longest lags are the late spectator's catch-up from its keyframe.

## Fast downloads: the server's files over HTTPS

A player who lacks a map waits for it. Over the game's own channel a file comes in pieces paced by
the client's rate: a base-protocol client at the classic 25000 bytes a second waits 25 s for
aerowalk (632 kB) on a LAN, and a stock server on the internet took over 35 s for one map. So every
dedicated server can also send its files over HTTPS from a TCP port beside its game port
(`engine/server/web_files.hpp` over `engine/net/file_server.hpp`), and a client of any classic
game takes them from there first (`plugins/classic/content/web_downloads.hpp`), the channel behind
it for whatever the web gives back.

| Setting | Default | What |
| --- | --- | --- |
| `sv_http_port` | 0 (off) | the TCP port the files are served on |
| `sv_http_rate` | 20000 | kB a second to every player together, shared fairly among those fetching |
| `sv_http_rate_player` | 5000 | kB a second to one player (one address: their parallel fetches share it) |
| `sv_http_cert`, `sv_http_key` | none | PEM files of a certificate a public authority signed |
| `sv_http_url` | none | the address told (a host name the certificate names, or a proxy in front) |
| `sv_http_plain` | 0 | 1: plain HTTP behind the operator's own TLS proxy |

**Secure with nothing set up.** With no certificate given the server makes its own when it starts
and tells its clients that certificate's SHA-256 beside the address (QuakeWorld's server info
`*dlpin`, Quake II's ` dlpin=` in the connect answer). Our client trusts that one certificate and
nothing else on that connection: the same trust as the game connection the address came over, and
nothing between them can change a byte. Each answer also names the file's SHA-256 (`Repr-Digest`,
RFC 9530) and the client keeps a file only when the bytes match it; a map is then checked against
the server's checksum as every map is.

**What is served** is what the game's channel would send: the game folder played or its base
folder, the kinds of file the game's servers send (never a pack), and the server's own
`sv_download_*` switches (`plugins/classic/content/web_serve.hpp`). A path that climbs (`..`, a
backslash, a colon, an empty part) is refused before the lookup sees it.

**The address told.** `sv_http_url` when set; else `https://<sv_publicaddr's host>:<port>/`; else
`https://{server}:<port>/`, which our client reads as the host it joined. When `sv_downloadserver`
names an outside host (a CDN, a GitHub repository) that is told instead. A real address is also put
in the server info as `sv_dlURL`, the key other QuakeWorld clients read, and Quake II's stock
clients read ` dlserver=`; a client of another engine cannot check a pin, so to serve them too an
operator sets a public certificate and `sv_http_url`.

**The client** asks several files at once (`cl_http_max_connections`), resumes a file cut short
from where it stopped (a `Range` request, three tries), holds every fetch together to
`cl_http_max_rate` kB a second (0: no cap) and refuses a file larger than `cl_http_max_size` MB.
The console's download bar follows the fetch (`web_progress`).

The `se-fastdl` lab times both ways on a real map: `make lab LAB=se-fastdl
ARGS=--map=<path>/aerowalk.bsp`, or a Quake II map with `--game=q2`. Quake II's channel sends one
piece of a packet's room for each `nextdl` the client answers, so a map comes at about 13 kB a
second whichever protocol the client speaks; HTTPS is what makes it fast there.

## The port opened on the home router

A dedicated server run at home sits behind the home router, which drops what the internet sends to
it unless someone forwards the game port by hand. `sv_open_port 1` asks the router to do it
(`engine/server/router.hpp` over `engine/net/portmap.hpp`): PCP first, then NAT-PMP, then UPnP,
from a socket of its own beside the game's. The forward is the game's UDP port, the same port
outside, asked for an hour at a time and renewed at half its life; a lease the router drops is
asked for again. When the server stops, the router is asked to take the forward down.

| Setting | Default | What |
| --- | --- | --- |
| `sv_open_port` | 0 (off) | 1: ask the home router to forward the game's UDP port |
| `sv_gateway` | none | the router's address (port 5351 when left out); none: the machine's default route (Linux), or UPnP's search |

The console says once what came of it: "The router forwards UDP 27500 to this server (pcp):
players reach it at 203.0.113.7:27500.", or, when no router answered, the reason and what to do
(open the port in the router's settings by hand, or set `sv_gateway`). The HTTPS file port
(`sv_http_port`) is TCP, which these protocols' UDP forward does not cover: forward it by hand.
`tests/server_router_test.cpp` runs it against the simulated router (`engine/net/sim_router.hpp`).

## Open for extension

What a server will grow is a lot: everything a mature classic server offers and match mods on top.
The core is built so each is an addition, not a rewrite:

- **Features.** A feature is compiled in and composed by the server's `main` (a list, not a
  registry): its cvars and meta-cvars, its console commands, and its hooks into one ordered hook
  table (a player connects, disconnects, chats, changes their info, runs a command; the frame;
  a phase changes; the server info is written). A hook may answer a command or a chat line and
  stop it there. The first features are the administrator's: rcon over the authenticated link
  (M17i), the status query, kick, ban lists kept in a file, the log.

  BUILT (M53f): `engine/server/feature.hpp` holds the list and its hooks (admit, connect,
  disconnect, chat, info, command, frame, phase, server info) and `Server::features` points the
  core at it; the core calls each hook in the list's order and names no feature. A player's
  console line reaches the server as a session COMMAND on the same authenticated link as chat
  (held to the chat's bucket) and the answer comes back as PRINT lines to that player alone
  (`engine/net/session.hpp`); a line no feature answers is told it is not a command. The
  administrator's features (`engine/server/admin.hpp`) are the dedicated process's list for a game
  on the core's session: `status` (players' without addresses, the console's with them), `kick`,
  the ban list in `sv_banfile` (`addip`, `removeip`, `listip`, `writeip`, `ban <seat|name>
  [minutes]`; a refused address gets no seat, a seated player an entry comes to match is removed),
  `rcon <password> <line>` (the console's printout sent back; wrong passwords counted, three in a
  row make the seat's tries wait), and the log (`sv_logfile`, never a password). A game that speaks
  its own protocol answers its own clients' words (Shooter's Edge's classic endpoint,
  `qw_admin.hpp`) and runs the log alone: what its console prints (joins, drops, refusals,
  bans, a mended map, the rules' lines) goes to the terminal and to `sv_logfile`, each line stamped
  with the server's time (Quake II's server, M18v). The `server-core` lab's
  admin trial runs it all over the simulated network and adds a feature of its own to the list.
- **Recording and broadcasting.** A match recorded from the server (every player's view, not one
  client's), relayed to watchers with a delay, and watched by spectators: features over the same
  hooks and the replay module (`engine/replay`, M23).
- **Server-side match mods.** The match tools a competitive server runs (votes, timeouts, a
  coach, stats, per-mode rules) as a mod over the hooks. The game-agnostic half (votes, timeouts,
  admin) is engine features every game gets; a game's own half is its mod, written as an engine
  mod (`engine/mod`, M52) or, for Shooter's Edge, also as QC progs on its mod server. Shooter's
  Edge ships its own match mod, and runs the classic match mod existing servers use unchanged on
  its QC VM and mod server, with the extension builtins that mod expects (M53h).
- **Modern switches.** Our own server and match mod turn the modern behaviour on through
  meta-cvars, so a classic `server.cfg` runs as it did and one line upgrades it.

What maps onto which classic feature is kept in the working area
(`porting/shooters-edge/servers.md`), never here.

## Decisions

| Decision | Taken | Considered and not taken |
| --- | --- | --- |
| Where the server lives | `engine/server`, one core for every game | Each game its own (today's Shooter's Edge copy): Fall Line would write it again |
| Listen and dedicated | One `Server`, stepped by the client loop or by `main` | A listen server as a special client path: two code paths to keep agreeing |
| One binary per game | `ge_add_server` links the engine's `main` with the game | One server that loads games as libraries: a game's code is not a plugin, and replays need the game's own build |
| The game's seam | A table of function pointers, called once a tick | Virtual classes (the tree's style avoids them); templates (every game's server compiled into the engine's headers) |
| Config precedence | Layers in a fixed order, every game directory's `server.cfg` read, the source of each value kept | Only the winning `server.cfg` (a mod would have to copy the whole base file) |
| Meta-cvars | Explicit beats implied, whatever the order | Last write wins: a mod's `sv_modern 1` after its own `sv_fps 72` would silently undo it |
| Features | Compiled in, composed by a list in `main` | Registered at run time: against G8 and the determinism contract |
| The lobby | Lifted into the engine after Shooter's Edge's lands | Built generic first: the game's working lobby is the honest spec |

## Labs

| Lab | Row | What it shows |
| --- | --- | --- |
| `server-core` | M53a | A dedicated server and a listen server running the same game program on the simulated network, clients joining both, digests equal |
| `server-cfg` | M53b, M53e | A base, a mod and a profile `server.cfg` with the explained table drawn as an image; meta-cvars on and off |
| `server-lobby` | M53c | Warmup, ready and break, countdown, match, intermission, rotation, charted per phase, for both games |
| `server-vote` | M53h | People coming and going while votes are called and answered: each vote's count over time against the yes needed, how each ended, every change put back at the map's end, replayed to the bit |
| `se-midair` | M53h | Midair's shared rules charted, and one rocket at a target at each height on Quake II's rules: counted or not, the frags, the line, the lift; on a hall or a real map's slice |
| `server-record` | M53g | A sample match recorded, played back from every player's view against the live game's, relayed with a 2 s delay to viewers, and watched by spectators on the server's own TV, under every network preset: whole, at the digest, the lag |
| `se-server-record` | M53g | Shooter's Edge's classic and Quake II servers recorded by the core: every slot's view played back against the live server's, every player's path drawn, relayed with a delay under every preset |
| `server-launch` | M53d | The game spawning a dedicated server, joining it, leaving, finding it again and stopping it |
