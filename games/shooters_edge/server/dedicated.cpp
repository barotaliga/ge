// Shooter's Edge's ServerGame, defined: the host and its stand-ins made from the server's settings,
// a frame, the status, the input log written and replayed.
//
// ge:summary Shooter's Edge's ServerGame definitions: create, frame, status, log, replay
// ge:keywords dedicated-server servergame stand-in input-log replay
// ge:skill shooter net
// ge:test tests/server_core_test.cpp
#include "shooters_edge/server/dedicated.hpp"

#include <cstdio>
#include <memory>
#include <vector>

#include "net/packet.hpp"
#include "shooters_edge/server/host.hpp"
#include "shooters_edge/server/stand_in.hpp"

namespace shooters_edge::server {

namespace {

namespace gs = ge::server;

struct Game {
  Host host;
  std::vector<StandIn> stand_ins;  // the sources point into it: sized once, never grown
};

void* create(const gs::ServerConfig& c, std::string& error) {
  auto g = std::make_unique<Game>();
  HostSettings s;
  s.maxclients = c.maxclients;
  s.max_edicts = 64 + 16 * c.maxclients < 1024 ? 64 + 16 * c.maxclients : 1024;
  s.seed = c.seed;
  s.map = c.map;
  s.record = !c.log.empty();
  if (!host_create(g->host, s, error)) return nullptr;
  g->stand_ins.resize(static_cast<size_t>(c.bots));
  for (int b = 0; b < c.bots; b++) {
    g->stand_ins[static_cast<size_t>(b)] = make_stand_in(c.seed, b);
    host_add_bot(g->host, stand_in_source(g->stand_ins[static_cast<size_t>(b)]));
  }
  return g.release();
}

void destroy(void* g) { delete static_cast<Game*>(g); }

double frame_seconds(const void* g) { return static_cast<const Game*>(g)->host.sv.set.mintic; }

void frame(void* g) { host_frame(static_cast<Game*>(g)->host, nullptr, 0); }

int seated(const void* g) {
  const Host& h = static_cast<const Game*>(g)->host;
  return host_count(h, SlotKind::bot) + host_count(h, SlotKind::remote);
}

uint64_t hash(const void* g) {
  return ge::shooter::server_hash(static_cast<const Game*>(g)->host.sv);
}

std::string status(const void* g) {
  const Host& h = static_cast<const Game*>(g)->host;
  char out[64];
  std::snprintf(out, sizeof out, "commands=%lld", static_cast<long long>(h.sv.counts.commands));
  return out;
}

bool save_log(const void* g, std::vector<uint8_t>& bytes, std::string& error) {
  const Host& h = static_cast<const Game*>(g)->host;
  if (!h.settings.record) {
    error = "the run kept no log; next: pass --log=<path> when it starts";
    return false;
  }
  bytes = write_input_log(h.log);
  return true;
}

bool replay(const std::vector<uint8_t>& bytes, gs::ServerReplay& out, std::string& error) {
  InputLog log;
  if (!read_input_log(bytes, log, error)) return false;
  std::vector<uint64_t> hashes;
  if (!replay_input_log(log, out.first_diverged, error, &hashes)) return false;
  out.frames = static_cast<int>(log.frames.size());
  out.digest = hashes.empty() ? 0 : hashes.back();
  return true;
}

gs::ServerGame make() {
  gs::ServerGame g;
  g.binary = "shooters_edge_server";
  g.title = "Shooter's Edge";
  g.protocol = ge::net::protocol_id("shooters_edge", 1);
  g.default_port = kServerPort;
  g.discovery_port = kDiscoveryPort;
  g.max_clients = 32;
  g.default_map = "arena";  // the test arena, roofed: the only map until compiled maps load (M18b)
  g.create = create;
  g.destroy = destroy;
  g.frame_seconds = frame_seconds;
  g.frame = frame;
  g.seated = seated;
  g.hash = hash;
  g.status = status;
  g.save_log = save_log;
  g.replay = replay;
  return g;
}

}  // namespace

const gs::ServerGame& dedicated_game() {
  static const gs::ServerGame g = make();
  return g;
}

}  // namespace shooters_edge::server
