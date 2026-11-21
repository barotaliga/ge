// A KEY ITEM AS IT SHOWS, REPORTED (se-people): who takes each key item once it is back (by item
// and by spot), the fight there (`showfight.<item>.*`: the deaths near it as it shows, who wins a
// launcher's fight and an armour's, whether the last killer's team takes it) and the armour worn
// by type. The counts are show_read.hpp's; this writes their metrics, people and bots by one rule.
//
// ge:summary se-people's report of the key items as they show, the fight there and the armour worn
// ge:keywords key-item showing taken taker wait fight-at-showing rl-duel armour-duel armour-worn
// ge:keywords se-people
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>

#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/labs/keys_read.hpp"
#include "shooters_edge/labs/show_read.hpp"

namespace shooters_edge::labs {

// The metrics and the by-spot table off se-people's sums: `show` and `fight` by item kind (`names`
// the kinds' metric names), `show_spots` by spot, `wear` the armour worn.
template <size_t N>
void report_show(ge::lab::Lab& lab, std::array<ShowRead, N>& show,
                 std::map<std::string, ShowRead>& show_spots, const std::array<FightRead, N>& fight,
                 const ArmourWear& wear, const char* const (&names)[N]) {
  using ge::core::num::to_fixed;
  {
    const auto sshare = [](int64_t part, int64_t whole) {
      return whole > 0 ? static_cast<double>(part) / static_cast<double>(whole) : 0;
    };
    for (int k = 1; k < static_cast<int>(N); k++) {
      ShowRead& r = show[static_cast<size_t>(k)];
      if (r.shown == 0) continue;
      const std::string key = std::string("people.show.") + names[k];
      lab.metric(key + ".shown", static_cast<double>(r.shown), "count");
      lab.metric(key + ".mate-on", sshare(r.mate_on, r.shown), "share");
      lab.metric(key + ".enemy-on", sshare(r.enemy_on, r.shown), "share");
      lab.metric(key + ".mate-near", sshare(r.mate_near, r.shown), "share");
      lab.metric(key + ".enemy-near", sshare(r.enemy_near, r.shown), "share");
      lab.metric(key + ".nobody-near", sshare(r.nobody_near, r.shown), "share");
      lab.metric(key + ".by-self", sshare(r.by_self, r.taken), "share");
      lab.metric(key + ".by-mate", sshare(r.by_mate, r.taken), "share");
      lab.metric(key + ".by-enemy", sshare(r.by_enemy, r.taken), "share");
      lab.metric(key + ".taker-waited", sshare(r.taker_waited, r.taken), "share");
      lab.metric(key + ".taken-full", sshare(r.taker_full, r.taken), "share");
      lab.metric(key + ".taker-rl", sshare(r.taker_rl, r.taken), "share");
      lab.metric(key + ".owner-share", sshare(r.owner_takes, r.team_takes), "share");
      lab.metric(key + ".wait-p50", keys_quantile(r.waits, 0.5), "s");
      lab.metric(key + ".wait-p75", keys_quantile(r.waits, 0.75), "s");
      int64_t quick = 0;
      for (const float w : r.waits) quick += w <= 1;
      lab.metric(key + ".within-1s", sshare(quick, r.taken), "share");
    }
    ge::lab::LabTable by_spot{
        "each key spot as its item shows (show_read.hpp): showings, the last taker's mate within "
        "200 and 600, the wait to its going (p50, p75), taken within 1 s, the taker there waiting, "
        "taken by an enemy",
        {"spot", "shown", "mate on", "mate near", "wait p50", "wait p75", "within 1 s", "waited",
         "by enemy", "owner"},
        {}};
    for (auto& [at, r] : show_spots) {
      int64_t quick = 0;
      for (const float w : r.waits) quick += w <= 1;
      by_spot.rows.push_back(
          {at, std::to_string(r.shown), to_fixed(sshare(r.mate_on, r.shown), 2),
           to_fixed(sshare(r.mate_near, r.shown), 2), to_fixed(keys_quantile(r.waits, 0.5), 1),
           to_fixed(keys_quantile(r.waits, 0.75), 1), to_fixed(sshare(quick, r.taken), 2),
           to_fixed(sshare(r.taker_waited, r.taken), 2), to_fixed(sshare(r.by_enemy, r.taken), 2),
           to_fixed(sshare(r.owner_takes, r.team_takes), 2)});
    }
    lab.table(by_spot);
    int64_t worn = 0;
    for (const int64_t wn : wear.n) worn += wn;
    const char* const wname[4] = {"none", "ga", "ya", "ra"};
    for (size_t k = 0; k < 4; k++) {
      lab.metric(std::string("people.armour.wear.") + wname[k], sshare(wear.n[k], worn), "share");
      if (k > 0)
        lab.metric(std::string("people.armour.wear.") + wname[k] + "-points",
                   wear.n[k] > 0 ? wear.points[k] / static_cast<double>(wear.n[k]) : 0, "points");
    }
    lab.metric("people.armour.strong", sshare(wear.strong, worn), "share");
  }
  // The fight at a showing (show_read.hpp `fight_join`).
  {
    const auto fshare = [](int64_t part, int64_t whole) {
      return whole > 0 ? static_cast<double>(part) / static_cast<double>(whole) : 0;
    };
    FightRead all;
    const auto put = [&](const std::string& key, const FightRead& r) {
      lab.metric(key + ".shows", static_cast<double>(r.shows), "count");
      lab.metric(key + ".fought", fshare(r.fought, r.shows), "share");
      lab.metric(key + ".deaths-per-show",
                 r.shows > 0 ? static_cast<double>(r.deaths) / static_cast<double>(r.shows) : 0,
                 "per-show");
      lab.metric(key + ".holders-died", fshare(r.holders, r.deaths), "share");
      lab.metric(key + ".killer-known", fshare(r.known, r.deaths), "share");
      lab.metric(key + ".victim-rl", fshare(r.victim_rl, r.known), "share");
      lab.metric(key + ".killer-rl", fshare(r.killer_rl, r.known), "share");
      lab.metric(key + ".rl-duels", fshare(r.rl_duels, r.known), "share");
      lab.metric(key + ".rl-won", fshare(r.rl_won, r.rl_duels), "share");
      lab.metric(key + ".armour-duels", fshare(r.armour_duels, r.known), "share");
      lab.metric(key + ".armour-won", fshare(r.armour_won, r.armour_duels), "share");
      lab.metric(key + ".victim-armour",
                 r.known > 0 ? r.victim_armour / static_cast<double>(r.known) : 0, "points");
      lab.metric(key + ".killer-armour",
                 r.known > 0 ? r.killer_armour / static_cast<double>(r.known) : 0, "points");
      lab.metric(key + ".winner-took", fshare(r.winner_took, r.decided), "share");
      lab.metric(key + ".strong-near", fshare(r.strong, r.shows), "share");
      lab.metric(key + ".strong-holder-near", fshare(r.strong_holder, r.shows), "share");
    };
    for (size_t k = 1; k < N; k++) {
      if (fight[k].shows == 0) continue;
      put(std::string("people.showfight.") + names[k], fight[k]);
      add(all, fight[k]);
    }
    put("people.showfight.key", all);
  }
}

}  // namespace shooters_edge::labs
