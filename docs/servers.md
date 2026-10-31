# Servers

How every multiplayer game on the engine is hosted: one server core in `engine/server` that a
game plugs its rules into, run either inside the player's own process (a LISTEN server) or as its
own headless process (a DEDICATED server), configured by layered `server.cfg` files, with the
lobby before the match and the match's phases in the engine rather than in each game, and seams
for what a server will grow later: an administrator's tools, recording and broadcasting matches,
server-side match mods, and cvars that switch on whole groups of others. Rows M53a–M53h
([`goals/M53.md`](goals/M53.md)).

This page is the design. Nothing in `engine/server` exists yet; what each row builds is in its
row file.

## What is there today, and what is missing

| Piece | Where | What it already does |
| --- | --- | --- |
| The link, the node, channels | `engine/net` (M17a–M17c) | Sockets, peers, reliable and unreliable channels, the simulated network |
| Sessions | `engine/net/session` (M17d) | Seats, the leader, teams, ready, the settings' bytes, `lobby` and `playing`, chat, kick, reconnect by token |
| Hosting and the browser | `engine/net/hosting`, `directory` (M17r) | LAN answers, pings, the router's forward, the rendezvous's list, visibility; "the same for a listen and a dedicated server" |
| Cfg files | `engine/console/cfg` (M21a) | `start_server`: `server.cfg`, then the command line; the server's tier (`kServer`, `kServerInfo`) |
| The search path | `engine/vfs/search` (M18a) | A base game directory, then a directory per mod over it, later sources winning |
| A dedicated server | `games/shooters_edge/server` (M18f) | Shooter's Edge's own host and `shooters_edge_server` binary, with its own flags parser, bots, input log and replay |
| The lobby's match flow | Shooter's Edge (the lobby row in flight) | Warmup on the map, two teams, ready and break, the leader's start, a countdown, the match restarting the map |

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
  `Server` the client loop steps between its own frames, with the host's player on a loopback
  link (`net/local_pipe`) like anyone else; a dedicated server is the same `Server` stepped by
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
| `sv_map`, `sv_maprotation` | the first map and the rotation | server info |
| `sv_mod` | the mod's game directory, mounted over the base | server, latched |
| `sv_admin` | how to reach the server's owner, shown in its info | server info |

and the flags `--port`, `--ip`, `--maxclients`, `--name`, `--description`, `--cfg`, `--mod`,
`--map`, which are the same cvars set at layer 6. The game adds its own (`fraglimit`, a mountain's
seed) the same way.

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
  `break` (unready) as console commands and lobby buttons, `team <name>` and the leader's `start`.
- **Countdown**, cancelled by a `break`; the **match**, which the game restarts the map for; an
  **intermission** with the scores, then back to warmup or to the next map in the rotation.
- **The game decides what each phase means** through `ServerGame`'s phase events; the engine owns
  the transitions, the commands, the timers (`sv_countdown`, `sv_intermission`) and what the
  lobby shows. A free ride (Fall Line's pilot) skips the match and stays in an endless warmup with
  the lobby's picks.

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
| `server-launch` | M53d | The game spawning a dedicated server, joining it, leaving, finding it again and stopping it |
