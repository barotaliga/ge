// The sequel's dedicated server game (sequel_game.hpp).
//
// ge:summary The sequel's dedicated server game: its maps read, the rules started and moved on
// ge:keywords sequel-dedicated-server sequel_game protocol-34 servergame
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_net_test.cpp
#include "shooters_edge/net/sequel_game.hpp"

#include <iterator>
#include <memory>
#include <string>

#include "classicenhance/profile.hpp"
#include "core/num.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/sequel_login.hpp"
#include "shooters_edge/net/sequel_net_server.hpp"
#include "shooters_edge/server/sequel_server.hpp"
#include "shooters_edge/server/sequel_wire.hpp"

namespace shooters_edge::net {

namespace {

namespace cl = shooters_edge::client;
namespace gs = ge::server;

// The rules' settings a server's cfg, `+set` or the console may set (the rules' own cvars).
constexpr gs::GameRule kRules[] = {
    {"dmflags", "Deathmatch's rule bits.", nullptr,
     "0: none (the default); N: the sum of the bits, such as 8192 for infinite ammunition"},
    {"fraglimit", "The frags that end the level.", nullptr,
     "0: no limit (the default); 1..N: that many frags"},
    {"timelimit", "The minutes that end the level.", nullptr,
     "0: no limit (the default); 1..N: that many minutes"},
    {"sv_gravity", "The pull down on players, in units a second a second.", nullptr,
     "N: that pull (800 the default)", false},
};

struct Game {
  cl::SearchPath files;
  std::unique_ptr<cl::LoadedMap> map;
  sv::RulesRunner rules;
  SequelNetConfig cfg;
  std::unique_ptr<SequelNetServer> net;  // on the network only
  // The logins (sequel_login.hpp), when sv_login asks for them or the server has accounts.
  gs::LoginSettings login_set;
  ge::account::Service* accounts = nullptr;
  ge::net::Transport* account_link = nullptr;
  int64_t (*unix_us)() = nullptr;
  bool logins_wanted = false;
  std::unique_ptr<SequelLogin> login;
};

void* create(const gs::ServerConfig& c, std::string& error) {
  auto g = std::make_unique<Game>();
  const ge::classicenhance::Profile* sequel = ge::classicenhance::find_profile("sequel");
  if (sequel == nullptr) {
    error = "this build has no content row for the sequel, so its game folder is unknown";
    return nullptr;
  }
  const std::string base(sequel->base_dir);
  if (!cl::open_game_dir(c.root, base, g->files, error, *sequel)) return nullptr;
  const std::string name = c.map.empty() ? std::string("q2dm1") : c.map;
  g->map = cl::load_named_map(g->files, name, error);
  if (!g->map) return nullptr;
  if (g->map->spec.brushes == nullptr) {
    error = "maps/" + name + ".bsp is not a map of the sequel's family; name one from " + c.root +
            "/" + base;
    return nullptr;
  }
  sv::SequelServerOptions so;
  so.seed = c.seed;
  std::unique_ptr<sv::SequelServer> sq = sv::make_sequel_server(so);
  sq->sv.set.maxclients = c.maxclients;
  g->rules = sv::sequel_runner(std::move(sq));
  g->rules.console("deathmatch 1");
  if (!c.maprotation.empty()) {  // the rules' own list: this map, then the rotation's, wrapping
    g->rules->cvars.set("sv_maplist", name + " " + c.maprotation, ge::console::Source::local);
  }
  for (const auto& [rule, value] : c.rules) {
    if (value.empty()) continue;  // unset: the rules' own default
    std::string line = rule;
    line += ' ';
    line += value;
    g->rules.console(line);
  }
  if (!g->rules.start(g->map->spec, error)) return nullptr;
  g->cfg.hostname = c.hostname;
  g->cfg.max_clients = c.maxclients;
  g->cfg.password = c.password;
  g->cfg.wire.map_checksum = sv::sequel_map_checksum(g->map->file);
  sv::sequel_sky(*g->map->spec.brushes, g->cfg.wire);
  g->login_set = c.login;
  g->accounts = c.accounts;
  g->account_link = c.account_link;
  g->unix_us = c.unix_us;
  g->logins_wanted = c.login.login > 0 || c.accounts != nullptr || c.account_link != nullptr;
  return g.release();
}

// The map the rules asked for (an intermission's end, `map`, `changelevel`, `restart`) loaded and
// the rules started on it, the clients told; a map that cannot be read starts this one again.
void change_level(Game& g) {
  sv::RulesCommon& s = *g.rules;
  if (s.pending_map.empty()) return;
  const std::string asked = s.pending_map;
  std::string error;
  std::unique_ptr<cl::LoadedMap> next = cl::load_named_map(g.files, asked, error);
  if (next && next->spec.brushes == nullptr) {
    error = "maps/" + asked + ".bsp is not a map of the sequel's family";
    next.reset();
  }
  g.rules.carry();
  if (!next || !g.rules.start(next->spec, error)) {
    if (g.net) g.net->log.push_back("the level stays on " + g.map->name + ": " + error);
    if (!g.rules.start(g.map->spec, error)) {
      if (g.net) g.net->log.push_back("the level could not start again: " + error);
      s.pending_map.clear();
      return;
    }
  } else {
    g.map = std::move(next);
  }
  g.cfg.wire.map_checksum = sv::sequel_map_checksum(g.map->file);
  sv::sequel_sky(*g.map->spec.brushes, g.cfg.wire);
  if (g.net) sequel_net_level(*g.net, g.cfg.wire);
}

void destroy(void* g) {
  Game* game = static_cast<Game*>(g);
  if (game->net) sequel_net_stop(*game->net);
  delete game;
}

double frame_seconds(const void* g) { return static_cast<const Game*>(g)->rules->sv.set.mintic; }

void frame(void* g) {
  Game& game = *static_cast<Game*>(g);
  game.rules.frame(nullptr, 0);
  change_level(game);
}

int seated(const void* g) {
  const Game& game = *static_cast<const Game*>(g);
  return game.net ? sequel_net_clients(*game.net) : 0;
}

uint64_t hash(const void* g) { return sv::rules_hash(*static_cast<const Game*>(g)->rules); }

void fill_status(const void* g, ge::net::Status& st) {
  const Game& game = *static_cast<const Game*>(g);
  st.map = game.map->name;
}

bool serve(void* g, ge::net::Transport& port, std::string& error) {
  Game& game = *static_cast<Game*>(g);
  game.net = std::make_unique<SequelNetServer>();
  if (!sequel_net_start(*game.net, game.rules, port, game.cfg, error)) return false;
  if (!game.logins_wanted) return true;
  game.login = std::make_unique<SequelLogin>();
  return sequel_login_open(*game.login, *game.net, game.login_set, game.accounts, game.account_link,
                           game.unix_us, error);
}

void net_frame(void* g, int64_t now_us) {
  Game& game = *static_cast<Game*>(g);
  sequel_net_frame(*game.net, now_us);
  change_level(game);
  if (game.login) sequel_login_frame(*game.login);
}

const gs::ServerGame& make() {
  static gs::ServerGame g = [] {
    gs::ServerGame s;
    s.binary = "shooters_edge_sequel_server";
    s.title = "Shooter's Edge on the sequel's protocol";
    s.default_port = kPortSequel;
    s.max_clients = 32;
    s.default_map = "q2dm1";
    s.base_dir = "baseq2";
    s.rules = kRules;
    s.rule_count = static_cast<int>(std::size(kRules));
    s.create = create;
    s.destroy = destroy;
    s.frame_seconds = frame_seconds;
    s.frame = frame;
    s.seated = seated;
    s.hash = hash;
    s.fill_status = fill_status;
    s.serve = serve;
    s.net_frame = net_frame;
    return s;
  }();
  return g;
}

}  // namespace

const gs::ServerGame& sequel_game() { return make(); }

}  // namespace shooters_edge::net
