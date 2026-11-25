// A PLAYER'S ROUND OF THE KEY ITEMS (se-people, M18r.md P1): each key item a player takes and the
// next key item he takes in the same life, by spot. People run rounds by the clock (dm2: the red
// armour then the mega in its room is their commonest pair, the bots' rare), so the pairs and the
// gap between two takes say whether a player times one item at a time or a round of them. People
// and bots by one rule, off the recordings.
//
// ge:summary se-people's rounds of the key items: takes a minute by spot, the next take and its gap
// ge:keywords key-item round circuit next-take chain pickups-per-min by-spot se-people
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/labs/keys_read.hpp"

namespace shooters_edge::labs {

constexpr double kChainNext_s = 30;  // a next take within this is a pair of the round

struct ChainRead {
  std::map<std::string, int64_t> takes;                          // by spot ("ra@2480,-1992")
  std::map<std::pair<std::string, std::string>, int64_t> pairs;  // a take, the next within 30 s
  std::vector<float> gaps;  // a take to the same life's next take, s
  int64_t firsts = 0;       // a life's first take
};

// A player's last key take in this life (`spot` empty: none yet).
struct ChainPlayer {
  double t = 0;
  std::string spot;
};

inline void chain_take(ChainRead& r, ChainPlayer& p, double t, const std::string& spot) {
  r.takes[spot]++;
  if (p.spot.empty()) {
    r.firsts++;
  } else {
    r.gaps.push_back(static_cast<float>(t - p.t));
    if (t - p.t < kChainNext_s) r.pairs[{p.spot, spot}]++;
  }
  p = {t, spot};
}

inline void chain_death(ChainPlayer& p) { p.spot.clear(); }

inline void add(ChainRead& a, const ChainRead& b) {
  for (const auto& [k, n] : b.takes) a.takes[k] += n;
  for (const auto& [k, n] : b.pairs) a.pairs[k] += n;
  a.gaps.insert(a.gaps.end(), b.gaps.begin(), b.gaps.end());
  a.firsts += b.firsts;
}

// `people.chain.*`: key takes a player-minute alive (`alive_min`), the gap to the next take
// (p25/p50/p75), the share of takes followed within 10, 20 and 30 s, and a table by spot: its takes
// a player-minute and the spots taken next within 30 s, the commonest first.
inline void report_chain(ge::lab::Lab& lab, ChainRead& r, double alive_min) {
  using ge::core::num::to_fixed;
  int64_t all = 0;
  for (const auto& [k, n] : r.takes) all += n;
  if (all == 0 || alive_min <= 0) return;
  lab.metric("people.chain.takes-per-min", static_cast<double>(all) / alive_min, "per-player-min");
  lab.metric("people.chain.gap-p25", keys_quantile(r.gaps, 0.25), "s");
  lab.metric("people.chain.gap-p50", keys_quantile(r.gaps, 0.5), "s");
  lab.metric("people.chain.gap-p75", keys_quantile(r.gaps, 0.75), "s");
  for (const int within : {10, 20, 30}) {
    int64_t n = 0;
    for (const float g : r.gaps) n += g < static_cast<float>(within) ? 1 : 0;
    lab.metric("people.chain.within-" + std::to_string(within) + "s",
               static_cast<double>(n) / static_cast<double>(all), "share");
  }
  ge::lab::LabTable t{
      "each key spot's takes and the spot its taker took next within 30 s, in the "
      "same life (chain_read.hpp): people run rounds of the key items by the clock",
      {"spot", "takes/min", "next within 30 s (spot count), the commonest first"},
      {}};
  std::vector<std::pair<int64_t, std::string>> spots;
  spots.reserve(r.takes.size());
  for (const auto& [k, n] : r.takes) spots.emplace_back(-n, k);
  std::sort(spots.begin(), spots.end());
  for (const auto& [neg, spot] : spots) {
    std::vector<std::pair<int64_t, std::string>> next;
    for (const auto& [pair, n] : r.pairs)
      if (pair.first == spot) next.emplace_back(-n, pair.second);
    std::sort(next.begin(), next.end());
    std::string cell;
    for (size_t k = 0; k < next.size() && k < 4; k++)
      cell += (k ? ", " : "") + next[k].second + " " + std::to_string(-next[k].first);
    t.rows.push_back(
        {spot, to_fixed(static_cast<double>(-neg) / alive_min, 3), cell.empty() ? "-" : cell});
  }
  lab.table(t);
}

}  // namespace shooters_edge::labs
