// THE LAUNCHER'S USE, REPORTED (se-people; docs/goals/M18r.md 18b): off a recording, the rockets
// a minute carrying the launcher and holding it up, by the shooter's sight (in view, just lost,
// blind), by the nearest enemy's range, a blind one's burst against the unseen enemies, the lob
// carried and fired, and the launcher's carrying spells by where they began. The counts are the
// se-people player record's (`all`); this writes their metrics, people and bots by one rule.
//
// ge:summary se-people's report of the launcher's use: by sight, by range, blind, the lob, spells
// ge:keywords launcher rockets-per-owned-min rl-shots blind-fire lob gl spells se-people finger
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "lab/lab.hpp"

namespace shooters_edge::labs {

// The launcher's metrics off `all` (se-people's summed player: `owned`, `held`, `alive`, the
// `rl_*` and `gl_*` counts), `every` the sample's spacing, `shots` the weapon sounds by name, the
// launcher's sounds and those with it up, and the carrying spells by source (spot, pack).
template <typename Player>
void report_launcher_use(ge::lab::Lab& lab, const Player& all, double every,
                         const std::map<std::string, int64_t>& shots, int64_t rl_sounds,
                         int64_t rl_sounds_held, const int64_t (&rl_spells)[2],
                         const double (&rl_spell_s)[2]) {
  // (M18r.md 18b) The launcher's rockets (its own sound) a minute carrying it and a minute holding
  // it up: se-team's `team.finger.rockets-per-owned-min` and `-per-held-min` by the same rule.
  {
    double rl_shots = 0;
    for (const auto& [name, k] : shots)
      if (name == "weapons/sgun1.wav") rl_shots = static_cast<double>(k);
    const auto per_min = [&](int64_t n) {
      return n > 0 ? rl_shots * 60 / (static_cast<double>(n) * every) : 0;
    };
    lab.metric("people.rockets-per-owned-min", per_min(all.owned[6]), "per-min");
    lab.metric("people.rockets-per-held-min", per_min(all.held[6]), "per-min");
    // The rockets by the shooter's sight, and a minute with the launcher loaded, an enemy in plain
    // view and not: who fires blind, and how fast with one in view.
    const double rl_all =
        static_cast<double>(all.rl_shots_view + all.rl_shots_recent + all.rl_shots_blind);
    const auto of_rl = [&](int64_t k) { return rl_all > 0 ? static_cast<double>(k) / rl_all : 0; };
    lab.metric("people.rl.shots-in-view", of_rl(all.rl_shots_view), "share");
    lab.metric("people.rl.shots-recent", of_rl(all.rl_shots_recent), "share");
    lab.metric("people.rl.shots-blind", of_rl(all.rl_shots_blind), "share");
    lab.metric("people.rl.loaded-in-view",
               all.rl_up_n > 0
                   ? static_cast<double>(all.rl_up_view) / static_cast<double>(all.rl_up_n)
                   : 0,
               "share");
    const auto per_min_n = [&](int64_t k, int64_t n) {
      return n > 0 ? static_cast<double>(k) * 60 / (static_cast<double>(n) * every) : 0;
    };
    lab.metric("people.rl.rockets-per-view-min", per_min_n(all.rl_shots_view, all.rl_up_view),
               "per-min");
    static const char* kRlBand[3] = {"near", "mid", "far"};
    int64_t vb_all = 0;
    for (int b = 0; b < 3; b++) vb_all += all.rl_view_band[b];
    for (int b = 0; b < 3; b++) {
      const std::string key = std::string("people.rl.") + kRlBand[b];
      lab.metric(
          key,
          vb_all > 0 ? static_cast<double>(all.rl_view_band[b]) / static_cast<double>(vb_all) : 0,
          "share");
      lab.metric(key + ".rockets-per-min", per_min_n(all.rl_shots_band[b], all.rl_view_band[b]),
                 "per-min");
    }
    static const char* kAge[3] = {"0.4", "0.8", "1.5"};
    for (int b = 0; b < 3; b++)
      lab.metric(std::string("people.rl.recent-under-") + kAge[b], of_rl(all.rl_recent_age[b]),
                 "share");
    const auto of_blind = [&](int64_t k) {
      return all.rl_blind_n > 0 ? static_cast<double>(k) / static_cast<double>(all.rl_blind_n) : 0;
    };
    lab.metric("people.rl.blind.enemy-in-reach", of_blind(all.rl_blind_hurt), "share");
    lab.metric("people.rl.blind.enemy-in-room", of_blind(all.rl_blind_room), "share");
    lab.metric("people.rl.blind.enemy-open", of_blind(all.rl_blind_open), "share");
    lab.metric("people.rl.blind.key-spot", of_blind(all.rl_blind_spot), "share");
    lab.metric("people.rl.blind.long", of_blind(all.rl_blind_far), "share");
    {
      const double gn = static_cast<double>(all.gl_shots[0] + all.gl_shots[1] + all.gl_shots[2]);
      static const char* kGl[3] = {"in-view", "recent", "blind"};
      for (int b = 0; b < 3; b++)
        lab.metric(std::string("people.gl.shots-") + kGl[b],
                   gn > 0 ? static_cast<double>(all.gl_shots[b]) / gn : 0, "share");
    }
    lab.metric(
        "people.gl.with-rl",
        all.gl_rl_n > 0 ? static_cast<double>(all.gl_with) / static_cast<double>(all.gl_rl_n) : 0,
        "share");
    lab.metric("people.gl.without-rl",
               all.gl_no_rl_n > 0
                   ? static_cast<double>(all.gl_without) / static_cast<double>(all.gl_no_rl_n)
                   : 0,
               "share");
    lab.metric("people.rl.rockets-per-calm-min",
               per_min_n(all.rl_shots_recent + all.rl_shots_blind, all.rl_up_n - all.rl_up_view),
               "per-min");
    // The launcher's carrying spells: a player-minute alive, by where they began (a launcher's
    // spot, a backpack), and how long one lasts.
    {
      const double player_min = static_cast<double>(all.alive) * every / 60;
      static const char* kSrc[2] = {"spot", "pack"};
      for (int b = 0; b < 2; b++) {
        const std::string key = std::string("people.rl.spells.") + kSrc[b];
        lab.metric(key + ".per-min",
                   player_min > 0 ? static_cast<double>(rl_spells[b]) / player_min : 0, "per-min");
        lab.metric(key + ".s",
                   rl_spells[b] > 0 ? rl_spell_s[b] / static_cast<double>(rl_spells[b]) : 0, "s");
      }
    }
    // How far the active weapon's stat is to be believed: the launcher's shots with it up.
    lab.metric(
        "people.rl-shots-held",
        rl_sounds > 0 ? static_cast<double>(rl_sounds_held) / static_cast<double>(rl_sounds) : 0,
        "share");
  }
}

}  // namespace shooters_edge::labs
