// LAB `net-predict`: how fresh and how steady a predicting client is, under each condition
// preset, with TIME SYNC off (a fixed interpolation delay, the client's clock its own) and on (the
// server's slack steering the client's clock, the interpolation delay following the jitter), and
// with the clients' clocks skewed. It answers what a player feels: how long an input takes from
// the hand to the server's step, how often the server ran short and repeated one, how far behind
// the others are drawn and how often the drawing ran past the newest snapshot, how often and how
// far a prediction is corrected. It charts one client's input queue on the server over the run,
// sync off against on, and the view delay each preset settles to. Everything runs on a virtual
// clock off one seed.
//
// ge:summary Lab: prediction's freshness: input latency, queue, view delay, sync off against on
// ge:keywords lab net predict prediction time-sync dilation clock-skew input-latency queue
// ge:keywords interpolation-delay jitter view-delay corrections multiplayer
// ge:skill net labs
// ge:test tests/net_replicate_test.cpp
#include <cstdio>
#include <string>
#include <utility>
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

std::string hex(uint64_t h) {
  char b[20];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(h));
  return b;
}

void run(gl::Lab& lab) {
  gn::ReplicateTrialConfig base;
  base.clients = static_cast<int>(lab.arg_num("clients"));
  base.npcs = static_cast<int>(lab.arg_num("npcs"));
  base.seconds = lab.arg_num("seconds");
  base.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  const double skew = lab.arg_num("skew");
  if (base.clients < 1 || base.clients > 32 || base.npcs < 0 || base.seconds < 2 || skew < 0 ||
      skew > 0.2) {
    lab.fail("--clients, --npcs, --seconds or --skew is out of range",
             "Pass --clients=1 to 32, --npcs=0 or more, --seconds=2 or more and --skew=0 to "
             "0.2; skill: net.");
    return;
  }
  gl::LabTable table{std::to_string(base.clients) + " clients predicting their own mover among " +
                         std::to_string(base.npcs) + " others, time sync off and on",
                     {"preset", "clocks", "sync", "input ms p50", "p95", "repeated", "trimmed",
                      "view ms", "held", "corr/min", "worst m", "log"},
                     {}};
  gl::LineChart queue{
      {"ONE CLIENT'S INPUTS WAITING ON THE SERVER",
       "preset " + lab.arg("chart-preset") + ", clocks skewed " + fmt(skew * 100, 1) + "%", "t (s)",
       "inputs queued"},
      {},
      {},
      {},
      {},
      false,
      false,
      false};
  gl::LineChart delay{
      {"THE VIEW DELAY WITH TIME SYNC, EACH PRESET",
       "how far behind the server one client draws the others", "t (s)", "view delay (ms)"},
      {},
      {},
      {},
      {},
      false,
      false,
      false};
  std::vector<gl::Sample> inputs;
  for (const std::string& name : split(lab.arg("presets"))) {
    gn::LinkConditions link;
    if (!gn::conditions_preset(name, link)) {
      lab.fail("unknown preset '" + name + "'",
               "Pick from lan, broadband, wifi, mobile, bad, awful (engine/net/conditions.cpp); "
               "skill: net.");
      continue;
    }
    for (const double sk : {0.0, skew}) {
      for (const bool sync : {false, true}) {
        gn::ReplicateTrialConfig k = base;
        k.link = link;
        k.skew = sk;
        k.replicate.sync = sync;
        const gn::ReplicateTrial o = gn::run_replicate_trial(k);
        const gl::Summary in = gl::summarize(o.input_ms);
        const double repeated =
            o.inputs + o.repeated
                ? static_cast<double>(o.repeated) / static_cast<double>(o.inputs + o.repeated)
                : 0;
        table.rows.push_back(
            {name, sk > 0 ? "+-" + fmt(sk * 100, 1) + "%" : "true", sync ? "on" : "off",
             fmt(in.p50, 0), fmt(in.p95, 0), fmt(repeated * 100, 1) + "%",
             std::to_string(o.trimmed), fmt(o.view_delay_ms, 0), fmt(o.underrun * 100, 1) + "%",
             fmt(o.corrections_per_min, 1), fmt(o.worst_correction, 2), hex(o.hash)});
        const std::string key = name + (sk > 0 ? "-skew" : "") + (sync ? "-sync" : "-fixed") + "-";
        lab.metric(key + "input-ms-p95", in.p95, "ms");
        lab.metric(key + "repeated", repeated, "ratio");
        lab.metric(key + "view-ms", o.view_delay_ms, "ms");
        lab.metric(key + "held", o.underrun, "ratio");
        if (sync && sk == 0) {
          inputs.push_back({name, o.input_ms});
          delay.traces.push_back({name, o.t_q, o.delay_ms});
        }
        if (name == lab.arg("chart-preset") && sk == skew)
          queue.traces.push_back({sync ? "sync on" : "sync off", o.t_q, o.queue});
        if (sync && o.trimmed > 0)
          lab.fail(name + ": with time sync the server still dropped " + std::to_string(o.trimmed) +
                       " inputs for a full queue",
                   "The clock should have slowed before the queue filled: read sync_clock in "
                   "engine/net/replicate_client.cpp and the slack window in "
                   "replicate_server.cpp; skill: net.");
      }
      if (skew == 0) break;
    }
  }
  lab.table(std::move(table));
  lab.note(
      "clocks: true, or each client's clock run fast (even clients) or slow (odd) by the skew. "
      "sync off: the client's clock is its own and the others are drawn a fixed 6 ticks behind "
      "the newest snapshot; on: the server reports each client's slack (the fewest inputs left "
      "waiting over half a second) and the client speeds or slows its clock up to 3% to hold one "
      "in reserve, and draws the others behind its reckoning of the server's tick by the "
      "snapshots' spacing plus 2.5 times their jitter. input ms: from an input made to the "
      "server stepping it. repeated: server ticks that stepped a client's last input again for "
      "want of a new one. trimmed: inputs dropped for a queue past 32. view: how far behind "
      "the server's clock the others are drawn. held: drawn ticks past the newest snapshot (the "
      "others stand still). corr/min and worst: predictions moved, a client a minute, and the "
      "largest move.");
  const auto shoot = [&](const std::string& id, const gl::Chart& chart, const std::string& caption,
                         const std::string& look_for) {
    const std::string svg = gl::to_svg(chart.plot);
    lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
    lab.shot({id,
              chart.title,
              caption,
              look_for,
              {{"presets", lab.arg("presets")},
               {"skew", lab.arg("skew")},
               {"clients", lab.arg("clients")},
               {"seed", lab.arg("seed")}}},
             gl::to_picture(chart.plot));
  };
  if (!queue.traces.empty())
    shoot("queue", gl::line_chart(queue),
          "How many of one client's inputs wait on the server, ten times a second, with its "
          "clock skewed: time sync off and on.",
          "off: a fast clock's queue climbing until the server drops inputs (or a slow one's "
          "pinned at zero, repeating); on: the queue held at the reserve whatever the clock.");
  if (!delay.traces.empty())
    shoot("delay", gl::line_chart(delay),
          "How far behind the server one client draws the others with time sync, one line a "
          "preset.",
          "each line settling to the snapshots' spacing plus their jitter: low on a clean link, "
          "higher on a jittery one, and never stepping, only easing.");
  if (!inputs.empty())
    shoot("inputs",
          gl::box_chart({"INPUT TO SERVER STEP, TIME SYNC ON, EACH PRESET",
                         "every input of every client", "latency (ms)", ""},
                        inputs),
          "How long an input takes from the hand to the server's step, one box a preset.",
          "boxes at the one-way latency plus a tick in reserve, wider as the link jitters.");
}

const gl::LabSpec kSpec{
    "net-predict",
    gl::LabKind::sim,
    "engine",
    "engine/net (replicate, replicate_trial)",
    "net",
    "Under each condition, how fresh is a predicting client (input to server step, view delay) "
    "and how steady (repeats, held frames, corrections), with time sync off and on, true clocks "
    "and skewed?",
    {
        {"presets", "lan,broadband,wifi,mobile,bad,awful", "condition presets, comma-separated"},
        {"clients", "16", "clients, each predicting its own mover (1 to 32)"},
        {"npcs", "32", "movers the server moves"},
        {"seconds", "20", "how long each trial runs"},
        {"skew", "0.02", "clients' clocks this much fast or slow in the skewed rows (0: none)"},
        {"chart-preset", "bad", "the preset whose queue is charted"},
        {"seed", "1", "the network's, the hands' and the crowd's stream"},
    },
    "--seconds=4 --presets=lan,bad --clients=4 --npcs=8",
    run,
};
GE_LAB(kSpec);

}  // namespace
