// Shooter's Edge's playable dedicated server, defined: the mod's server made from the settings,
// its map loaded, the classic endpoint started on the port the core hands it.
//
// ge:summary Shooter's Edge's playable dedicated server's definitions: create, serve, the frames
// ge:keywords classic_game serve net_frame create status
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_game_test.cpp
#include "shooters_edge/net/classic_game.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "core/num.hpp"
#include "mod/loader.hpp"
#include "net/packet.hpp"
#include "progs/file.hpp"
#include "shooter/settings.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/lobby_room.hpp"
#include "shooters_edge/net/classic_admin.hpp"
#include "shooters_edge/net/classic_coop.hpp"
#include "shooters_edge/net/classic_coop_game.hpp"
#include "shooters_edge/net/classic_qtv.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/announce.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/dedicated.hpp"
#include "shooters_edge/server/lobby.hpp"
#include "shooters_edge/server/qvm_server.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace shooters_edge::net {

namespace {

namespace gs = ge::server;
namespace cl = shooters_edge::client;
namespace sh = ge::shooter;

struct Game {
  // The C++ mod packages named in sv_mod (docs/mods.md § On the server), open while any rules run.
  std::vector<std::unique_ptr<ge::mod::LoadedMod>> packages;
  ge::progs::Progs progs;
  const ge::progs::Progs* program = &progs;  // the one played: `progs`, or a listen's own
  std::vector<uint8_t> module;               // the mod's compiled module (a .qvm); empty: `program`
  std::shared_ptr<cl::LoadedMap> map;        // the spec's models point into it: outlives `rules`
  sv::RulesRunner rules;                     // reads `progs`
  std::unique_ptr<ClassicServer> classic;    // serves `rules` once the core hands it the port
  ServerConfig cfg;
  gs::ServerConfig settings;  // what the server was opened with: every map is made from it
  cl::SearchPath files;       // the game folder the maps are read from (empty: the arena only)
  int changes = 0;            // maps changed to
  std::string failed;         // why the last change did not happen ("": it did)
  std::string held_map;       // the next map, while the intermission keeps the scores up
  // The lobby and the bots (sv_lobby, sv_bots; null: neither). Without sv_lobby it stays in its
  // match: the bots play every map as it comes.
  std::unique_ptr<cl::Room> room;
  std::array<const sv::RulesCommon*, gs::kLobbySeats> applied{};  // the rules a seat's team is on
  std::array<gs::Team, gs::kLobbySeats> shown{};                  // the team its user info says
  int said = 0;                                                   // the lobby's lines printed
  // With a classic game's packs in the folder: its item names and server words as local info
  // (profiles/items.hpp, profiles/words.hpp), set on every map before the rules start.
  std::vector<std::string> info_lines;
  // A listen server (ServerConfig::host): the window's content and its view of the game; slot 0
  // its player, a local seat. Null on a dedicated server.
  cl::ListenHost* host = nullptr;
  // A dedicated server with `sv_coop 1`: the campaign in coop in place of the match (every call
  // below hands over to it; the match's own members stay empty).
  std::unique_ptr<CoopGame> coop;
  // The players' votes on the settings for this map (engine/server/vote.hpp): a passed one is
  // written into `settings.rules`, so a restart keeps it, and put back when the map changes.
  gs::VoteBook vote;
  // The stream port for classic relays (classic_qtv.hpp; qtv_streamport 0: none).
  int qtv_port = 0;
  QtvConfig qtv_cfg;
  std::unique_ptr<QtvPort> qtv;
  ~Game() {
    if (qtv && classic) qtv_close(*qtv, *classic);
    qtv.reset();
    coop.reset();
    classic.reset();
    rules = {};  // its views into the map go before the map does
    map.reset();
  }
};

void host_bind(Game& g);
// A line printed to a listen server's local seat: its own player's (slot 0) goes to the window.
void host_print(void* ctx, int slot, const std::string& text) {
  const cl::ListenHost& h = *static_cast<const cl::ListenHost*>(ctx);
  if (slot == 0 && h.print != nullptr) h.print(h.ctx, text);
}

// A voice the endpoint relayed to a listen server's own player (slot 0), to the window.
void host_voice(void* ctx, int slot, const SvcMsg& v) {
  const cl::ListenHost& h = *static_cast<const cl::ListenHost*>(ctx);
  if (slot == 0 && h.heard != nullptr && v.ints.size() >= 3)
    h.heard(h.ctx, v.ints[0], v.ints[1], v.ints[2], v.data);
}

// The rules' frame handed to a listen server's window (its player's prints, effects, blows).
void host_tap(void* ctx, const sv::RulesCommon& game) {
  const cl::ListenHost& h = *static_cast<const cl::ListenHost*>(ctx);
  if (h.tap != nullptr) h.tap(h.ctx, game);
}

// The mod's own rules a server owner sets (server.cfg, `+set fraglimit 20`); the mod server keeps
// each one's default (server/qc_server.cpp), so an unset rule is left to it. The move's and the
// loop's settings (plugins/shooter/settings.hpp: `sv_gravity`, `sv_mintic`, `pm_ktjump`) follow,
// then the endpoint's own, which the mod never sees.
constexpr gs::GameRule kModRules[] = {
    {"deathmatch", "the match's rules: 1 and up deathmatch, as the mod reads them"},
    {"teamplay", "team rules, as the mod reads them"},
    {"fraglimit", "frags that end the match (0 none)"},
    {"timelimit", "the match's length, minutes (0 none)"},
    {"samelevel", "1: the map does not change at the end"},
    {"pausable", "1: a player may pause the game (0: only unpausing)"},
    // The aim help's weapons (server/shot_help.hpp): names, spaces between, `*` any characters.
    {"sv_aimhelp_allow", "the weapons the aim help may bend (empty: all but the denied)"},
    {"sv_aimhelp_deny", "the weapons the aim help never bends (\"rl lg\")"},
};

constexpr gs::GameRule kEndpointRules[] = {
    {"sv_maxrate", "the most bytes/s a client may ask for with its rate (0 no cap; 0)"},
    {"sv_antilag", "1: a shot sees the others where the shooter saw them (0)"},
    {"sv_voip", "1: players' voice is relayed to their team (0)"},
    {"maxfps", "the most moves a second a client sends, told in the server info (77)"},
    {"allow_download", "0: no file is offered to a client that lacks one (1)"},
    {"allow_download_skins", "0: no skins/ file is offered (1)"},
    {"allow_download_models", "0: no progs/ model is offered (1)"},
    {"allow_download_sounds", "0: no sound/ file is offered (1)"},
    {"allow_download_maps", "0: no maps/ file is offered (1)"},
    {"allow_download_other", "1: files outside those folders are offered too (0)"},
    {"sv_downloadchunksperframe", "chunks of 1 KB a downloading client gets a frame, 1..30 (30)"},
    // The stream port for classic relays (classic_qtv.hpp).
    {"qtv_streamport", "the TCP port classic relays take the match from, 0..65534 (0: none)"},
    {"qtv_maxstreams", "relays streamed at once (0 or less: no limit; 1)"},
    {"qtv_password", "the password a relay must give (empty: none)"},
    {"qtv_pendingtimeout", "seconds a relay has to finish its request, 1..60 (5)"},
    {"qtv_streamtimeout", "seconds a relay may take nothing before it is dropped, 5..600 (45)"},
    {"qtv_sayenabled", "1: a relay's viewers' chat reaches the players, not just spectators (0)"},
    // The coop campaign in place of a match (classic_coop_game.hpp): the endpoint's, never a mod's.
    {"sv_coop", "1: the game folder's campaign in coop, not a match (0)"},
    {"sv_coopbots", "coop: companion bots playing beside the players, 0..7 (0)"},
    {"skill", "coop: the monsters' skill for everyone, 0..3 (1)"},
    {"sv_friendlyfire", "coop: 1 lets one player's shots harm another (0)"},
    {"sv_friendlypush", "coop: 1 lets a spared teammate's blow push you, harmless (0)"},
    {"sv_episodes_done", "coop: the episodes done, a bit each; the start map opens the next (0)"},
};

// Every rule, in the order `--explain` lists them: built once.
const std::vector<gs::GameRule>& all_rules() {
  static const std::vector<gs::GameRule> rules = [] {
    std::vector<gs::GameRule> out(std::begin(kModRules), std::end(kModRules));
    int count = 0;
    const sh::Setting* table = sh::settings_table(count);
    for (int i = 0; i < count; i++) out.push_back({table[i].name, table[i].help});
    out.insert(out.end(), std::begin(kEndpointRules), std::end(kEndpointRules));
    return out;
  }();
  return rules;
}

bool is_endpoint_rule(const std::string& name) {
  for (const gs::GameRule& r : kEndpointRules)
    if (name == r.name) return true;
  return false;
}

// An endpoint rule as a whole number in `low..high`; unset leaves `out`. False with `error`.
bool endpoint_int(const gs::ServerConfig& c, const char* name, int low, int high, int& out,
                  std::string& error) {
  const auto it = c.rules.find(name);
  if (it == c.rules.end() || it->second.empty()) return true;
  char* end = nullptr;
  const long v = std::strtol(it->second.c_str(), &end, 10);
  if (end == it->second.c_str() || *end != '\0' || v < low || v > high) {
    error = std::string(name) + " \"" + it->second + "\" is not a whole number in " +
            std::to_string(low) + ".." + std::to_string(high) + " (--explain=" + name +
            " shows where it was set)";
    return false;
  }
  out = static_cast<int>(v);
  return true;
}

// The endpoint's own settings from the rules: false with `error` on a value out of range.
// The stream port's settings from the rules: false with `error` on a value out of range.
bool qtv_settings(const gs::ServerConfig& c, int& port, QtvConfig& q, std::string& error) {
  int pending = static_cast<int>(q.pending_s), timeout = static_cast<int>(q.stream_timeout_s);
  int say = q.say_enabled ? 1 : 0;
  if (!endpoint_int(c, "qtv_streamport", 0, 65534, port, error) ||
      !endpoint_int(c, "qtv_maxstreams", -1, 1024, q.max_streams, error) ||
      !endpoint_int(c, "qtv_pendingtimeout", 1, 60, pending, error) ||
      !endpoint_int(c, "qtv_streamtimeout", 5, 600, timeout, error) ||
      !endpoint_int(c, "qtv_sayenabled", 0, 1, say, error))
    return false;
  if (const auto it = c.rules.find("qtv_password"); it != c.rules.end()) q.password = it->second;
  q.pending_s = pending;
  q.stream_timeout_s = timeout;
  q.say_enabled = say != 0;
  return true;
}

bool endpoint_settings(const gs::ServerConfig& c, ServerConfig& cfg, std::string& error) {
  int antilag = cfg.antilag ? 1 : 0, voice = cfg.voice ? 1 : 0;
  int all = cfg.allow_download ? 1 : 0, skins = cfg.allow_skins ? 1 : 0;
  int models = cfg.allow_models ? 1 : 0, sounds = cfg.allow_sounds ? 1 : 0;
  int maps = cfg.allow_maps ? 1 : 0, other = cfg.allow_other ? 1 : 0;
  if (!endpoint_int(c, "sv_maxrate", 0, 1000000, cfg.maxrate, error) ||
      !endpoint_int(c, "sv_antilag", 0, 1, antilag, error) ||
      !endpoint_int(c, "sv_voip", 0, 1, voice, error) ||
      !endpoint_int(c, "maxfps", 20, 1000, cfg.maxfps, error) ||
      !endpoint_int(c, "allow_download", 0, 1, all, error) ||
      !endpoint_int(c, "allow_download_skins", 0, 1, skins, error) ||
      !endpoint_int(c, "allow_download_models", 0, 1, models, error) ||
      !endpoint_int(c, "allow_download_sounds", 0, 1, sounds, error) ||
      !endpoint_int(c, "allow_download_maps", 0, 1, maps, error) ||
      !endpoint_int(c, "allow_download_other", 0, 1, other, error) ||
      !endpoint_int(c, "sv_downloadchunksperframe", 1, 30, cfg.chunks_per_frame, error))
    return false;
  cfg.antilag = antilag != 0;
  cfg.voice = voice != 0;
  cfg.allow_download = all != 0;
  cfg.allow_skins = skins != 0;
  cfg.allow_models = models != 0;
  cfg.allow_sounds = sounds != 0;
  cfg.allow_maps = maps != 0;
  cfg.allow_other = other != 0;
  return true;
}

// A file of the game's search path, for a compiled module's reads.
bool read_game_file(void* ctx, const std::string& name, std::vector<uint8_t>& bytes) {
  const auto& files = *static_cast<const cl::SearchPath*>(ctx);
  std::vector<uint8_t> store;
  const std::span<const uint8_t> found = cl::find_file(files, name, store);
  if (found.empty()) return false;
  bytes.assign(found.begin(), found.end());
  return true;
}

// The mod's rules on the map `name`, set from the settings: into `map` and `rules`.
// A listen server's map is the one the window read (`supplied`), or the one playing again.
bool open_map(const Game& g, const std::string& name, std::shared_ptr<cl::LoadedMap>& map,
              sv::RulesRunner& rules, std::string& error,
              std::shared_ptr<cl::LoadedMap> supplied = nullptr) {
  const gs::ServerConfig& c = g.settings;
  if (supplied)
    map = std::move(supplied);
  else if (g.host != nullptr && g.map && g.map->name == name)
    map = g.map;
  else
    map = cl::load_named_map(g.files, name, error);
  if (!map) return false;
  const float speedjump = g.host != nullptr ? g.host->speedjump : 0.0f;  // pm_speedjump
  // A map of the family that carries brushes runs on the rules that trace them (the sequel's),
  // whatever the program: the programs' rules trace clip hulls (docs/goals/M18v.md, M18v2).
  const bool brushes = map->spec.brushes != nullptr;
  if (brushes) {
    sv::SequelServerOptions so;
    so.seed = c.seed;
    so.clients_take_view = true;  // the classic protocol's clients take the server's view
    so.speedjump = speedjump;
    std::unique_ptr<sv::SequelServer> sq = sv::make_sequel_server(so);
    sq->sv.set.maxclients = c.maxclients;
    rules = sv::sequel_runner(std::move(sq));
  } else if (!g.module.empty()) {
    sv::QvmServerOptions so;
    so.seed = c.seed;
    so.loop.maxclients = c.maxclients;
    so.loop.max_edicts = 1024;
    so.loop.move.speedjump = speedjump;
    so.read_file = &read_game_file;
    so.files_ctx = const_cast<cl::SearchPath*>(&g.files);
    so.list_dirs = {g.files.folder};
    if (!c.root.empty()) so.list_dirs.push_back(c.root + "/" + kGameFolder);
    so.write_dir = g.files.folder;
    std::unique_ptr<sv::QvmServer> qvm = sv::make_qvm_server(g.module, so, error);
    if (!qvm) return false;
    rules = sv::qvm_runner(std::move(qvm));
  } else {
    sv::QcServerOptions so;
    so.seed = c.seed;
    so.loop.maxclients = c.maxclients;
    so.loop.max_edicts = 1024;
    so.loop.move.speedjump = speedjump;
    std::unique_ptr<sv::QcServer> qc = sv::make_qc_server(*g.program, so, error);
    if (!qc) return false;
    rules = sv::qc_runner(std::move(qc));
  }
  if (g.host != nullptr) {  // the window's own lines: the items' names, the arsenal, its settings
    for (const std::string& line : g.host->lines) rules.console(line);
  } else {
    rules.console("deathmatch 1");
    // A mod checks it runs from its own folder by the server's `*gamedir`.
    if (g.cfg.gamedir != decltype(g.cfg){}.gamedir)
      rules.console("serverinfo *gamedir \"" + g.cfg.gamedir + "\"");
    for (const std::string& line : g.info_lines) rules.console(line);
  }
  for (const auto& [rule, value] : c.rules) {
    // A module checks a client's rate against the endpoint's cap, so it is told the cap.
    if (rule == "sv_maxrate" && !g.module.empty() && !value.empty() && value != "0")
      rules.console("sv_maxrate \"" + value + "\"");
    if (value.empty() || is_endpoint_rule(rule)) continue;
    sh::ServerSettings check;
    if (sh::find_setting(rule) && !sh::set_setting(check, rule, value, error)) {
      error.append(" (--explain=").append(rule).append(" shows where it was set)");
      return false;
    }
    char* end = nullptr;
    std::strtod(value.c_str(), &end);  // every rule is a number but the aim help's lists
    std::string line = rule;
    line.append(" \"").append(value).append("\"");
    // The sequel's rules keep their own few settings and leave the rest of a mod's alone.
    const bool taken = rules.console(line) || brushes;
    const bool text = rule == "sv_aimhelp_allow" || rule == "sv_aimhelp_deny";  // names
    if ((!text && (end == value.c_str() || *end != '\0')) || !taken) {
      error = line;
      error.append(" is not a number (--explain=");
      error.append(rule).append(" shows where it was set)");
      return false;
    }
  }
  for (const auto& p : g.packages) {  // the C++ mods, laid over the rules as the map starts
    rules->mods.tables.push_back(p->table);
    rules->mods.names.push_back(p->manifest.name);
  }
  rules->mods.seed = c.seed;
  if (!rules.start(map->spec, error)) {
    error = "the map " + name + " did not load: " + error;
    return false;
  }
  if (map->from_file) sv::use_map_sets(*rules, map->bsp);
  return true;
}

// Warmup's hold on the mod's limits: nothing ends a warmup but the lobby.
void hold_limits(const sv::RulesRunner& rules) {
  rules.console("fraglimit 0");
  rules.console("timelimit 0");
}

// The room: the lobby (a player needed before a countdown), the `sv_bots` fighters seated.
bool open_room(Game& g, std::string& error) {
  const gs::ServerConfig& c = g.settings;
  if (c.bots == 0 && c.lobby == 0) return true;
  g.room = std::make_unique<cl::Room>();
  cl::Room& r = *g.room;
  r.seed = c.seed;
  r.lobby.settings.humans = 1;  // bots alone warm up until a player joins
  r.lobby.settings.countdown_s = c.countdown_s;
  r.lobby.settings.intermission_s = c.intermission_s;
  if (g.host != nullptr) {  // the window's own player has seat 0, the bots the ones after
    r.lobby.settings = g.host->lobby_settings;
    gs::lobby_join(r.lobby, 0, g.host->player, false, g.rules->sv.time);
  }
  if (c.lobby == 0) r.lobby.phase = gs::LobbyPhase::match;
  // The bots find their way on clip hulls; on a map of brushes they wait for their lift
  // (docs/goals/M18v.md, M18v8), and the match plays without them.
  const int bots = g.map->spec.brushes != nullptr ? 0 : c.bots;
  if (bots < c.bots)
    g.rules->console.push_back("The bots do not play this kind of map yet, so the match has none.");
  for (int i = 0; i < bots; i++)
    if (cl::room_add_bot(r, g.rules, g.map->spec, sh::AimSkill::average, error) < 0) {
      std::string why = "sv_bots " + std::to_string(c.bots) + ": ";
      why.append(error).append(" (keep sv_bots under sv_maxclients)");
      error = std::move(why);
      return false;
    }
  if (r.lobby.phase != gs::LobbyPhase::match) hold_limits(g.rules);
  return true;
}

// The mod on top: the last of `sv_mod`'s comma-separated folders ("" when none is named).
std::string top_mod(const std::string& mods) {
  const size_t comma = mods.find_last_of(',');
  std::string top = comma == std::string::npos ? mods : mods.substr(comma + 1);
  while (!top.empty() && top.back() == ' ') top.pop_back();
  while (!top.empty() && top.front() == ' ') top.erase(0, 1);
  return top;
}

// The C++ mod packages among `sv_mod`'s comma-separated folders (a folder holding a
// `mod.manifest`, under `--root` or as given), loaded in order into `g.packages`; the other folders
// are left in `folders` for the game folder. False with `error` when a package is refused.
bool load_packages(Game& g, const gs::ServerConfig& c, std::string& folders, std::string& error) {
  std::string rest;
  size_t at = 0;
  while (at <= c.mod.size()) {
    size_t comma = c.mod.find(',', at);
    if (comma == std::string::npos) comma = c.mod.size();
    std::string folder = c.mod.substr(at, comma - at);
    at = comma + 1;
    while (!folder.empty() && folder.back() == ' ') folder.pop_back();
    while (!folder.empty() && folder.front() == ' ') folder.erase(0, 1);
    if (folder.empty()) continue;
    std::filesystem::path dir = std::filesystem::path(c.root) / folder;
    std::error_code ec;
    if (!std::filesystem::exists(dir / "mod.manifest", ec)) dir = folder;
    if (!std::filesystem::exists(dir / "mod.manifest", ec)) {
      if (!rest.empty()) rest += ',';
      rest += folder;
      continue;
    }
    auto loaded = std::make_unique<ge::mod::LoadedMod>();
    const ge::mod::LoadResult r = ge::mod::load_mod(dir, {SE_MOD_GAME, SE_MOD_API}, *loaded);
    if (!r.ok()) {
      error = "the mod package " + folder + " was refused: " + r.errors.front();
      return false;
    }
    g.packages.push_back(std::move(loaded));
  }
  folders = std::move(rest);
  return true;
}

// The program: a named mod's own (its compiled module `qwprogs.qvm` when it has one, else its
// folder's program files through the game's adapter, the base game's under it), else the
// baseline mod.
bool open_program(Game& g, const std::string& mod, std::string& error) {
  if (mod.empty() || mod == kGameFolder) return cl::baseline_progs(g.progs, error);
  if (read_game_file(&g.files, "qwprogs.qvm", g.module)) return true;
  const cl::GameAdapter& game = cl::classic_adapter();
  cl::FoundProgram found;
  std::string why;
  if (game.find_program == nullptr || !game.find_program(game, g.files, false, found, why)) {
    error = "the mod folder " + mod + " holds no multiplayer program this server runs" +
            (why.empty() ? std::string() : " (" + why + ")") + ". Put the mod's qwprogs.dat in " +
            g.files.folder + ", or leave sv_mod empty for the baseline mod.";
    return false;
  }
  g.progs = std::move(found.progs);
  return true;
}

void* create(const gs::ServerConfig& c, std::string& error) {
  auto g = std::make_unique<Game>();
  g->settings = c;
  std::string folders;  // sv_mod's game folders, the packages taken out
  if (!load_packages(*g, c, folders, error)) return nullptr;
  if (c.host == nullptr && coop_asked(c)) {
    if (!g->packages.empty()) {
      error =
          "C++ mods can't run in a coop campaign yet. Take the package out of sv_mod, or set "
          "sv_coop to 0.";
      return nullptr;
    }
    g->coop = coop_open(c, error);
    return g->coop ? g.release() : nullptr;
  }
  if (c.host != nullptr) {  // a listen server: the window's program, folder and first map
    g->host = static_cast<cl::ListenHost*>(c.host);
    g->program = g->host->progs;
    if (g->host->files != nullptr) g->files = *g->host->files;
    g->map = g->host->map;
  } else {
    const std::string mod = top_mod(folders);
    const std::string folder = mod.empty() ? std::string(kGameFolder) : mod;
    // The built-in arena needs no game folder; any other map, a rotation's or a mod's, is read
    // from one (the mod's over the base game's).
    if (!cl::open_game_dir(c.root, folder, g->files, error)) {
      if (c.map != "arena" || !mod.empty()) return nullptr;
      g->files = cl::SearchPath{};
      error.clear();
    }
    if (!open_program(*g, mod, error)) return nullptr;
    if (!mod.empty()) g->cfg.gamedir = mod;  // serverdata names it: a client mounts the mod too
    if (!cl::archive_paths(g->files).empty()) {
      const profiles::GameProfile& game = profiles::classic_game();
      g->info_lines = profiles::item_name_lines(game.tables.names);
      for (std::string& line : profiles::word_lines(game.words)) g->info_lines.push_back(line);
    }
  }
  if (!open_map(*g, c.map, g->map, g->rules, error)) return nullptr;
  if (!open_room(*g, error)) return nullptr;
  g->vote.settings = gs::vote_settings(c);
  if (g->host != nullptr) host_bind(*g);
  g->cfg.hostname = c.hostname;
  g->cfg.password = c.password;
  g->cfg.spectator_password = c.spectator_password;
  g->cfg.rcon_password = c.rcon_password;
  g->cfg.seed = c.seed;
  g->cfg.timeout_s = c.timeout_s;
  g->cfg.flood_msg = c.floodprot_msg;
  if (!gs::parse_flood_rule(c.floodprot, g->cfg.flood, error)) return nullptr;
  if (!endpoint_settings(c, g->cfg, error)) return nullptr;
  if (!qtv_settings(c, g->qtv_port, g->qtv_cfg, error)) return nullptr;
  return g.release();
}

// The map after `current` when a match ends: the next in `sv_maprotation` (round to its first
// after its last; a map not in it goes to its first), else the one the mod asked for.
std::string next_map(const gs::ServerConfig& c, const std::string& current,
                     const std::string& asked) {
  std::vector<std::string> rotation;
  size_t at = 0;
  while (at < c.maprotation.size()) {
    const size_t space = c.maprotation.find(' ', at);
    const size_t stop = space == std::string::npos ? c.maprotation.size() : space;
    if (stop > at) rotation.push_back(c.maprotation.substr(at, stop - at));
    at = stop + 1;
  }
  if (rotation.empty()) return asked;
  for (size_t i = 0; i < rotation.size(); i++)
    if (rotation[i] == current) return rotation[(i + 1) % rotation.size()];
  return rotation.front();
}

// ---- The players' votes on the settings (engine/server/vote.hpp) ----
//
// What may be voted on here: the mod's rules (`fraglimit`, `teamplay`) and the move's and the
// loop's settings (`sv_gravity`, `pm_speedjump`), past the vote's lists; never the endpoint's own
// (read when the server starts). A passed change is written into `settings.rules`, which every map
// is made from: a mod's rule is set in the running rules at once (a limit the warmup holds waits
// for the match), and a setting the players' own moves are predicted with restarts the map, so
// every client is told it as it signs on again. A restart keeps the change; the next map puts it
// back.
//
// `vote <aid> <id|name> on|off|<value>` grants one player an aid for this map (`speedjump`,
// `aimhelp`, `rjump`: classic_admin.hpp's grants, engine/server/aids.hpp), or takes it back. The
// change is keyed `<aid>:<user id>`, so two votes on one player are one change and two players are
// two; its value names the player and the grant (`Ann on`, `Ann 0.5`, `Ann off`), its `before`
// what he had. The next map puts each back; a player who left is passed over.

// A rule the warmup holds at 0 (hold_limits) while the lobby waits for its match.
bool held_now(const Game& g, std::string_view name) {
  return g.room && g.settings.lobby != 0 && g.room->lobby.phase != gs::LobbyPhase::match &&
         (name == "fraglimit" || name == "timelimit");
}

// `vote <aid> <id|name> on|off|<value>`: the player named, and the grant asked for him.
bool aid_vote_check(const Game& g, const gs::AidKind& kind, std::string_view value,
                    gs::VoteCheck& out) {
  std::string who, grant;
  if (!g.classic) {
    out.error = std::string(kind.words) + " is granted only on a server players join.";
    return false;
  }
  if (!gs::aid_vote_split(kind, value, who, grant, out.error)) return false;
  const ClassicServer& cs = *g.classic;
  const int slot = classic_find_seat(cs, who);
  if (slot < 0) {
    out.error = "No player is called " + who + ". Type \"users\" to see the ids and names.";
    return false;
  }
  const ClassicClient& c = cs.clients[slot];
  out.key = gs::aid_vote_key(kind, c.userid);
  out.value = c.name + " " + gs::aid_vote_word(kind, grant);
  out.before = c.name + " " + gs::aid_vote_word(kind, classic_aid_granted(cs, slot, kind));
  return true;
}

// The aid a vote's change is about (its name), or null.
const gs::AidKind* vote_aid(std::string_view name) { return gs::aid_find(classic_aids(), name); }

// A passed or put-back aid change made: the grant set on the user id its key names, while he is on
// the server.
void aid_vote_apply(Game& g, const gs::AidKind& kind, const gs::VoteChange& c) {
  if (!g.classic) return;
  const int userid = std::atoi(c.key.c_str() + std::string_view(kind.name).size() + 1);
  const size_t space = c.value.find_last_of(' ');
  std::string grant, error;
  gs::aid_value(kind, space == std::string::npos ? c.value : c.value.substr(space + 1), grant,
                error);
  classic_grant(*g.classic, kind, userid, grant, error);
}

bool vote_check(void* ctx, std::string_view name, std::string_view value, gs::VoteCheck& out) {
  const Game& g = *static_cast<const Game*>(ctx);
  if (const gs::AidKind* aid = vote_aid(name)) return aid_vote_check(g, *aid, value, out);
  const std::string rule(name), text(value);
  bool known = false;
  for (const gs::GameRule& r : all_rules()) known = known || rule == r.name;
  if (!known) {
    out.error = rule +
                " is not a setting of this server. Type \"vote rules\" to see what may be "
                "voted on (fraglimit, timelimit, teamplay, sv_gravity, pm_speedjump, "
                "speedjump <player> on, aimhelp <player> on...).";
    return false;
  }
  if (is_endpoint_rule(rule)) {
    out.error = rule + " is set when the server starts, so it can't be voted on";
    return false;
  }
  if (const sh::Setting* setting = sh::find_setting(rule); setting != nullptr) {
    sh::ServerSettings check;
    std::string error;
    if (!sh::set_setting(check, rule, text, error)) {
      out.error = error;
      return false;
    }
  } else if (rule == "sv_aimhelp_allow" || rule == "sv_aimhelp_deny") {
    // A list of weapons' names: any text.
  } else {
    char* end = nullptr;
    std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') {
      out.error = rule + " \"" + text + "\" is not a number";
      return false;
    }
  }
  const auto it = g.settings.rules.find(rule);
  out.before = it == g.settings.rules.end() ? std::string() : it->second;
  return true;
}

// The vote's lines printed (each to everyone, or to the one player), and its passed changes made.
void vote_turn(Game& g) {
  gs::VoteBook& b = g.vote;
  if (g.classic)
    for (size_t i = 0; i < b.said.size(); i++) {
      const gs::VoteNote& n = b.notes[i];
      const std::string line = b.said[i] + "\n";
      if (n.to < 0)
        classic_print_all(*g.classic, line);
      else if (n.to < sh::kMaxClients)
        client_print(*g.classic, n.to, 2, line);
    }
  b.said.clear();
  b.notes.clear();
  bool restart = false;
  for (const gs::VoteChange& c : gs::vote_take(b)) {
    if (const gs::AidKind* aid = vote_aid(c.name)) {
      aid_vote_apply(g, *aid, c);
      continue;
    }
    g.settings.rules[c.name] = c.value;
    // The aim help's leeway is the server's alone: nobody predicts with it, so no restart.
    if (sh::find_setting(c.name) != nullptr && !c.name.starts_with("sv_aimhelp"))
      restart = true;
    else if (!held_now(g, c.name))
      g.rules.console(c.name + " \"" + c.value + "\"");
  }
  if (restart && g.classic) {
    classic_print_all(*g.classic, "The map restarts so everyone moves by the new setting\n");
    sv::RulesCommon& now = *g.rules;
    now.pending_map = g.map->name;
    now.pending_change = sv::MapChange::restart;
  }
}

// The voters kept to the endpoint: every person on the server (players and spectators who have
// signed on, a listen server's own player), never a bot; an open vote's time checked.
void vote_sync(Game& g) {
  if (!g.classic) return;
  const double now = g.rules->sv.time;
  for (int i = 0; i < sh::kMaxClients && i < gs::kVoteSeats; i++) {
    const ClassicClient& c = g.classic->clients[i];
    const bool person =
        c.state == ConnState::spawned && (!c.local || (i == 0 && g.host != nullptr));
    gs::vote_seat(g.vote, i, person ? std::string_view(c.name) : std::string_view(), now);
  }
  gs::vote_tick(g.vote, now);
  vote_turn(g);
}

// A map change: every change a vote made goes back in the settings the next map is made from.
// Their lines go out at once, while the players are still on the map that is ending.
void vote_map_over(Game& g) {
  for (const gs::VoteChange& c : gs::vote_map_over(g.vote)) {
    if (const gs::AidKind* aid = vote_aid(c.name))
      aid_vote_apply(g, *aid, c);
    else
      g.settings.rules[c.name] = c.value;
  }
  vote_turn(g);
}

// The game moved onto the map `name` (`supplied`: a listen server's, read by its window): the
// next map's server made, the endpoint moved onto it, the old one let go. A map that does not load
// leaves the game on the one it was playing (`failed` says why, `error` too).
bool move_to(Game& g, const std::string& name, bool match_over, std::string& error,
             std::shared_ptr<cl::LoadedMap> supplied = nullptr) {
  // A vote lasts the map: put back before the next map is made (a restart keeps it).
  if (match_over) vote_map_over(g);
  std::shared_ptr<cl::LoadedMap> map;
  sv::RulesRunner next;
  if (!open_map(g, name, map, next, error, std::move(supplied))) {
    g.failed = error;
    return false;
  }
  // The endpoint keeps the runner's address: it is moved to where it lives before it is handed on.
  sv::RulesRunner old = std::move(g.rules);
  g.rules = std::move(next);
  if (g.classic && !classic_change(*g.classic, g.rules, error)) {
    g.rules = std::move(old);
    g.failed = error;
    return false;
  }
  old = {};  // the old rules go before the map they view
  g.map = std::move(map);
  g.changes++;
  g.failed.clear();
  if (g.host != nullptr && g.classic)  // the player joins the next map's rules (sync_host)
    g.classic->clients[0].rules_active = false;
  if (g.room) {  // the seats onto the new rules: warmup again after a match the mod ended
    if (match_over && g.settings.lobby != 0) gs::lobby_back(g.room->lobby);
    cl::room_enter(*g.room, g.rules, g.map->spec);
    g.applied.fill(nullptr);
  }
  host_bind(g);
  return true;
}

// The change the mod or the console asked for (`changelevel`, `map`, `restart`). A listen server's
// next map is its window's to read (`ListenHost::asked`, then `change`); a restart is the same map.
void change_if_asked(Game& g) {
  sv::RulesCommon& now = *g.rules;
  const std::string current = g.map->name;
  std::string name;
  bool match_over = true;
  gs::Lobby* lobby = g.room && g.settings.lobby != 0 ? &g.room->lobby : nullptr;
  if (lobby != nullptr && lobby->phase == gs::LobbyPhase::intermission) {
    // The scores stay up: the map the match's end asked for waits until the intermission is over
    // (what the mod asks meanwhile, its intermission's own, is dropped).
    now.pending_map.clear();
    if (!gs::lobby_next_due(*lobby, now.sv.time)) return;
    name = std::move(g.held_map);
    g.held_map.clear();
  } else {
    if (now.pending_map.empty()) return;
    name = now.pending_map;
    // The mod's end of a match asks for the next map, or for its own again: the rotation's turn.
    if (now.pending_change == sv::MapChange::restart)
      name = current;
    else if (now.pending_change == sv::MapChange::change_level || name == current)
      name = next_map(g.settings, current, name);
    match_over = now.pending_change != sv::MapChange::restart;
    now.pending_map.clear();
    if (match_over && lobby != nullptr && lobby->phase == gs::LobbyPhase::match &&
        lobby->settings.intermission_s > 0) {  // the intermission first, then this map
      gs::lobby_end(*lobby, now.sv.time);
      g.held_map = std::move(name);
      return;
    }
  }
  if (g.host != nullptr && name != current) {
    g.host->asked = name;
    return;
  }
  std::string error;
  move_to(g, name, match_over, error);
}

// A listen server's player (slot 0) on the rules while the lobby has it on the map (always without
// a lobby), off them on the lobby's screen.
void sync_host(Game& g) {
  if (g.host == nullptr || !g.classic) return;
  ClassicClient& c = g.classic->clients[0];
  const bool on = !g.room || gs::lobby_on_map(g.room->lobby, 0);
  if (on && !c.rules_active) {
    g.rules.connect(0, g.host->player, false, false);
    c.rules_active = true;
    g.applied[0] = nullptr;  // its team again
  } else if (!on && c.rules_active) {
    g.rules.drop(0);
    c.rules_active = false;
  }
}

// The room kept to the endpoint's seats: a bot seated is shown as a local seat and one gone is
// freed; a client who signed on takes a lobby seat and one who left frees it; a seat's team is
// put in its user info (every client sees the colours) and the mod's `.team`.
void sync_seats(Game& g) {
  if (!g.room || !g.classic) return;
  cl::Room& r = *g.room;
  ClassicServer& cs = *g.classic;
  const sv::RulesCommon& now_rules = *g.rules;
  const double now = now_rules.sv.time;
  for (int i = 0; i < gs::kLobbySeats && i < sh::kMaxClients; i++) {
    const auto at = static_cast<size_t>(i);
    const bool bot = r.bots[at] != nullptr;
    if (cs.clients[i].local && !bot && !(i == 0 && g.host != nullptr)) {
      cs.clients[i].rules_active = false;  // the room let it go from the rules
      classic_drop(cs, i, "left the room");
    }
    std::string error;
    if (bot && cs.clients[i].state == ConnState::free)
      classic_add_local(cs, i, r.bots[at]->name, error);
    const ClassicClient& c = cs.clients[i];
    gs::LobbySeat& seat = r.lobby.seats[at];
    if (!c.local && c.state == ConnState::free && seat.used && !seat.bot) {
      gs::lobby_leave(r.lobby, i, now);
      g.applied[at] = nullptr;
    }
    if (i == 0) sync_host(g);
    if (!c.rules_active) continue;
    if (!c.local && !seat.used) {
      gs::lobby_join(r.lobby, i, c.name, false, now);
      seat.warm = true;  // a client who joined over the network is on the map already
    }
    if (!seat.used || (g.applied[at] == &now_rules && g.shown[at] == seat.team)) continue;
    const std::string colour = std::to_string(sv::team_colour(seat.team));
    classic_set_info(cs, i, "team", gs::team_name(seat.team));
    classic_set_info(cs, i, "topcolor", colour);
    classic_set_info(cs, i, "bottomcolor", colour);
    sv::lobby_apply(g.rules, r.lobby, i);  // the mod's own `.team`
    g.applied[at] = &now_rules;
    g.shown[at] = seat.team;
  }
}

// The lobby's turn after a frame: its lines printed to everyone, and a countdown run out restarts
// the map with every seat on it, the match.
void lobby_turn(Game& g) {
  if (!g.room) return;
  cl::Room& r = *g.room;
  sv::RulesCommon& now = *g.rules;
  if (g.settings.lobby != 0 && gs::lobby_due(r.lobby, now.sv.time)) {
    now.pending_map = g.map->name;
    now.pending_change = sv::MapChange::restart;
  }
  g.said += static_cast<int>(r.lobby.said.size());
  sv::MatchLook look;
  if (const ge::console::Cvar* c = now.cvars.find("deathmatch"); c != nullptr)
    look.deathmatch = static_cast<int>(c->number);
  if (const ge::console::Cvar* c = now.cvars.find("teamplay"); c != nullptr)
    look.teamplay = static_cast<int>(c->number);
  std::vector<std::string> lines;
  if (g.host != nullptr) lines = g.host->lines;
  for (const auto& [rule, value] : g.settings.rules)
    if (!value.empty()) {
      std::string line = rule;
      line += ' ';
      line += value;
      lines.push_back(std::move(line));
    }
  sv::announce(r.announcer, r.lobby, now.localinfo, sv::match_look(look, lines), now.sv.time);
}

// The lobby's words handed to the rules playing now (after a change, the new map's): they go out
// with the next frame's events, and into its recording.
void lobby_words(Game& g) {
  if (!g.room) return;
  sv::announce_phase(g.rules->localinfo, g.room->lobby.phase);
  sv::announce_flush(g.room->announcer, g.rules->out, g.rules->sv.time);
}

// A client's words the server takes before the mod: the vote's (`vote ...`, and `yes` and `no`
// while one is open) from anyone; with a room, `addbot [skill]` and `removebot` from the leader,
// and with sv_lobby the lobby's own (ready, break, team, start, lobby).
bool room_words(void* ctx, int slot, const std::string& line) {
  Game& g = *static_cast<Game*>(ctx);
  if (g.classic) {
    const gs::VoteTarget target{&g, vote_check};
    if (gs::vote_command(g.vote, target, slot, line, g.rules->sv.time)) {
      vote_turn(g);
      return true;
    }
  }
  if (!g.room) return false;
  cl::Room& r = *g.room;
  ClassicServer& cs = *g.classic;
  const size_t space = line.find(' ');
  const std::string word = line.substr(0, space);
  if (word == "addbot" || word == "removebot") {
    const int leader = gs::lobby_leader(r.lobby);
    if (slot != leader) {
      const std::string who =
          leader >= 0 ? r.lobby.seats[static_cast<size_t>(leader)].name : std::string("nobody");
      client_print(cs, slot, 2, word + ": only the leader (" + who + ") changes the bots\n");
      return true;
    }
    std::string error;
    if (word == "removebot") {
      for (int i = gs::kLobbySeats - 1; i > 0; i--)
        if (r.bots[static_cast<size_t>(i)]) {
          cs.clients[i].rules_active = false;  // room_remove_bot lets it go from the rules
          break;
        }
      if (!cl::room_remove_bot(r, g.rules))
        client_print(cs, slot, 2, "There are no bots to remove.\n");
      sync_seats(g);
      return true;
    }
    sh::AimSkill skill = sh::AimSkill::average;
    const std::string named = space == std::string::npos ? "" : line.substr(space + 1);
    if (!named.empty() && !cl::skill_named(named, skill)) {
      client_print(cs, slot, 2,
                   "addbot: unknown skill \"" + named + "\" (use novice, average or expert)\n");
      return true;
    }
    r.held = 0;
    for (int i = 0; i < sh::kMaxClients; i++)
      if (cs.clients[i].state != ConnState::free) r.held |= 1u << i;
    if (cl::room_add_bot(r, g.rules, g.map->spec, skill, error) < 0)
      client_print(cs, slot, 2, "addbot: " + error + "\n");
    sync_seats(g);
    return true;
  }
  if (g.settings.lobby == 0) return false;
  return gs::lobby_command(r.lobby, slot, line, g.rules->sv.time);
}

void destroy(void* g) { delete static_cast<Game*>(g); }

double frame_seconds(const void* g) {
  const Game& game = *static_cast<const Game*>(g);
  return game.coop ? sv::kCampaignStep : game.rules->sv.set.mintic;
}

// A bot's move length: the server's frame, ms.
int frame_msec(const Game& g) {
  return static_cast<int>(ge::core::num::round(g.rules->sv.set.mintic * 1000));
}

// Offline: the mod's frame with nobody's moves, its streams emptied as the endpoint would.
void frame(void* g) {
  Game& game = *static_cast<Game*>(g);
  if (game.coop) return coop_frame(*game.coop);
  std::vector<sh::Packet> moves;
  if (game.room) cl::room_packets(*game.room, *game.rules, frame_msec(game), moves);
  game.rules.frame(moves.data(), static_cast<int>(moves.size()));
  sv::RulesCommon& now = *game.rules;
  now.out.all.clear();
  for (auto& one : now.out.one) one.clear();
  sv::clear_frame(now.out);
  lobby_turn(game);
  change_if_asked(game);
  lobby_words(game);
}

bool serve(void* g, ge::net::Transport& port, std::string& error) {
  Game& game = *static_cast<Game*>(g);
  if (game.coop) return coop_serve(*game.coop, port, error);
  game.classic = std::make_unique<ClassicServer>();
  if (!classic_start(*game.classic, game.rules, port, game.cfg, error)) return false;
  game.classic->game_command = room_words;
  game.classic->game_ctx = &game;
  if (game.host != nullptr) {  // the window's player: slot 0, a local seat the window moves
    if (!classic_add_local(*game.classic, 0, game.host->player, error, false)) return false;
    game.classic->clients[0].rules_active = false;  // sync_host puts it on the map
    game.classic->local_tap = host_tap;
    game.classic->local_print = host_print;
    game.classic->local_voice = host_voice;
    game.classic->local_ctx = game.host;
  }
  if (game.qtv_port > 0) {
    game.qtv = std::make_unique<QtvPort>();
    if (!qtv_open(*game.qtv, ge::net::ipv4(0, 0, 0, 0, static_cast<uint16_t>(game.qtv_port)),
                  game.qtv_cfg, error))
      return false;
  }
  sync_host(game);
  sync_seats(game);
  return true;
}

void net_frame(void* g, int64_t now_us) {
  Game& game = *static_cast<Game*>(g);
  if (game.coop) return coop_net_frame(*game.coop, now_us);
  if (game.room)
    cl::room_packets(*game.room, *game.rules, frame_msec(game), game.classic->local_moves);
  classic_frame(*game.classic, now_us);
  if (game.qtv) qtv_update(*game.qtv, *game.classic);
  sync_host(game);
  sync_seats(game);
  vote_sync(game);
  lobby_turn(game);
  change_if_asked(game);
  lobby_words(game);
  sync_host(game);
  sync_seats(game);  // a new map's seats
}

int seated(const void* g) {
  const Game& game = *static_cast<const Game*>(g);
  if (game.coop) return coop_seated(*game.coop);
  return game.classic ? classic_count(*game.classic, ConnState::spawned) : 0;
}

uint64_t hash(const void* g) {
  const Game& game = *static_cast<const Game*>(g);
  return game.coop ? coop_hash(*game.coop) : sv::rules_hash(*game.rules);
}

// The hub's record (docs/hub.md): the map, the mode the rules make, the phase, every client.
void fill_status(const void* g, ge::net::Status& st) {
  const Game& game = *static_cast<const Game*>(g);
  if (game.map) st.map = game.map->name;
  auto number = [&](const char* name) {
    const ge::console::Cvar* v = game.rules ? game.rules->cvars.find(name) : nullptr;
    return v ? std::atoi(v->value.c_str()) : 0;
  };
  const int seats = game.settings.maxclients;
  if (game.coop || number("coop") != 0) {
    st.mode = "coop";
  } else if (number("teamplay") != 0) {
    st.mode = seats <= 4 ? "2on2" : "4on4";
  } else {
    st.mode = seats <= 2 ? "1on1" : "ffa";
  }
  if (!game.held_map.empty()) st.phase = ge::net::StatusPhase::intermission;
  for (const char* name : {"fraglimit", "timelimit"}) {
    st.keys.emplace_back(name, std::to_string(number(name)));
  }
  if (game.classic) status_players(*game.classic, st.players);
}

// The rules the mod is playing by, then the endpoint's counts.
std::string status(const void* g) {
  const Game& game = *static_cast<const Game*>(g);
  if (game.coop) return coop_status(*game.coop);
  std::string out = "map=" + game.map->name;
  if (!game.failed.empty()) out += " (no change: " + game.failed + ")";
  for (const char* name : {"deathmatch", "fraglimit", "timelimit"}) {
    const ge::console::Cvar* v = game.rules->cvars.find(name);
    out.append(" ").append(name).append("=").append(v ? v->value : "?");
  }
  if (game.room) {
    const gs::Lobby& l = game.room->lobby;
    int bots = 0;
    for (const auto& b : game.room->bots) bots += b ? 1 : 0;
    out.append(" bots=").append(std::to_string(bots));
    if (game.settings.lobby != 0)
      out.append(" lobby=")
          .append(gs::phase_name(l.phase))
          .append(" ready=")
          .append(std::to_string(gs::lobby_ready_count(l)))
          .append("/")
          .append(std::to_string(gs::lobby_seated(l)))
          .append(" matches=")
          .append(std::to_string(l.matches));
  }
  if (!game.classic) return out + " offline";
  const ServerCounts& n = game.classic->counts;
  char counts[128];
  std::snprintf(counts, sizeof counts, " connected=%d connects=%lld drops=%lld out=%lld B",
                classic_count(*game.classic, ConnState::connected),
                static_cast<long long>(n.connects), static_cast<long long>(n.drops),
                static_cast<long long>(n.bytes_out));
  return out + counts;
}

// ---- A listen server's calls (cl::ListenHost) ----

bool host_words(void* g, const std::string& line) {
  Game& game = *static_cast<Game*>(g);
  const bool taken = room_words(&game, 0, line);
  sync_host(game);
  sync_seats(game);
  lobby_turn(game);  // the answer goes out with the next frame
  lobby_words(game);
  return taken;
}

bool host_admin(void* g, const std::string& line, std::string& out) {
  Game& game = *static_cast<Game*>(g);
  return game.classic && classic_admin(*game.classic, line, out);
}

void host_move(void* g, const sh::Packet& p) {
  Game& game = *static_cast<Game*>(g);
  if (game.classic) game.classic->local_moves.push_back(p);
}

bool host_change(void* g, std::shared_ptr<cl::LoadedMap> map, std::string& error) {
  Game& game = *static_cast<Game*>(g);
  const std::string name = map->name;
  if (!move_to(game, name, true, error, std::move(map))) return false;
  sync_host(game);
  sync_seats(game);
  return true;
}

void host_info(void* g, const std::string& key, const std::string& value) {
  Game& game = *static_cast<Game*>(g);
  if (game.classic) classic_set_info(*game.classic, 0, key, value);
}

// The window's player talking: his voice message relayed by the endpoint as a client's.
void host_speak(void* g, int gen, int seq, const std::vector<uint8_t>& data) {
  Game& game = *static_cast<Game*>(g);
  if (!game.classic) return;
  ClcMsg m;
  m.id = clc::voice;
  m.gen = gen;
  m.seq = seq;
  m.data = data;
  classic_local_voice(*game.classic, 0, m);
}

void host_bind(Game& g) {
  if (g.host == nullptr) return;
  cl::ListenHost& h = *g.host;
  h.game = &g;
  h.rules = &g.rules;
  h.room = g.room.get();
  h.map = g.map;
  h.changes = g.changes;
  h.words = host_words;
  h.move = host_move;
  h.change = host_change;
  h.set_info = host_info;
  h.speak = host_speak;
  h.admin = host_admin;
}

gs::ServerGame make() {
  gs::ServerGame g;
  g.binary = "shooters_edge_server";
  g.title = "Shooter's Edge";
  g.protocol = ge::net::protocol_id("shooters_edge", 1);
  g.default_port = server::kServerPort;
  g.discovery_port = 0;  // classic browsers ask the port itself (status, ping)
  g.max_clients = 32;
  g.default_map = "arena";
  g.base_dir = kGameFolder;
  g.rules = all_rules().data();
  g.rule_count = static_cast<int>(all_rules().size());
  g.create = create;
  g.destroy = destroy;
  g.frame_seconds = frame_seconds;
  g.frame = frame;
  g.seated = seated;
  g.hash = hash;
  g.status = status;
  g.fill_status = fill_status;
  g.version = 1;
  g.serve = serve;
  g.net_frame = net_frame;
  return g;
}

}  // namespace

const gs::ServerGame& classic_game() {
  static const gs::ServerGame g = make();
  return g;
}

cl::ListenOptions listen_options() {
  cl::ListenOptions o;
  o.game = &classic_game();
  o.door = &classic_coop();
  return o;
}

}  // namespace shooters_edge::net
