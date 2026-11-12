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
`+commands`), `ge_add_server`, a sample game and the server trial, and `shooters_edge_server`
running on it. M53b: `server.cfg` in layers (`engine/server/config`: the base game directory's,
each mod's lowest first, a profile's winning copy, the command line, the console), every server
value's file and line kept (`Cvars::set_watch`), the rest of the server's cvars, and
`--explain`. M53a part two: a game that speaks its own protocol is handed the port
(`ServerGame::serve`, `net_frame`), and `shooters_edge_server` now serves the game's own
deathmatch over the classic protocol (`games/shooters_edge/net/classic_game.hpp`), so a classic
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
build/debug/shooters_edge_server --name="Arena Night" --description="the arena" --port=27500 \
  --maxclients=16 --realtime --for=0
build/debug/shooters_edge_server --root=<install> --map=dm6 --port=27512 --realtime --for=0 \
  +set fraglimit 20 +set timelimit 10
build/debug/shooters_edge_server --root=servers --mod=arena --cfg=duel --explain=sv_
```

The layers are read as loose files under `--root` (`<root>/<dir>/server.cfg`); a copy inside a
pack is not read yet, and `ge server cfg --explain` comes with `ge server` (M53d) over the
binary's own `--explain`.

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
  `shooters_edge_server`. There is no one binary that loads any game at run time: a game is C++
  compiled into its own executables (G8: nothing is registered at run time), and a server runs
  the game's own code so a replay and a client's prediction agree to the bit.
- **Both ways can start the other's work.** The game's HOST page either runs a listen `Server`
  or SPAWNS the dedicated binary as its own process (desktop only: the stores forbid it), hands
  it a cfg, joins it, and can find and stop it again by its local address. The dedicated server
  in turn LAUNCHES matches: a map rotation, a mode, a mod, each switch re-reading the config
  layers below and restarting the match.

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
set it. `ge server cfg --explain [<prefix>]` prints each value with its source and what it
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
| `sv_visibility` | `lan`, `invite` or `listed` (M17r) | server |
| `sv_rendezvous` | the rendezvous to register and list with | server, latched |
| `sv_master` | the masters a community's hub reads (`docs/hub.md`), space-separated, port 27900 when none is given; the server beats to each and answers status asks on its own port, a game on its own protocol too | server, latched |
| `sv_map`, `sv_maprotation` | the first map and the rotation | server info |
| `sv_bots` | seats the game's bots fill at the start | server, latched |
| `sv_lobby` | 1: every map warms up until everyone is ready, then the countdown and the match | server info, latched |
| `sv_countdown` | with `sv_lobby`: seconds from everyone ready to the match (5) | server |
| `sv_intermission` | with `sv_lobby`: seconds the scores stay up after a match before the next map (0: none) | server |
| `sv_mod` | the mod's game directory, mounted over the base | server, latched |
| `sv_admin` | how to reach the server's owner, shown in its info | server info |
| `sv_timeout` | seconds a client may go unheard before it is dropped (65): a dead link, not an idle player | server |
| `sv_floodprot`, `sv_floodprotmsg` | the chat flood rule, `"N S T"`: N lines in any S seconds, else silent for T (`10 1 1`), and what a flooder is told (`engine/server/guards.hpp`) | server |
| `sv_vote` | 1: players may vote on the settings for the map (`vote <setting> <value>`, § Votes) | server |
| `sv_vote_time`, `sv_vote_wait` | seconds a vote stays open (30); seconds a player whose vote failed waits before calling another (30) | server |
| `sv_vote_allow`, `sv_vote_deny` | the settings a vote may change (empty: every one but the deny list's), and those it never may (`*` any characters; the default keeps passwords, rcon, the name, the slots, the address, the vote's own, the mod, the seed, the rotation and the frame rate out) | server |

and the flags `--port`, `--ip`, `--maxclients`, `--name`, `--description`, `--cfg`, `--mod`,
`--map`, `--bots`, `--lobby`, which are the same cvars set at layer 6. The game adds its own (`fraglimit`, a mountain's
seed) the same way.

**A game's own names for these settings** (M53i). A game whose configs, admins and mods already
use other names lists them in `ServerGame::names` (`SettingName`): each is a synonym on the
server's console (`docs/console.md` § Synonyms), so a `server.cfg` line, `+set`, rcon or a vote by
either name sets the one setting, and the two can never disagree. A name may stand for one word of
a setting that holds several. Shooter's Edge's table serves both classic games, since their names
do not clash with each other, only with ours:

| Ours | The first classic game's | The sequel's |
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
| `sv_maxping` | `sv_maxping` | a player over this ping (ms) on entering plays as a spectator |
| `sv_minping`, `sv_minpingcmd` | `sv_minping`, `sv_enable_cmd_minping` | each player's packets held until its ping reaches this; players may set it with `minping` before a match |

With `sv_maxspectators` above 0 the spectators have their own slots beside `sv_maxclients`'s
players (the loop's slots are both, at most 32); at 0 a spectator takes a free player slot.

A mod reads the server it runs on by its game's names too: before the rules start, the game puts
`hostname`, `maxclients` and `maxspectators` in the rules' own table, so a bytecode or compiled
mod's `cvar("maxclients")` reads the real number, not a default of its own. A rule only one
game's mods know (the sequel's `dmflags`) is left alone by a map of the other.

## Meta-cvars: one switch for a group

Some settings are a bundle: "a modern server" means lag compensation on, a faster tick, the
newer protocol extensions, spectator chat off. A META-CVAR is a cvar whose value sets a group of
others, declared as data next to the feature that owns it:

```
meta sv_modern  0 { }                                    // off: every member at its layer below
                1 { sv_antilag 1; sv_fps 77; sv_extensions 2 }
                2 { sv_modern 1; sv_predictworld 1 }     // a level may include another level
```

- **Explicit beats implied.** A value set at any cfg layer, the command line or the console wins
  over a meta-cvar's, whatever order they ran in; a meta-cvar fills only what nothing set. So a
  mod can say `sv_modern 1` and still keep `sv_fps 72`.
- **Turning one off restores.** The members it set go back to what the layers under it say.
- **Nesting** is a level including another meta-cvar's level; a cycle is refused when the meta is
  declared, naming the loop.
- **Visible.** `--explain` names the meta-cvar behind every value it set; `cvarlist` marks meta
  cvars; `help sv_modern` lists each level's members.

This lives in `engine/console` (the cvar's source and the meta table), so a client's settings can
use it too (a "competitive" switch for the HUD), and the server's modern feature sets are the
first users.

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
  (`plugins/shooter/midair.hpp`); the sequel's rules play it today, the classic arena's with their
  move to C++. A vote on `k_midair` restarts the map into or out of the mode; on rules that do not
  play it the vote is refused.

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
  the line a mod traces for its shot (`shot_help.hpp`); the sequel's server bends its own lines.
- **Missiles are led** (`aim_lead`): a rocket, a grenade or a nail he launches is flown ahead (a
  step 1/128 s, gravity for a thrown one) beside each other player foreseen the same way (running
  on at his velocity, or falling until the floor under him); when it would pass him by no more
  than the leeway at the moment it reaches him, it is sent where he will be instead: a straight
  missile at its own speed, a thrown one on the arc that lands on him after the same flight. The
  classic mods' servers lead the missile the loop has just spawned (`Server::missile_watch`); the
  sequel's leads its own launch.
- **The weapons it may bend** are the server's (`sv_aimhelp_allow`, `sv_aimhelp_deny`: weapon
  names, spaces or commas between, `*` any characters; deny wins, an empty allow list allows the
  rest), named by the weapon held: `axe sg ssg ng sng gl rl lg` in the classic game, `blaster sg
  ssg mg cg gl rl hb rg bfg` in the sequel. `sv_aimhelp_deny "rl lg"` keeps it off the two
  strongest; a casual server leaves both lists empty.

## Open for extension

What a server will grow is a lot: everything a mature classic server offers and match mods on top.
The core is built so each is an addition, not a rewrite:

- **Features.** A feature is compiled in and composed by the server's `main` (a list, not a
  registry): its cvars and meta-cvars, its console commands, and its hooks into one ordered hook
  table (a player connects, disconnects, chats, changes their info, runs a command; the frame;
  a phase changes; the server info is written). A hook may answer a command or a chat line and
  stop it there. The first features are the administrator's: rcon over the authenticated link
  (M17i), the status query, kick, ban lists kept in a file, the log.
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
| `se-midair` | M53h | Midair's shared rules charted, and one rocket at a target at each height on the sequel's rules: counted or not, the frags, the line, the lift; on a hall or a real map's slice |
| `server-launch` | M53d | The game spawning a dedicated server, joining it, leaving, finding it again and stopping it |
