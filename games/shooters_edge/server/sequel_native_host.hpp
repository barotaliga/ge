// The hosted game library's server side, shared by the runner (sequel_native.cpp) and the calls
// the library makes (sequel_native_calls.cpp): what the server keeps for it.
//
// ge:summary The hosted game library's server state: settings, the map, links, messages, jumps
// ge:keywords game-library native-host native-calls imports setjmp error-jump m18v10
// ge:skill shooter mod
// ge:test tests/shooters_edge_sequel_native_test.cpp
#pragma once

#include <csetjmp>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/trace.hpp"
#include "mod/library.hpp"
#include "shooter/brush_hull.hpp"
#include "shooters_edge/server/sequel_native.hpp"
#include "shooters_edge/server/sequel_native_abi.hpp"

namespace shooters_edge::server {

// One setting the library holds a pointer to: its record never moves (a deque's element), its
// texts live in the strings beside it.
struct NativeSetting {
  native::NativeCvar rec{};
  std::string name, value, latched, fallback;
  bool has_latched = false;
};

struct NativeHost {
  NativeSequel* s = nullptr;
  ge::mod::Library library;
  native::NativeImports imports{};
  native::NativeExports* game = nullptr;
  int max_clients = 8;
  bool dead = false;             // the library called `error`: nothing more is called
  std::string error_text;        // its text
  std::jmp_buf* jump = nullptr;  // where `error` leaves to (the innermost call into the library)

  std::deque<NativeSetting> settings;  // the records the library keeps pointers to
  std::vector<std::string> argv;       // the arguments of the command being run
  std::string args;
  std::vector<uint8_t> buf;           // the message being written
  std::map<void*, int> blocks;        // the library's memory, by its tag
  std::vector<std::string> commands;  // text the library queued (`add_command_string`)

  std::shared_ptr<ge::brush::Map> map;  // this level's, its hearing sets built
  std::unique_ptr<ge::brush::BrushTracer> tracer;
  // A map of clip hulls (the classic family, played under classic remix): its models' hulls
  // (pointing into the caller's map, which outlives the level) in place of `map` and `tracer`.
  // A box is traced through the hull its size fits (none, the player's, the large one), every
  // point is in every set, and the contents are the sequel's bits for the classic numbers.
  std::vector<ge::shooter::BrushModel> hulls;
  std::vector<native::NativeSurface> surfaces;  // by the map's texture index
  native::NativeSurface no_surface{};           // what a trace that hits nothing reports
  std::vector<uint8_t> linked;  // by entity number: 0 not linked, 1 solid, 2 trigger
  bool loading = false;         // the map's spawn and its settling frames
  float air_accelerate = 0;
  // `sv_physics classic` (classic remix's `cr_physics 1`, also over a mod under `cr_over_mods`):
  // the players move with the classic game's ground and air (profiles/remix_arsenal.hpp's
  // `kClassicFeel`). Kept by `native_set` as the setting changes.
  bool classic_physics = false;

  double clock = 0;  // host time since the level started, s
  double next_frame = 0;
  uint32_t last_sequence[32] = {};
  bool connected[32] = {};  // the game said yes to the slot
  bool begun[32] = {};      // the game's client_begin ran: the slot is in the game
  bool carried[32] = {};    // in the game on the last level: its next begin is client_begin alone
  // What the game last sent each slot to show (a scoreboard's layout program, the inventory's
  // counts), for the window's game to draw (rules_world.hpp's `layout`, `inventory`).
  std::string layout[32];
  std::vector<int16_t> inventory[32];

  native::NativeEntityHead* entity(int n) const {
    return reinterpret_cast<native::NativeEntityHead*>(game->edicts +
                                                       ptrdiff_t(n) * ptrdiff_t(game->edict_size));
  }
  int number_of(const native::NativeEntityHead* e) const {
    if (e == nullptr || game == nullptr || game->edict_size <= 0) return -1;
    return static_cast<int>((reinterpret_cast<const uint8_t*>(e) - game->edicts) /
                            game->edict_size);
  }
  NativeSetting* find_setting(const std::string& name);
  // The map's models, the world's included (either form).
  size_t model_count() const { return map ? map->models.size() : hulls.size(); }
};

// The library's calls (sequel_native_calls.cpp): the table filled, and the host whose world they
// answer (one at a time: the runner makes it current before each call into its library).
void native_fill_imports(native::NativeImports& t);
void native_make_current(NativeHost* h);
NativeSetting& native_setting(NativeHost& h, const std::string& name, const std::string& value,
                              int flags);
void native_set(NativeHost& h, NativeSetting& c, const std::string& value, bool force);
// A slot's bytes (a print to slot `slot`, reliable).
void native_print(NativeHost& h, int slot, int level, const std::string& text);
// Splits `line` into the arguments the library reads (`argc`, `argv`, `args`).
void native_split(NativeHost& h, const std::string& line);

}  // namespace shooters_edge::server
