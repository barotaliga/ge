// LAB `net-replicate`: one server-authoritative world over the simulated network, once under each
// condition preset: every client owns a mover it steers with a held direction that changes every
// half second, and the game moves a crowd of others in straight lines that bounce off the field's
// edge. Each client predicts its own, draws the rest interpolated, and the server rewinds each
// tick to what a client drew. It answers what replication costs and how well it keeps its word
// on each link: the bytes a client is sent a second, how often a prediction is corrected and by
// how much, the snapshots lost or left without a baseline, the rows a budget starved, and how far
// a rewind lands from what the client drew. The bandwidth is charted over the run and the rewind
// gaps drawn as boxes. Everything runs on a virtual clock off one seed, so the lab is a
// determinism test too.
//
// ge:summary Lab: a 32-client replicated world over each preset: bandwidth, corrections, rewind
// ge:keywords lab net replicate replication snapshot delta prediction correction interpolation
// ge:keywords rewind lag-compensation bandwidth interest budget preset multiplayer
// ge:skill net labs
// ge:test tests/net_replicate_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "core/prng.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/stats.hpp"
#include "net/replicate.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace gc = ge::core;

constexpr int64_t kTick = 16667;  // 60 Hz
constexpr int kTicksPerSecond = 60;
constexpr double kHalf = 200;  // the field is [-kHalf, kHalf] on x and z
enum : uint8_t { kX, kZ, kHeading, kHealth, kFields };

// Up to 0.32 m a tick (19 m/s) on each axis: a skier's pace.
void step_mover(void*, double* v, const uint8_t* input) {
  v[kX] += static_cast<int8_t>(input[0]) * 0.004;
  v[kZ] += static_cast<int8_t>(input[1]) * 0.004;
  v[kHeading] += static_cast<int8_t>(input[2]) * 0.002;
}

gn::Schema schema() {
  gn::Schema s;
  s.fields = kFields;
  s.spec[kX] = {-256, 256, 20, false};
  s.spec[kZ] = {-256, 256, 20, false};
  s.spec[kHeading] = {0, 6.283185307179586, 12, true};
  s.spec[kHealth] = {0, 100, 8, false};
  s.input_size = 3;
  s.step = step_mover;
  return s;
}

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at <= s.size()) {
    const size_t comma = s.find(',', at);
    const std::string part =
        s.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
    if (!part.empty()) out.push_back(part);
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  return out;
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

gn::NodeConfig node_config(bool host, uint64_t seed) {
  gn::NodeConfig c;
  c.protocol = gn::protocol_id("net-replicate", 1);
  c.accept = host;
  c.max_peers = host ? gn::kMaxPeers : 1;
  c.channels.count = 2;
  c.channels.delivery[0] = gn::Delivery::unreliable;
  c.channels.delivery[1] = gn::Delivery::unreliable;
  c.timeout_us = 3000000;
  c.seed = seed;
  return c;
}

struct Client {
  gn::Transport t;
  gn::Node n;
  gn::ReplicateClient r;
  gc::Rng hand{0};
  uint8_t held[3] = {};
  uint64_t corrections_seen = 0;
};

struct Args {
  int clients = 32, npcs = 64, seconds = 20;
  double radius = 0;
  size_t budget = 0;
  uint32_t seed = 1;
};

struct Outcome {
  int owned = 0;
  double down_mean_kbps = 0, down_max_kbps = 0;  // per client, over the run
  double corrections_per_min = 0, worst_correction = 0;
  uint64_t dropped = 0, decoded = 0, full = 0, starved = 0, repeated = 0, inputs = 0;
  uint64_t rewinds = 0, missed = 0;
  std::vector<double> gaps;  // a rewind against what was drawn, metres
  std::vector<double> t_s, kbps;
  uint64_t hash = 0;
};

Outcome play(const gn::LinkConditions& link, const Args& a) {
  Outcome o;
  gn::SimNetwork net = gn::make_sim_network(a.seed, link);
  gn::Transport host_t;
  gn::open_sim_transport(host_t, net, gn::sim_address(0, 1));
  auto host_n = std::make_unique<gn::Node>();
  gn::node_init(*host_n, node_config(true, uint64_t{a.seed} * 7 + 1), host_t);
  gn::ReplicateConfig config;
  config.max_entities = a.clients + a.npcs;
  config.ring = 64;
  config.interest_x = kX;
  config.interest_z = kZ;
  config.interest_radius = a.radius;
  config.snapshot_budget = a.budget;
  auto server = std::make_unique<gn::ReplicateServer>();
  gn::replicate_server_init(*server, schema(), config, *host_n);

  // The crowd the game moves: straight lines at walking to running pace, bouncing off the edge.
  gc::Rng world = gc::make_rng(a.seed * 31 + 3);
  std::vector<uint16_t> npcs;
  std::vector<double> vel;
  for (int i = 0; i < a.npcs; i++) {
    double v[kFields] = {gc::range(world, -kHalf, kHalf), gc::range(world, -kHalf, kHalf),
                         gc::range(world, 0, 6.28), 100};
    npcs.push_back(gn::replicate_spawn(*server, -1, v));
    vel.push_back(gc::range(world, -0.1, 0.1));
    vel.push_back(gc::range(world, -0.1, 0.1));
  }
  std::vector<std::unique_ptr<Client>> cs;
  for (int i = 0; i < a.clients; i++) {
    auto c = std::make_unique<Client>();
    const auto id = static_cast<uint32_t>(i + 1);
    gn::open_sim_transport(c->t, net, gn::sim_address(id, 1));
    gn::node_init(c->n, node_config(false, uint64_t{a.seed} * 1000 + id), c->t);
    const int peer = gn::node_connect(c->n, gn::sim_address(0, 1), 0);
    gn::replicate_client_init(c->r, schema(), config, c->n, peer);
    c->hand = gc::make_rng(a.seed * 977 + id);
    cs.push_back(std::move(c));
  }

  int64_t now = 0;
  const int ticks = a.seconds * kTicksPerSecond;
  uint64_t bytes_mark = 0;
  for (int t = 1; t <= ticks; t++) {
    now += kTick;
    gn::sim_advance(net, now);
    gn::node_update(*host_n, now);
    for (const gn::Event& e : host_n->events) {
      if (e.type != gn::EventType::connected) continue;
      const double v[kFields] = {(e.peer % 8) * 20.0 - 70, (e.peer / 8) * 20.0 - 30, 0, 100};
      gn::replicate_spawn(*server, e.peer, v);
    }
    gn::replicate_server_receive(*server);
    for (size_t i = 0; i < npcs.size(); i++) {
      double v[kFields];
      std::memcpy(v, gn::replicate_get(*server, npcs[i]), sizeof v);
      for (int axis = 0; axis < 2; axis++) {
        double& p = v[axis == 0 ? kX : kZ];
        double& d = vel[i * 2 + static_cast<size_t>(axis)];
        p += d;
        if (p > kHalf || p < -kHalf) d = -d;
      }
      if (t % 180 == static_cast<int>(i % 180)) v[kHealth] = gc::range(world, 0, 100);
      gn::replicate_set(*server, npcs[i], v);
    }
    gn::replicate_server_tick(*server);
    gn::node_flush(*host_n, now);
    for (auto& c : cs) {
      gn::node_update(c->n, now);
      gn::replicate_client_receive(c->r);
      if (c->r.stats.corrections != c->corrections_seen) {
        c->corrections_seen = c->r.stats.corrections;
        o.worst_correction = std::max(o.worst_correction, c->r.last_error);
      }
      if (t % 30 == 0) {
        c->held[0] = static_cast<uint8_t>(gc::int_in(c->hand, -80, 80));
        c->held[1] = static_cast<uint8_t>(gc::int_in(c->hand, -80, 80));
        c->held[2] = static_cast<uint8_t>(gc::int_in(c->hand, -40, 40));
      }
      gn::replicate_client_input(c->r, c->held);
      gn::node_flush(c->n, now);
      // Lag compensation: the server judges the crowd at the tick this client drew it at.
      const uint32_t at = gn::replicate_client_render_tick(c->r);
      if (at < 256) continue;  // still drawing the first snapshot: nothing in the past to judge
      for (size_t i = static_cast<size_t>(t) % 8; i < npcs.size(); i += 8) {
        double drawn[gn::kMaxFields], judged[gn::kMaxFields];
        if (!gn::replicate_client_sample(c->r, npcs[i], drawn)) continue;
        o.rewinds++;
        if (!gn::replicate_rewind(*server, npcs[i], at, judged)) {
          o.missed++;
          continue;
        }
        const double dx = drawn[kX] - judged[kX], dz = drawn[kZ] - judged[kZ];
        o.gaps.push_back(std::sqrt(dx * dx + dz * dz));
      }
    }
    if (t % kTicksPerSecond == 0) {
      const uint64_t bytes = server->stats.bytes;
      o.t_s.push_back(static_cast<double>(t) / kTicksPerSecond);
      o.kbps.push_back(static_cast<double>(bytes - bytes_mark) * 8 / 1000 / a.clients);
      bytes_mark = bytes;
    }
  }
  const double seconds = static_cast<double>(a.seconds);
  double sum = 0, worst = 0;
  uint64_t corrections = 0;
  uint64_t h = 0xcbf29ce484222325ull;
  for (auto& c : cs) {
    if (c->r.own != gn::kNoEntity) o.owned++;
    const double kbps = static_cast<double>(c->r.stats.bytes) * 8 / 1000 / seconds;
    sum += kbps;
    worst = std::max(worst, kbps);
    corrections += c->r.stats.corrections;
    o.dropped += c->r.stats.dropped;
    o.decoded += c->r.stats.snapshots;
    double v[gn::kMaxFields];
    gn::replicate_client_predicted(c->r, v);
    for (int f = 0; f < kFields; f++) h = fnv_double(h, v[f]);
    h = fnv(h, c->r.stats.corrections);
  }
  for (double v : server->state) h = fnv_double(h, v);
  h = fnv(h, server->stats.bytes);
  o.hash = h;
  o.down_mean_kbps = sum / a.clients;
  o.down_max_kbps = worst;
  o.corrections_per_min = static_cast<double>(corrections) / a.clients / (seconds / 60);
  o.full = server->stats.full;
  o.starved = server->stats.starved;
  o.repeated = server->stats.repeated;
  o.inputs = server->stats.inputs;
  return o;
}

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

void run(gl::Lab& lab) {
  Args a;
  a.clients = static_cast<int>(lab.arg_num("clients"));
  a.npcs = static_cast<int>(lab.arg_num("npcs"));
  a.seconds = static_cast<int>(lab.arg_num("seconds"));
  a.radius = lab.arg_num("radius");
  a.budget = static_cast<size_t>(lab.arg_num("budget"));
  a.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  if (a.clients < 1 || a.clients > gn::kMaxPeers || a.npcs < 0 || a.npcs > 512 || a.seconds < 2) {
    lab.fail("--clients, --npcs or --seconds is out of range",
             "Pass --clients=1 to 32, --npcs=0 to 512 and --seconds=2 or more; skill: net.");
    return;
  }
  gl::LabTable table{std::to_string(a.clients) + " clients and " + std::to_string(a.npcs) +
                         " movers the game moves, each preset",
                     {"preset", "owned", "down kbit/s", "max", "corr/min", "worst m", "dropped",
                      "full", "starved", "repeated", "rewind p50 m", "p95", "missed", "log"},
                     {}};
  gl::LineChart down{
      {"SNAPSHOT BANDWIDTH PER CLIENT, EACH PRESET", "the server's sends", "t (s)", "kbit/s"},
      {},
      {},
      {},
      {},
      false,
      false,
      false};
  std::vector<gl::Sample> gaps;
  for (const std::string& name : split(lab.arg("presets"))) {
    gn::LinkConditions link;
    if (!gn::conditions_preset(name, link)) {
      lab.fail("unknown preset '" + name + "'",
               "Pick from lan, broadband, wifi, mobile, bad, awful (engine/net/conditions.cpp); "
               "skill: net.");
      continue;
    }
    const Outcome o = play(link, a);
    const gl::Summary g = gl::summarize(o.gaps);
    char hash[20];
    std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(o.hash));
    const double drop_share = o.decoded + o.dropped ? static_cast<double>(o.dropped) /
                                                          static_cast<double>(o.decoded + o.dropped)
                                                    : 0;
    const double repeat_share =
        o.inputs + o.repeated
            ? static_cast<double>(o.repeated) / static_cast<double>(o.inputs + o.repeated)
            : 0;
    table.rows.push_back({name, std::to_string(o.owned) + "/" + std::to_string(a.clients),
                          fmt(o.down_mean_kbps, 1), fmt(o.down_max_kbps, 1),
                          fmt(o.corrections_per_min, 1), fmt(o.worst_correction, 3),
                          fmt(drop_share * 100, 1) + "%", std::to_string(o.full),
                          std::to_string(o.starved), fmt(repeat_share * 100, 1) + "%",
                          fmt(g.p50, 4), fmt(g.p95, 4), std::to_string(o.missed), hash});
    const std::string k = name + "-";
    lab.metric(k + "down-kbps", o.down_mean_kbps, "kbit/s");
    lab.metric(k + "corrections-per-min", o.corrections_per_min, "count");
    lab.metric(k + "worst-correction-m", o.worst_correction, "m");
    lab.metric(k + "dropped-share", drop_share, "ratio");
    lab.metric(k + "rewind-p50-m", g.p50, "m");
    down.traces.push_back({name, o.t_s, o.kbps});
    gaps.push_back({name, o.gaps});
    if (o.owned != a.clients)
      lab.fail(name + ": " + std::to_string(a.clients - o.owned) +
                   " clients never heard of their "
                   "own entity",
               "Read send_snapshot's own entity and on_snapshot in engine/net/replicate_*.cpp, "
               "then run tests/net_replicate_test.cpp; skill: net.");
    if (link.loss == 0 && link.jitter_ms < 1 && o.corrections_per_min > 0)
      lab.fail(name + ": a prediction was corrected on a link that loses nothing",
               "The server and the client must step the same quantized state: read normalize in "
               "replicate_server.cpp and reconcile in replicate_client.cpp; skill: net.");
    if (o.missed)
      lab.fail(name + ": " + std::to_string(o.missed) + " rewinds fell outside the history",
               "Raise ReplicateConfig::ring past the round trip plus interp_ticks, or read "
               "replicate_rewind in engine/net/replicate_server.cpp; skill: net.");
    if (g.n && g.p50 > 0.05)
      lab.fail(name + ": a rewind lands " + fmt(g.p50, 3) + " m from what was drawn (median)",
               "Compare replicate_client_sample's interpolation with replicate_rewind's; both "
               "must use the client's render tick; skill: net.");
  }
  lab.table(std::move(table));
  lab.note(
      "owned: clients that heard of their own entity. down: snapshot bytes each client was sent, "
      "as kbit/s over the run (mean, then the worst client). corr/min: snapshots that moved a "
      "prediction, per client a minute; worst: the largest move, metres. dropped: snapshots a "
      "client could not decode (stale, or their baseline gone). full: snapshots sent whole. "
      "starved: rows a budget left out. repeated: server ticks that reused a client's last input "
      "for want of a new one. rewind: how far the server's rewind of a mover at the tick a client "
      "drew it lands from where the client drew it. log: a hash of every end's final state.");
  const auto shoot = [&](const std::string& id, const gl::Chart& chart, const std::string& caption,
                         const std::string& look_for) {
    const std::string svg = gl::to_svg(chart.plot);
    lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
    lab.shot({id,
              chart.title,
              caption,
              look_for,
              {{"presets", lab.arg("presets")},
               {"clients", lab.arg("clients")},
               {"npcs", lab.arg("npcs")},
               {"seed", lab.arg("seed")}}},
             gl::to_picture(chart.plot));
  };
  if (!down.traces.empty()) {
    shoot("bandwidth", gl::line_chart(down),
          "The snapshot bytes the server sent each second, per client, one line a preset.",
          "a jump in the first second (whole snapshots), then a level line: deltas of what moved; "
          "a lossy preset sits a little higher, sending against older baselines.");
    gl::ChartFrame frame{"REWIND AGAINST WHAT WAS DRAWN, EACH PRESET", "every rewind of a mover",
                         "gap (m)", ""};
    shoot("rewind", gl::box_chart(frame, gaps),
          "How far the server's rewind lands from where the client drew the same mover, one box a "
          "preset: p25 to p75, the median, whiskers p5 to p95.",
          "every box at or near zero: a hit is judged against what the player saw.");
  }
}

const gl::LabSpec kSpec{
    "net-replicate",
    gl::LabKind::sim,
    "engine",
    "engine/net (replicate_server, replicate_client)",
    "net",
    "On each network condition, what does a replicated world of 32 players cost a client a "
    "second, how often is a prediction corrected, and does a rewind land where the client drew?",
    {
        {"presets", "lan,wifi,bad", "condition presets, comma-separated"},
        {"clients", "32", "clients, each steering its own mover (1 to 32)"},
        {"npcs", "64", "movers the game moves (0 to 512)"},
        {"seconds", "20", "how long the world runs"},
        {"radius", "0", "interest radius, metres (0: everyone sees everything)"},
        {"budget", "0", "bytes a snapshot may take (0: the largest message)"},
        {"seed", "1", "the network's, the hands' and the crowd's stream"},
    },
    "--clients=8 --npcs=16 --seconds=4 --presets=lan,bad",
    run,
};
GE_LAB(kSpec);

}  // namespace
