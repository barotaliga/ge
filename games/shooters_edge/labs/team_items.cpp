// THE ITEMS, REPORTED (se-team, team_items.hpp): a team match's packs, hits by the target's state,
// key items lying untaken, launchers lying near the unarmed, and pickups by kind.
//
// ge:summary se-team's report of the items: packs known, hits by state, items lying, pickups
// ge:keywords packs-known pack-guess hits-by-state lying-why rl-near took-per-min se-team
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#include "shooters_edge/labs/team_items.hpp"

#include <algorithm>
#include <string>

#include "shooters_edge/bots/fighter.hpp"

namespace shooters_edge::labs {

namespace cn = shooters_edge::net;
namespace bt = shooters_edge::bots;
namespace sb = ge::shooterbots;

void report_packs(ge::lab::Lab& lab, const cn::TeamMatchReport& r) {
  // Every death's pack known (bots/packs.hpp), by how its place was known; the weapon guessed
  // right by the guesser's skill (novice, average, expert); trips to them and what ended them.
  {
    static constexpr const char* kFrom[5] = {"seen", "cry", "blast", "aim", "lying"};
    int64_t known[5] = {}, guesses[4] = {}, right[4] = {}, guess_rl[4] = {}, truth_rl[4] = {};
    int64_t trips = 0, reached = 0, gone = 0;
    for (const cn::TeamPlayer& p : r.players) {
      for (int k = 0; k < 5; k++) known[k] += p.packs_known[k];
      const auto sk = static_cast<size_t>(p.skill);
      guesses[sk] += p.pack_guesses;
      right[sk] += p.pack_right;
      guess_rl[sk] += p.pack_guess_rl;
      truth_rl[sk] += p.pack_truth_rl;
      trips += p.pack_trips;
      reached += p.pack_reached;
      gone += p.pack_seen_gone;
    }
    for (int k = 0; k < 5; k++)
      lab.metric(std::string("team.packs.known.") + kFrom[k], static_cast<double>(known[k]),
                 "count");
    for (int k = 1; k < 4; k++) {  // by the guesser's skill (none plays `perfect` here)
      const std::string pre =
          std::string("team.packs.guess.") + sb::aim_skill_name(static_cast<sb::AimSkill>(k));
      lab.metric(pre + ".count", static_cast<double>(guesses[k]), "count");
      lab.metric(pre + ".right",
                 guesses[k] ? static_cast<double>(right[k]) / static_cast<double>(guesses[k]) : 0,
                 "share");
      lab.metric(pre + ".rl-guessed", static_cast<double>(guess_rl[k]), "count");
      lab.metric(pre + ".rl-true", static_cast<double>(truth_rl[k]), "count");
    }
    lab.metric("team.packs.trips", static_cast<double>(trips), "count");
    lab.metric("team.packs.reached", static_cast<double>(reached), "count");
    lab.metric("team.packs.seen-gone", static_cast<double>(gone), "count");
  }
}

void report_hits(ge::lab::Lab& lab, const cn::TeamMatchReport& r) {
  // Shots and damage by the target's state (net/qw_hits.hpp): the pellets' and the rockets'
  // hits over those fired at a target in that state, the damage he took a minute lived in it,
  // and the share of the time lived in it.
  {
    int64_t all = 0;
    for (const int64_t f : r.frames_at) all += f;
    for (int k = 0; k < cn::kTargetStates; k++) {
      const auto i = static_cast<size_t>(k);
      const std::string key = std::string("team.target.") + cn::kTargetStateNames[k];
      const auto rate = [](const cn::HitCount& c) {
        return c.fired > 0 ? static_cast<double>(c.hit) / static_cast<double>(c.fired) : 0;
      };
      // (player-minutes: a frame lasts the match's seconds over its frames)
      const double lived = static_cast<double>(r.frames_at[i]) * r.seconds /
                           std::max<double>(1, static_cast<double>(r.frames)) / 60;
      lab.metric(key + ".pellets", rate(r.pellets_at[i]), "share");
      lab.metric(key + ".rockets", rate(r.rockets_at[i]), "share");
      lab.metric(key + ".rockets-fired", static_cast<double>(r.rockets_at[i].fired), "count");
      lab.metric(key + ".damage-per-min", lived > 0 ? r.given_at[i] / lived : 0, "hp/min");
      lab.metric(key + ".time",
                 all > 0 ? static_cast<double>(r.frames_at[i]) / static_cast<double>(all) : 0,
                 "share");
    }
  }
  // The key items lying untaken (qw_team.hpp), and what the fighters made of it: nobody
  // believed it there, nobody wanted it, the one who wanted it was fighting, one was on his way.
  for (int k = 0; k < cn::kLyingItems; k++) {
    const auto ki = static_cast<size_t>(k);
    if (r.lying_n[ki] == 0) continue;
    const std::string key = std::string("team.lying.") + cn::kLyingItemNames[k];
    lab.metric(key, static_cast<double>(r.lying[ki]) / static_cast<double>(r.lying_n[ki]), "share");
    lab.metric(key + ".takers",
               r.lying[ki] > 0
                   ? static_cast<double>(r.lying_takers[ki]) / static_cast<double>(r.lying[ki])
                   : 0,
               "share");
    for (int w = 0; w < cn::kLyingWhy; w++)
      lab.metric(key + "." + cn::kLyingWhyNames[w],
                 r.lying[ki] > 0 ? static_cast<double>(r.lying_why[ki][static_cast<size_t>(w)]) /
                                       static_cast<double>(r.lying[ki])
                                 : 0,
                 "share");
  }
  // A launcher lying with a fighter near holding none (qw_team.hpp kRlNearWhy): what he made of
  // it, and the role of those not going for it.
  {
    int64_t all = 0;
    for (const int64_t v : r.rl_near) all += v;
    lab.metric("team.rl-near.samples", static_cast<double>(all), "count");
    for (int w = 0; w < cn::kRlNearWhy; w++)
      lab.metric(std::string("team.rl-near.") + cn::kRlNearWhyNames[w],
                 all > 0 ? static_cast<double>(r.rl_near[static_cast<size_t>(w)]) /
                               static_cast<double>(all)
                         : 0,
                 "share");
    const auto idle = static_cast<double>(std::max<int64_t>(1, r.rl_near_idle));
    lab.metric("team.rl-near.idle.seen", static_cast<double>(r.rl_near_idle_seen) / idle, "share");
    lab.metric("team.rl-near.idle.close", static_cast<double>(r.rl_near_idle_close) / idle,
               "share");
    const char* const kRoleNames[5] = {"free", "hold", "rush", "carry", "push"};
    for (int k = 0; k < 5; k++)
      lab.metric(std::string("team.rl-near.role.") + kRoleNames[k],
                 all > 0 ? static_cast<double>(r.rl_near_role[static_cast<size_t>(k)]) /
                               static_cast<double>(all)
                         : 0,
                 "share");
  }
}

void report_pickups(ge::lab::Lab& lab, const cn::TeamMatchReport& r) {
  // Pickups by kind a player-minute, set beside people's (a match's statistics count them so).
  static const char* kKinds[12] = {"health", "mega",    "armor1", "armor2", "armor3", "shells",
                                   "nails",  "rockets", "cells",  "weapon", "quad",   "pent"};
  int64_t ways = 0;
  for (const cn::TeamPlayer& p : r.players) ways += p.on_the_way;
  lab.metric("team.on-the-way", static_cast<double>(ways), "count");
  const double player_min = static_cast<double>(r.players.size()) * r.seconds / 60.0;
  for (size_t k = 0; k < 12; k++) {
    int64_t n = 0;
    for (const cn::TeamPlayer& p : r.players) n += p.took[k];
    lab.metric(std::string("team.took-per-min.") + kKinds[k],
               player_min > 0 ? static_cast<double>(n) / player_min : 0, "per-min");
  }
}

}  // namespace shooters_edge::labs
