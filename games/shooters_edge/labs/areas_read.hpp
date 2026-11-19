// THE KEY AREAS READ OFF A RECORDING: who keeps each key item's area and how closely, alike for
// people's recordings and the bots'. A holder need not stand on his item: knowing nobody is coming,
// he waits outside it to meet whoever does, so where the holder stands says little; the area's
// state says what matters.
//
// An AREA is a key item's spot (the armours, the mega, the quad, the shield, the launcher); a
// player is IN it within kAreaIn of the spot. Every sample, for each area:
//
// - THE KEEPER: the team that last had a live player in it with none of the other team's there.
//   It keeps it until an enemy reaches it alone (the area LOST, `changes`); before anyone has
//   reached it, nobody keeps it (`unkept`).
// - CONTESTED: both teams have a live player in it.
// - COVERED: kept, not contested, and a keeper's player within kAreaNear of it (near enough to
//   meet whoever comes);
// - OPEN: kept, not contested, and none of the keeper's within kAreaNear: unsafe, anyone may walk
//   in.
// - CLOSENESS: the nearest live player of the keeper's team and of the other team, banded by
//   kAreaBand (`keeper-near.*`, `enemy-near.*`), and how often the enemy stands nearer than the
//   keeper (`enemy-nearer`).
// - THE MAIN KEEPER: per recording, the share of the kept time the team keeping it most keeps it
//   (one team's area: near 1; traded back and forth: near 0.5).
//
// Distances are straight lines (a floor above or below counts its height), not the way there.
//
// ge:summary Lab helper: key areas kept, covered, open or contested off a recording, people and bots
// ge:keywords key-areas area-control holding keeper covered open contested closeness item-control
// ge:keywords se-people people-areas map-control
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "shooter/vec.hpp"

namespace shooters_edge::labs {

constexpr float kAreaIn = 256;    // a player this near the spot is in the area, units
constexpr float kAreaNear = 768;  // a keeper's player this near covers it, units
constexpr float kAreaBand = 256;  // the closeness bands' width, units
constexpr int kAreaBands = 6;     // 0-256 .. 1280+, the last open

struct AreaCount {
  int64_t samples = 0, unkept = 0, covered = 0, open = 0, contested = 0;
  int64_t enemy_nearer = 0;  // kept, and an enemy nearer the spot than any keeper
  int64_t changes = 0;       // kept by one team, then reached by the other alone
  double seconds = 0;        // the time sampled
  double main_kept = 0;      // the main keeper's share of the kept time, summed over recordings
  int64_t main_n = 0;        // recordings it was kept in
  std::array<int64_t, kAreaBands> keeper_near{}, enemy_near{};
};

inline void add(AreaCount& a, const AreaCount& b) {
  a.samples += b.samples;
  a.unkept += b.unkept;
  a.covered += b.covered;
  a.open += b.open;
  a.contested += b.contested;
  a.enemy_nearer += b.enemy_nearer;
  a.changes += b.changes;
  a.seconds += b.seconds;
  a.main_kept += b.main_kept;
  a.main_n += b.main_n;
  for (int k = 0; k < kAreaBands; k++) {
    a.keeper_near[static_cast<size_t>(k)] += b.keeper_near[static_cast<size_t>(k)];
    a.enemy_near[static_cast<size_t>(k)] += b.enemy_near[static_cast<size_t>(k)];
  }
}

// One area as a recording is read: its spot, its keeper, and the samples each team kept it.
struct AreaSpot {
  int kind = -1;
  ge::shooter::Vec3f at;
  std::string keeper;                       // empty: nobody yet
  std::vector<std::pair<std::string, int64_t>> kept;  // samples kept, by team
  AreaCount count;                                    // this spot's own
};

// A live player at a sample.
struct AreaPlayer {
  ge::shooter::Vec3f at;
  std::string team;
};

inline int area_band(float d) {
  const int b = static_cast<int>(d / kAreaBand);
  return b < kAreaBands - 1 ? b : kAreaBands - 1;
}

// One sample `dt` s after the last: every area's state into `by_kind[area.kind]`.
// One area's sample into `c` (its kind's count, or its own).
inline void area_sample(AreaSpot& a, AreaCount& c, const std::vector<AreaPlayer>& live, double dt,
                        bool own) {
  c.samples++;
  c.seconds += dt;
  // Each team's nearest, and who stands in it.
  std::string in_team;
  bool two_in = false;
  float keeper_d = 1e30f, enemy_d = 1e30f;
  for (const AreaPlayer& p : live) {
    const ge::shooter::Vec3f v = p.at - a.at;
    const float d = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (d < kAreaIn) {
      if (in_team.empty())
        in_team = p.team;
      else if (in_team != p.team)
        two_in = true;
    }
    if (!a.keeper.empty()) {
      float& near = p.team == a.keeper ? keeper_d : enemy_d;
      if (d < near) near = d;
    }
  }
  if (two_in) {
    c.contested++;
    return;
  }
  if (!in_team.empty() && in_team != a.keeper) {
    if (!a.keeper.empty()) c.changes++;
    if (own) a.keeper = in_team;  // the keeper moves once, on the spot's own pass
    keeper_d = 0;
    enemy_d = 1e30f;
    for (const AreaPlayer& p : live) {
      if (p.team == in_team) continue;
      const ge::shooter::Vec3f v = p.at - a.at;
      enemy_d = std::fmin(enemy_d, std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z));
    }
  }
  if (a.keeper.empty() && in_team.empty()) {
    c.unkept++;
    return;
  }
  if (own) {
    const std::string& k = a.keeper;
    bool found = false;
    for (auto& [team, n] : a.kept)
      if (team == k) {
        n++;
        found = true;
      }
    if (!found) a.kept.emplace_back(k, 1);
  }
  if (keeper_d < kAreaNear)
    c.covered++;
  else
    c.open++;
  if (enemy_d < keeper_d) c.enemy_nearer++;
  if (keeper_d < 1e29f) c.keeper_near[static_cast<size_t>(area_band(keeper_d))]++;
  if (enemy_d < 1e29f) c.enemy_near[static_cast<size_t>(area_band(enemy_d))]++;
}

// One sample `dt` s after the last: every area's state into `by_kind[area.kind]` and its own.
template <size_t N>
void areas_update(std::vector<AreaSpot>& areas, std::array<AreaCount, N>& by_kind,
                  const std::vector<AreaPlayer>& live, double dt) {
  for (AreaSpot& a : areas) {
    if (a.kind < 0 || static_cast<size_t>(a.kind) >= N) continue;
    area_sample(a, by_kind[static_cast<size_t>(a.kind)], live, dt, false);
    area_sample(a, a.count, live, dt, true);
  }
}

// The recording read: each area's main keeper's share of its kept time into its count.
template <size_t N>
void areas_end(std::vector<AreaSpot>& areas, std::array<AreaCount, N>& by_kind) {
  for (AreaSpot& a : areas) {
    if (a.kind < 0 || static_cast<size_t>(a.kind) >= N) continue;
    int64_t all = 0, most = 0;
    for (const auto& [team, n] : a.kept) {
      all += n;
      most = n > most ? n : most;
    }
    if (all == 0) continue;
    const double share = static_cast<double>(most) / static_cast<double>(all);
    by_kind[static_cast<size_t>(a.kind)].main_kept += share;
    by_kind[static_cast<size_t>(a.kind)].main_n++;
    a.count.main_kept += share;
    a.count.main_n++;
  }
}

// Each spot's count added into `into` by its kind and place (a map with two of a kind told apart).
inline void areas_merge(std::vector<AreaSpot>& into, const std::vector<AreaSpot>& from) {
  for (const AreaSpot& a : from) {
    AreaSpot* same = nullptr;
    for (AreaSpot& b : into)
      if (b.kind == a.kind && std::fabs(b.at.x - a.at.x) < 1 && std::fabs(b.at.y - a.at.y) < 1 &&
          std::fabs(b.at.z - a.at.z) < 1)
        same = &b;
    if (same == nullptr) {
      into.push_back({a.kind, a.at, {}, {}, {}});
      same = &into.back();
    }
    add(same->count, a.count);
  }
}

}  // namespace shooters_edge::labs
