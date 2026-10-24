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

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/stats.hpp"
#include "net/replicate_trial.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;

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

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

void run(gl::Lab& lab) {
  gn::ReplicateTrialConfig a;
  a.clients = static_cast<int>(lab.arg_num("clients"));
  a.npcs = static_cast<int>(lab.arg_num("npcs"));
  a.seconds = lab.arg_num("seconds");
  a.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  a.replicate.interest_radius = lab.arg_num("radius");
  a.replicate.snapshot_budget = static_cast<size_t>(lab.arg_num("budget"));
  a.replicate.update_rate = static_cast<int>(lab.arg_num("update-rate"));
  a.replicate.cmd_rate = static_cast<int>(lab.arg_num("cmd-rate"));
  a.replicate.rate = static_cast<uint32_t>(lab.arg_num("rate"));
  if (a.clients < 1 || a.clients > gn::kMaxPeers || a.npcs < 0 || a.npcs > 512 || a.seconds < 2) {
    lab.fail("--clients, --npcs or --seconds is out of range",
             "Pass --clients=1 to 32, --npcs=0 to 512 and --seconds=2 or more; skill: net.");
    return;
  }
  gl::LabTable table{
      std::to_string(a.clients) + " clients and " + std::to_string(a.npcs) +
          " movers the game moves, each preset",
      {"preset", "owned", "down kbit/s", "max", "up kbit/s", "view ms", "corr/min", "worst m",
       "dropped", "full", "starved", "repeated", "rewind p50 m", "p95", "missed", "log"},
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
    a.link = link;
    const gn::ReplicateTrial o = gn::run_replicate_trial(a);
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
    table.rows.push_back(
        {name, std::to_string(o.owned) + "/" + std::to_string(a.clients), fmt(o.down_kbps, 1),
         fmt(o.down_max_kbps, 1), fmt(o.wire_up_kbps, 1), fmt(o.view_delay_ms, 0),
         fmt(o.corrections_per_min, 1), fmt(o.worst_correction, 3), fmt(drop_share * 100, 1) + "%",
         std::to_string(o.full), std::to_string(o.starved), fmt(repeat_share * 100, 1) + "%",
         fmt(g.p50, 4), fmt(g.p95, 4), std::to_string(o.missed), hash});
    const std::string k = name + "-";
    lab.metric(k + "down-kbps", o.down_kbps, "kbit/s");
    lab.metric(k + "up-kbps", o.wire_up_kbps, "kbit/s");
    lab.metric(k + "view-delay-ms", o.view_delay_ms, "ms");
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
      "as kbit/s over the run (mean, then the worst client). up: every packet a client sent, "
      "headers "
      "and keepalives too. view: how far behind the server's clock the others are drawn. corr/min: "
      "snapshots that moved a "
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
        {"update-rate", "0", "snapshots a second each client asks for (0: every other tick)"},
        {"cmd-rate", "0", "input messages a second each client sends (0: one a tick)"},
        {"rate", "0", "snapshot bytes a second each client takes (0: no cap)"},
        {"seed", "1", "the network's, the hands' and the crowd's stream"},
    },
    "--clients=8 --npcs=16 --seconds=4 --presets=lan,bad",
    run,
};
GE_LAB(kSpec);

}  // namespace
