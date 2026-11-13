// The sequel's dedicated server game (sequel_game.hpp).
//
// ge:summary The sequel's dedicated server game: the map read, the rules started, the endpoint run
// ge:keywords sequel-dedicated-server sequel_game protocol-34 servergame
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_net_test.cpp
#include "shooters_edge/net/sequel_game.hpp"

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
  for (const auto& [rule, value] : c.rules) {
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

void destroy(void* g) {
  Game* game = static_cast<Game*>(g);
  if (game->net) sequel_net_stop(*game->net);
  delete game;
}

double frame_seconds(const void* g) { return static_cast<const Game*>(g)->rules->sv.set.mintic; }

void frame(void* g) { static_cast<Game*>(g)->rules.frame(nullptr, 0); }

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
