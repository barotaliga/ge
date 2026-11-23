# Common features: one piece of code for what the games share

WORKING AREA (deleted when porting ends). This audit follows the settings audit
(`common-settings.md`). It covers the code that does the same job in more than one of Quake,
QuakeWorld and Quake II (commands, HUD pieces, demos, the network, menus, servers), lifted into the
shared layer under the lift-before-port rule (`port-from-ledger` skill, step 2):

- Test the shared version in every game.
- Fix a bug in the shared code once, for every game.
- When one game's solution is the better one, move it down into the shared layer.
- Never change an old game's feel.

Started 2026-11-23 (the coordinator's next slice after B1 to B9).

## How it was built

1. Every function defined in a game's own files (`q2_*` against `qw_*` and `q1_*` under
   `games/shooters_edge/`) was listed with its game prefix stripped. The names two families both
   define were read side by side (118 names; most are words like `send` or `drop` that do
   different jobs).
2. Every console command was checked for registration in more than one game's files. None is: the
   commands already live in shared files (`client/`, `net/game_commands.cpp`,
   `net/window_words.cpp`).
3. The HUD (`hud/`), menus (`menu/`) and demo playback (`client/demo_playback.cpp`) were read for
   per-game copies. They are already one code path over per-game tables and programs (Quake II's
   status bar program, its menu rows, its `.dm2` codec). The codecs carry each game's own wire and
   stay apart.

Most helpers were lifted earlier: rate limits, bans, logins, votes, talk and voice routing, the
net channel, info strings and the address lists all live in `engine/server`, `engine/net` and
`plugins/classic/protocol`. What is left is the glue at each server face.

## Ranked

Rank 1 is gameplay or daily use (joining, playing, watching). Rank 2 is a feature one game has
better. Rank 3 is code only, with no player-visible change.

| # | Rank | What | Copies | One game's better way, and the bugs | Status |
| --- | --- | --- | --- | --- | --- |
| A1 | 1 | The connect challenge (`getchallenge`, `connect`) | 4: QuakeWorld's server, Quake II's server, the TV node's Quake II face and its QuakeWorld face | QuakeWorld's server is the better way: one challenge per address and port, the same number when asked again, and found by host and number when a router moved the port. Quake II's server keyed by address and port, so a moved port was refused with "No challenge for address", and a client that asked again got a new number. The two TV faces kept one challenge per host in a map that grew without bound under a flood of forged asks. QuakeWorld's 64 entries let a flood evict real players. | lifted (`plugins/classic/protocol/challenges.hpp`, 1024 kept, each game's own number) |
| A2 | 3 | The host part of an address compared (`same_host`) | 6: `engine/net/master.cpp`, QuakeWorld's server, Quake II's server, both TV faces, `q2_pingservers` | none | lifted (`engine/net/address.hpp`) |
| A3 | 3 | A face's datagram out, its out-of-band text and its refusal (`send_to`, `send_oob`, `refuse`) | 4: both servers and both TV faces | The refusal text differs by game (`print` against `n`). It stays each game's, and only the framing would be shared. | kept apart: three lines each over the shared transport and channel; the framing and words are each game's, so a shared one would only add a parameter for every difference |
| A4 | 3 | A viewer's reliable queue (`queue`, `transmit`, `drop`) | 2: the TV node's two faces | Quake II's holds back the signon until echoed (`go`); QuakeWorld's sends a datagram with it | kept apart: two mechanisms around the shared channel (`plugins/classic/protocol/channel.hpp`) |
| A5 | 2 | A player's own voice in the client's recording | QuakeWorld has it (`record_own_voice`); Quake II's client does not | Quake II's recordings missed your own voice | done: a block of its own in the level's recording, the server's voice message (`net/q2_client.cpp`), so `record` while joined keeps the whole talk |

## The mission packs' settings

The mission pack threads check `common-settings.md` before naming a setting (told 2026-11-23). On
Ground Zero, `strong_mines` and `g_disruptor` have no meaning in another game, and the `dmflags`
bits are values of Quake II's own `dmflags`. Its tag mode (`gamerules 2`) is to be checked against
Dissolution of Eternity's before it is named.
