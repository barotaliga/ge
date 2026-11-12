# A player's game CLI

Every game on the engine gets a command line for its **players' agents**: `<game>_cli`
(`shooters_edge_cli` for Shooter's Edge). It does everything a player does with a game except
the playing: change a setting, find a server and who is on it, host a game or join a friend's,
run a command on a server's console, read what went wrong. A person asks their agent ("host a
game for four of us on e1m2", "join Ann's server", "turn my mouse down"), and the agent runs the
CLI and reads one JSON object back.

`ge` (engine/cli) is the developer's command line: it builds, tests and grows a game repo. The
game CLI is the player's: it ships beside the game's window and dedicated server and never needs
the source.

## The layers

The CLI is built in the three homes every feature lives in (docs/architecture.md § Plugins):

| Layer | Where | What it gives |
| --- | --- | --- |
| Engine | `engine/gamecli/` (`ge::gamecli`, `cmake/GameCli.cmake`) | The frame (the command table, the parser, the answer, `help`, `help --json`, `skill`) and every generic command: `where`, `settings list/get/set/reset`, `server settings/set`, `servers`, `server info`, `players`, `rcon`, `route`, `host`, `host list`, `host stop`, `join`, `logs` |
| Plugin | `plugins/<name>/cli/` (built into `ge_<name>_cli`) | A group of rows any game linking the plugin adds: classic enhance's `content` (the install's packs, their release by SHA-256, the enhanced cache) |
| Game | `games/<game>/cli/` | The game's `Spec` (its programs, ports, protocol, places, launch words, the settings it registers) and its own rows: Shooter's Edge's `demos`, `demos play`, `demos pack`, `demos unpack` |

A generic command works for a game through its `Spec`; a field it leaves null makes the
commands that need it say the game lacks it. A game's main is one line:

```cpp
int main(int argc, char** argv) { return ge::gamecli::main(argc, argv, my_spec()); }
```

`ge_add_game_cli(<target> OUTPUT_NAME <game>_cli SOURCES main.cpp LINK <libs>)` builds it at the
build's root beside the window and the server, which the CLI starts from there (`--bin=<dir>`
points elsewhere).

## The answer

Text by default, a few lines; with `--json`, one object:

```json
{"command":"settings set","ok":true,"result":{"name":"sensitivity","before":"0.066","value":"0.09",...},"summary":"...","next":"..."}
{"command":"settings set","ok":false,"exit":1,"error":"no setting is called 'sensitivty'; the nearest is 'sensitivity'","next":"shooters_edge_cli settings list sen --json"}
```

Exit codes: 0 done, 1 refused (the error says why, `next` the step), 2 a usage error (an unknown
command or flag is refused, never ignored). `help --json` is the catalog. `skill` prints a
`SKILL.md` generated from the same rows (the loop, every command, the rules an agent keeps): a
player's agent installs it with `shooters_edge_cli skill > ~/.claude/skills/shooters-edge/SKILL.md`.

## Settings

- **The player's config** is the game's own `config.cfg`, read on a console of the CLI's own and
  written back with the console's `save_text`, so a change is saved exactly as the game saves
  it. Only `config.cfg` is written; a setting `autoexec.cfg` also sets is named, since it wins at
  the start. The game reads its config at its start and writes it at its quit: change settings
  while it is closed, or in its console while it runs.
- **Described and checked** by the game's registration: the settings the CLI links itself
  (`Spec::add_cvars`: everything the game's libraries without a window register) and the
  **catalog** the window writes beside the config each time it saves (`cvars.txt`,
  `engine/console/catalog.hpp`: every cvar's kind, flags, default, range and help). With the
  catalog a value is clamped and refused by tier as the game would; an unknown name is refused
  with the nearest. Without one, an unknown name is kept and the answer says so.
- **A server's settings** (`sv_` names, the game's rules) are refused by `settings set` and
  written by `server set` into the base game directory's `server.cfg`, the line that sets it
  rewritten in place. `server settings` works them out exactly as the dedicated process does
  (`engine/server/config.hpp`'s layers, in this process) and says where each value came from.

## Servers

`servers` is the in-game browser's list (`engine/net/server_list.hpp`) on a socket of the CLI's
own, driven by the wall clock until every reply is in or `--wait` passes: the LAN broadcast, the
masters (the player's `sb_masters`, the install's lists, `--master`) and the named servers
(`sb_servers`, `--server`). `players` and `server info` ask one server; `rcon` sends the game's
remote console request (`--password`, else the player's `rcon_password`) and gathers the reply's
packets. `route <address>` runs the same refresh, then asks every forwarder it found for its list
and answers the lowest summed ping to the server (`engine/forward/routes.hpp`): straight, or the
forwarders to go through, nearest first, with the chain the game's route setting takes.

## Hosting and joining

- `host --dedicated` starts the game's dedicated server detached (`engine/gamecli/process.hpp`:
  its own session, so it outlives the CLI and a closed terminal) with `--root=<install>
  --port --for=0 --realtime` and the flags asked, its output in `<logs>/dedicated-<port>.log`
  (line-buffered, so the log reads as it runs). The CLI then asks it for its status for up to
  ten seconds and answers whether it serves, or with the end of its log when it died.
- `host` alone starts the window as a listen server, with the game's own words
  (`Spec::listen_words`: Shooter's Edge's `+sv_port`, `+sv deathmatch 1`, `+map`).
- Each is recorded in the state folder (`hosted-<port>.txt`): `host list` says which still run,
  `host stop <port>` (or `--all`) ends them. A second server on a port the CLI runs is refused.
- `join <address>` starts the window on a server (`Spec::join_words`: `+connect <address>`).
- `--dry-run` says what would start and starts nothing. A password asked of a host is never
  printed back.

## Places

The game's `locate` says where its install, settings, logs and state are; `where` prints them
and which programs exist. Shooter's Edge's are the window's own: the install from `--basedir`,
else the classic game loaded last (kept in the per-user folder), the game folder over it, the
settings in `<install>/<game folder>/settings/`. Logs and state default to the per-user folder
(`~/Library/Application Support/gameengine2/<game>/` on macOS, `~/.local/share/...` on Linux,
`%APPDATA%\...` on Windows). `--settings`, `--logs`, `--state` and `--bin` override each.

## The lab

`make lab LAB=gamecli` runs the sessions through the same code the executable runs and draws each
as a terminal: the settings, a friend's server (a stand-in on the loopback answering the status
query and the remote console, or `--server=<host:port>` a real one), hosting and joining as dry
runs, the JSON answers and the skill. `--basedir=<install>` adds the packs' `content`. The digest
is each command's exit code; the terminals carry pings and ports, so they are measured.

## Not yet

- The listen server's name and password from `host` (the window has no setting for them yet).
- `host` reaching the internet: the answer says to forward the port; opening it on the router
  (`engine/net/portmap.hpp`) and listing with a rendezvous are later rows.
- Talking to a running window (a setting changed live, the console's output read): the CLI
  works on files and the network today.
