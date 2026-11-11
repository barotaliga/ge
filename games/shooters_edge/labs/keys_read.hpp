// THE KEYS READ OFF A RECORDING: which movement keys a player held, inferred from how his body
// moved, alike for people's recordings and the bots' (a recording carries no commands).
//
// A multiview recording updates a player about 29 times a second, and each update holds two or
// three of his 13 ms commands: a frame's speed is off by up to a third, its DIRECTION is exact. So
// the reads lean on directions and on long windows:
//
// - ON THE GROUND, RUNNING (kReadFrames updates level on the floor at a run's speed, kRunLo to
//   kRunHi: faster is a hop's speed sliding on, not a key's; the view turning less than kTurnMax
//   over them): at a steady run the move's friction and push line
//   the velocity up with the wish, so the velocity's angle off the view is the wish's. Its nearest
//   of the eight key sets (F, FR, R, BR, B, BL, L, FL) is the read, and its angle off that set is
//   `off` (a keyboard's wish lies on the eight; a stick's or a bot's anywhere).
// - IN THE AIR (kReadFrames updates off the floor, the view within kAirView of the velocity): the
//   air adds along the wish only across the velocity, so the path's curve says the side key: right,
//   left, or neither under kCurveMin degrees a second. Forward cannot be read in the air.
// - PART-WAY (kPartFrames updates level on the floor, the speed of the window's two halves within
//   kSteady of each other): a key is down or up, so a person runs at 320 or slows to a stop; a bot
//   pressing forward 0.6 walks at 192 as long as it presses. The share of the steady windows whose
//   speed lies in kPartLo..kPartHi.
//
// Water, a teleport or a blast (a frame past kFastest) and an update gap past kGapHi restart the
// player's window. A SIDE RUN is the same side read again and again (the ground's set or the air's
// curve), a gap of no read under kRunGap carried; its length is how long a side key is held.
// SOCD (both of a pair held, the classic client then moving neither way) cannot be told off the
// body from no key.
//
// ge:summary Lab helper: movement keys inferred off a recording's motion, people and bots alike
// ge:keywords keys keyboard wish key-set octant side-key run part-pressed analog se-people
// ge:keywords inferred-keys strafe forward diagonal curve
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/hull.hpp"

namespace shooters_edge::labs {

constexpr int kKeyReadFrames = 8;     // updates a ground or air read spans
constexpr int kKeyPartFrames = 10;    // updates a part-way read spans
constexpr double kKeyGapHi = 0.08;    // an update gap past this restarts the window, s
constexpr double kKeyFastest = 1000;  // a frame faster than this is a teleport or a blast, u/s
constexpr double kKeyRunLo = 280, kKeyRunHi = 340;  // a ground read runs between, u/s
constexpr double kKeyTurnMax = 3;    // and its view turns less than this over the read, degrees
constexpr double kKeyAirView = 60;   // an air read's view within this of the velocity, degrees
constexpr double kKeyCurveMin = 40;  // an air path curving slower than this: no side, deg/s
constexpr double kKeyRunGap = 0.2;   // no read for this long ends a side run, s
constexpr double kKeySteady = 0.06;  // a part-way read's halves' speeds within this share
constexpr double kKeyPartLo = 80, kKeyPartHi = 280;  // a steady speed between is part-way, u/s
constexpr int kKeyRing = 16;

// The key sets by the wish's angle off forward, 45 degrees apart, rightward positive.
inline constexpr std::array<const char*, 8> kKeySets{"f", "fr", "r", "br", "b", "bl", "l", "fl"};

struct KeysRead {
  std::array<int64_t, 8> ground{};  // ground reads by key set
  std::array<int64_t, 3> air{};     // air reads: left, neither, right
  std::vector<float> off;           // each ground read's angle off its key set, degrees
  int64_t changes = 0;              // the side read changing from one read to the next
  double read_s = 0;                // the time between reads summed, s
  std::vector<float> side_runs;     // each finished side run's length, s
  int64_t steady = 0, part = 0;     // steady ground windows, and those part-way
};

inline void add(KeysRead& a, const KeysRead& b) {
  for (size_t i = 0; i < 8; i++) a.ground[i] += b.ground[i];
  for (size_t i = 0; i < 3; i++) a.air[i] += b.air[i];
  a.off.insert(a.off.end(), b.off.begin(), b.off.end());
  a.changes += b.changes;
  a.read_s += b.read_s;
  a.side_runs.insert(a.side_runs.end(), b.side_runs.begin(), b.side_runs.end());
  a.steady += b.steady;
  a.part += b.part;
}

// One update of a player.
struct KeysFrame {
  double t = 0;
  ge::shooter::Vec3f at;
  float yaw = 0;
  bool ground = false;
};

// One player's last updates and what he was read holding.
struct KeysState {
  std::array<KeysFrame, kKeyRing> ring{};
  int n = 0;                          // updates in the window (0..kKeyRing)
  int head = 0;                       // the next slot
  int side = 2;                       // the side run under way: -1, 0, 1; 2 none read yet
  double side_from = 0, side_to = 0;  // its first and last read
};

inline const KeysFrame& keys_back(const KeysState& k, int i) {  // i updates back, 0 the last
  return k.ring[static_cast<size_t>((k.head - 1 - i + 2 * kKeyRing) % kKeyRing)];
}

// Ends `k`'s side run into `r`.
inline void keys_end_run(KeysState& k, KeysRead& r) {
  if (k.side >= -1 && k.side <= 1 && k.side_to > k.side_from)
    r.side_runs.push_back(static_cast<float>(k.side_to - k.side_from));
  k.side = 2;
}

// The player is gone (dead, out of the match, in water): his window restarts.
inline void keys_reset(KeysState& k, KeysRead& r) {
  keys_end_run(k, r);
  k.n = 0;
}

inline double keys_wrap(double deg) {
  while (deg > 180) deg -= 360;
  while (deg < -180) deg += 360;
  return deg;
}

// The heading of a step, degrees (yaw's convention: 0 along +x, 90 along +y).
inline double keys_heading(ge::shooter::Vec3f a, ge::shooter::Vec3f b) {
  return ge::core::libm::atan2(b.y - a.y, b.x - a.x) * 180 / ge::core::kPi;
}

inline double keys_flat(ge::shooter::Vec3f a, ge::shooter::Vec3f b) {
  return std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
}

// A side read at `t`: the run and the changes.
inline void keys_side(KeysState& k, KeysRead& r, double t, int side) {
  if (k.side != 2 && t - k.side_to < kKeyRunGap) {
    r.read_s += t - k.side_to;
    r.changes += side != k.side;
    if (side != k.side) {
      keys_end_run(k, r);
      k.side = side;
      k.side_from = t;
    }
  } else {
    keys_end_run(k, r);
    k.side = side;
    k.side_from = t;
  }
  k.side_to = t;
}

// One update of a live player out of water: `t` the clock, `at` his origin, `yaw` his view
// (degrees), `ground` whether he stands.
inline void keys_update(KeysState& k, KeysRead& r, double t, ge::shooter::Vec3f at, float yaw,
                        bool ground) {
  if (k.n > 0) {
    const KeysFrame& last = keys_back(k, 0);
    const double gap = t - last.t;
    if (gap <= 0) return;
    if (gap > kKeyGapHi || keys_flat(last.at, at) / gap > kKeyFastest) keys_reset(k, r);
  }
  k.ring[static_cast<size_t>(k.head)] = KeysFrame{t, at, yaw, ground};
  k.head = (k.head + 1) % kKeyRing;
  k.n = std::min(k.n + 1, kKeyRing);
  if (k.n <= kKeyReadFrames) return;
  // The window's updates, all on the floor or all off it.
  bool all_ground = true, all_air = true;
  for (int i = 0; i <= kKeyReadFrames; i++) {
    const KeysFrame& f = keys_back(k, i);
    all_ground = all_ground && f.ground;
    all_air = all_air && !f.ground;
  }
  const KeysFrame& a = keys_back(k, kKeyReadFrames);
  const KeysFrame& b = keys_back(k, 0);
  const double span = b.t - a.t;
  if (all_ground) {
    bool level = true;
    for (int i = 0; i < kKeyReadFrames; i++)
      level = level && std::fabs(keys_back(k, i).at.z - keys_back(k, i + 1).at.z) < 1;
    const double speed = keys_flat(a.at, b.at) / span;
    double turn = 0;
    for (int i = 0; i < kKeyReadFrames; i++)
      turn += std::fabs(keys_wrap(keys_back(k, i).yaw - keys_back(k, i + 1).yaw));
    if (level && speed > kKeyRunLo && speed < kKeyRunHi && turn < kKeyTurnMax) {
      // The last half's heading against the view in its middle.
      const KeysFrame& m = keys_back(k, kKeyReadFrames / 2);
      const double ang = keys_wrap(m.yaw - keys_heading(m.at, b.at));  // rightward positive
      double a360 = ang < 0 ? ang + 360 : ang;
      const int oct = static_cast<int>(std::floor(a360 / 45 + 0.5)) % 8;
      double off = std::fabs(a360 - oct * 45.0);
      if (off > 180) off = 360 - off;
      r.ground[static_cast<size_t>(oct)]++;
      r.off.push_back(static_cast<float>(off));
      const int side = (oct >= 1 && oct <= 3) ? 1 : (oct >= 5 && oct <= 7) ? -1 : 0;
      keys_side(k, r, b.t, side);
    }
    if (level && k.n >= kKeyPartFrames) {
      bool flat = true;
      for (int i = 0; i < kKeyPartFrames - 1; i++) {
        const KeysFrame& f = keys_back(k, i);
        flat = flat && f.ground && std::fabs(f.at.z - keys_back(k, i + 1).at.z) < 1;
      }
      const KeysFrame& p0 = keys_back(k, kKeyPartFrames - 1);
      const KeysFrame& pm = keys_back(k, (kKeyPartFrames - 1) / 2);
      if (flat && p0.ground && pm.t > p0.t && b.t > pm.t) {
        const double s0 = keys_flat(p0.at, pm.at) / (pm.t - p0.t);
        const double s1 = keys_flat(pm.at, b.at) / (b.t - pm.t);
        const double mean = (s0 + s1) / 2;
        if (mean > 40 && std::fabs(s1 - s0) < kKeySteady * mean) {
          r.steady++;
          r.part += mean > kKeyPartLo && mean < kKeyPartHi;
        }
      }
    }
  } else if (all_air) {
    // The path's curve: the heading of the window's first half against its last half's.
    const KeysFrame& m = keys_back(k, kKeyReadFrames / 2);
    if (keys_flat(a.at, m.at) < 1 || keys_flat(m.at, b.at) < 1) return;
    const double h0 = keys_heading(a.at, m.at), h1 = keys_heading(m.at, b.at);
    const double curve = keys_wrap(h1 - h0) / (span / 2);  // leftward positive, deg/s
    if (std::fabs(keys_wrap(m.yaw - h1)) > kKeyAirView) return;
    const int side = curve > kKeyCurveMin ? -1 : curve < -kKeyCurveMin ? 1 : 0;
    r.air[static_cast<size_t>(side + 1)]++;
    keys_side(k, r, b.t, side);
  }
}

// The p-th quantile of `v` (sorted in place), 0 when empty.
inline double keys_quantile(std::vector<float>& v, double p) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  const size_t i = std::min(v.size() - 1, static_cast<size_t>(p * static_cast<double>(v.size())));
  return v[i];
}

}  // namespace shooters_edge::labs
