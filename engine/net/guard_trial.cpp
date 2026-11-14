// The guard trial (net/guard_trial.hpp).
//
// ge:summary The guard trial run: honest players and cheaters, the guard judging, travel measured
// ge:keywords net guard trial cheats honest travel score
// ge:skill net
// ge:test tests/net_guard_test.cpp
#include "net/guard_trial.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

#include "core/prng.hpp"
#include "net/node.hpp"
#include "net/replicate.hpp"
#include "net/replicate_wire.hpp"
#include "net/sim_network.hpp"
#include "net/transport.hpp"
#include "net/wire.hpp"

namespace ge::net {

namespace {

enum : uint8_t { kX, kZ, kHeading, kFields };
constexpr int kInputBytes = 3;
constexpr double kFullStickMps = 8;  // a mover at full stick (100), metres a second
constexpr double kTeleportM = 40;

void step_mover(void* ctx, double* v, const uint8_t* input) {
  const double scale = *static_cast<const double*>(ctx);
  v[kX] += static_cast<int8_t>(input[0]) * scale;
  v[kZ] += static_cast<int8_t>(input[1]) * scale;
  v[kHeading] += static_cast<int8_t>(input[2]) * scale / 8;
}

NodeConfig node_config(bool host, uint64_t seed) {
  NodeConfig c;
  c.protocol = protocol_id("net-guard", 1);
  c.accept = host;
  c.max_peers = host ? kMaxPeers : 1;
  c.channels.count = 2;
  c.channels.delivery[0] = Delivery::unreliable;
  c.channels.delivery[1] = Delivery::unreliable;
  c.timeout_us = 3000000;
  c.seed = seed;
  return c;
}

struct Player {
  TrialCheat cheat = TrialCheat::none;
  Transport t;
  Node n;
  ReplicateClient r;
  core::Rng hand{0};
  int peer = -1;
  uint8_t held[kInputBytes] = {};
  uint32_t raw_seq = 0;  // the future cheater's own input count (it sends its messages itself)
  Address address;
  int slot = -1;  // its peer slot on the server
  uint16_t entity = kNoEntity;
  double place[2] = {};  // the place it reports (a peer game's), x and z
  bool placed = false;
};

// The future cheater's input message: one input, its view three seconds past what it draws.
void send_future(Player& p, const ReplicateConfig& k, int hz) {
  namespace rw = replicate_wire;
  p.raw_seq++;
  uint8_t out[64];
  Writer w = make_writer(out, sizeof out);
  write_u8(w, rw::kInput);
  write_u32(w, p.r.has_snapshot ? p.r.newest : 0);
  write_u32(w, replicate_client_render_tick(p.r) + static_cast<uint32_t>(hz) * 3 * 256);
  write_u32(w, k.rate);
  write_u16(w, static_cast<uint16_t>(std::clamp(k.update_rate, 0, 65535)));
  write_u32(w, p.raw_seq);
  write_u8(w, 1);
  write_bytes(w, p.held, kInputBytes);
  write_u16(w, 0);
  if (!w.overflow) node_send(p.n, p.peer, k.input_channel, out, w.size);
}

}  // namespace

const char* trial_cheat_name(TrialCheat c) {
  switch (c) {
    case TrialCheat::none:
      return "honest";
    case TrialCheat::speed:
      return "speed";
    case TrialCheat::off_grid:
      return "off-grid";
    case TrialCheat::flood:
      return "flood";
    case TrialCheat::future:
      return "future";
    case TrialCheat::teleport:
      return "teleport";
  }
  return "?";
}

GuardTrial run_guard_trial(const GuardTrialConfig& k) {
  GuardTrial o;
  const int hz = std::max(1, k.tick_hz);
  const int64_t tick_us = 1000000 / hz;
  double scale = kFullStickMps / (100.0 * hz);
  o.full_stick_mps = kFullStickMps * std::sqrt(2.0);  // both axes at full
  Schema schema;
  schema.fields = kFields;
  schema.spec[kX] = {-512, 512, 22, false};
  schema.spec[kZ] = {-512, 512, 22, false};
  schema.spec[kHeading] = {0, 6.283185307179586, 12, true};
  schema.input_size = kInputBytes;
  schema.step = step_mover;
  schema.ctx = &scale;

  GuardRules rules;
  rules.fields = {
      {0, 1, true, -100, 100, 10}, {1, 1, true, -100, 100, 10}, {2, 1, true, -40, 40, 1}};
  rules.tick_hz = hz;
  // A peer game's step: full stick on both axes, with half again for a link's bunching.
  rules.max_step = o.full_stick_mps * 1.5 / hz;
  Guard guard;
  guard_init(guard, rules);

  SimNetwork net = make_sim_network(k.seed, k.link);
  Transport host_t;
  open_sim_transport(host_t, net, sim_address(0, 1));
  auto host_n = std::make_unique<Node>();
  node_init(*host_n, node_config(true, uint64_t{k.seed} * 7 + 1), host_t);
  ReplicateConfig config;
  config.tick_hz = hz;
  config.max_entities = k.honest + 8;
  auto server = std::make_unique<ReplicateServer>();
  replicate_server_init(*server, schema, config, *host_n);
  server->guard = &guard;

  const TrialCheat cheats[] = {TrialCheat::speed, TrialCheat::off_grid, TrialCheat::flood,
                               TrialCheat::future, TrialCheat::teleport};
  std::vector<std::unique_ptr<Player>> ps;
  const int count = std::max(0, k.honest) + static_cast<int>(std::size(cheats));
  for (int i = 0; i < count; i++) {
    auto p = std::make_unique<Player>();
    p->cheat = i < k.honest ? TrialCheat::none : cheats[i - k.honest];
    const auto id = static_cast<uint32_t>(i + 1);
    p->address = sim_address(id, 1);
    open_sim_transport(p->t, net, p->address);
    node_init(p->n, node_config(false, uint64_t{k.seed} * 1000 + id), p->t);
    p->peer = node_connect(p->n, sim_address(0, 1), 0);
    replicate_client_init(p->r, schema, config, p->n, p->peer);
    p->hand = core::make_rng(k.seed * 977 + id);
    ps.push_back(std::move(p));
  }
  o.players.resize(ps.size());
  for (size_t i = 0; i < ps.size(); i++) o.players[i].cheat = ps[i]->cheat;

  std::vector<double> last_x(ps.size()), last_z(ps.size()), second_x(ps.size()),
      second_z(ps.size());
  std::vector<bool> seen(ps.size(), false);
  bool first_of[static_cast<size_t>(Cheat::count)] = {};
  size_t flags_read = 0;
  const auto ticks = static_cast<int>(k.seconds * hz);
  int64_t now = 0;
  for (int t = 1; t <= ticks; t++) {
    now += tick_us;
    sim_advance(net, now);
    node_update(*host_n, now);
    for (const Event& e : host_n->events) {
      if (e.type != EventType::connected) continue;
      for (auto& p : ps) {
        if (p->address != host_n->peers[static_cast<size_t>(e.peer)].address) continue;
        p->slot = e.peer;
        const int i = static_cast<int>(&p - &ps.front());
        const double v[kFields] = {(i % 6) * 30.0 - 75, (i / 6) * 30.0 - 30, 0};
        p->entity = replicate_spawn(*server, e.peer, v);
      }
    }
    replicate_server_receive(*server);
    replicate_server_tick(*server);
    node_flush(*host_n, now);

    // A peer game's view: each player's reported place against the last, a step at most.
    for (size_t i = 0; i < ps.size(); i++) {
      Player& p = *ps[i];
      if (p.entity == kNoEntity) continue;
      const double* v = replicate_get(*server, p.entity);
      if (v == nullptr) continue;
      double place[2] = {v[kX], v[kZ]};
      if (p.cheat == TrialCheat::teleport) {
        const double jumps = static_cast<double>(t / hz);
        place[0] += jumps * kTeleportM;
      }
      if (p.placed) {
        const double before[kFields] = {p.place[0], p.place[1], 0};
        const double after[kFields] = {place[0], place[1], 0};
        guard_motion(guard, p.slot, before, after, server->tick);
      }
      p.place[0] = place[0];
      p.place[1] = place[1];
      p.placed = true;
      // How far the server moved it, and its fastest second.
      if (seen[i]) {
        const double dx = v[kX] - last_x[i], dz = v[kZ] - last_z[i];
        o.players[i].travelled += std::sqrt(dx * dx + dz * dz);
      } else {
        second_x[i] = v[kX];
        second_z[i] = v[kZ];
      }
      seen[i] = true;
      last_x[i] = v[kX];
      last_z[i] = v[kZ];
      if (t % hz == 0) {
        const double dx = v[kX] - second_x[i], dz = v[kZ] - second_z[i];
        o.players[i].top_speed = std::max(o.players[i].top_speed, std::sqrt(dx * dx + dz * dz));
        second_x[i] = v[kX];
        second_z[i] = v[kZ];
      }
    }

    for (auto& pp : ps) {
      Player& p = *pp;
      node_update(p.n, now);
      replicate_client_receive(p.r);
      if (t % std::max(1, hz / 2) == 0) {
        p.held[0] = static_cast<uint8_t>(static_cast<int8_t>(core::int_in(p.hand, -8, 8) * 10));
        p.held[1] = static_cast<uint8_t>(static_cast<int8_t>(core::int_in(p.hand, -8, 8) * 10));
        p.held[2] = static_cast<uint8_t>(static_cast<int8_t>(core::int_in(p.hand, -40, 40)));
        if (p.cheat == TrialCheat::speed) {
          p.held[0] = static_cast<uint8_t>(int8_t{127});
          p.held[1] = static_cast<uint8_t>(int8_t{127});
        } else if (p.cheat == TrialCheat::off_grid) {
          p.held[0] = static_cast<uint8_t>(int8_t{37});
        }
      }
      if (p.cheat == TrialCheat::future) {
        if (p.r.has_snapshot) send_future(p, config, hz);
      } else {
        const int times = p.cheat == TrialCheat::flood ? 3 : 1;
        for (int j = 0; j < times; j++) replicate_client_input(p.r, p.held);
      }
      node_flush(p.n, now);
    }

    for (; flags_read < guard.flags.size() && guard.raised <= kMaxFlags; flags_read++) {
      const GuardFlag& f = guard.flags[flags_read];
      const auto c = static_cast<size_t>(f.cheat);
      if (first_of[c]) continue;
      first_of[c] = true;
      o.first_flags += std::string(cheat_name(f.cheat)) + " at tick " + std::to_string(f.tick) +
                       " (sent " + std::to_string(f.value) + ")\n";
    }
    for (size_t i = 0; i < ps.size(); i++) {
      if (ps[i]->slot < 0) continue;
      const GuardClient& g = guard.clients[static_cast<size_t>(ps[i]->slot)];
      if (g.out && o.players[i].out_s < 0) o.players[i].out_s = static_cast<double>(t) / hz;
    }
    if (t % std::max(1, hz / 10) == 0) {
      o.t_s.push_back(static_cast<double>(t) / hz);
      for (size_t i = 0; i < ps.size(); i++) {
        const int slot = ps[i]->slot;
        o.players[i].score.push_back(slot < 0 ? 0.0
                                              : guard.clients[static_cast<size_t>(slot)].score);
      }
    }
  }
  for (size_t i = 0; i < ps.size(); i++) {
    const int slot = ps[i]->slot;
    if (slot < 0) continue;
    const GuardClient& g = guard.clients[static_cast<size_t>(slot)];
    o.players[i].counts = g.counts;
    o.players[i].worst = g.worst;
    o.players[i].out = g.out;
  }
  o.flags = guard.raised;
  return o;
}

}  // namespace ge::net
