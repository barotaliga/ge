// The hosted game library's server side, shared by the runner (q2_native.cpp) and the calls
// the library makes (q2_native_calls.cpp): what the server keeps for it.
//
// ge:summary The hosted game library's server state: settings, the map, links, messages, jumps
// ge:keywords game-library native-host native-calls imports setjmp error-jump m18v10
// ge:skill shooter mod
// ge:test tests/shooters_edge_q2_native_test.cpp
#pragma once

#include <csetjmp>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/trace.hpp"
#include "classic/protocol/q2_limits.hpp"
#include "mod/library.hpp"
#include "shooter/brush_hull.hpp"
#include "shooters_edge/server/q2_native.hpp"
#include "shooters_edge/server/q2_native_abi.hpp"

namespace shooters_edge::server {

// One setting the library holds a pointer to: its record never moves (a deque's element), its
// texts live in the strings beside it.
struct NativeSetting {
  native::NativeCvar rec{};
  std::string name, value, latched, fallback;
  bool has_latched = false;
};

struct NativeHost {
  NativeQ2* s = nullptr;
  ge::mod::Library library;
  native::NativeImports imports{};
  native::NativeExports* game = nullptr;
  // The extended interface (q2_native_abi.hpp): the server's table and the game's, null when the
  // game has none.
  native::NativeImportsEx imports_ex{};
  const native::NativeExportsEx* ex = nullptr;
  // The files the extended interface reads, the game's folder first then the base folder's, and
  // writes, in the game's folder; the files it has open, by handle less 1 (null: free).
  std::vector<std::filesystem::path> folders;
  std::filesystem::path write_dir, base_dir;
  struct OpenFile {
    std::FILE* file = nullptr;
    bool write = false;
    int error = 0;  // the first error it met, kept
  };
  std::vector<OpenFile> files;
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
  // Its area portals as the game set them (all shut at the level's start), and the areas flooded
  // over the open ones (brush/areas.hpp).
  std::vector<uint8_t> portals;
  std::vector<int32_t> area_groups;
  std::unique_ptr<ge::brush::BrushTracer> tracer;
  // A map of clip hulls (the classic family, played under classic remix): its models' hulls
  // (pointing into the caller's map, which outlives the level) in place of `map` and `tracer`.
  // A box is traced through the hull its size fits (none, the player's, the large one), every
  // point is in every set, and the contents are Quake II's bits for Quake's numbers.
  std::vector<ge::shooter::BrushModel> hulls;
  std::vector<native::NativeSurface> surfaces;  // by the map's texture index
  native::NativeSurface no_surface{};           // what a trace that hits nothing reports
  std::vector<uint8_t> linked;  // by entity number: 0 not linked, 1 solid, 2 trigger
  bool loading = false;         // the map's spawn and its settling frames
  int thinking = -1;            // the slot whose command the game is running (-1 none)
  // THE EXTENDED LIMITS (plugins/classic/protocol/q2_limits.hpp): 0 the ordinary ones; 1 the
  // library set the protocol-extensions feature after its start (8192 entities, the extended
  // configstring layout, its entities' extension block read); 2 that, and the library is the
  // newer interface (`new_interface`), so clients get the second level. Every index and limit
  // below goes through `cs()` and `limits()`.
  int ext = 0;
  bool new_interface = false;  // the library's interface is the newer one (version 3302)
  const ge::classic::protocol::Q2CsLayout& cs() const {
    return ge::classic::protocol::q2_cs_layout(ext > 0);
  }
  const ge::classic::protocol::Q2Limits& limits() const {
    return ge::classic::protocol::q2_limits(ext > 0);
  }
  float air_accelerate = 0;
  // `sv_physics q1` (classic remix's `cr_physics q1`, also over a mod under `cr_over_mods`):
  // the players move with Quake's ground and air (profiles/remix_arsenal.hpp's
  // `kQ1Feel`). Kept by `native_set` as the setting changes.
  bool q1_physics = false;

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

  // Single player and co-op (q2_native_save.cpp): the level string the last change asked for, its
  // quotes off (`*base2$start`); the game's working folder, where it writes the levels kept in
  // the unit and its game file (made at first use, removed with the host); the autosave made as
  // a level was entered, for the host to take; a save asked to be loaded by the next start.
  std::string level;
  std::filesystem::path dir;
  std::vector<uint8_t> autosave;
  std::vector<uint8_t> load;
  std::string load_name;
  bool from_load = false;  // the level now was started by a load

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

// The library's calls (q2_native_calls.cpp): the table filled, and the host whose world they
// answer (one at a time: the runner makes it current before each call into its library).
void native_fill_imports(native::NativeImports& t);
void native_fill_imports_ex(native::NativeImportsEx& t);
// The game's feature bits as it set them (`g_features`), and the server's it reads
// (`kNativeServerFeatures`: proper in-use, every disconnect, enhanced saves, the extra user info,
// IPv6 addresses, the index lists' overflow answered with 0, the extended limits).
int32_t native_features(NativeHost& h);
inline constexpr int32_t kNativeServerFeatures =
    native::kGmfProperInUse | native::kGmfAllDisconnects | native::kGmfEnhancedSaves |
    native::kGmfExtraUserInfo | native::kGmfIpv6 | native::kGmfIndexOverflow |
    native::kGmfProtocolExtensions;
// `sys_allow_unsafe_savegames`: 1 lets a library without enhanced saves (`g_features` lacking
// kGmfEnhancedSaves) be saved, loaded and kept level to level anyway. Set at start only (the
// command line, server.cfg): a later set is refused with a line saying so.
inline constexpr const char* kUnsafeSavesSetting = "sys_allow_unsafe_savegames";
// The file calls (q2_native_files.cpp) and the files left open closed.
const native::NativeFilesApi* native_files_api();
void native_close_files(NativeHost& h);
void native_make_current(NativeHost* h);
NativeHost* native_current();
NativeSetting& native_setting(NativeHost& h, const std::string& name, const std::string& value,
                              int flags);
void native_set(NativeHost& h, NativeSetting& c, const std::string& value, bool force);
// map_noareas set: every area joined to every other, the portals ignored.
bool native_no_areas(const NativeHost& h);
// A slot's bytes (a print to slot `slot`, reliable).
void native_print(NativeHost& h, int slot, int level, const std::string& text);
// Splits `line` into the arguments the library reads (`argc`, `argv`, `args`).
void native_split(NativeHost& h, const std::string& line);

// Levels kept, autosaves, saves and loads (q2_native_save.cpp), the server's side of a game
// library's single player:
// - `native_leave`: the level being left written into the working folder (a change within the
//   unit, outside deathmatch: the players' entities marked unused while it is, so they come back
//   at a spawn point), or the folder emptied (a new unit, a new game);
// - `native_new_game`: a new game's settings put in (those the game holds for the next game), and
//   the game restarted when nobody is in it, as a new game starts it again;
// - `native_enter`: after the spawn and its settling frames, the level kept or loaded read back
//   (the config strings, the area portals, the game's own level), 100 frames run with nobody in
//   it when the level is entered again, then the autosave made outside deathmatch;
// - `native_load_begin`: before the spawn, a load's settings set, the game restarted, its
//   files put in the working folder and the game's file read; every player comes back in full.
void native_leave(NativeQ2& s, MapChange change);
bool native_new_game(NativeQ2& s, std::string& error);
// Runs `frame` (the game's frame and its view) 100 times after a level is entered again.
bool native_enter(NativeQ2& s, std::string& error, void (*frame)(NativeQ2&));
bool native_load_begin(NativeQ2& s, std::string& error);
void native_forget(NativeHost& h);  // the working folder removed
// The game started again: shut down, its memory freed, its `init` run; every slot out of it.
bool native_restart(NativeHost& h, std::string& error);
// The runner's save calls.
std::string native_save_problem(const NativeQ2& s);
bool native_save(NativeQ2& s, std::vector<uint8_t>& out, std::string& error, int64_t saved_unix);

// Runs `call` into the library with `h` current; false when the library called `error` (the map
// stops, the text in `stop_reason`). Nothing is called once it has.
template <typename F>
inline bool guarded(NativeHost& h, F&& call) {
  if (h.dead || h.game == nullptr) return false;
  native_make_current(&h);
  std::jmp_buf here;
  std::jmp_buf* outer = h.jump;
  h.jump = &here;
  if (setjmp(here) == 0) {  // NOLINT(cert-err52-cpp): the interface leaves `error` by a long jump
    call();
    h.jump = outer;
    return true;
  }
  h.jump = outer;
  h.s->map_stopped = true;
  h.s->stop_reason = "the game stopped: " + h.error_text;
  h.s->console.push_back(h.s->stop_reason);
  return false;
}

}  // namespace shooters_edge::server
