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
};

void* create(const gs::ServerConfig& c, std::string& error) {
  auto g = std::make_unique<Game>();
  const ge::classicenhance::Profile* sequel = ge::classicenhance::find_profile("sequel");
  if (sequel == nullptr) {
    error = "the sequel's content row is missing; next: classicenhance/profile.hpp";
    return nullptr;
  }
  const std::string base(sequel->base_dir);
  if (!cl::open_game_dir(c.root, base, g->files, error, *sequel)) return nullptr;
  const std::string name = c.map.empty() ? std::string("q2dm1") : c.map;
  g->map = cl::load_named_map(g->files, name, error);
  if (!g->map) return nullptr;
  if (g->map->spec.brushes == nullptr) {
    error = "maps/" + name + ".bsp is not a map of the sequel's family; next: name one from " +
            c.root + "/" + base;
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
  return sequel_net_start(*game.net, game.rules, port, game.cfg, error);
}

void net_frame(void* g, int64_t now_us) { sequel_net_frame(*static_cast<Game*>(g)->net, now_us); }

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
