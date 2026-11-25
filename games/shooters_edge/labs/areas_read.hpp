// THE KEY AREAS READ OFF A RECORDING: which team holds each key item's area, alike for people's
// recordings and the bots'. Holding is the item's, not where anyone stands (the maintainer's rule):
//
// - HELD: a team takes the key item twice in a row; it holds the area from then on. An enemy
//   walking in changes nothing.
// - CONTESTED: a player of the holding team killed in the area (within kAreaHold of the item) makes
//   it contested until the item has been taken twice in a row by one team: by the enemy (who then
//   holds it) or by the holders again; or, once an enemy has been killed in the area too, until a
//   holder takes it once (the holders back in it, the enemy gone).
// - LOST WITHOUT A FIGHT: the enemy taking it twice in a row while it is held (the holders fled)
//   makes it theirs.
// - UNHELD: no team has taken it twice in a row yet.
//
// A POWERUP's area (the quad, the pent) is worth holding only from a while before it comes back
// (`AreaSpot::lead_s` before its `respawn_s`: the quad 15 of 60, the pent 30 of 300) until it is
// taken; outside that window its samples are not counted (a team holds other items meanwhile).
//
// A pickup's taker is the nearest live player within kTakerNear of the item as it goes (none: the
// pickup is not counted). Each sample also reads the closeness while held: the holding team's
// nearest live player and the other team's, banded by kAreaBand (`keeper-near.*`, `enemy-near.*`),
// COVERED while a holder is within kAreaNear; and per recording the MAIN HOLDER's share of the held
// time (one team's area: near 1; traded: near 0.5). Distances are straight lines.
//
// ge:summary Lab helper: key areas held, contested or unheld by the item's pickups, people and bots
// ge:keywords key-areas area-control holding holder held contested unheld pickups twice-in-a-row
// ge:keywords closeness covered se-people people-areas map-control
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "shooter/vec.hpp"

namespace shooters_edge::labs {

constexpr float kAreaHold = 400;   // a holder killed this near the item contests the area, units
constexpr float kTakerNear = 160;  // a pickup's taker: the nearest live player this near, units
constexpr float kAreaNear = 768;   // a holder this near covers the area, units
constexpr float kAreaBand = 256;   // the closeness bands' width, units
constexpr int kAreaBands = 6;      // 0-256 .. 1280+, the last open

struct AreaCount {
  int64_t samples = 0, unheld = 0, held = 0, covered = 0, contested = 0;
  int64_t enemy_nearer = 0;  // held, and an enemy nearer the item than any holder
  int64_t changes = 0;       // the holder changed (by two pickups in a row)
  int64_t lost = 0;          // a holder killed in the area: contested from then
  int64_t pickups = 0;       // pickups with a taker
  double seconds = 0;        // the time sampled
  double main_held = 0;      // the main holder's share of the held time, summed over recordings
  int64_t main_n = 0;        // recordings it was held in
  std::array<int64_t, kAreaBands> keeper_near{}, enemy_near{};
};

inline void add(AreaCount& a, const AreaCount& b) {
  a.samples += b.samples;
  a.unheld += b.unheld;
  a.held += b.held;
  a.covered += b.covered;
  a.contested += b.contested;
  a.enemy_nearer += b.enemy_nearer;
  a.changes += b.changes;
  a.lost += b.lost;
  a.pickups += b.pickups;
  a.seconds += b.seconds;
  a.main_held += b.main_held;
  a.main_n += b.main_n;
  for (int k = 0; k < kAreaBands; k++) {
    a.keeper_near[static_cast<size_t>(k)] += b.keeper_near[static_cast<size_t>(k)];
    a.enemy_near[static_cast<size_t>(k)] += b.enemy_near[static_cast<size_t>(k)];
  }
}

// One area as a recording is read.
struct AreaSpot {
  int kind = -1;
  ge::shooter::Vec3f at;
  std::string holder;       // empty: unheld
  bool contested = false;   // a holder killed in it, not yet taken twice in a row since
  bool enemy_fell = false;  // and since then an enemy killed in it (a holder's one pickup ends it)
  std::string streak_team;  // the team of the last pickups in a row
  int streak = 0;           // and how many
  // A powerup's window (`lead_s` > 0): counted only while it is there or due within lead_s.
  float respawn_s = 0, lead_s = 0;
  bool present = true;                                   // the item there at the last sample
  double taken_at = -1e9;                                // when it last went
  std::vector<std::pair<std::string, int64_t>> held_by;  // samples held, by team
  AreaCount count;                                       // this spot's own
};

// A live player at a sample.
struct AreaPlayer {
  ge::shooter::Vec3f at;
  std::string team;
};

inline float area_dist(ge::shooter::Vec3f a, ge::shooter::Vec3f b) {
  const ge::shooter::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

inline int area_band(float d) {
  const int b = static_cast<int>(d / kAreaBand);
  return b < kAreaBands - 1 ? b : kAreaBands - 1;
}

// The item of `a` was taken, with `live` the players then: its taker's team counted toward a hold.
template <size_t N>
void area_pickup(AreaSpot& a, std::array<AreaCount, N>& by_kind,
                 const std::vector<AreaPlayer>& live) {
  const AreaPlayer* taker = nullptr;
  float near = kTakerNear;
  for (const AreaPlayer& p : live) {
    const float d = area_dist(p.at, a.at);
    if (d < near) {
      near = d;
      taker = &p;
    }
  }
  if (taker == nullptr || a.kind < 0 || static_cast<size_t>(a.kind) >= N) return;
  AreaCount* counts[2] = {&by_kind[static_cast<size_t>(a.kind)], &a.count};
  for (AreaCount* c : counts) c->pickups++;
  if (taker->team == a.streak_team) {
    a.streak++;
  } else {
    a.streak_team = taker->team;
    a.streak = 1;
  }
  // Contested, the enemy killed there too: the holders' one pickup takes it back.
  if (a.contested && a.enemy_fell && taker->team == a.holder) {
    a.contested = false;
    return;
  }
  if (a.streak < 2) return;
  if (a.holder != a.streak_team) {
    if (!a.holder.empty())
      for (AreaCount* c : counts) c->changes++;
    a.holder = a.streak_team;
  }
  a.contested = false;
}

// A player of `team` killed at `at`: a holder killed in its area contests it; an enemy killed in a
// contested area lets the holders take it back with one pickup.
template <size_t N>
void areas_death(std::vector<AreaSpot>& areas, std::array<AreaCount, N>& by_kind,
                 const std::string& team, ge::shooter::Vec3f at) {
  for (AreaSpot& a : areas) {
    if (a.holder.empty() || a.kind < 0 || static_cast<size_t>(a.kind) >= N ||
        area_dist(at, a.at) > kAreaHold)
      continue;
    if (a.contested) {
      if (a.holder != team) a.enemy_fell = true;
      continue;
    }
    if (a.holder != team) continue;
    a.contested = true;
    a.enemy_fell = false;
    by_kind[static_cast<size_t>(a.kind)].lost++;
    a.count.lost++;
  }
}

// One area's sample into `c`.
inline void area_sample(const AreaSpot& a, AreaCount& c, const std::vector<AreaPlayer>& live,
                        double dt) {
  c.samples++;
  c.seconds += dt;
  if (a.holder.empty()) {
    c.unheld++;
    return;
  }
  if (a.contested) {
    c.contested++;
    return;
  }
  c.held++;
  float keeper_d = 1e30f, enemy_d = 1e30f;
  for (const AreaPlayer& p : live) {
    float& near = p.team == a.holder ? keeper_d : enemy_d;
    near = std::fmin(near, area_dist(p.at, a.at));
  }
  if (keeper_d < kAreaNear) c.covered++;
  if (enemy_d < keeper_d) c.enemy_nearer++;
  if (keeper_d < 1e29f) c.keeper_near[static_cast<size_t>(area_band(keeper_d))]++;
  if (enemy_d < 1e29f) c.enemy_near[static_cast<size_t>(area_band(enemy_d))]++;
}

// One sample at `t`, `dt` s after the last: every area's state into its kind's count and its own
// (a powerup's only inside its window).
template <size_t N>
void areas_update(std::vector<AreaSpot>& areas, std::array<AreaCount, N>& by_kind,
                  const std::vector<AreaPlayer>& live, double t, double dt) {
  for (AreaSpot& a : areas) {
    if (a.kind < 0 || static_cast<size_t>(a.kind) >= N) continue;
    if (a.lead_s > 0 && !a.present && t < a.taken_at + a.respawn_s - a.lead_s) continue;
    area_sample(a, by_kind[static_cast<size_t>(a.kind)], live, dt);
    area_sample(a, a.count, live, dt);
    if (!a.holder.empty() && !a.contested) {
      bool found = false;
      for (auto& [team, n] : a.held_by)
        if (team == a.holder) {
          n++;
          found = true;
        }
      if (!found) a.held_by.emplace_back(a.holder, 1);
    }
  }
}

// The recording read: each area's main holder's share of its held time into its counts.
template <size_t N>
void areas_end(std::vector<AreaSpot>& areas, std::array<AreaCount, N>& by_kind) {
  for (AreaSpot& a : areas) {
    if (a.kind < 0 || static_cast<size_t>(a.kind) >= N) continue;
    int64_t all = 0, most = 0;
    for (const auto& [team, n] : a.held_by) {
      all += n;
      most = n > most ? n : most;
    }
    if (all == 0) continue;
    const double share = static_cast<double>(most) / static_cast<double>(all);
    by_kind[static_cast<size_t>(a.kind)].main_held += share;
    by_kind[static_cast<size_t>(a.kind)].main_n++;
    a.count.main_held += share;
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
      into.push_back({});
      same = &into.back();
      same->kind = a.kind;
      same->at = a.at;
    }
    add(same->count, a.count);
  }
}

}  // namespace shooters_edge::labs
