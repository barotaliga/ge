// THE GLANCE AT A KEY ITEM'S SPOT, READ OFF A RECORDING: whether a player running past where a key
// item spawns looks at the spot, alike for people's recordings and the bots' (M18r.md § 6b).
//
// Read every 40 Hz tick for each live player and each key spot (the yellow and red armours, the
// mega, the quad, the pent, the rocket launcher; the green armour is not one):
//
// - A PASS starts when the spot comes within kGlanceNear of his eye with a clear line to it (the
//   world's hull 0, to kGlanceUp over the spot's origin), and ends when the line is lost for
//   kGlanceGap, the spot is past kGlanceFar, he dies or teleports. A pass is COUNTED when it lasts
//   kGlanceMinS or more, is run at kGlanceRun or faster (the way across over its time), and never
//   comes within kGlanceVisit of the spot (one who goes to the item visits it, he does not pass).
// - OFF: the spot was off his screen (the bots' 120 by 88.5 degrees) as the pass started. The
//   share of the passes that start off is how often the run itself does not show the spot.
// - GLANCED: of those, the view came within kGlanceOn degrees of the spot during the pass. The
//   first such look is THE glance: how long it stayed on (s), how long after the pass's start it
//   came (s), and the spot's angle off the run then (degrees, flat): one past kGlanceAside is a
//   look ASIDE (a run turning to the item brings it on the view by itself, at a small angle).
// - SHOWN: of those off, the spot came onto his screen at all during the pass (what a bot needs to
//   learn the item: knowledge.cpp learns a spot in the view's cone), and SHOWN ASIDE: it came on
//   while lying kGlanceShownAside or more off his run (a view along the run cannot show it: the
//   view was turned off the run toward it, or swept past it).
//
// SEEN: of every pass counted, the spot on his screen at some tick of it (at the start or later):
// how often a pass tells him whether the item is there. Each spot's own (`spots`, by its place).
//
// Split by whether an enemy was in a clear line within the fight's window (the caller's `fight`)
// and whether the item lay there as the pass started. Each player's own share (passes off and
// glanced) is kept for the spread between players (people's levels mixed).
//
// ge:summary Lab helper: glances at key item spots read off a recording, people and bots alike
// ge:keywords glance item-glance key-spot pass look-aside belief item-spot se-people
// ge:keywords glance-key passes-off spot-off-screen
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/hull.hpp"

namespace shooters_edge::labs {

constexpr float kGlanceNear = 600;       // a pass starts with the spot this near the eye, units
constexpr float kGlanceFar = 700;        // and ends with it past this, units
constexpr float kGlanceUp = 16;          // the spot's point over the item's origin, units
constexpr double kGlanceGap = 0.5;       // the line lost this long ends a pass, s
constexpr double kGlanceMinS = 0.3;      // a pass this long at least is counted, s
constexpr double kGlanceRun = 150;       // and run this fast at least, u/s
constexpr float kGlanceVisit = 96;       // a pass this near the spot is a visit, not counted, units
constexpr float kGlanceOn = 20;          // the view within this of the spot looks at it, degrees
constexpr float kGlanceAside = 30;       // a glance with the spot this far off the run is aside
constexpr float kGlanceShownAside = 60;  // the spot shown this far off the run: the view turned
constexpr float kGlanceJump = 60;        // a tick moving further than this is a teleport, units
constexpr int kGlanceKinds = 7;          // se-people's item kinds (ga, ya, ra, mh, quad, pent, rl)

// One spot's passes counted, those off as they started, seen, shown aside and glanced.
struct GlanceSpot {
  int kind = -1;
  int64_t passes = 0, off = 0, seen = 0, shown_aside = 0, glanced = 0;
};

// One player's pass by one spot.
struct GlancePass {
  bool on = false, off = false, fight = false, there = false, looking = false, shown = false;
  bool shown_aside = false;
  double t0 = 0, last_line = 0, glance_t = -1, glance_len = 0;
  float near = 1e9f, path = 0, aside = 0;
  ge::shooter::Vec3f last;
};

struct GlanceRead {
  std::array<int64_t, kGlanceKinds> passes{}, off{}, glanced{}, aside{}, shown{}, shown_aside{};
  int64_t calm_off = 0, calm_glanced = 0, fight_off = 0, fight_glanced = 0;
  int64_t there_off = 0, there_glanced = 0, gone_off = 0, gone_glanced = 0;
  double live_s = 0;
  std::vector<float> len, first, aside_deg;     // each glance: on (s), after the start (s), off run
  std::vector<std::array<int64_t, 2>> players;  // each player's passes off and glanced
  std::array<int64_t, kGlanceKinds> seen{};     // passes counted with the spot seen at all
  std::map<std::string, GlanceSpot> spots;      // by the spot's place, "x,y,z"
};

inline void add(GlanceRead& a, const GlanceRead& b) {
  for (int k = 0; k < kGlanceKinds; k++) {
    const auto i = static_cast<size_t>(k);
    a.passes[i] += b.passes[i];
    a.off[i] += b.off[i];
    a.glanced[i] += b.glanced[i];
    a.aside[i] += b.aside[i];
    a.shown[i] += b.shown[i];
    a.shown_aside[i] += b.shown_aside[i];
  }
  a.calm_off += b.calm_off;
  a.calm_glanced += b.calm_glanced;
  a.fight_off += b.fight_off;
  a.fight_glanced += b.fight_glanced;
  a.there_off += b.there_off;
  a.there_glanced += b.there_glanced;
  a.gone_off += b.gone_off;
  a.gone_glanced += b.gone_glanced;
  a.live_s += b.live_s;
  a.len.insert(a.len.end(), b.len.begin(), b.len.end());
  a.first.insert(a.first.end(), b.first.begin(), b.first.end());
  a.aside_deg.insert(a.aside_deg.end(), b.aside_deg.begin(), b.aside_deg.end());
  a.players.insert(a.players.end(), b.players.begin(), b.players.end());
  for (int k = 0; k < kGlanceKinds; k++)
    a.seen[static_cast<size_t>(k)] += b.seen[static_cast<size_t>(k)];
  for (const auto& [at, sp] : b.spots) {
    GlanceSpot& o = a.spots[at];
    o.kind = sp.kind;
    o.passes += sp.passes;
    o.off += sp.off;
    o.seen += sp.seen;
    o.shown_aside += sp.shown_aside;
    o.glanced += sp.glanced;
  }
}

// A spot's key in GlanceRead::spots: its place to the unit.
inline std::string glance_spot_key(ge::shooter::Vec3f at) {
  return std::to_string(static_cast<int>(std::lround(at.x))) + "," +
         std::to_string(static_cast<int>(std::lround(at.y))) + "," +
         std::to_string(static_cast<int>(std::lround(at.z)));
}

// The pass `g` ends: counted into `r` (kind `kind`, its spot `at`, the player's own tally `mine`)
// when it was one.
inline void glance_end(GlancePass& g, GlanceRead& r, int kind, ge::shooter::Vec3f at,
                       std::array<int64_t, 2>& mine) {
  if (!g.on) return;
  g.on = false;
  const double dur = g.last_line - g.t0;
  if (dur < kGlanceMinS || g.path / dur < kGlanceRun || g.near < kGlanceVisit) return;
  const auto k = static_cast<size_t>(kind);
  r.passes[k]++;
  r.seen[k] += !g.off || g.shown;
  GlanceSpot& sp = r.spots[glance_spot_key(at)];
  sp.kind = kind;
  sp.passes++;
  sp.seen += !g.off || g.shown;
  if (!g.off) return;
  sp.off++;
  sp.shown_aside += g.shown_aside;
  sp.glanced += g.glance_t >= 0;
  r.off[k]++;
  mine[0]++;
  const bool seen = g.glance_t >= 0;
  (g.fight ? r.fight_off : r.calm_off)++;
  (g.there ? r.there_off : r.gone_off)++;
  r.shown[k] += g.shown;
  r.shown_aside[k] += g.shown_aside;
  if (!seen) return;
  r.glanced[k]++;
  mine[1]++;
  (g.fight ? r.fight_glanced : r.calm_glanced)++;
  (g.there ? r.there_glanced : r.gone_glanced)++;
  r.aside[k] += g.aside >= kGlanceAside;
  r.len.push_back(static_cast<float>(g.glance_len));
  r.first.push_back(static_cast<float>(g.glance_t - g.t0));
  r.aside_deg.push_back(g.aside);
}

// A 40 Hz tick of a live player by one spot: his eye, his view's forward axis and whether the
// spot's point is on his screen (`on_screen`), his way over the last ticks (`run`, flat; zero when
// still), `line` the clear line within kGlanceNear (kGlanceFar while a pass is on) and its length
// `dist`; `fight` and `there` as the header says; `at` the item's place.
inline void glance_tick(GlancePass& g, GlanceRead& r, int kind, ge::shooter::Vec3f at,
                        std::array<int64_t, 2>& mine, double t, ge::shooter::Vec3f origin,
                        ge::shooter::Vec3f eye, ge::shooter::Vec3f forward, ge::shooter::Vec3f spot,
                        bool on_screen, ge::shooter::Vec3f run, bool line, float dist, bool fight,
                        bool there) {
  namespace sh = ge::shooter;
  if (g.on) {
    const float mx = origin.x - g.last.x, my = origin.y - g.last.y;
    const float step = std::sqrt(mx * mx + my * my);
    if (step > kGlanceJump || dist > kGlanceFar || (!line && t - g.last_line > kGlanceGap)) {
      glance_end(g, r, kind, at, mine);
    } else {
      g.path += step;
      g.last = origin;
    }
  }
  if (!g.on) {
    if (!line || dist > kGlanceNear) return;
    g = {};
    g.on = true;
    g.t0 = g.last_line = t;
    g.off = !on_screen;
    g.fight = fight;
    g.there = there;
    g.last = origin;
  }
  if (line) g.last_line = t;
  g.near = std::fmin(g.near, dist);
  const sh::Vec3f h = spot - eye;
  const float rl = std::sqrt(run.x * run.x + run.y * run.y);
  const float fl = std::sqrt(h.x * h.x + h.y * h.y);
  const auto off_run = [&] {
    const double c = rl > 1 && fl > 1 ? (run.x * h.x + run.y * h.y) / (rl * fl) : 1;
    return static_cast<float>(ge::core::libm::acos(std::fmax(-1.0, std::fmin(1.0, c))) * 180 /
                              ge::core::kPi);
  };
  if (on_screen && line && !g.shown) {
    g.shown = true;
    g.shown_aside = off_run() >= kGlanceShownAside;
  }
  const float hl = std::sqrt(sh::dot(h, h));
  const bool on =
      hl > 1 && sh::dot(forward, h) >=
                    hl * static_cast<float>(ge::core::libm::cos(kGlanceOn * ge::core::kPi / 180));
  if (on && g.glance_t < 0) {
    g.glance_t = t;
    g.looking = true;
    g.aside = off_run();
  }
  if (g.looking) {
    if (on)
      g.glance_len = t - g.glance_t;
    else
      g.looking = false;
  }
}

}  // namespace shooters_edge::labs
