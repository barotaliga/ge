---
name: game-cli
description: "Use to give this game a player's CLI (`<game>_cli`, ge::gamecli) that a player's agent drives: change settings, find servers and who is on them, run a server's remote console, host a game or join a friend's, read the logs. The game's Spec (programs, ports, protocol, places, launch words, settings), ge_add_game_cli, the settings catalog the window writes, a plugin's group of commands, the game's own rows, the --json answer and the skill the CLI prints."
---

# A player's CLI for this game

`ge::gamecli` (link `ge::gamecli`; headers `gamecli/*.hpp`) is a command line for the game's
players and their agents: everything a player does with the game except the playing. The engine
holds the generic commands (`where`, `settings list/get/set/reset`, `server settings/set`,
`servers`, `server info`, `players`, `rcon`, `host`, `host list`, `host stop`, `join`, `logs`),
`help --json` and `skill`; the game fills a `Spec` and may add rows of its own. Every answer is
text, or one JSON object with `--json` (`ok`, `result`, `summary`, `next`; a refusal `error` and
`next`; exit 1 refused, 2 a usage error).

## Add it

1. A `cli/spec.cpp` that returns the game's `ge::gamecli::Spec` (`gamecli/gamecli.hpp` states
   every field):
   - `game`, `title`, `cli`, `window`, `server`: the programs' names (`""` for none);
     `prefs_org`, `prefs_app`: the per-user folder the window keeps its state in;
   - `default_port`, `lan_first`..`lan_last`, `master_port`;
   - `query` (`net/server_list.hpp`'s `ServerQuery`: the status query and its reader, the
     masters'), `rcon` and `read_rcon` when the game has a remote console;
   - `server_game`: the dedicated server's `ServerGame`, for `server settings` and `server set`;
   - `add_cvars`: registers on a console the settings the game's libraries without a window hold
     (as the window's main registers them), so they are described before the game ran once;
   - `locate`: the install, the settings folder, logs, state and the masters the install names;
   - `join_words`, `listen_words`: the window's command line to join an address and to host.
2. `cli/main.cpp`: `int main(int argc, char** argv) { return ge::gamecli::main(argc, argv, spec()); }`
3. In CMake: `ge_add_game_cli(<game>_cli_exe OUTPUT_NAME <game>_cli SOURCES cli/main.cpp LINK
   <the libraries the spec links>)`; it lands at the build's root beside the window and the
   server, which it starts from there.
4. The window writes the **catalog** beside the player's config each time it saves:
   `con.write_file(ge::console::kCatalogFile, ge::console::catalog_text(con.cvars()))`. With it
   the CLI checks every name and range the window registers, its own included.

## Grow it

- A row of the game's own: `spec.commands.push_back({"name", "<args>", "what it does",
  "example args", "<game>", run})` where `run(Ctx&)` answers with `ge::gamecli::answer(ctx,
  result_json, text, summary, next)` or `refuse(ctx, exit, error, next)` (build JSON with
  `core/json_write.hpp`). Places come from `need_places(ctx)`.
- A plugin's rows: a plugin with a `cli/` directory builds `ge_<name>_cli` (classic enhance's
  `content`); append its `commands()` to `spec.commands` and link it.
- Every flag a row reads is declared, or the call is refused: the engine's are in
  `engine/gamecli/gamecli.cpp`'s table; the game's own go in `spec.flags`.

## Check it

- `<game>_cli help`, `<game>_cli where --json`, and `<game>_cli skill`, which prints the SKILL.md a
  player's agent installs: it is generated from the rows, so a new row appears there at once.
- Test the Spec with `ge::gamecli::run(spec, args, out, bin_dir)`: answers parse with
  `core/json_read.hpp`'s `read_flat(out, j, error, true)`. A stand-in server on the loopback
  answers the browser's queries in a test without the game's server.
