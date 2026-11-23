// Ground Zero's weapons in the hand (q2_gz.hpp): the fire runs that are not the base game's one
// shot a frame range, and the weapon a dry body turns to.
//
// ge:summary Ground Zero's fire runs: chainfist swings, ETF and plasma loops, the tesla's throw
// ge:keywords q2 ground-zero hand fire-run chainfist swing etf-rifle plasma-beam tesla throw
// ge:keywords best-weapon
// ge:skill shooter
// ge:test tests/shooters_edge_q2_gz_test.cpp
#include <algorithm>

#include "core/prng.hpp"
#include "shooters_edge/server/q2_gz.hpp"

namespace shooters_edge::server {

namespace {

int ammo(const SqBody& b, SqAmmo a) { return b.ammo[static_cast<int>(a)]; }

// Whether `body` has a shot of `w` left (`infinite`: always).
bool loaded(const SqBody& b, int w, bool infinite) {
  const SqWeaponRow& r = kSqWeapons[w];
  return infinite || r.per_shot == 0 || ammo(b, r.ammo) >= r.per_shot;
}

// One shot's ammo taken (the disruptor takes its round even with infinite ammo).
void take(SqBody& b, int w, bool infinite) {
  const SqWeaponRow& r = kSqWeapons[w];
  if (r.per_shot > 0 && (!infinite || w == int(SqWeapon::disruptor)))
    b.ammo[static_cast<int>(r.ammo)] -= r.per_shot;
}

// Out of ammo in the middle of a run: the click (once a second at most) and the best weapon next.
void dry(SqHand& h, const SqBody& b, double now, SqStep& out) {
  if (now >= h.click_at + 1) {
    out.click = true;
    h.click_at = now;
  }
  const int best = gz_best_weapon(b);
  if (best != h.weapon) h.pending = best;
}

// The chainfist's swings: A from frame 6, B from 14, C from 24; their last frames 13, 23, 32.
constexpr int kSwingFrom[3] = {6, 14, 24};
constexpr int kSwingTo[3] = {13, 23, 32};

bool chainfist_hit(int f) { return f == 8 || f == 9 || (f >= 16 && f <= 18) || f == 30 || f == 31; }

// The next swing: the first about a third each (A a little more), after one the other two, the
// first of them twice as often. Drawn off the level time, never the rules' stream.
int next_swing(double now, int last, bool first) {
  ge::core::Rng r = ge::core::make_rng(static_cast<uint32_t>(now * 10 + 0.5) * 2654435761u + 0xc4a1u);
  const double x = ge::core::range(r, 0.0, 1.0);
  if (first) return x < 0.34 ? 0 : x < 0.67 ? 1 : 2;
  const int a = last == 0 ? 1 : 0, b = last == 2 ? 1 : 2;
  return x < 2.0 / 3.0 ? a : b;
}

}  // namespace

int gz_best_weapon(const SqBody& body) {
  if (sq_can_fire(body, int(SqWeapon::railgun))) return int(SqWeapon::railgun);
  if (sq_can_fire(body, int(SqWeapon::plasma_beam))) return int(SqWeapon::plasma_beam);
  constexpr SqWeapon kRest[] = {SqWeapon::etf_rifle, SqWeapon::chaingun, SqWeapon::machinegun,
                                SqWeapon::super_shotgun, SqWeapon::shotgun};
  for (SqWeapon w : kRest)
    if (sq_can_fire(body, int(w))) return int(w);
  return int(SqWeapon::blaster);
}

SqStep gz_hand_fire(SqHand& h, SqBody& body, bool attack, double now, bool infinite) {
  SqStep out;
  const int w = h.weapon;
  const SqWeaponRow& r = kSqWeapons[w];
  const int f = h.frame;
  switch (static_cast<SqWeapon>(w)) {
    case SqWeapon::chainfist: {
      if (f == r.last_activate + 1) {  // the press: the first swing
        h.swing = next_swing(now, 0, true);
        h.frame = kSwingFrom[h.swing];
        return out;
      }
      if (chainfist_hit(f)) out.shots = 1;
      const bool end = f == kSwingTo[0] || f == kSwingTo[1] || f == kSwingTo[2];
      if (end && attack) {  // held: another swing, never the one just played
        h.swing = next_swing(now, h.swing, false);
        h.frame = kSwingFrom[h.swing];
      } else if (end) {  // let go: to the idle frames
        h.state = SqHandState::ready;
        h.frame = r.last_fire + 1;
      } else {
        h.frame++;
      }
      return out;
    }
    case SqWeapon::etf_rifle:
    case SqWeapon::plasma_beam: {
      const bool etf = w == int(SqWeapon::etf_rifle);
      const int loop_at = r.last_fire + 1;  // 8 or 13: back to the first fire frame while held
      int at = f;
      if (at == loop_at) {
        if (!attack || !loaded(body, w, infinite)) {
          h.state = SqHandState::ready;
          return out;
        }
        at = r.fire_from;
      }
      if (at >= r.fire_from && at <= r.fire_to) {
        if (!loaded(body, w, infinite)) {  // dry in the run: to the idle frames
          h.frame = loop_at;
          h.state = SqHandState::ready;
          if (etf) dry(h, body, now, out);
          return out;
        }
        out.shots = 1;
        take(body, w, infinite);
        h.swing = at - r.fire_from;  // the ETF rifle's barrel: 0 the right one, 1 the left
      }
      h.frame = at + 1;
      if (at > r.last_fire) h.state = SqHandState::ready;
      return out;
    }
    case SqWeapon::tesla: {
      if (f == 1) {  // held on frame 1 (3.2 s at most), thrown on 2 once let go
        if (h.fuse_at <= 0) h.fuse_at = now + kSqGrenadeFuse;
        if (attack && now < h.fuse_at) return out;
        const double held = now - (h.fuse_at - kSqGrenadeFuse);
        h.throw_speed = std::min(400.0f + float(held - 0.2) * (400.0f / 3.0f), 800.0f);
        h.fuse_at = 0;
        h.frame = 2;
        out.shots = 1;
        take(body, w, infinite);
        return out;
      }
      if (f >= r.last_fire) {
        h.state = SqHandState::ready;
        h.frame = r.last_fire + 1;
      } else {
        h.frame++;
      }
      return out;
    }
    default:  // the prox launcher and the disruptor: one shot on their fire frame
      if (f > r.last_fire) {
        h.state = SqHandState::ready;
        return out;
      }
      if (f >= r.fire_from && f <= r.fire_to) {
        out.shots = 1;
        take(body, w, infinite);
      }
      h.frame++;
      return out;
  }
}

}  // namespace shooters_edge::server
