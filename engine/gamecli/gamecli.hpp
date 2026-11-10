// A PLAYER'S GAME CLI: everything a player does with a game except the playing, as commands an
// agent can run for them (`<game>_cli`, docs/gamecli.md): the settings in the player's config,
// the servers out there and one server's players, a server hosted (in the window or as a
// dedicated process) and stopped, a friend's game joined, a server's remote console, the logs of
// what it launched. `ge` is the developer's command line; this is the player's.
//
// THE LAYERS. Every command a game's CLI answers is one row of a `Command` table. The engine's
// rows (`engine_commands`, this module) are the generic ones and work for every game through the
// `Spec` the game fills: its programs' names, its ports, its server protocol (net/server_list.hpp's
// `ServerQuery`, the remote console's packet), where it keeps its settings (`locate`) and the words
// that start its window on a server or a map. A plugin a game links adds its own group (a row's
// `group` names it: classicenhance's `content`); the game adds the rows only it has. The game's
// main is its Spec and one call:
//
//   int main(int argc, char** argv) { return ge::gamecli::main(argc, argv, my_game_cli()); }
//
// built by `ge_add_game_cli` (cmake/GameCli.cmake) at the build's root beside the window and the
// dedicated server, so the CLI finds both beside itself.
//
// THE ANSWER. Text by default, a few lines an agent or a person reads; with `--json`, one object:
// `{"command", "ok", "result", "summary", "next"}` on success and `{"command", "ok": false,
// "exit", "error", "next"}` on a refusal, `next` the action to take. Exit codes: 0 done, 1 refused
// (the error says why and what next), 2 a usage error. `help --json` is the whole catalog;
// `skill` prints a SKILL.md an agent installs to drive the game, generated from the same rows, so
// it never falls behind them.
//
// ge:summary A player's game CLI: the command table, the game's Spec, the answer, help and skill
// ge:keywords gamecli game-cli player-cli shooters_edge_cli agentic settings servers host join
// ge:keywords rcon players logs command-table spec places answer json help skill catalog
// ge:keywords engine_commands locate join_words listen_words
// ge:skill agent-interface
// ge:test tests/gamecli_cli_test.cpp
#pragma once

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"
#include "net/server_list.hpp"
#include "server/server.hpp"

namespace ge::gamecli {

// Where a game keeps what its CLI reads and writes, as the game's `locate` works it out.
struct Places {
  std::string install;   // the game's install: its content, server.cfg's directory under it
  std::string settings;  // the folder of the player's config.cfg and the catalog (cvars.txt)
  std::string logs;      // where the programs the CLI starts write their output
  std::string state;     // where the CLI keeps the servers it hosts
  std::vector<std::string> masters;  // masters the install names, beside the player's sb_masters
};

struct Ctx;
using RunFn = int (*)(Ctx& ctx);

// One command: `name` is one or two words (`settings set`); the longest name the command line
// starts with runs.
struct Command {
  const char* name;
  const char* args;     // the positional words, as help shows them ("<name> <value>")
  const char* summary;  // what it does, one line
  const char* example;  // a whole command line
  const char* group;    // who provides it: "engine", a plugin's name, the game's
  RunFn run;
};

// A flag a command reads (shown by `help`); every flag is `--name=value` or `--name`.
struct Flag {
  const char* name;
  const char* value;  // "" for a switch
  const char* help;
};

// What a host was asked for (the `host` command's flags, read by the engine).
struct HostAsk {
  std::string map, name, password;
  int port = 0, maxclients = 0, bots = -1;  // 0 / -1: the game's own default
};

// THE GAME'S HALF. Every pointer may be null: the commands that need it then say the game lacks
// it.
struct Spec {
  const char* game = "game";   // its folder and programs' stem: `shooters_edge`
  const char* title = "Game";  // its name for people
  const char* cli = "game_cli";
  const char* window = "";                // the window's program ("": none)
  const char* server = "";                // the dedicated server's program ("": none)
  const char* prefs_org = "gameengine2";  // the per-user folder the window keeps its state in
  const char* prefs_app = "game";
  uint16_t default_port = 27500;
  uint16_t lan_first = 27500, lan_last = 27500;  // the ports a LAN broadcast reaches
  uint16_t master_port = 27000;                  // a master named without a port
  net::ServerQuery query;                        // the server browser's protocol
  // The dedicated server's game: its settings (`server settings`, `server set`) are worked out
  // in this process exactly as the server works them out (engine/server/main.hpp).
  const server::ServerGame* server_game = nullptr;
  // The remote console's request and its reply's text (false: not a reply).
  size_t (*rcon)(std::string_view password, std::string_view command, uint8_t* out,
                 size_t cap) = nullptr;
  bool (*read_rcon)(const uint8_t* data, size_t size, std::string& text) = nullptr;
  // The settings the CLI registers itself from the game's libraries it links (those without a
  // window), so they are described before the game has written its catalog; may be null.
  void (*add_cvars)(console::Console& con) = nullptr;
  // The places; false with `error` (what is missing and the next action).
  bool (*locate)(const Ctx& ctx, Places& out, std::string& error) = nullptr;
  // The window's command line to join `address`, and to host a listen server on `ask`.
  void (*join_words)(const Ctx& ctx, const std::string& address,
                     std::vector<std::string>& words) = nullptr;
  void (*listen_words)(const Ctx& ctx, const HostAsk& ask,
                       std::vector<std::string>& words) = nullptr;
  std::vector<Command> commands;  // the plugins' and the game's rows, after the engine's
  std::vector<Flag> flags;        // the game's own flags (`--basedir`), every command takes them
  std::string notes;              // the game's paragraph in `skill`
};

// A command's call.
struct Ctx {
  explicit Ctx(const Spec& s) : spec(s) {}
  const Spec& spec;
  std::string command;                                    // the command's name as run
  std::vector<std::string> words;                         // the positional words after it
  std::map<std::string, std::string, std::less<>> flags;  // `--name=value` (a switch: "")
  bool json = false;
  std::string self_dir;  // the folder of the running CLI: the game's programs are looked for there
  std::string out;       // what the command prints
  Places places;
  bool located = false;
  std::string locate_error;

  bool has(std::string_view name) const { return flags.find(name) != flags.end(); }
  std::string flag(std::string_view name, std::string_view fallback = "") const;
};

// The places worked out once (the game's `locate`, then `--settings`, `--logs` and `--state`
// over it); false with the answer already refused. `try_places` refuses nothing: false leaves
// the reason in `ctx.locate_error`.
bool need_places(Ctx& ctx);
bool try_places(Ctx& ctx);

// The answer: `result` is JSON (an object), `text` the lines printed without --json.
int answer(Ctx& ctx, const std::string& result, const std::string& text, const std::string& summary,
           const std::string& next = "");
// A refusal: `exit` 1 or 2, `error` what failed, `next` the action.
int refuse(Ctx& ctx, int exit, const std::string& error, const std::string& next);

// The engine's commands, every game's.
std::span<const Command> engine_commands();
// The common flags every command takes, and those named commands read.
std::span<const Flag> common_flags();
std::span<const Flag> command_flags(std::string_view command);

// The command line `args` (without the program's name) run, printing into `out`; the exit code.
int run(const Spec& spec, const std::vector<std::string>& args, std::string& out,
        const std::string& self_dir);
// The process: `run` on argv, printed to stdout.
int main(int argc, char** argv, const Spec& spec);

std::string help_text(const Spec& spec, std::string_view command = {});
std::string catalog_json(const Spec& spec);
// A SKILL.md for an agent driving the game through its CLI.
std::string skill_text(const Spec& spec);

// The platform's per-user folder for `org`/`app`, ending in a separator, as the window's
// platform layer names it: `~/Library/Application Support/<org>/<app>/` on macOS,
// `$XDG_DATA_HOME/<org>/<app>/` (else `~/.local/share/...`) on Linux, `%APPDATA%\<org>\<app>\` on
// Windows; "" when the home folder is unknown.
std::string prefs_dir(std::string_view org, std::string_view app);

}  // namespace ge::gamecli
