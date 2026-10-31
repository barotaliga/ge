// A SERVER'S SETTINGS IN LAYERS (docs/servers.md § The settings): a server's cvars set from
// several cfg files in a fixed order, each later layer winning, so a mod's `server.cfg` changes
// only what it sets and inherits the rest from the base game's:
//
// 1. each cvar's fallback (the engine's, and the game's ServerGame defaults);
// 2. the BASE game directory's `server.cfg` (`<root>/<base>/server.cfg`);
// 3. each MOD's `server.cfg`, lowest first (`<root>/<mod>/server.cfg`, `--mod=a,b`);
// 4. a named PROFILE (`--cfg=duel`: `duel.cfg` from the highest directory that has one);
// 5. the COMMAND LINE: the flags (each a `+set`), then any `+commands`;
// 6. at RUN TIME: the server's own console, never written back.
//
// A file that is not there is skipped. The files are read through the console's reader
// (`Console::read_file`: the search directories, or a host's own), by names relative to the root
// ("base/server.cfg"), so a test serves them from memory.
//
// WHERE A VALUE CAME FROM: every set of a server cvar (`kServer`) from a cfg, the command line or
// the console is kept with its layer, file and line (`ServerCfg::history`, through the registry's
// watch), the same value set again included: a mod that sets what the base set still owns it.
// `explain` prints each value with its source and what it overrode, which is what
// `<game>_server --explain` prints.
//
// ge:summary A server's settings in layers: base and mod server.cfg, a profile, the command line
// ge:keywords server-cfg layers base mod profile command-line explain history origin override
// ge:keywords load_server_cfg server-cfg-history cfg-layer value-source
// ge:skill net console
// ge:test tests/server_cfg_test.cpp
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"

namespace ge::server {

enum class Layer : uint8_t { fallback, base, mod, profile, command_line, run_time };
// "fallback", "base", "mod", "profile", "command line", "run time".
const char* layer_name(Layer layer);

// One set of a cvar: by which layer, where (a file and its line; "" and 0 for the command line or
// a typed line) and the value as given.
struct ValueSource {
  Layer layer = Layer::fallback;
  std::string file;
  int line = 0;
  std::string value;
};

struct ServerCfg {
  std::string base = "base";      // the base game directory, under the console's root
  std::vector<std::string> mods;  // the mods' directories, lowest first
  std::string profile;            // a named profile (`duel` reads duel.cfg); "" none
  // Every set of a server cvar, oldest first, by the cvar's name.
  std::map<std::string, std::vector<ValueSource>> history;
  std::vector<std::string> read;  // the files read, in order
  Layer layer = Layer::fallback;  // the layer setting values now
};

// Layers 2 to 5 run on `console` (whose server cvars are registered), each value's source kept
// in `cfg`, which must outlive the console's later sets (layer 6 is kept too). The command line
// `line` is `+commands` (the flags already turned into `+set`). False with the first error's file,
// line and next action in `error`.
bool load_server_cfg(console::Console& console, ServerCfg& cfg,
                     const std::vector<std::string>& line, std::string& error);

// Each server cvar whose name starts with `prefix`, its value, the source that set it last and
// what that overrode, a line each, in name order:
//   sv_hostname   "Arena Night"  arena/server.cfg:3  (over base/server.cfg:2 "Shooter's Edge")
// A value never set says "fallback".
std::string explain(const console::Console& console, const ServerCfg& cfg,
                    std::string_view prefix = {});

// Where a source is, as `explain` writes it: "base/server.cfg:3", "command line", "console".
std::string where_text(const ValueSource& v);

}  // namespace ge::server
