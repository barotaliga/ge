// The replication trial's world and its measurements (net/replicate_trial.hpp).
//
// ge:summary The replication trial's world, its clients and crowd, and what it measures
// ge:keywords net replicate trial world crowd movers measure bandwidth rewind
// ge:skill net
// ge:test tests/net_replicate_test.cpp
#include "net/replicate_trial.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

#include "core/prng.hpp"
#include "net/node.hpp"
#include "net/sim_network.hpp"
#include "net/transport.hpp"

namespace ge::net {

namespace {

constexpr double kHalf = 200;  // the field is [-kHalf, kHalf] on x and z
enum : uint8_t { kX, kZ, kHeading, kHealth, kFields };

// Up to 19 m/s on each axis, a skier's pace, whatever the tick: ctx is the metres a unit a tick.
void step_mover(void* ctx, double* v, const uint8_t* input) {
  const double scale = *static_cast<const double*>(ctx);
  v[kX] += static_cast<int8_t>(input[0]) * scale;
  v[kZ] += static_cast<int8_t>(input[1]) * scale;
  v[kHeading] += static_cast<int8_t>(input[2]) * scale / 2;
}

uint64_t fnv(uint64_t h, uint64_t v) {
  for (int i = 0; i < 8; i++) h = (h ^ ((v >> (8 * i)) & 0xff)) * 0x100000001b3ull;
  return h;
}
uint64_t fnv_double(uint64_t h, double d) {
  uint64_t bits;
  std::memcpy(&bits, &d, sizeof bits);
  return fnv(h, bits);
}

NodeConfig node_config(bool host, uint64_t seed) {
  NodeConfig c;
  c.protocol = protocol_id("net-replicate", 1);
  c.accept = host;
  c.max_peers = host ? kMaxPeers : 1;
  c.channels.count = 2;
  c.channels.delivery[0] = Delivery::unreliable;
  c.channels.delivery[1] = Delivery::unreliable;
  c.timeout_us = 3000000;
  c.seed = seed;
  return c;
}

struct Client {
  Transport t;
  Node n;
  ReplicateClient r;
  core::Rng hand{0};
  int peer = -1;
  uint8_t held[3] = {};
  uint64_t corrections_seen = 0;
  double rate = 1;  // its clock against the network's
  double acc = 0;   // ticks of its own owed
  int slot = -1;    // its slot on the server, once its entity is known
  uint32_t applied = 0;
  int64_t made[256] = {};  // when each input was made, by its number
};

}  // namespace

ReplicateTrial run_replicate_trial(const ReplicateTrialConfig& k) {
  ReplicateTrial o;
  const int hz = std::max(1, k.replicate.tick_hz);
  const int64_t tick_us = 1000000 / hz;
  double scale = 0.004 * 60.0 / hz;
  Schema schema;
  schema.fields = kFields;
  schema.spec[kX] = {-256, 256, 20, false};
  schema.spec[kZ] = {-256, 256, 20, false};
  schema.spec[kHeading] = {0, 6.283185307179586, 12, true};
  schema.spec[kHealth] = {0, 100, 8, false};
  schema.input_size = 3;
  schema.step = step_mover;
  schema.ctx = &scale;

  SimNetwork net = make_sim_network(k.seed, k.link);
  Transport host_t;
  open_sim_transport(host_t, net, sim_address(0, 1));
  auto host_n = std::make_unique<Node>();
  node_init(*host_n, node_config(true, uint64_t{k.seed} * 7 + 1), host_t);
  ReplicateConfig config = k.replicate;
  config.max_entities = k.clients + k.npcs;
  config.ring = std::max(config.ring, hz);  // a second of history to rewind into
  config.interest_x = kX;
  config.interest_z = kZ;
  auto server = std::make_unique<ReplicateServer>();
  replicate_server_init(*server, schema, config, *host_n);

  // The crowd the server moves: straight lines at walking to running pace, bouncing off the edge.
  core::Rng world = core::make_rng(k.seed * 31 + 3);
  std::vector<uint16_t> npcs;
  std::vector<double> vel;
  for (int i = 0; i < k.npcs; i++) {
    double v[kFields] = {core::range(world, -kHalf, kHalf), core::range(world, -kHalf, kHalf),
                         core::range(world, 0, 6.28), 100};
    npcs.push_back(replicate_spawn(*server, -1, v));
    vel.push_back(core::range(world, -6, 6) / hz);
    vel.push_back(core::range(world, -6, 6) / hz);
  }
  std::vector<std::unique_ptr<Client>> cs;
  for (int i = 0; i < k.clients; i++) {
    auto c = std::make_unique<Client>();
    const auto id = static_cast<uint32_t>(i + 1);
    open_sim_transport(c->t, net, sim_address(id, 1));
    node_init(c->n, node_config(false, uint64_t{k.seed} * 1000 + id), c->t);
    c->peer = node_connect(c->n, sim_address(0, 1), 0);
    replicate_client_init(c->r, schema, config, c->n, c->peer);
    c->hand = core::make_rng(k.seed * 977 + id);
    c->rate = 1 + (i % 2 == 0 ? k.skew : -k.skew);
    cs.push_back(std::move(c));
  }

  int64_t now = 0;
  const auto ticks = static_cast<int>(k.seconds * hz);
  uint64_t bytes_mark = 0;
  double delay_sum = 0;
  uint64_t delay_n = 0, drawn_n = 0, held_n = 0;
  for (int t = 1; t <= ticks; t++) {
    now += tick_us;
    sim_advance(net, now);
    node_update(*host_n, now);
    for (const Event& e : host_n->events) {
      if (e.type != EventType::connected) continue;
      const int col = e.peer % 8, row = e.peer / 8;  // eight abreast, 20 m apart
      const double v[kFields] = {col * 20.0 - 70, row * 20.0 - 30, 0, 100};
      replicate_spawn(*server, e.peer, v);
    }
    replicate_server_receive(*server);
    for (size_t i = 0; i < npcs.size(); i++) {
      double v[kFields];
      std::memcpy(v, replicate_get(*server, npcs[i]), sizeof v);
      for (int axis = 0; axis < 2; axis++) {
        double& p = v[axis == 0 ? kX : kZ];
        double& d = vel[i * 2 + static_cast<size_t>(axis)];
        p += d;
        if (p > kHalf || p < -kHalf) d = -d;
      }
      if (t % (3 * hz) == static_cast<int>(i % static_cast<size_t>(3 * hz)))
        v[kHealth] = core::range(world, 0, 100);
      replicate_set(*server, npcs[i], v);
    }
    replicate_server_tick(*server);
    node_flush(*host_n, now);
    for (auto& c : cs) {
      node_update(c->n, now);
      replicate_client_receive(c->r);
      if (c->r.stats.corrections != c->corrections_seen) {
        c->corrections_seen = c->r.stats.corrections;
        o.worst_correction = std::max(o.worst_correction, c->r.last_error);
      }
      if (t % std::max(1, hz / 2) == 0) {
        c->held[0] = static_cast<uint8_t>(core::int_in(c->hand, -80, 80));
        c->held[1] = static_cast<uint8_t>(core::int_in(c->hand, -80, 80));
        c->held[2] = static_cast<uint8_t>(core::int_in(c->hand, -40, 40));
      }
      // The client's own clock: its rate against the network's, sped or slowed by time sync.
      c->acc += c->rate * replicate_client_dilation(c->r);
      while (c->acc >= 1) {
        c->acc -= 1;
        replicate_client_input(c->r, c->held);
        c->made[c->r.seq % 256] = now;
      }
      if (c->slot < 0 && c->r.own != kNoEntity) {
        for (int p = 0; p < kMaxPeers; p++) {
          if (server->clients[static_cast<size_t>(p)].entity == c->r.own) c->slot = p;
        }
      }
      if (c->slot >= 0) {
        const ReplicateClientSlot& sl = server->clients[static_cast<size_t>(c->slot)];
        for (uint32_t q = c->applied + 1; q <= sl.applied && c->applied; q++) {
          if (sl.applied - q < 256 && c->made[q % 256] > 0)
            o.input_ms.push_back(static_cast<double>(now - c->made[q % 256]) / 1000.0);
        }
        c->applied = sl.applied;
        if (&c == &cs.front() && t % std::max(1, hz / 10) == 0) {
          o.t_q.push_back(static_cast<double>(t) / hz);
          o.queue.push_back(static_cast<double>(sl.queue_seq.size()));
          const uint32_t at = replicate_client_render_tick(c->r);
          o.delay_ms.push_back(
              at < 256 ? 0 : static_cast<double>((server->tick << 8) - at) / 256.0 * 1000.0 / hz);
          o.dilation.push_back(replicate_client_dilation(c->r));
        }
      }
      node_flush(c->n, now);
      // Lag compensation: the server judges the crowd at the tick this client drew it at.
      const uint32_t at = replicate_client_render_tick(c->r);
      if (at < 256) continue;  // still drawing the first snapshot: nothing in the past to judge
      drawn_n++;
      held_n += at > (c->r.newest << 8);
      delay_sum += static_cast<double>((server->tick << 8) - at) / 256.0 * 1000.0 / hz;
      delay_n++;
      for (size_t i = static_cast<size_t>(t) % 8; i < npcs.size(); i += 8) {
        double drawn[kMaxFields], judged[kMaxFields];
        if (!replicate_client_sample(c->r, npcs[i], drawn)) continue;
        o.rewinds++;
        if (!replicate_rewind(*server, npcs[i], at, judged)) {
          o.missed++;
          continue;
        }
        const double dx = drawn[kX] - judged[kX], dz = drawn[kZ] - judged[kZ];
        o.gaps.push_back(std::sqrt(dx * dx + dz * dz));
      }
    }
    if (t % hz == 0) {
      const uint64_t bytes = server->stats.bytes;
      o.t_s.push_back(static_cast<double>(t) / hz);
      o.kbps.push_back(static_cast<double>(bytes - bytes_mark) * 8 / 1000 / k.clients);
      bytes_mark = bytes;
    }
  }
  const double seconds = static_cast<double>(ticks) / hz;
  uint64_t corrections = 0;
  uint64_t h = 0xcbf29ce484222325ull;
  for (auto& c : cs) {
    if (c->r.own != kNoEntity) o.owned++;
    const double kbps = static_cast<double>(c->r.stats.bytes) * 8 / 1000 / seconds;
    o.down_kbps += kbps / k.clients;
    o.down_max_kbps = std::max(o.down_max_kbps, kbps);
    if (c->peer >= 0) {
      const LinkStats& ls = c->n.peers[static_cast<size_t>(c->peer)].conn.stats;
      o.wire_up_kbps += static_cast<double>(ls.bytes_sent) * 8 / 1000 / seconds / k.clients;
      o.wire_down_kbps += static_cast<double>(ls.bytes_received) * 8 / 1000 / seconds / k.clients;
    }
    corrections += c->r.stats.corrections;
    o.dropped += c->r.stats.dropped;
    o.decoded += c->r.stats.snapshots;
    double v[kMaxFields];
    replicate_client_predicted(c->r, v);
    for (int f = 0; f < kFields; f++) h = fnv_double(h, v[f]);
    h = fnv(h, c->r.stats.corrections);
  }
  for (double v : server->state) h = fnv_double(h, v);
  h = fnv(h, server->stats.bytes);
  o.hash = h;
  o.view_delay_ms = delay_n ? delay_sum / static_cast<double>(delay_n) : 0;
  o.corrections_per_min = static_cast<double>(corrections) / k.clients / (seconds / 60);
  o.full = server->stats.full;
  o.starved = server->stats.starved;
  o.repeated = server->stats.repeated;
  o.inputs = server->stats.inputs;
  o.trimmed = server->stats.trimmed;
  o.underrun = drawn_n ? static_cast<double>(held_n) / static_cast<double>(drawn_n) : 0;
  return o;
}

}  // namespace ge::net
