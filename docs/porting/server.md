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

### The settings sheet (every ported game owes it)

OPTIONS ▸ SERVER SETTINGS (`docs/servers.md` § The settings sheet) works for a game once it gives
three things: its server's `ServerGame::sheet` (the rows from its rules, each rule's group and words,
the vote's lists; Shooter's Edge's `net/qw_sheet.hpp` is the pattern), a wire for its own clients
(a bit in its extension set and one message carrying `sheet_row_text`, paced on the reliable
channel), and readers for the stock servers of that game in `plugins/classic/server_sheet.hpp`
(what a joined client can ask without a word the server would echo as chat). A protocol whose
server never answers the client's extension bits offers the sheet instead, as Quake II's does
(`net/q2_votes.hpp`: an empty sheet as the player enters, then `ge_sheet`), and keeps each packet
within what that game's client reads. The menu page is the shell's and needs nothing from the game.

## Each game's

| | Quake | Quake II | Next |
| --- | --- | --- | --- |
| The `ServerGame` | `net/qw_game.hpp` | `net/q2_game.hpp` | `net/<game>_game.hpp` |
| The endpoint | `net/qw_server.hpp`, its own settings `net/qw_endpoint_cfg.hpp` | `net/q2_net_server.hpp` | its own |
| Admin words | `net/qw_admin.hpp` | `net/q2_admin.hpp`: the modern server's `status` and its letters, kick, kickban, dumpuser, stuff, stuffcvar, printall, killserver, pickclient; `dumpents` and `listmasters` in `net/q2_game.cpp` | its own, over `engine/server/operator.hpp` |
| A player's queries | (the mod's) | `info`, `sinfo`, `lag`, `packetdup`, `stopdl`, `nogamedata` in `net/q2_net_server.cpp` | its own |
| The multiview recorder | (none) | `mvdrecord`, `mvdstop`, the relay stream over TCP on the game port (`sv_mvd_enable 2`), `addgtvhost` and `addgtvban` (`net/q2_mvd_server.hpp`); frames from the rules (`server/q2_mvd2_live.hpp`) | its own |
| The multiview relay | (none) | `mvdconnect`, `mvdplay` and the channel words, the Waiting Room, a view per viewer forked off the channel's reader, the viewers' words and chat (`net/q2_mvd_relay.hpp`) | its own; the TV face (`net/q2_tv_face.hpp`) serves each view |
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
our server never stops on a fault, since a mod's error is caught and the game goes on. A Quake
single-player program's `dprint` lines show only with `developer 1`; a multiplayer program's
always do. A QuakeWorld server's info carries `*progs` (the mod program file's CRC-16) and `*cheats ON` with the game rule `cheats 1`; `sv_gamedir` changes only
the game folder the server info and the next handshake name; `sv_highchars 0` strips a joining
player's info to plain characters and turns a later `setinfo`'s coloured letters plain; and a
player whose `pmodel` or `emodel` differs from the CRC-16 of the server's own player or eyes model
is announced to everyone at his `begin` ("<name> WARNING: non standard player/eyes model
detected"), the player kept (`net/qw_signon.cpp`). With `sv_mapcheck 1` (the default) a player
whose map checksum differs from the server's own file's is told and dropped. On a server started
with cheats the operator's `god <id>` and `give <id> <item> <count>` reach the player, and the
rules take `sv_cheats` with it. `fraglogfile` writes the frag lines to `frag_<n>.log` and the
out-of-band `log` query answers with the last ten-minute buffer of them (`net/qw_fraglog.hpp`).
Every dedicated server runs the lines typed on its own terminal in the foreground
(`engine/server/main.cpp`).

Quake II's ledger (`porting/shooters-edge/q2-ledger-server.md`) is the checklist; the facts of the
operator's words, the player's queries and the connect checks are in
`porting/shooters-edge/q2-operator.md`; those of the later protocols 35 and 36 in
`porting/shooters-edge/q2-proto36-server.md`; those of the general console words and the
shutdown in `porting/shooters-edge/q2-console-words-facts.md`; those of the game library's extended
interface in `porting/shooters-edge/q2-game-api-ex-facts.md`; those of the recorder's dummy and the
relay's lines in `porting/shooters-edge/q2-mvd-dummy-facts.md`. Still to come there: the extended
limits and compressed downloads.

Every classic game's search path answers the file words in the classic layer
(`plugins/classic/content/fs_words.hpp`): `dir`, `fdir`, `whereis` and the read-only links (`link`,
`softlink` and their `unlink`s, kept in the search path's `FileLinks` and applied by `find_file`),
and gives `fs_restart` its banner; Quake II's server wires them with `path` and the game folder's
name check. A game whose own words are another game's wires the same calls. Quake II's `sv addip`
and its other filter words are the engine's ban list under the game's words
(`net/q2_admin.cpp`, `sv_filter`), and its status answer's keys come from the rules' live settings,
the build and the administrator's `sets` keys (`q2_status_keys`, through `ServerConfig::admin_info`).

The dedicated process (`engine/server/main.cpp`) gives every game `quit [reason]` and `recycle
[reason]` (the words joined by spaces, handed to the game's `ServerGame::quit` before it closes;
both exit with success, a recycle's players told to reconnect for whatever supervises the process
to start it again), `showip` (the machine's addresses, `engine/net/udp_socket.hpp`, then the
sockets the core and the game listen on), `net_restart` (`engine/server/net_restart.hpp`: the socket
opened again, the last good port when the new one fails, the game's own listeners reopened through
`ServerGame::reopened`), `macrolist` and `$com_maplist` (`engine/server/
macros.hpp`) beside the console's clock macros. Quake II's farewell is "Server quit" or "Server
restarted", with the reason, to every player and relay viewer; after `killserver` its endpoint
answers only rcon and runs no frame until the next map.

At a terminal the dedicated process is a line editor (`engine/server/terminal.hpp`): the keys
edit the line after `]`, Up and Down walk the typed lines (`con_history` kept and saved to
`.syshistory` in the base folder), and Tab lists the matches as the player's console does
(`engine/console/tab_list.hpp`, `con_completion_mode` and `con_completion_threshold`); a line
printed meanwhile goes above the one being typed. Every printed line can be kept in
`logs/<name>.log` too (`engine/server/console_log.hpp`: `con_logfile`, `con_logfile_name`,
`con_logfile_prefix`, `con_logfile_flush`). Quake II's names for these six are synonyms
(`sys_history`, `com_completion_mode`, `com_completion_treshold`, `logfile` and its three).
Quake II's dedicated server saves a single player's or co-op game (`save <dir>`, `autosave`,
`load <dir>`, net/q2_game.cpp's `save_words`) where the window keeps it, `saves/<dir>.save`, and
keeps each level's autosave as `save0`.
Every dedicated server can write its traffic as text (`engine/net/packet_log.hpp`, `net_log`,
`net_log_name`, `net_log_flush`; a transport's `log`, beside its capture and tap). A game's
recoverable error (`ServerGame::drop_error`, Quake II's hosted library calling its error) drops
every player with `Server crashed:` and leaves the server idle until the next map; with
`com_fatal_error` the process ends with a failure instead, for its supervisor to start again.
`fs_autoexec` 0 makes a mod switch change the folders alone.

Quake II's frame is cut by the areas as well as the sets (`server/q2_wire.hpp`'s cut): an entity
is sent only when the viewer's area is joined through the open area portals to one of its two
areas, so a shut door hides the room behind it, and the frame's area bits mark the joined areas
for the client to leave the rest undrawn. Area 0 (a solid, outside) is joined to nothing. Sounds
and effects, our rules' (`q2_sight_hears`) and a hosted library's (`send_to`), go to a client whose
origin is in the point's set with the areas joined; a sound with no attenuation to everyone. A
sound leaves its position out for a client whose frame holds its entity, so it is played on the
entity as it moves. The frame is sized to the room the packet has after the reliable data it
carries (`channel_unreliable_room`), so a long reliable message costs a frame its last entities,
effects and sounds rather than the whole frame.

Quake II's downloads (`net/q2_net_server.cpp`, facts in `porting/shooters-edge/
q2-downloads-facts.md`) make the asked name plain first (slashes, `.` and `..`), then offer each
kind by its own setting (`sv_download_textures` for textures and skies, `sv_download_pics`,
`sv_download_others` off by default), refuse a file past `sv_max_download_size`, tell a resume past
the end that the file size differs, and send a downloading client reliable data alone. The connect
answer names `sv_downloadserver`'s web copy and the map. A long rcon answer comes in packets of
1384 bytes, whole lines where they fit; a wrong password is noted without the guess.

A hosted game library with the extended interface (`server/q2_native.cpp`, the calls in
`q2_native_calls.cpp` and `q2_native_files.cpp`) is found by its second entry point and handed our
table at version 3: a sound to one client, a configstring read, a trace against one entity, the
visibility test of a chosen kind, a block grown under its tag, and the file extension (loose files
under the game's folder then the base folder; packs and compressed files are not offered). Its
answers are taken: whether it may be saved, its own end of frame in place of the server's event
clearing, and at version 3 what each client sees of each entity, through `Q2Raw::touch` as each
client's frame is written. `sv_features` offers what our server honours (proper in-use, every
disconnect, enhanced saves, the extra user info, IPv6 addresses, the index lists' overflow); the
client number, the recorder's dummy (the recorder records our rules only), a variable frame rate
and the protocol extensions are not offered. The dedicated terminal prints `console_prefix` before each line; a relay tells a channel a
viewer's new name and leaving, and with `mvd_filter_version` hides the version trigger in the
players' chat.

What exists only in the reference's debug builds (`fs_stats`, `sv_debug`, `sv_pad_packets`,
`mvd_shownet`, `com_debug_break`, the file fuzzing settings, the library's debug drawing) is not
taken: our servers are debugged with the log channels and the labs (`docs/debugging.md`), never a
separate build with its own words.

Quake II's later protocols are the endpoint's (`net/q2_net_server.cpp`, the connect's tail and the
later moves in `net/q2_net_ext.hpp`, the frame's top and the streamed signon in
`server/q2_wire_ext.cpp`); the newer channel (30-bit sequences, a message in pieces, a one-byte
qport) is the classic layer's channel behind fields that default to the older form
(`plugins/classic/protocol/channel.hpp`), and the deflater that packs the signon is the engine's
(`engine/core/deflate.hpp`).

Quake II's multiview recorder (`net/q2_mvd_server.hpp`, facts in
`porting/shooters-edge/q2-mvd-relay.md`) writes the `.mvd2` our demo player reads and serves the
same blocks live to relays over TCP, deflated as one zlib stream with the engine's streaming
deflater. Its access lists are the classic layer's address list (`plugins/classic/protocol/
addr_list.hpp`: masks, comments, hits and the last hit), ready for any modern classic server's
lists. Its own spectator `[MVDSPEC]` (`net/q2_mvd_dummy.hpp`) is a slot the endpoint marks
`Q2Slot::dummy`: in the rules as a spectator, past the players' seats, never sent a packet, left
out of `status` and the browser's count; it runs a command buffer each 10 Hz frame before the
recorder's block, and the capture (`server/q2_mvd2_live.cpp`) names it the observer, keeps its
layouts and prints alone and tells its scoreboard's state back. A relay (below) treats it as no one
to chase, copies `mvd_stats_hack`'s stats from it and takes it frozen as the intermission
(`mvd_freeze_hack`), through the converter's `touch_observer`.

A movie or picture between maps (`.cin`, `.pcx` in a level string) is a level of its own on the
network (`net/q2_game.cpp`'s shows): the rules wait on the map that follows while the endpoint's
signon is the server data alone, naming no slot and the file (`Q2WireOptions::show`); a client's
`nextserver <count>` runs the `nextserver` setting. The window plays the same chain itself
(`client/cinematic.hpp`).

The relay side (`net/q2_mvd_relay.hpp`) is the same server switched into relay mode: it takes the
game's port, dials other servers' recorders (`mvdconnect`) or plays `.mvd2` files (`mvdplay`), and
serves each channel to stock clients. A channel keeps one reader of the stream; a viewer's view of
the player it chases is that reader forked (`client/demo_convert.hpp`'s `convert_fork`) and served
by its own TV face, so a new target is a fresh signon (the client reconnects, as at a level
change). The views are frozen moves with no prediction and the attract loop off, so viewers type
their words. `mvd_stats_score` chooses the frags a viewer's status bar shows (none, the channel's
number, the followed player's). Not taken yet: free flight, the dummy's stats and intermission
rewrites, `mvd_filter_version`.

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
