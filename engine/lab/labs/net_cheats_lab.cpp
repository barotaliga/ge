// LAB `cheats`: honest players and cheaters on one server-authoritative world, the guard judging
// what arrives (engine/net/guard.hpp), once under each condition preset. Each cheater plays one
// cheat the whole run (the stick past full, a stick off the pad's grid, three inputs a tick, a
// view a second ahead, a peer's place stated 40 m on), and the lab answers whether each is caught
// and how soon, whether any honest player is flagged on any link, and whether a cheat moved
// anyone faster than full stick. The scores are charted over the run, one line a player, with
// the out score ruled across. Everything runs on a virtual clock off one seed.
//
// ge:summary Lab: honest players and five cheaters under each preset: caught, how soon, no false flag
// ge:keywords lab net cheats anti-cheat guard speed-hack teleport flood future-view off-grid
// ge:keywords false-positive score preset multiplayer
// ge:skill net labs
// ge:test tests/net_guard_test.cpp
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/guard_trial.hpp"

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

std::string counts_text(const gn::GuardTrialPlayer& p) {
  std::string out;
  for (size_t k = 0; k < p.counts.size(); k++) {
    if (p.counts[k] == 0) continue;
    if (!out.empty()) out += ", ";
    out +=
        std::string(gn::cheat_name(static_cast<gn::Cheat>(k))) + " " + std::to_string(p.counts[k]);
  }
  return out.empty() ? "-" : out;
}

void run(gl::Lab& lab) {
  gn::GuardTrialConfig k;
  k.honest = static_cast<int>(lab.arg_num("honest"));
  k.seconds = lab.arg_num("seconds");
  k.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  if (k.honest < 1 || k.honest > 24 || k.seconds < 2) {
    lab.fail("--honest or --seconds is out of range",
             "Pass --honest=1 to 24 and --seconds=2 or more; skill: net.");
    return;
  }
  gl::LabTable table{
      "Each player under each preset: what the guard flagged, and when it was out",
      {"preset", "player", "flags", "worst score", "out at s", "top m/s", "travelled m"},
      {}};
  bool charted = false;
  gl::LineChart scores{
      {"GUARD SCORE, EACH PLAYER", "", "t (s)", "score"}, {}, {}, {}, {}, false, false, false};
  for (const std::string& name : split(lab.arg("presets"))) {
    gn::LinkConditions link;
    if (!gn::conditions_preset(name, link)) {
      lab.fail("unknown preset '" + name + "'",
               "Pick from lan, broadband, wifi, mobile, bad, awful (engine/net/conditions.cpp); "
               "skill: net.");
      continue;
    }
    k.link = link;
    const gn::GuardTrial o = gn::run_guard_trial(k);
    int honest_flagged = 0, caught = 0;
    double honest_worst = 0, honest_top = 0;
    for (const gn::GuardTrialPlayer& p : o.players) {
      const std::string who = gn::trial_cheat_name(p.cheat);
      if (p.cheat == gn::TrialCheat::none) {
        honest_worst = std::max(honest_worst, p.worst);
        honest_top = std::max(honest_top, p.top_speed);
        if (p.worst > 0) honest_flagged++;
        if (p.out)
          lab.fail(name + ": an honest player was put out (" + counts_text(p) + ")",
                   "Read the flag's check in engine/net/guard.cpp and widen its slack only as far "
                   "as a link explains; skill: net.");
        continue;
      }
      table.rows.push_back({name, who, counts_text(p), fmt(p.worst, 1),
                            p.out ? fmt(p.out_s, 2) : "never", fmt(p.top_speed, 1),
                            fmt(p.travelled, 0)});
      lab.metric(name + "-" + who + "-out-s", p.out ? p.out_s : -1, "s");
      if (p.out) caught++;
      if (!p.out)
        lab.fail(
            name + ": the " + who + " cheat was not caught (worst score " + fmt(p.worst, 1) + ")",
            "Read its check in engine/net/guard.cpp and the cheat in "
            "engine/net/guard_trial.cpp; skill: net.");
      if (p.top_speed > o.full_stick_mps + 0.5)
        lab.fail(name + ": the " + who + " cheater moved at " + fmt(p.top_speed, 1) +
                     " m/s, past full stick's " + fmt(o.full_stick_mps, 1),
                 "Read guard_input's clamp and the one input a tick in "
                 "engine/net/replicate_server.cpp; skill: net.");
      if (!charted) {
        scores.traces.push_back({who, o.t_s, p.score});
      }
    }
    table.rows.push_back({name, "honest x" + std::to_string(k.honest),
                          std::to_string(honest_flagged) + " flagged", fmt(honest_worst, 1),
                          "never", fmt(honest_top, 1), "-"});
    lab.metric(name + "-caught", caught, "count");
    lab.metric(name + "-honest-flagged", honest_flagged, "count");
    lab.metric(name + "-flags", static_cast<double>(o.flags), "count");
    if (!charted) {
      // The honest players' scores, every one, as one line: their highest at each moment.
      std::vector<double> top(o.t_s.size(), 0);
      for (const gn::GuardTrialPlayer& p : o.players)
        if (p.cheat == gn::TrialCheat::none)
          for (size_t i = 0; i < top.size() && i < p.score.size(); i++)
            top[i] = std::max(top[i], p.score[i]);
      scores.traces.push_back({"honest (highest)", o.t_s, top});
      scores.frame.subtitle = "preset " + name + ", the out score dashed";
      scores.rules.push_back(gn::GuardRules{}.out_score);
      charted = true;
    }
  }
  lab.table(std::move(table));
  lab.note(
      "flags: what the guard raised for each player, by kind. worst score: the highest its score "
      "reached (each flag adds its weight, a second takes 1 away). out at s: when the score "
      "passed the out score (40). top m/s: the fastest second its mover went on the server; full "
      "stick on both axes is 11.3 m/s. honest: every player without a cheat, together.");
  if (scores.traces.empty()) return;
  const gl::Chart chart = gl::line_chart(scores);
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach("scores.svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot(
      {"scores",
       chart.title,
       "Each cheater's guard score over the run on the first preset, and the highest of the "
       "honest players' at each moment; past the dashed line a player is out.",
       "every cheater's line crossing the dashed line within two seconds but the teleporter's, "
       "which climbs a step at each jump and crosses at the sixth; the flooder caught by its "
       "clock running ahead before its first window closes; the honest line flat on zero.",
       {{"presets", lab.arg("presets")},
        {"honest", lab.arg("honest")},
        {"seconds", lab.arg("seconds")},
        {"seed", lab.arg("seed")}}},
      gl::to_picture(chart.plot));
}

const gl::LabSpec kSpec{
    "cheats",
    gl::LabKind::sim,
    "engine",
    "engine/net (guard, guard_trial)",
    "net",
    "Is every scripted cheat caught on every link, how soon, and is any honest player flagged?",
    {
        {"presets", "lan,broadband,wifi,mobile,bad,awful", "condition presets, comma-separated"},
        {"honest", "8", "players with no cheat (1 to 24)"},
        {"seconds", "20", "how long each run lasts"},
        {"seed", "1", "the network's and the hands' stream"},
    },
    "--seconds=8 --presets=lan,bad --honest=4",
    run,
};
GE_LAB(kSpec);

}  // namespace
