// Shooter's Edge's playable dedicated server, defined: the mod's server made from the settings,
// its map loaded, the classic endpoint started on the port the core hands it.
//
// ge:summary Shooter's Edge's playable dedicated server's definitions: create, serve, the frames
// ge:keywords classic_game serve net_frame create status
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_game_test.cpp
#include "shooters_edge/net/classic_game.hpp"

#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "net/packet.hpp"
#include "progs/file.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/dedicated.hpp"

namespace shooters_edge::net {

namespace {

namespace gs = ge::server;
namespace cl = shooters_edge::client;

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
  ~Game() {
    classic.reset();
    rules = {};  // its views into the map go before the map does
    map.reset();
  }
};

// The mod's own rules a server owner sets (server.cfg, `+set fraglimit 20`); the mod server keeps
// each one's default (server/qc_server.cpp), so an unset rule is left to it.
constexpr gs::GameRule kRules[] = {
    {"deathmatch", "the match's rules: 1 and up deathmatch, as the mod reads them"},
    {"teamplay", "team rules, as the mod reads them"},
    {"fraglimit", "frags that end the match (0 none)"},
    {"timelimit", "the match's length, minutes (0 none)"},
    {"samelevel", "1: the map does not change at the end"},
    {"sv_gravity", "the world's gravity, units/s^2"},
    {"sv_maxspeed", "a player's run speed, units/s"},
};

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
    if (value.empty()) continue;
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

void* create(const gs::ServerConfig& c, std::string& error) {
  if (c.bots != 0) {
    error = "sv_bots " + std::to_string(c.bots) +
            ": bots on the classic server come with the engine's lobby (M53c); next: --bots=0";
    return nullptr;
  }
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
  g->cfg.hostname = c.hostname;
  g->cfg.password = c.password;
  g->cfg.spectator_password = c.spectator_password;
  g->cfg.seed = c.seed;
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
}

void destroy(void* g) { delete static_cast<Game*>(g); }

double frame_seconds(const void* g) { return static_cast<const Game*>(g)->rules->sv.set.mintic; }

// Offline: the mod's frame with nobody's moves, its streams emptied as the endpoint would.
void frame(void* g) {
  Game& game = *static_cast<Game*>(g);
  game.rules.frame(nullptr, 0);
  sv::RulesCommon& now = *game.rules;
  now.out.all.clear();
  for (auto& one : now.out.one) one.clear();
  sv::clear_frame(now.out);
  change_if_asked(game);
}

bool serve(void* g, ge::net::Transport& port, std::string& error) {
  Game& game = *static_cast<Game*>(g);
  game.classic = std::make_unique<ClassicServer>();
  return classic_start(*game.classic, game.rules, port, game.cfg, error);
}

void net_frame(void* g, int64_t now_us) {
  Game& game = *static_cast<Game*>(g);
  classic_frame(*game.classic, now_us);
  change_if_asked(game);
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
  g.rules = kRules;
  g.rule_count = static_cast<int>(std::size(kRules));
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
