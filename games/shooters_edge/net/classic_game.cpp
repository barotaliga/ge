// Shooter's Edge's playable dedicated server, defined: the mod's server made from the settings,
// its map loaded, the classic endpoint started on the port the core hands it.
//
// ge:summary Shooter's Edge's playable dedicated server's definitions: create, serve, the frames
// ge:keywords classic_game serve net_frame create status
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_game_test.cpp
#include "shooters_edge/net/classic_game.hpp"

#include <cstdio>
#include <memory>

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
  std::unique_ptr<cl::LoadedMap> map;      // the spec's models point into it: outlives `qc`
  std::unique_ptr<sv::QcServer> qc;        // reads `progs`
  std::unique_ptr<ClassicServer> classic;  // serves `qc` once the core hands it the port
  ServerConfig cfg;
  ~Game() {
    classic.reset();
    qc.reset();  // its views into the map go before the map does
    map.reset();
  }
};

void* create(const gs::ServerConfig& c, std::string& error) {
  if (c.bots != 0) {
    error = "sv_bots " + std::to_string(c.bots) +
            ": bots on the classic server come with the engine's lobby (M53c); next: --bots=0";
    return nullptr;
  }
  auto g = std::make_unique<Game>();
  if (!cl::baseline_progs(g->progs, error)) return nullptr;
  if (c.map == "arena") {
    g->map = cl::load_named_map(cl::SearchPath{}, c.map, error);
  } else {
    cl::SearchPath files;
    if (!cl::open_game_dir(c.root, kGameFolder, files, error)) return nullptr;
    g->map = cl::load_named_map(files, c.map, error);
  }
  if (!g->map) return nullptr;
  sv::QcServerOptions so;
  so.seed = c.seed;
  so.loop.maxclients = c.maxclients;
  so.loop.max_edicts = 1024;
  g->qc = sv::make_qc_server(g->progs, so, error);
  if (!g->qc) return nullptr;
  sv::server_command(*g->qc, "deathmatch 1");
  if (!sv::load_map(*g->qc, g->map->spec, error)) {
    error = "the map " + c.map + " did not load: " + error;
    return nullptr;
  }
  if (g->map->from_file) sv::use_map_sets(*g->qc, g->map->bsp);
  g->cfg.hostname = c.hostname;
  g->cfg.password = c.password;
  g->cfg.spectator_password = c.spectator_password;
  g->cfg.seed = c.seed;
  return g.release();
}

void destroy(void* g) { delete static_cast<Game*>(g); }

double frame_seconds(const void* g) { return static_cast<const Game*>(g)->qc->sv.set.mintic; }

// Offline: the mod's frame with nobody's moves, its streams emptied as the endpoint would.
void frame(void* g) {
  sv::QcServer& qc = *static_cast<Game*>(g)->qc;
  sv::qc_frame(qc, nullptr, 0);
  qc.out.all.clear();
  for (auto& one : qc.out.one) one.clear();
  sv::clear_frame(qc.out);
}

bool serve(void* g, ge::net::Transport& port, std::string& error) {
  Game& game = *static_cast<Game*>(g);
  game.classic = std::make_unique<ClassicServer>();
  return classic_start(*game.classic, *game.qc, port, game.cfg, error);
}

void net_frame(void* g, int64_t now_us) { classic_frame(*static_cast<Game*>(g)->classic, now_us); }

int seated(const void* g) {
  const Game& game = *static_cast<const Game*>(g);
  return game.classic ? classic_count(*game.classic, ConnState::spawned) : 0;
}

uint64_t hash(const void* g) {
  return ge::shooter::server_hash(static_cast<const Game*>(g)->qc->sv);
}

std::string status(const void* g) {
  const Game& game = *static_cast<const Game*>(g);
  if (!game.classic) return "offline";
  const ServerCounts& n = game.classic->counts;
  char out[128];
  std::snprintf(out, sizeof out, "connected=%d connects=%lld drops=%lld out=%lld B",
                classic_count(*game.classic, ConnState::connected),
                static_cast<long long>(n.connects), static_cast<long long>(n.drops),
                static_cast<long long>(n.bytes_out));
  return out;
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
