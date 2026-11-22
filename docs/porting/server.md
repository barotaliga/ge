# Server: the core, the endpoints, settings, admin, match rules, anti-cheat

How a classic game is hosted: the engine's server core it plugs its rules into, listen or
dedicated; the ENDPOINT that speaks the game's own protocol to its clients; settings in layers;
who may join and at what rate; the administrator's tools; the match's phases; and the checks on
what clients send. The design is `docs/servers.md`; this page places a game's pieces in it.

## Shared

| Layer | File | What it gives |
| --- | --- | --- |
| Engine | `engine/server/server.hpp`, `main.hpp`, `spawn.hpp`, `process.hpp` | THE CORE: `ServerGame`'s table a game fills, the frame, node, session and hosting round it; the dedicated process (`ge_add_server`) |
| Engine | `engine/server/config.hpp` | `server.cfg` in layers (base, each mod's, a profile, the command line), `--explain` |
| Engine | `engine/server/rate.hpp` | `sv_minrate`, `sv_maxrate`: a client's send rate held, the player told |
| Engine | `engine/server/admission.hpp` | `sv_iplimit` and VIP slots (`sv_vipslots`, `sv_vippassword`) |
| Engine | `engine/server/idle.hpp`, `names.hpp`, `skew.hpp` | Idle seats dropped (`sv_idletime`), renames limited (`sv_namelimit`, `sv_shownames`), clocks running fast (`sv_speedwarn`, `sv_speedkick`) |
| Engine | `engine/server/admin.hpp`, `bans.hpp`, `guards.hpp`, `chat.hpp`, `feature.hpp` | Status, kick, bans in a file, rcon, the log; the chat flood rule; the chat route with spectators; the feature seam |
| Engine | `engine/server/operator.hpp` | The operator's words any endpoint mounts: a player picked by slot, name or a part of it (`sv_partialnames`), a line's raw rest (`stuff`, `stuffall`, `printall`), the connect and begin stuff lists |
| Classic | `plugins/classic/protocol/info.hpp`, `masters.hpp` | The strict user info rule (`info_strict`); the masters and `listmasters` |
| Engine | `engine/server/match.hpp`, `overtime.hpp`, `timeout.hpp`, `vote.hpp`, `aids.hpp`, `homemap.hpp` | The lobby and the match's phases, overtime, timeouts, votes, player aids granted, the home map |
| Engine | `engine/server/record.hpp`, `autorecord.hpp`, `playback.hpp`, `spectators.hpp` | Every match recorded, played back, watched on the server's own TV node |
| Engine | `engine/server/login.hpp`, `accounts.hpp` | Logins against the server's own service or its master (`online.md`) |
| Engine | `engine/net/guard.hpp` | The guard's score (anti-cheat) |
| Shooter kit | `plugins/shooter/settings.hpp` | The move's and the loop's settings every shooter shares (`sv_gravity`, `sv_maxspeed`, `pm_` ...) |
| Shooter kit | `plugins/shooter/server.hpp`, `frame.hpp`, `client.hpp` | The classic server's world and services; the frame on the run clock; a player's packet and the speed check |
| Shell | `games/shooters_edge/server/host.hpp`, `dedicated.hpp`, `input_log.hpp` | The host: the loop on the run clock, slots, bots, the input log and its hash |
| Shell | `games/shooters_edge/server/lobby.hpp`, `announce.hpp`, `chat_rules.hpp`, `commands.hpp` | The lobby's words, the server's chat for every runner, client commands to the rules |
| Shell | `games/shooters_edge/net/qw_cheats.hpp` | The speed check's cuts scored by the guard, named in the match report |

## Each game's

| | Quake | Quake II | Next |
| --- | --- | --- | --- |
| The `ServerGame` | `net/qw_game.hpp` | `net/q2_game.hpp` | `net/<game>_game.hpp` |
| The endpoint | `net/qw_server.hpp`, its own settings `net/qw_endpoint_cfg.hpp` | `net/q2_net_server.hpp` | its own |
| Admin words | `net/qw_admin.hpp` | `net/q2_admin.hpp`: the modern server's `status` and its letters, kick, kickban, dumpuser, stuff, stuffcvar, printall, killserver, pickclient; `dumpents` and `listmasters` in `net/q2_game.cpp` | its own, over `engine/server/operator.hpp` |
| A player's queries | (the mod's) | `info`, `sinfo`, `lag`, `packetdup`, `stopdl`, `nogamedata` in `net/q2_net_server.cpp` | its own |
| Later protocols | (none) | 35 and 36: the connect's tail (`sv_maxmsglen`), the newer channel, the packed frame, the streamed and compressed signon, batched moves, the client's settings (`net/q2_net_ext.hpp`, `server/q2_wire_ext.cpp`) | its own |
| Connect checks | the endpoint's | the strict user info, a name to show, the name cut to 15, the version probe, the forced reconnect (`sv_forcereconnect`) | its own |
| Logins | `net/qw_login.hpp` | `net/q2_login.hpp` | its own |
| Match rules | `net/qw_match_rules.hpp` (overtime over the mod's clock, prewar, the lock) | the engine's rules over its runner | the engine's |
| Its own server settings | the endpoint's table | its move feel (`sv_landjump`, `sv_waterjump`), powerup drops, the biosuit | its own |
| Defaults | off where the feature never existed | the modern server fork's | the game's own |

## Adding a game

1. A reader's INVENTORY of the game's server settings and commands, tagged original release or
   later fork (`porting/shooters-edge/<game>-server-inventory.md`).
2. A GAPS table ranked against it (`porting/shooters-edge/<game>-server-gaps.md`): one row a name,
   what we do, its layer, its round.
3. Each setting: OUR short name; the game's spelling a synonym only in its own layer
   (`kSettingNames`); a rule row (`GameRule`) and a pure helper in the ENGINE from the start, read
   by every endpoint at start, with a per-client watch where it acts on each client.
4. Defaults per game: the game's own; off in a game where the feature never existed.
5. A unit test of the helper and one integration test per game.
6. A setting that must change live (locking the server) needs an admin or rcon command too: endpoints read
   the rules only at start.

## Brought and left behind

| Benefit, now every game's | From | Layer |
| --- | --- | --- |
| Rates, IP limits, VIP slots, idle drops, speed-skew kicks, rename limits | both games' later servers | `engine/server` |
| Bans, rcon, the server log, chat floods, spectator talk | Quake's later servers | `engine/server` |
| Overtime, timeouts, votes, the lobby, prewar, the home map | Quake's competitive mods and servers | `engine/server` |
| Every match recorded and watchable | Quake's later servers | `engine/server`, `engine/tv` |
| Speed cheats named in a match report and banned by account | ours | `qw_cheats.hpp`, the guard |

| Drawback left behind | How |
| --- | --- |
| Each game's server with its own set of admin tools | one core; each game words it |
| Settings in one flat config | layers with every value's file and line (`--explain`) |
| A speed cheat that gains time | cut to the time that passed; flagged, scored and named |

## Gaps

The ranked gap lists in `porting/shooters-edge/server-gaps.md` and `q2-server-gaps.md`;
spectator slots and team locks in Quake. The classic masters (`setmaster`, `heartbeat`, the
shutdown) are one model for both games in `plugins/classic/protocol/masters.hpp`, each game's
server sending its own form; Quake II's `public` setting is still to come (its ledger). The
lines a Quake server's players read are each game's own (`server/commands.cpp`): the operator's
`say` ("console: " in QuakeWorld, "<hostname> " in Quake), the pause told by name in both (worded
by the state before it in QuakeWorld, so a pause reads "unpaused", as that game does), "Server is
paused." to one who joins a paused game and the release when the last player leaves, the refused
suicide of the dead and of spectators, "<name> overflowed", and `quit` (`engine/server`): every
QuakeWorld player told "server shutdown" in the datagram that drops it, then the masters. A Quake
server also answers `tell` (the first player by whole name, ignoring case, reads "<sender>: <the
whole arguments>", cut to 62 bytes) and `ping` ("Client ping times:" and each player's), and its
console refuses both ("Can't "tell", not connected"); `version` on the dedicated console prints
the game's version. Quake's spare program settings (`noexit`, `nomonsters`, `gamecfg`, `temp1`,
`scratch1` to `scratch4`, `savedgamecfg` and `saved1` to `saved4`, the last five kept in the
config) are registered so a program reads and sets them. There is no "server crashed" line:
our server never stops on a fault, since a mod's error is caught and the game goes on.

Quake II's ledger (`porting/shooters-edge/q2-ledger-server.md`) is the checklist; the facts of the
operator's words, the player's queries and the connect checks are in
`porting/shooters-edge/q2-operator.md`; those of the later protocols 35 and 36 in
`porting/shooters-edge/q2-proto36-server.md`. Still to come there: `quit` and `recycle` with the
lines players read (over the dedicated process's `quit`), the extended limits, compressed downloads
and layouts, the game library's extended interface, the multi-view recorder's words and its relay
protocol.

Quake II's later protocols are the endpoint's (`net/q2_net_server.cpp`, the connect's tail and the
later moves in `net/q2_net_ext.hpp`, the frame's top and the streamed signon in
`server/q2_wire_ext.cpp`); the newer channel (30-bit sequences, a message in pieces, a one-byte
qport) is the classic layer's channel behind fields that default to the older form
(`plugins/classic/protocol/channel.hpp`), and the deflater that packs the signon is the engine's
(`engine/core/deflate.hpp`).

## Labs and tests

`se-server`, `se-dedicated`, `se-dedicated-lobby`, `se-admin`, `se-cheats`, `se-login`,
`match-report`, `server-core`; `tests/shooters_edge_q2_server_test.cpp`,
`tests/shooters_edge_q2_admin_test.cpp`, `tests/server_operator_test.cpp`.

## Lessons

- Put a rule in the engine from the start: VIP slots began in Quake's endpoint and had to
  move.
- GCC 13 at `-O3` flags a vector range-insert of a one-row `GameRule` table; gather rows with
  `push_back` loops.
- New rate bounds or defaults move the labs' digests; rewrite them in the same PR.
- A server state that a console command fills (the masters) must not be reset by the first
  frame: a config's `setmaster` runs before it.
- The first inventory came from a modern fork, not the original release: ask the reader to tag
  which is which.
- A game whose rules were written for a fixed beat (Quake II's tenths) can step faster without
  rewriting its world: keep the frame counter on the old beat, run what moves (movers, shots,
  pieces) every fast frame, and spread a mover's old-beat push over the fast frames, rounded as the
  old one was and stepped at the old beat's end, so every old-beat frame lands on the same bits.
- A map's entities live twice once it is read: as text and parsed. Our Quake II rules spawn from
  the parsed list, a hosted game library from the text; an override replaces both, and a test
  that seats a player (not a frame's hash with nobody in it) is what shows the rules took it.
- A version probe stuffed before the first `new`'s server data makes a recording of that client
  open with a stuffed line, and the demo readers refuse it: send probes after the server data.
- A client's string command waits for the acknowledgement of its last one; while the server
  sends only reliable data (`nogamedata`), that is up to a second away.
- A game that answers its own clients (`ServerGame::serve`) gets none of the session's features, so
  wire `ServerGame::console` and `console_lines` from the start: without them the dedicated
  server's terminal can neither run the game's admin words nor show what the game printed.
