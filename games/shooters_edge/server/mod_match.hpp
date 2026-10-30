// A MATCH OF A MOD WITH BOTS: a multiplayer progs loaded from a path (or the built-in test mod)
// runs on the test arena under the mod server (qc_server.hpp), its entities spawned from the
// arena's entity text, with bots for players, until the mod changes the level (its time limit) or
// the frame budget runs out. The run is a pure function of the options: the bots draw from their
// own seeded generator, the mod from the server's, and the date from the fixed clock.
//
// WHICH PROGS (`load_mod`): the path given (`ModMatchOptions::progs`, the command line's
// `--progs`), else the `sv_progs` cvar as the console lines set it (`sv_progs <path>`), else the
// test mod (test_mod.hpp). A path is a progs file as a compiler writes it; its line table is read
// from `<path>.lines` beside it when there (plugins/progs/file.hpp). A file that does not load,
// or a progs without the engine-side globals, stops the match with the reason and what to do.
//
// THE CONSOLE LINES (`ModMatchOptions::console`) run on the server's console before the map
// loads, in order (`timelimit 0.25`, `set sv_gravity 600`, `serverinfo hostname arena`, ...).
//
// THE BOTS: bot k is client slot k, named "bot<k+1>". Each frame each picks the nearest other
// player, turns toward it at most `kBotTurn` degrees a second (aiming at its feet, so a rocket's
// blast reaches), runs at it through the doorway when the wall is between them, strafes left or
// right in spells its generator draws, backs off when close, and holds attack when its aim is
// within 8 degrees. It sends one packet a server frame (its three newest commands), never lost.
//
// THE REPORT: every player's path and frags, every missile's path, the hook log and counts, the
// builtin calls by name, the frag log, the prints, what the network builtins wrote, the faults,
// and a digest per frame: the loop's state hash folded with every word of the program's globals
// and entities, so a run that agrees on every frame's digest replays to the bit.
//
// ge:summary A mod (a progs from a path or the test mod) played by bots on the arena to its end
// ge:keywords mod-match run_mod_match load_mod bots sv_progs progs-path timelimit frags
// ge:keywords missile-paths player-paths hook-log builtin-counts digest
// ge:skill progs shooter
// ge:test tests/shooters_edge_mod_match_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "progs/file.hpp"
#include "shooter/vec.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::server {

constexpr float kBotTurn = 540;  // degrees a second a bot's view may turn

struct ModMatchOptions {
  std::string progs;                 // a progs file's path ("" : sv_progs, else the test mod)
  std::vector<std::string> console;  // server console lines run before the map loads
  int bots = 4;                      // 1..32
  int max_frames = 4000;             // server frames of 13 ms at most
  uint32_t seed = 1;                 // the mod's generator; the bots' is seed + 1
};

struct PlayerPath {
  std::string name;
  std::vector<sh::Vec3f> points;  // a server frame a point
  std::vector<float> frags_at;    // the frags field after each frame
  float frags = 0;
  int deaths = 0;  // respawns after the first
};

struct MissilePath {
  int ent = 0;
  int first_frame = 0;
  std::vector<sh::Vec3f> points;
};

struct BuiltinCount {
  int number = 0;
  std::string name;
  int64_t calls = 0;
};

struct ModMatchReport {
  bool ok = false;
  std::string error;   // why the match did not run (with the next step)
  std::string source;  // "the test mod" or the progs' path
  int frames = 0;
  double end_time = 0;
  bool ended = false;  // the mod changed the level (its time limit)
  std::vector<PlayerPath> players;
  std::vector<MissilePath> missiles;
  std::vector<HookCall> hook_log;
  int64_t hook_counts[static_cast<int>(HookId::count)] = {};
  std::vector<BuiltinCount> builtins;  // by number, those called at least once
  std::vector<std::string> frag_log;
  std::vector<std::string> prints;  // every print event's text, in order
  std::vector<std::string> console;
  std::vector<std::string> map_messages;
  int64_t events_total = 0;
  int64_t multicast_bytes = 0;
  int64_t faults = 0;
  std::string last_fault;
  int inhibited = 0;
  int64_t statements = 0;
  std::vector<uint64_t> digests;  // a frame a digest
  uint64_t final_digest = 0;
};

// The progs to run: from `path`, else the test mod (`source` says which). False with `error`.
bool load_mod(const std::string& path, ge::progs::Progs& out, std::string& source,
              std::string& error);

ModMatchReport run_mod_match(const ModMatchOptions& opt);

// The test arena's world as the map's models (one: the world, its doorway open).
std::vector<sh::BrushModel> arena_models();

}  // namespace shooters_edge::server
