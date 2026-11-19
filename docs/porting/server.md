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
| Engine | `engine/server/match.hpp`, `overtime.hpp`, `timeout.hpp`, `vote.hpp`, `aids.hpp`, `homemap.hpp` | The lobby and the match's phases, overtime, timeouts, votes, player aids granted, the home map |
| Engine | `engine/server/record.hpp`, `autorecord.hpp`, `playback.hpp`, `spectators.hpp` | Every match recorded, played back, watched on the server's own TV node |
| Engine | `engine/server/login.hpp`, `accounts.hpp` | Logins against the server's own service or its master (`online.md`) |
| Engine | `engine/net/guard.hpp` | The guard's score (anti-cheat) |
| Shooter kit | `plugins/shooter/settings.hpp` | The move's and the loop's settings every shooter shares (`sv_gravity`, `sv_maxspeed`, `pm_` ...) |
| Shooter kit | `plugins/shooter/server.hpp`, `frame.hpp`, `client.hpp` | The classic server's world and services; the frame on the run clock; a player's packet and the speed check |
| Shell | `games/shooters_edge/server/host.hpp`, `dedicated.hpp`, `input_log.hpp` | The host: the loop on the run clock, slots, bots, the input log and its hash |
| Shell | `games/shooters_edge/server/lobby.hpp`, `announce.hpp`, `chat_rules.hpp`, `commands.hpp` | The lobby's words, the server's chat for every runner, client commands to the rules |
| Shell | `games/shooters_edge/net/classic_cheats.hpp` | The speed check's cuts scored by the guard, named in the match report |

## Each game's

| | The first game | The sequel | Next |
| --- | --- | --- | --- |
| The `ServerGame` | `net/classic_game.hpp` | `net/sequel_game.hpp` | `net/<game>_game.hpp` |
| The endpoint | `net/classic_server.hpp`, its own settings `net/classic_endpoint_cfg.hpp` | `net/sequel_net_server.hpp` | its own |
| Admin words | `net/classic_admin.hpp` | `net/sequel_admin.hpp` | its own |
| Logins | `net/classic_login.hpp` | `net/sequel_login.hpp` | its own |
| Match rules | `net/classic_match_rules.hpp` (overtime over the mod's clock, prewar, the lock) | the engine's rules over its runner | the engine's |
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
| Bans, rcon, the server log, chat floods, spectator talk | the first game's later servers | `engine/server` |
| Overtime, timeouts, votes, the lobby, prewar, the home map | the first game's competitive mods and servers | `engine/server` |
| Every match recorded and watchable | the first game's later servers | `engine/server`, `engine/tv` |
| Speed cheats named in a match report and banned by account | ours | `classic_cheats.hpp`, the guard |

| Drawback left behind | How |
| --- | --- |
| Each game's server with its own set of admin tools | one core; each game words it |
| Settings in one flat config | layers with every value's file and line (`--explain`) |
| A speed cheat that gains time | cut to the time that passed; flagged, scored and named |

## Gaps

The ranked gap lists in `porting/shooters-edge/server-gaps.md` and `sequel-server-gaps.md`;
spectator slots and team locks in the first game.

## Labs and tests

`se-server`, `se-dedicated`, `se-dedicated-lobby`, `se-admin`, `se-cheats`, `se-login`,
`match-report`, `server-core`; `tests/shooters_edge_sequel_server_test.cpp`.

## Lessons

- Put a rule in the engine from the start: VIP slots began in the first game's endpoint and had to
  move.
- GCC 13 at `-O3` flags a vector range-insert of a one-row `GameRule` table; gather rows with
  `push_back` loops.
- New rate bounds or defaults move the labs' digests; rewrite them in the same PR.
- The first inventory came from a modern fork, not the original release: ask the reader to tag
  which is which.
- A game whose rules were written for a fixed beat (Quake II's tenths) can step faster without
  rewriting its world: keep the frame counter on the old beat, run what moves (movers, shots,
  pieces) every fast frame, and spread a mover's old-beat push over the fast frames, rounded as the
  old one was and stepped at the old beat's end, so every old-beat frame lands on the same bits.
- A map's entities live twice once it is read: as text and parsed. Our Quake II rules spawn from
  the parsed list, a hosted game library from the text; an override replaces both, and a test
  that seats a player (not a frame's hash with nobody in it) is what shows the rules took it.
