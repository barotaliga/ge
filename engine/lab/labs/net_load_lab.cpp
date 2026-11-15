// LAB `net-load`: many sessions in one host process. Each count of sessions runs a full house
// (every client connected, inputs up, snapshots down, reliable events both ways) on one
// simulated network, and each session's host is timed from its update to its flush on the
// wall clock. The deterministic half (who connected, what was sent and delivered, every
// reliable event whole, the memory held) is the digest; the times are measured: a table, the
// tick's share of its budget and a chart of the host's step against the number of sessions.
//
// ge:summary Lab: many sessions in one host process, each host's step timed and charted
// ge:keywords lab net load sessions host-process step-time tick budget multiplayer capacity
// ge:skill net labs
// ge:test tests/net_stress_test.cpp
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/stress.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;

int64_t wall_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

std::vector<int> counts(const std::string& s) {
  std::vector<int> out;
  size_t at = 0;
  while (at < s.size()) {
    size_t comma = s.find(',', at);
    if (comma == std::string::npos) comma = s.size();
    const std::string part = s.substr(at, comma - at);
    out.push_back(part.empty() ? 0 : std::atoi(part.c_str()));
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
  gn::LoadPlan base;
  base.clients = static_cast<int>(lab.arg_num("clients"));
  base.seconds = lab.arg_num("seconds");
  base.tick_hz = lab.arg_num("hz");
  base.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  base.clock_ns = wall_ns;
  const std::vector<int> sessions = counts(lab.arg("sessions"));
  bool sessions_ok = !sessions.empty();
  for (const int n : sessions) sessions_ok = sessions_ok && n >= 1 && n <= 256;
  if (!gn::conditions_preset(lab.arg("preset"), base.conditions) ||
      !gn::traffic_mix(lab.arg("mix"), base.mix) || base.clients < 1 || base.clients > 32 ||
      !sessions_ok || base.seconds <= 0 || base.tick_hz < 10 || base.tick_hz > 240) {
    lab.fail("--preset, --mix, --clients, --sessions, --seconds or --hz is out of range",
             "Pass a preset (lan, broadband, wifi, mobile, bad, awful), a mix (racing, action, "
             "rpg, world, lockstep, download), --clients=1 to 32, --sessions as counts 1 to 256 "
             "comma-separated, --seconds above 0 and --hz=10 to 240; skill: net.");
    return;
  }
  const std::string what = std::to_string(base.clients) + " clients a session, the " +
                           lab.arg("mix") + " mix on " + lab.arg("preset") + " at " +
                           lab.arg("hz") + " Hz";
  gl::LabTable counts_table{
      "What each house delivered: " + what,
      {"sessions", "connected", "inputs sent", "inputs in", "snapshots sent", "snapshots in",
       "events sent", "events in", "reliable whole", "held KB", "log"},
      {}};
  gl::LabTable times{"The host's step, timed: " + what,
                     {"sessions", "step mean us", "step p99 us", "worst session p99 us",
                      "all hosts a tick us", "of the tick", "sessions a tick fits"},
                     {}};
  gl::Trace mean_trace{"one host's step, mean", {}, {}};
  gl::Trace p99_trace{"one host's step, p99", {}, {}};
  gl::Trace tick_trace{"every host a tick, mean", {}, {}};
  double budget_us = 0;
  for (const int n : sessions) {
    gn::LoadPlan plan = base;
    plan.sessions = n;
    const gn::LoadReport r = gn::run_load(plan);
    budget_us = r.tick_budget_us;
    uint64_t in_sent = 0, in_got = 0, snap_sent = 0, snap_got = 0, ev_sent = 0, ev_got = 0;
    double worst_p99 = 0;
    for (const gn::LoadSession& s : r.sessions) {
      in_sent += s.inputs_sent;
      in_got += s.inputs_delivered;
      snap_sent += s.snapshots_sent;
      snap_got += s.snapshots_delivered;
      ev_sent += s.events_sent;
      ev_got += s.events_delivered;
      worst_p99 = std::max(worst_p99, s.step_us_p99);
    }
    const std::string label = std::to_string(n);
    counts_table.rows.push_back(
        {label, std::to_string(r.connected) + " of " + std::to_string(r.expected),
         std::to_string(in_sent), std::to_string(in_got), std::to_string(snap_sent),
         std::to_string(snap_got), std::to_string(ev_sent), std::to_string(ev_got),
         r.reliable_whole ? "yes" : "NO", fmt(r.held_kb, 0), hex(r.digest)});
    const double fits = r.step_us_mean > 0 ? r.tick_budget_us / r.step_us_mean : 0;
    times.rows.push_back({label, fmt(r.step_us_mean, 1), fmt(r.step_us_p99, 1), fmt(worst_p99, 1),
                          fmt(r.tick_us_mean, 1),
                          fmt(100.0 * r.tick_us_mean / r.tick_budget_us, 1) + " %", fmt(fits, 0)});
    lab.metric(label + "-connected", r.connected, "clients");
    lab.metric(label + "-held-kb", r.held_kb, "KB");
    lab.metric(label + "-step-us-mean", r.step_us_mean, "us", gl::Det::measured);
    lab.metric(label + "-step-us-p99", r.step_us_p99, "us", gl::Det::measured);
    lab.metric(label + "-tick-us-mean", r.tick_us_mean, "us", gl::Det::measured);
    lab.metric(label + "-sessions-a-tick", fits, "sessions", gl::Det::measured);
    mean_trace.x.push_back(n);
    mean_trace.y.push_back(r.step_us_mean);
    p99_trace.x.push_back(n);
    p99_trace.y.push_back(r.step_us_p99);
    tick_trace.x.push_back(n);
    tick_trace.y.push_back(r.tick_us_mean);
    if (r.connected != r.expected)
      lab.fail(label + " sessions: " + std::to_string(r.connected) + " of " +
                   std::to_string(r.expected) + " clients connected",
               "Read run_load's connect schedule in engine/net/stress_load.cpp, then node.cpp's "
               "handshake and max_peers; skill: net.");
    if (!r.reliable_whole)
      lab.fail(label + " sessions: a reliable event was lost",
               "Read connection.cpp's reliable resend; the drain in run_load is 3 s; skill: net.");
  }
  lab.table(std::move(counts_table));
  lab.table(std::move(times), gl::Det::measured);
  lab.note(
      "Every session is a host and its clients on one simulated network, run in one process; the "
      "clients connect in the first second, send for --seconds, then the run drains 3 s so every "
      "reliable event lands. step: one host's update, its messages read, its sends queued and its "
      "flush, on the wall clock (measured, this machine, this build). all hosts a tick: every "
      "session's step in one tick added up, against the tick's budget. sessions a tick fits: the "
      "budget over one host's mean step, the clients and the network left out.");
  gl::LineChart chart{{"THE HOST'S STEP AGAINST THE SESSIONS IN ONE PROCESS", what,
                       "sessions in the process", "us"},
                      {mean_trace, p99_trace, tick_trace},
                      {},
                      {},
                      {budget_us},
                      true,
                      false,
                      false};
  const gl::Chart c = gl::line_chart(chart);
  const std::string svg = gl::to_svg(c.plot);
  lab.attach("load.svg", std::vector<uint8_t>(svg.begin(), svg.end()), gl::Det::measured);
  lab.shot({"load",
            c.title,
            "One host's step (mean and p99) and every host's steps in one tick added up, against "
            "the number of sessions in the process; the dashed line is the tick's budget.",
            "one host's step flat as sessions are added (the hosts share nothing), and the "
            "tick's total climbing in a straight line toward the budget; where it would cross "
            "is how many sessions one core holds.",
            {{"sessions", lab.arg("sessions")},
             {"clients", lab.arg("clients")},
             {"preset", lab.arg("preset")},
             {"mix", lab.arg("mix")},
             {"hz", lab.arg("hz")}}},
           gl::to_picture(c.plot), gl::Det::measured);
}

const gl::LabSpec kSpec{
    "net-load",
    gl::LabKind::bench,
    "engine",
    "engine/net (stress_load, node)",
    "net",
    "How many full sessions does one host process hold: does every client still connect and "
    "every reliable event land, and what does each host's step cost as sessions are added?",
    {
        {"sessions", "1,4,16", "the counts of sessions to run, comma-separated"},
        {"clients", "32", "clients a session (1 to 32)"},
        {"preset", "broadband", "every link's conditions"},
        {"mix", "action", "the traffic mix"},
        {"seconds", "10", "seconds of sending after the connects"},
        {"hz", "60", "the hosts' tick rate"},
        {"seed", "1", "the network's stream"},
    },
    "--sessions=1,3 --clients=8 --seconds=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
