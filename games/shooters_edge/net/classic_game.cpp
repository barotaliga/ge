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
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "core/num.hpp"
#include "net/packet.hpp"
#include "progs/file.hpp"
#include "shooter/settings.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/lobby_room.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/dedicated.hpp"
#include "shooters_edge/server/lobby.hpp"

namespace shooters_edge::net {

namespace {

namespace gs = ge::server;
namespace cl = shooters_edge::client;
namespace sh = ge::shooter;

struct Game {
  ge::progs::Progs progs;
  std::unique_ptr<cl::LoadedMap> map;      // the spec's models point into it: outlives `rules`
  sv::RulesRunner rules;                   // reads `progs`
  std::unique_ptr<ClassicServer> classic;  // serves `rules` once the core hands it the port
  ServerConfig cfg;
  gs::ServerConfig settings;  // what the server was opened with: every map is made from it
  cl::SearchPath files;       // the game folder the maps are read from (empty: the arena only)
  int changes = 0;            // maps changed to
  std::string failed;         // why the last change did not happen ("": it did)
  // The lobby and the bots (sv_lobby, sv_bots; null: neither). Without sv_lobby it stays in its
  // match: the bots play every map as it comes.
  std::unique_ptr<cl::Room> room;
  std::array<const sv::RulesCommon*, gs::kLobbySeats> applied{};  // the rules a seat's team is on
  std::array<gs::Team, gs::kLobbySeats> shown{};                  // the team its user info says
  int said = 0;                                                   // the lobby's lines printed
  ~Game() {
    classic.reset();
    rules = {};  // its views into the map go before the map does
    map.reset();
  }
};

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
};

constexpr gs::GameRule kEndpointRules[] = {
    {"sv_maxrate", "the most bytes/s a client may ask for with its `rate` (0 no cap; 0)"},
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
            std::to_string(low) + ".." + std::to_string(high) +
            "; next: set it in range (--explain=" + name + " shows where it was set)";
    return false;
  }
  out = static_cast<int>(v);
  return true;
}

// The endpoint's own settings from the rules: false with `error` on a value out of range.
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

// The mod's rules on the map `name`, set from the settings: into `map` and `rules`.
bool open_map(const Game& g, const std::string& name, std::unique_ptr<cl::LoadedMap>& map,
              sv::RulesRunner& rules, std::string& error) {
  const gs::ServerConfig& c = g.settings;
  map = cl::load_named_map(g.files, name, error);
  if (!map) return false;
  sv::QcServerOptions so;
  so.seed = c.seed;
  so.loop.maxclients = c.maxclients;
  so.loop.max_edicts = 1024;
  std::unique_ptr<sv::QcServer> qc = sv::make_qc_server(g.progs, so, error);
  if (!qc) return false;
  rules = sv::qc_runner(std::move(qc));
  rules.console("deathmatch 1");
  for (const auto& [rule, value] : c.rules) {
    if (value.empty() || is_endpoint_rule(rule)) continue;
    sh::ServerSettings check;
    if (sh::find_setting(rule) && !sh::set_setting(check, rule, value, error)) {
      error.append(" (--explain=").append(rule).append(" shows where it was set)");
      return false;
    }
    char* end = nullptr;
    std::strtod(value.c_str(), &end);  // every rule is a number
    std::string line = rule;
    line.append(" \"").append(value).append("\"");
    if (end == value.c_str() || *end != '\0' || !rules.console(line)) {
      error = line;
      error.append(" is not a number; next: set ").append(rule).append(" to one (--explain=");
      error.append(rule).append(" shows where it was set)");
      return false;
    }
  }
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
  if (c.lobby == 0) r.lobby.phase = gs::LobbyPhase::match;
  for (int i = 0; i < c.bots; i++)
    if (cl::room_add_bot(r, g.rules, g.map->spec, sh::AimSkill::average, error) < 0) {
      std::string why = "sv_bots " + std::to_string(c.bots) + ": ";
      why.append(error).append("; next: lower sv_bots under sv_maxclients");
      error = std::move(why);
      return false;
    }
  if (r.lobby.phase != gs::LobbyPhase::match) hold_limits(g.rules);
  return true;
}

void* create(const gs::ServerConfig& c, std::string& error) {
  auto g = std::make_unique<Game>();
  g->settings = c;
  if (!cl::baseline_progs(g->progs, error)) return nullptr;
  // The built-in arena needs no game folder; any other map, or a rotation's, is read from one.
  if (!cl::open_game_dir(c.root, kGameFolder, g->files, error)) {
    if (c.map != "arena") return nullptr;
    g->files = cl::SearchPath{};
    error.clear();
  }
  if (!open_map(*g, c.map, g->map, g->rules, error)) return nullptr;
  if (!open_room(*g, error)) return nullptr;
  g->cfg.hostname = c.hostname;
  g->cfg.password = c.password;
  g->cfg.spectator_password = c.spectator_password;
  g->cfg.seed = c.seed;
  g->cfg.timeout_s = c.timeout_s;
  g->cfg.flood_msg = c.floodprot_msg;
  if (!gs::parse_flood_rule(c.floodprot, g->cfg.flood, error)) return nullptr;
  if (!endpoint_settings(c, g->cfg, error)) return nullptr;
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

// The change the mod or the console asked for (`changelevel`, `map`, `restart`): the next map's
// server made, the endpoint moved onto it, the old one let go. A map that does not load leaves
// the game on the one it was playing (`failed` says why).
void change_if_asked(Game& g) {
  sv::RulesCommon& now = *g.rules;
  if (now.pending_map.empty()) return;
  const std::string current = g.map->name;
  std::string name = now.pending_map;
  // The mod's end of a match asks for the next map, or for its own again: the rotation's turn.
  if (now.pending_change == sv::MapChange::restart)
    name = current;
  else if (now.pending_change == sv::MapChange::change_level || name == current)
    name = next_map(g.settings, current, name);
  const bool match_over = now.pending_change != sv::MapChange::restart;
  now.pending_map.clear();
  std::unique_ptr<cl::LoadedMap> map;
  sv::RulesRunner next;
  std::string error;
  if (!open_map(g, name, map, next, error)) {
    g.failed = error;
    return;
  }
  // The endpoint keeps the runner's address: it is moved to where it lives before it is handed on.
  sv::RulesRunner old = std::move(g.rules);
  g.rules = std::move(next);
  if (g.classic && !classic_change(*g.classic, g.rules, error)) {
    g.rules = std::move(old);
    g.failed = error;
    return;
  }
  old = {};  // the old rules go before the map they view
  g.map = std::move(map);
  g.changes++;
  g.failed.clear();
  if (g.room) {  // the seats onto the new rules: warmup again after a match the mod ended
    if (match_over && g.settings.lobby != 0) gs::lobby_back(g.room->lobby);
    cl::room_enter(*g.room, g.rules, g.map->spec);
    g.applied.fill(nullptr);
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
    if (cs.clients[i].local && !bot) {
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
    if (!c.rules_active) continue;
    if (!c.local && !seat.used) gs::lobby_join(r.lobby, i, c.name, false, now);
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
  for (const std::string& line : r.lobby.said) {
    if (g.classic) classic_print_all(*g.classic, line + "\n");
    g.said++;
  }
  r.lobby.said.clear();
}

// A client's words the room takes before the mod: `addbot [skill]` and `removebot` from the
// leader, and with sv_lobby the lobby's own (ready, break, team, start, lobby).
bool room_words(void* ctx, int slot, const std::string& line) {
  Game& g = *static_cast<Game*>(ctx);
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
        client_print(cs, slot, 2, "removebot: no bot is seated; next: `addbot`\n");
      sync_seats(g);
      return true;
    }
    sh::AimSkill skill = sh::AimSkill::average;
    const std::string named = space == std::string::npos ? "" : line.substr(space + 1);
    if (!named.empty() && !cl::skill_named(named, skill)) {
      client_print(cs, slot, 2,
                   "addbot " + named + ": no such skill; next: novice, average or expert\n");
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

double frame_seconds(const void* g) { return static_cast<const Game*>(g)->rules->sv.set.mintic; }

// A bot's move length: the server's frame, ms.
int frame_msec(const Game& g) {
  return static_cast<int>(ge::core::num::round(g.rules->sv.set.mintic * 1000));
}

// Offline: the mod's frame with nobody's moves, its streams emptied as the endpoint would.
void frame(void* g) {
  Game& game = *static_cast<Game*>(g);
  std::vector<sh::Packet> moves;
  if (game.room) cl::room_packets(*game.room, *game.rules, frame_msec(game), moves);
  game.rules.frame(moves.data(), static_cast<int>(moves.size()));
  sv::RulesCommon& now = *game.rules;
  now.out.all.clear();
  for (auto& one : now.out.one) one.clear();
  sv::clear_frame(now.out);
  lobby_turn(game);
  change_if_asked(game);
}

bool serve(void* g, ge::net::Transport& port, std::string& error) {
  Game& game = *static_cast<Game*>(g);
  game.classic = std::make_unique<ClassicServer>();
  if (!classic_start(*game.classic, game.rules, port, game.cfg, error)) return false;
  game.classic->game_command = room_words;
  game.classic->game_ctx = &game;
  sync_seats(game);
  return true;
}

void net_frame(void* g, int64_t now_us) {
  Game& game = *static_cast<Game*>(g);
  if (game.room)
    cl::room_packets(*game.room, *game.rules, frame_msec(game), game.classic->local_moves);
  classic_frame(*game.classic, now_us);
  sync_seats(game);
  lobby_turn(game);
  change_if_asked(game);
  sync_seats(game);  // a new map's seats
}

int seated(const void* g) {
  const Game& game = *static_cast<const Game*>(g);
  return game.classic ? classic_count(*game.classic, ConnState::spawned) : 0;
}

uint64_t hash(const void* g) {
  return ge::shooter::server_hash(static_cast<const Game*>(g)->rules->sv);
}

// The rules the mod is playing by, then the endpoint's counts.
std::string status(const void* g) {
  const Game& game = *static_cast<const Game*>(g);
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
  g.serve = serve;
  g.net_frame = net_frame;
  return g;
}

}  // namespace

const gs::ServerGame& classic_game() {
  static const gs::ServerGame g = make();
  return g;
}

}  // namespace shooters_edge::net
