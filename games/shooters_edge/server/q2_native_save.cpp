// A game library's single player on the server's side (q2_native_host.hpp, the clean-room note
// porting/shooters-edge/q2-native-sp.md): levels kept within a unit, the autosave as a level is
// entered, a save by hand and its load. The game writes and reads its own files by the full
// paths we hand it, in a working folder the host makes; a save packs that folder into an
// engine/save file and a load puts it back.
//
// THE CHUNK, version 1: `SNGM` the game: autosave or not, the level string it was started by, the
// map, the game's settings for the next game (name and value), and the working folder's files
// (the game's file, each kept level's own file and ours beside it: its config strings and area
// portals).
//
// ge:summary A game library's levels kept in a unit, its autosave, its saves and loads
// ge:keywords game-library native-save native-load savegame loadgame autosave kept-level unit
// ge:keywords write-level read-level write-game read-game sv_noreload m18v10
// ge:skill shooter save
// ge:test tests/shooters_edge_q2_native_test.cpp
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "brush/areas.hpp"
#include "save/file.hpp"
#include "save/io.hpp"
#include "shooters_edge/server/q2_campaign.hpp"
#include "shooters_edge/server/q2_native.hpp"
#include "shooters_edge/server/q2_native_host.hpp"

namespace shooters_edge::server {

namespace {

namespace fs = std::filesystem;
namespace save = ge::save;
namespace br = ge::brush;

constexpr const char* kSaveGameName = "shooters_edge_q2_native";  // the file's game (Meta::game)
constexpr uint32_t kSaveVersion = 1;
constexpr uint16_t kChunkVersion = 1;
constexpr uint32_t kGame = save::tag("SNGM");
constexpr const char* kGameFile = "game.ssv";
constexpr int kReturnFrames = 100;  // ten seconds of the game's frames, nobody in the level
constexpr int kStatHealth = 1;

struct GameRecord {
  bool autosave = false;
  std::string level, map;
  std::vector<std::string> names, values;  // the settings for the next game
  std::vector<std::string> files;          // the working folder's, by name
  std::vector<std::vector<uint8_t>> bytes;
};

template <class Io>
void game(Io& io, GameRecord& g) {
  io(g.autosave);
  io(g.level);
  io(g.map);
  io(g.names);
  io(g.values);
  io(g.files);
  io(g.bytes);
  if (g.names.size() != g.values.size() || g.files.size() != g.bytes.size())
    io.fail("the save's lists do not pair up: the file is damaged");
}

bool deathmatch(const NativeHost& h) {
  const NativeSetting* dm = const_cast<NativeHost&>(h).find_setting("deathmatch");
  return dm != nullptr && dm->rec.value != 0;
}

bool setting_on(NativeHost& h, const char* name) {
  const NativeSetting* c = h.find_setting(name);
  return c != nullptr && c->rec.value != 0;
}

// The working folder, made at first use.
const fs::path& folder(NativeHost& h) {
  if (h.dir.empty()) {
    static std::atomic<uint64_t> made{0};
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    h.dir = fs::temp_directory_path() /
            ("se-q2-game-" + std::to_string(now) + "-" + std::to_string(made++));
  }
  std::error_code ec;
  fs::create_directories(h.dir, ec);
  return h.dir;
}

void wipe(NativeHost& h) {
  if (h.dir.empty()) return;
  std::error_code ec;
  for (const fs::directory_entry& e : fs::directory_iterator(h.dir, ec)) fs::remove(e.path(), ec);
}

bool read_file(const fs::path& p, std::vector<uint8_t>& out) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return true;
}

bool write_file(const fs::path& p, const std::vector<uint8_t>& bytes) {
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  f.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
  return static_cast<bool>(f);
}

// Our file beside the game's for a level: its config strings and area portals.
template <class Io>
void server_level(Io& io, std::vector<std::string>& config, std::vector<uint8_t>& portals) {
  io(config);
  io(portals);
}

std::string path_text(const fs::path& p) { return p.string(); }

// The level now written into the working folder: the game's file and ours. With `bodies` false the
// players' entities are marked unused while the game writes it, so they come back at a spot.
bool write_level(NativeQ2& s, bool bodies) {
  NativeHost& h = *s.host;
  const fs::path dir = folder(h);
  const std::string file = path_text(dir / (s.map_name + ".sav"));
  std::vector<int32_t> inuse(size_t(h.max_clients), 0);
  if (!bodies)
    for (int i = 0; i < h.max_clients; i++) {
      inuse[size_t(i)] = h.entity(i + 1)->inuse;
      h.entity(i + 1)->inuse = 0;
    }
  const bool ok = guarded(h, [&] { h.game->write_level(file.c_str()); });
  if (!bodies)
    for (int i = 0; i < h.max_clients && !h.dead; i++) h.entity(i + 1)->inuse = inuse[size_t(i)];
  if (!ok) return false;
  std::vector<uint8_t> out;
  save::Writer w(out, kChunkVersion);
  server_level(w, s.raw.config, h.portals);
  return write_file(dir / (s.map_name + ".sv2"), out);
}

std::string title_of(const NativeQ2& s) {
  return s.raw.config.empty() || s.raw.config[0].empty() ? s.map_name : s.raw.config[0];
}

// The working folder and the game packed into a save.
bool pack(NativeQ2& s, bool autosave, std::vector<uint8_t>& out, std::string& error,
          int64_t saved_unix) {
  NativeHost& h = *s.host;
  const fs::path dir = folder(h);
  const std::string file = path_text(dir / kGameFile);
  if (!guarded(h, [&] { h.game->write_game(file.c_str(), autosave ? 1 : 0); })) {
    error = "save: the game stopped as it wrote its file: " + h.error_text;
    return false;
  }
  GameRecord g;
  g.autosave = autosave;
  g.level = h.level;
  g.map = s.map_name;
  for (const NativeSetting& c : h.settings)
    if ((c.rec.flags & native::kCvarLatch) != 0) {
      g.names.push_back(c.name);
      g.values.push_back(c.value);
    }
  std::error_code ec;
  std::vector<fs::path> files;
  for (const fs::directory_entry& e : fs::directory_iterator(dir, ec))
    if (e.is_regular_file(ec)) files.push_back(e.path());
  std::sort(files.begin(), files.end());
  for (const fs::path& p : files) {
    g.files.push_back(p.filename().string());
    g.bytes.emplace_back();
    if (!read_file(p, g.bytes.back())) {
      error = "save: the game's file " + p.filename().string() + " could not be read back";
      return false;
    }
  }
  save::Meta meta;
  meta.game = kSaveGameName;
  meta.game_version = kSaveVersion;
  meta.title = (autosave ? "ENTERING " : "") + title_of(s);
  meta.place = s.map_name;
  meta.play_s = h.clock;
  meta.saved_unix = saved_unix;
  meta.setup = "gamemap " + h.level;
  save::FileWriter w(out, meta);
  game(w.chunk(kGame, 0, kChunkVersion), g);
  std::string why;
  if (!w.finish(&why)) {
    error = "save: " + why;
    return false;
  }
  return true;
}

// The save `bytes` opened and its game read; false with `error`.
bool unpack(std::span<const uint8_t> bytes, std::string_view name, GameRecord& g,
            std::string& error) {
  save::FileReader f;
  if (!f.open(bytes, name, kSaveGameName)) {
    error = f.problem();
    return false;
  }
  if (f.meta().game_version > kSaveVersion) {
    error = std::string(name) + ": written by a newer game (save version " +
            std::to_string(f.meta().game_version) + ")";
    return false;
  }
  const save::ChunkInfo* info = f.find(kGame);
  if (info == nullptr || info->version > kChunkVersion) {
    error = std::string(name) + ": chunk SNGM " +
            (info == nullptr ? "missing" : "newer than this build reads") +
            ": it was saved by another version of the game";
    return false;
  }
  save::Reader r = f.reader(*info);
  game(r, g);
  if (r.ok() && !r.at_end()) r.fail("the chunk has bytes left over once read");
  if (!r.ok()) {
    error = std::string(name) + ": chunk SNGM at byte " + std::to_string(r.offset()) + ": " +
            r.problem() + " (the file is damaged or from another version of the game)";
    return false;
  }
  for (const std::string& file : g.files)
    if (file.empty() || file.find_first_of("/\\") != std::string::npos || file[0] == '.') {
      error = std::string(name) + ": a file named '" + file + "' in the save: it is damaged";
      return false;
    }
  return true;
}

}  // namespace

void native_forget(NativeHost& h) {
  if (h.dir.empty()) return;
  std::error_code ec;
  fs::remove_all(h.dir, ec);
  h.dir.clear();
}

bool native_restart(NativeHost& h, std::string& error) {
  if (h.game == nullptr || h.dead) {
    error = "the game library stopped (" + h.error_text + "); start the server again";
    return false;
  }
  guarded(h, [&] { h.game->shutdown(); });
  for (auto& [p, tag] : h.blocks) std::free(p);
  h.blocks.clear();
  native_close_files(h);
  if (NativeSetting* f = h.find_setting("g_features")) native_set(h, *f, "0", true);
  for (int i = 0; i < 32; i++) h.connected[i] = h.begun[i] = h.carried[i] = false;
  if (!guarded(h, [&] { h.game->init(); })) {
    error = "the game library stopped as it started again: " + h.error_text;
    return false;
  }
  if (h.game->edicts == nullptr || h.game->num_edicts <= h.max_clients) {
    error = "the game library made no entities for " + std::to_string(h.max_clients) +
            " players as it started again";
    h.dead = true;
    return false;
  }
  return true;
}

void native_leave(NativeQ2& s, MapChange change) {
  NativeHost& h = *s.host;
  if (h.dead || h.game == nullptr) return;
  const LevelString next = level_string(h.level);
  if (change != MapChange::change_level || next.unit) {
    wipe(h);
    return;
  }
  // A level of the unit left outside deathmatch: kept for its return.
  if (s.map_name.empty() || s.map_stopped || deathmatch(h) || h.model_count() == 0) return;
  write_level(s, false);
}

bool native_new_game(NativeQ2& s, std::string& error) {
  NativeHost& h = *s.host;
  bool latched = false;
  for (NativeSetting& c : h.settings)
    if (c.has_latched) {
      const std::string value = c.latched;
      native_set(h, c, value, true);
      latched = true;
    }
  bool anyone = false;
  for (int i = 0; i < h.max_clients; i++) anyone = anyone || h.begun[i];
  return !latched || anyone || native_restart(h, error);
}

bool native_load_begin(NativeQ2& s, std::string& error) {
  NativeHost& h = *s.host;
  GameRecord g;
  const std::vector<uint8_t> bytes = std::exchange(h.load, {});
  if (!unpack(bytes, h.load_name, g, error)) return false;
  for (size_t i = 0; i < g.names.size(); i++)
    native_set(h, native_setting(h, g.names[i], g.values[i], 0), g.values[i], true);
  if (!native_restart(h, error)) return false;
  wipe(h);
  const fs::path dir = folder(h);
  for (size_t i = 0; i < g.files.size(); i++)
    if (!write_file(dir / g.files[i], g.bytes[i])) {
      error = h.load_name + ": the game's file " + g.files[i] + " could not be put back";
      return false;
    }
  const std::string file = path_text(dir / kGameFile);
  if (!guarded(h, [&] { h.game->read_game(file.c_str()); })) {
    error = h.load_name + ": the game stopped as it read its file: " + h.error_text;
    return false;
  }
  h.level = g.level;
  h.from_load = true;
  return true;
}

bool native_enter(NativeQ2& s, std::string& error, void (*frame)(NativeQ2&)) {
  NativeHost& h = *s.host;
  const bool load = std::exchange(h.from_load, false);
  const bool dm = deathmatch(h);
  const fs::path dir = h.dir.empty() ? fs::path() : folder(h);
  std::error_code ec;
  const fs::path sav = dir / (s.map_name + ".sav");
  if (!dir.empty() && !dm && !setting_on(h, "sv_noreload") && fs::is_regular_file(sav, ec)) {
    std::vector<uint8_t> ours;
    std::vector<std::string> config;
    std::vector<uint8_t> portals;
    if (read_file(dir / (s.map_name + ".sv2"), ours)) {
      save::Reader r(ours.data(), ours.size(), kChunkVersion);
      server_level(r, config, portals);
      if (!r.ok()) {
        error = "the kept level " + s.map_name + " does not read: " + r.problem();
        return false;
      }
      if (config.size() == s.raw.config.size()) s.raw.config = std::move(config);
      if (portals.size() == h.portals.size()) h.portals = std::move(portals);
      if (h.map) br::flood_areas(*h.map, h.portals, &h.area_groups);
    }
    std::fill(h.linked.begin(), h.linked.end(), uint8_t{0});
    const std::string file = path_text(sav);
    if (!guarded(h, [&] { h.game->read_level(file.c_str()); })) {
      error = s.stop_reason;
      return false;
    }
    if (!load) {  // entered again: the level runs on alone first
      h.loading = true;
      for (int i = 0; i < kReturnFrames && !h.dead; i++) frame(s);
      h.loading = false;
    }
  }
  if (!load && !dm) {
    h.autosave.clear();
    if (!pack(s, true, h.autosave, error, 0)) h.autosave.clear();
    error.clear();
  }
  return !h.dead;
}

std::string native_save_problem(const NativeQ2& s) {
  const NativeHost& h = *s.host;
  if (h.dead || h.game == nullptr || s.map_name.empty() || s.map_stopped)
    return "You must be in a game to save.";
  if (deathmatch(h)) return "Can't savegame in a deathmatch";
  if (h.ex != nullptr && h.ex->can_save != nullptr) {  // the game's own word, in place of the
    int yes = 0;                                       // dead player's (it says why itself)
    NativeHost& m = const_cast<NativeHost&>(h);
    if (!guarded(m, [&] { yes = h.ex->can_save(); })) return "You must be in a game to save.";
    return yes != 0 ? "" : "The game does not allow saving now.";
  }
  const bool coop = setting_on(const_cast<NativeHost&>(h), "coop");
  if (!coop && h.begun[0] && h.entity(1)->client != nullptr &&
      h.entity(1)->client->ps.stats[kStatHealth] <= 0)
    return "Can't savegame while dead!";
  return "";
}

bool native_save(NativeQ2& s, std::vector<uint8_t>& out, std::string& error, int64_t saved_unix) {
  error = native_save_problem(s);
  if (!error.empty()) return false;
  if (!write_level(s, true)) {
    error = "save: the game stopped as it wrote its level: " + s.host->error_text;
    return false;
  }
  return pack(s, false, out, error, saved_unix);
}

std::string native_q2_save_map(std::span<const uint8_t> bytes, std::string_view name,
                               std::string& error) {
  GameRecord g;
  if (!unpack(bytes, name, g, error)) return "";
  return g.map;
}

std::string native_q2_load_ask(NativeQ2& s, std::span<const uint8_t> bytes, std::string_view name,
                               std::string& error) {
  GameRecord g;
  if (!unpack(bytes, name, g, error)) return "";
  s.host->load.assign(bytes.begin(), bytes.end());
  s.host->load_name = name;
  return g.map;
}

}  // namespace shooters_edge::server
