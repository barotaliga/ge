// AN EXISTING GAME LIBRARY OF QUAKE II HOSTED UNCHANGED (M18v10, docs/goals/M18v.md): a mod's
// rules, built as a native library for this platform, loaded and answered as Quake II's own
// server answers it, so the community's mods run on ours. The interface is q2_native_abi.hpp;
// what each call must do is porting/shooters-edge/q2-game-library.md.
//
// - **Loading** (`load_native_q2`): the library file found by `find_native_library` (the mod's
//   folder, then the base folder; `game.so`, `gamex86_64.so` or `gamei386.so` on Linux,
//   `game.dylib` on macOS, `game.dll` or `gamex86.dll` on Windows), its entry point called with the
//   server's table, the version checked (3), then its `init`. The server's own settings exist first
//   (`deathmatch`, `maxclients`, `dmflags`, `cheats`, `sv_airaccelerate`, ...), so the game finds
//   them with the server's values.
// - **The world** the game asks about: the map's brushes (`trace`, `pointcontents`, a brush
//   model's hull moved to its entity), the linked entities' boxes (every linked solid entity is
//   clipped; a list, not a tree), the visible and hearable sets by cluster, then the areas: an
//   entity linked takes the first area its box touches and the last other one, the game opens and
//   shuts the portals (all shut at the level's start) and the areas flood over the open ones, so
//   a shut door cuts sight and hearing sets between rooms (brush/areas.hpp).
//   A map of clip hulls (Quake's, under classic remix) is traced through the hull a box's
//   width fits (none, the player's, which is Quake II's player box, or the large one), its
//   contents read as Quake II's bits, every point in every set.
// - **The move** (`pmove`): Quake II's own (shooter/q2_move.hpp) over the game's trace and
//   contents callbacks, the air acceleration the server set for the level.
// - **The messages** the game writes: one buffer, sent by `unicast` or `multicast` (all, the
//   hearable or visible set of the point) onto each slot's reliable bytes or datagram; prints,
//   centred prints, sounds and configstrings likewise. A raw game shows the wire whole
//   (`Q2Raw`), so the net server sends what the game wrote.
// - **A frame**: each command a packet brings runs the game's `client_think` as it arrives; every
//   100 ms of host time the pings are set, `run_frame` runs, the frame's view is taken (every
//   entity sent but the no-client ones and the empty ones), and every entity's event is cleared.
// - **Players**: `connect` runs the game's `client_connect` with the slot's user info and then its
//   `client_begin`; a refusal prints its reason to the player and sends him away. On a level change
//   the players stay connected: the game spawns the next map with them in, and each one's next
//   `begin` runs `client_begin` alone.
// - **A level change** the game asks for (`gamemap "<map>"` through its command text) or the
//   console's `map` is `pending_map`; the host loads the map and calls `start` again.
// - **The window's own game** reads what every runner shows (rules_world.hpp): the lists from the
//   configstrings, each entity's row, each player's numbers (health, ammo, armour and score from
//   the stats, the whole stats for the game's bar), and its prints, centred prints and sounds as
//   events beside the bytes, so a listen server plays the mod (`ListenHost::native_library`).
// - **`error`** never returns into the library: the call that reached it is left by a long jump,
//   the map stops (`map_stopped`, `stop_reason` the game's text).
//
// Not yet: saving and loading (a deathmatch writes nothing), rotating brush models' hulls, a
// client's frame cut to what he can see (every entity is sent, within the frame's room).
//
// ge:summary An existing game library of Quake II hosted unchanged: loaded, its calls answered
// ge:keywords game-library native-game-library gamelib mod-library dlopen client-think run-frame
// ge:keywords pmove multicast unicast configstring linkentity box-edicts cvar m18v10 q2-mods
// ge:skill shooter mod
// ge:test tests/shooters_edge_q2_native_test.cpp
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "shooters_edge/server/q2_wire.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::server {

struct NativeHost;  // q2_native.cpp

struct NativeQ2Options {
  std::filesystem::path library;  // the game library's file
  int max_clients = 8;
  std::string game;  // the mod's folder name ("" the base game), the `game` setting
  // Settings set before the game starts (a server's cfg, `+set`): the game's `init` reads them.
  std::vector<std::pair<std::string, std::string>> settings;
};

// The runner's state: the common part, the raw wire the net server sends, and the host.
struct NativeQ2 : RulesCommon {
  int64_t frame_number = 0;
  int32_t map_checksum = 0;  // configstring 31, the caller's (q2_map_checksum)
  Q2Raw raw;
  std::unique_ptr<NativeHost> host;
  NativeQ2();
  ~NativeQ2();
  NativeQ2(const NativeQ2&) = delete;
  NativeQ2& operator=(const NativeQ2&) = delete;
};

// The game library a server of Quake II loads for mod folder `game` under `root` (the base
// folder `base` when the mod has none, or `game` is empty); false when neither holds one.
bool find_native_library(const std::filesystem::path& root, const std::string& game,
                         const std::string& base, std::filesystem::path& out);

// The library loaded and its `init` run; null with `error` naming the file and why.
std::unique_ptr<NativeQ2> load_native_q2(const NativeQ2Options& opt, std::string& error);

// The runner over a loaded library (form "native"): `start` spawns a map's entities.
RulesRunner native_q2_runner(std::unique_ptr<NativeQ2> s);
// A digest of what the game shows (its frame, every entity's and player's wire state): the
// server's status line and its checks. The game's own state is its library's and is not read.
uint64_t native_q2_hash(const NativeQ2& s);
// The native runner's state, or null when `r` is another runner.
NativeQ2* native_q2_of(const RulesRunner& r);

}  // namespace shooters_edge::server
