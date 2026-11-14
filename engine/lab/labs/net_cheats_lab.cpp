// LAB `cheats`: honest players and cheaters on one server-authoritative world, the guard judging
// what arrives (engine/net/guard.hpp), once under each condition preset. Each cheater plays one
// cheat the whole run (the stick past full, a stick off the pad's grid, three inputs a tick, a
// view seconds ahead, a peer's place stated 40 m on, its own sealed packets sent again, a sealed
// packet changed, every row read for the players out of sight), and the lab answers whether each
// is caught and how soon or refused before the game, whether any honest player is flagged on any
// link, whether a cheat moved anyone faster than full stick, and whether anyone was ever sent a
// player out of its sight. The scores are charted over the run, one line a player, with
// the out score ruled across. Everything runs on a virtual clock off one seed.
//
// ge:summary Lab: honest players and eight cheaters a preset: caught or refused, no false flag
// ge:keywords lab net cheats anti-cheat guard speed-hack teleport flood future-view off-grid
// ge:keywords false-positive score preset multiplayer replay forgery interest wall-hack
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
  gl::LabTable table{"Each player under each preset: what the guard flagged, and when it was out",
                     {"preset", "player", "flags", "worst score", "out at s", "top m/s",
                      "travelled m", "refused", "rows seen", "hidden rows"},
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
    int honest_flagged = 0, caught = 0, refused = 0;
    uint32_t hidden = 0;
    double honest_worst = 0, honest_top = 0;
    for (const gn::GuardTrialPlayer& p : o.players) {
      const std::string who = gn::trial_cheat_name(p.cheat);
      hidden += p.hidden_seen;
      if (p.hidden_seen > 0) {
        std::string what = name;
        what += ": the ";
        what += who;
        what += " player was sent ";
        what += std::to_string(p.hidden_seen);
        what += " rows of players out of its sight";
        lab.fail(what,
                 "Read visible() in engine/net/replicate_server.cpp and the row check in "
                 "engine/net/guard_trial.cpp; skill: net.");
      }
      if (p.cheat == gn::TrialCheat::none) {
        honest_worst = std::max(honest_worst, p.worst);
        honest_top = std::max(honest_top, p.top_speed);
        if (p.worst > 0) honest_flagged++;
        if (p.out)
          lab.fail(name + ": an honest player was put out, " + counts_text(p),
                   "Read the flag's check in engine/net/guard.cpp and widen its slack only as far "
                   "as a link explains; skill: net.");
        continue;
      }
      table.rows.push_back(
          {name, who, counts_text(p), fmt(p.worst, 1), p.out ? fmt(p.out_s, 2) : "never",
           fmt(p.top_speed, 1), fmt(p.travelled, 0),
           p.sent_bad > 0 ? std::to_string(p.refused) + " of " + std::to_string(p.sent_bad) : "-",
           std::to_string(p.rows_seen), std::to_string(p.hidden_seen)});
      std::string head = name;
      head += ": the ";
      head += who;
      // Refused, not caught: the replayer's and the forger's packets never reach the game, and
      // the fisher is never sent what it looks for.
      if (p.cheat == gn::TrialCheat::replay || p.cheat == gn::TrialCheat::forge ||
          p.cheat == gn::TrialCheat::fish) {
        const bool held = p.cheat == gn::TrialCheat::fish
                              ? p.hidden_seen == 0 && p.out_of_sight > 0
                              : p.worst == 0 && p.sent_bad > 0 && p.refused * 2 > p.sent_bad;
        if (held) refused++;
        if (!held)
          lab.fail(head + " cheat was not refused (sent " + std::to_string(p.sent_bad) +
                       ", refused " + std::to_string(p.refused) + ", worst score " +
                       fmt(p.worst, 1) + ")",
                   "Read the sealed packet's checks in engine/net/node.cpp (window_allows, "
                   "open_after_header) and the cheat in engine/net/guard_trial.cpp; skill: net.");
        std::string key = name;
        key += "-";
        key += who;
        key += "-refused";
        lab.metric(key, p.refused, "count");
        continue;
      }
      std::string key = name;
      key += "-";
      key += who;
      key += "-out-s";
      lab.metric(key, p.out ? p.out_s : -1, "s");
      if (p.out) caught++;
      if (!p.out)
        lab.fail(head + " cheat was not caught, worst score " + fmt(p.worst, 1),
                 "Read its check in engine/net/guard.cpp and the cheat in "
                 "engine/net/guard_trial.cpp; skill: net.");
      if (p.top_speed > o.full_stick_mps + 0.5)
        lab.fail(head + " cheater moved past full stick's " + fmt(o.full_stick_mps, 1) + " m/s",
                 "Read guard_input's clamp and the one input a tick in "
                 "engine/net/replicate_server.cpp; skill: net.");
      if (!charted) {
        scores.traces.push_back({who, o.t_s, p.score});
      }
    }
    table.rows.push_back({name, "honest x" + std::to_string(k.honest),
                          std::to_string(honest_flagged) + " flagged", fmt(honest_worst, 1),
                          "never", fmt(honest_top, 1), "-", "-", "-", "-"});
    lab.metric(name + "-caught", caught, "count");
    lab.metric(name + "-refused", refused, "count");
    lab.metric(name + "-hidden-rows", hidden, "count");
    lab.metric(name + "-forged", static_cast<double>(o.forged), "count");
    lab.metric(name + "-replayed", static_cast<double>(o.replayed), "count");
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
      "stick on both axes is 11.3 m/s. refused: of the bad packets it sent, how many the server's "
      "node dropped before the game (the rest were lost on the link). rows seen: other players' "
      "rows it was sent; hidden rows: those further than its sight (45 m), which must be none. "
      "honest: every player without a cheat, together.");
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
