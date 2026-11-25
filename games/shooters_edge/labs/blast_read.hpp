// THE LAUNCHER FIGHT, READ OFF A RECORDING (M18r.md 18b): where the blasts land against the
// players, who is knocked into the air and what follows before he lands, what each death's last
// blow was, and the jumps in a fight; people and bots by one rule (the maintainer: fire at the
// enemy's feet to throw him up or blast him, a direct shot risks missing whole; in the air, a
// timed rocket in the face or the lightning gun to finish him; under the quad, the scattergun
// near).
//
// Read every frame (an effect is one frame's):
// - A BURST (an explosion: a rocket's or a grenade's) is put to the nearest live player within
//   kBlastReach of it, unless he fired a weapon within kBlastOwnS and it burst within kBlastOwnNear
//   of his feet (his own rocket jump). It is BODY (within kBlastBody of his box), FEET (at the
//   height of his feet, kBlastFloorDz, while he stands), AIR (he is off the ground) or NEAR (a
//   wall, a ceiling, higher than his feet).
// - A KNOCK-UP: a player off the ground rising past kKnockRise within kKnockS of a burst put to
//   him. Followed until he stands again or kKnockFollowS: ANOTHER burst put to him in the air, a
//   BEAM ending within kBeamNear of him (the lightning gun), his DEATH in the air.
// - A DEATH: its last blow within kBlowS before it (a burst put to him, a beam's end near him,
//   pellets within kPelletNear: a scattergun's), whether he was in the air, whether the killer
//   (the slot whose frags rose that frame) held the quad.
// - A JUMP: a player on the ground leaving it rising past kJumpRise with no burst put to him; in a
//   fight when an enemy stood in his clear line within the last second (the caller's).
//
// ge:summary Lab helper: blasts at a player's body, feet or in the air, knock-ups and air finishes
// ge:keywords blast feet knock-up air-finish splash direct-hit rocket lightning quad-scattergun
// ge:keywords fight-jumps se-people launcher-fight
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

#include "shooter/hull.hpp"

namespace shooters_edge::labs {

constexpr float kBlastReach = 160;     // a burst this near a player's box is put to him, units
constexpr float kBlastBody = 12;       // within this of his box: a hit on the body
constexpr float kBlastFloorDz = 20;    // within this above his feet: the floor at his feet
constexpr double kBlastOwnS = 0.25;    // he fired this lately ...
constexpr float kBlastOwnNear = 96;    // ... and it burst this near his feet: his own jump
constexpr float kKnockRise = 200;      // rising faster than this, units/s: thrown up
constexpr double kKnockS = 0.15;       // that soon after the burst, s
constexpr double kKnockFollowS = 2.0;  // followed this long at most while off the ground, s
constexpr float kBeamNear = 64;        // a beam ending this near him hits him
constexpr float kPelletNear = 64;      // pellets this near him hit him
constexpr double kBlowS = 0.3;         // a death's last blow within this before it, s
constexpr float kJumpRise = 200;       // leaving the ground rising past this: a jump

// A player as the read wants him this frame.
struct BlastPlayer {
  bool sent = false;  // in this frame's list (a multiview recording leaves players out a while)
  bool live = false, ground = false, quad = false;
  ge::shooter::Vec3f at;  // the box's centre (the protocol's origin)
  double fired = -1e9;    // his last weapon sound
  int frags = 0;
  bool fight = false;  // an enemy in his clear line within the last second (the caller's)
};

// The blast kinds a burst is put down as.
enum BlastKind : uint8_t { kBlastBodyHit, kBlastFeet, kBlastAir, kBlastNear, kBlastKinds };
// A death's last blow.
enum BlowKind : uint8_t { kBlowBlast, kBlowBeam, kBlowPellets, kBlowOther, kBlowKinds };

struct BlastRead {
  int64_t bursts = 0, own = 0, wide = 0;  // every burst; his own jumps'; near nobody
  int64_t by_kind[kBlastKinds] = {};      // the rest, by kind
  int64_t knocks = 0, knock_burst = 0, knock_beam = 0, knock_death = 0;
  int64_t deaths = 0, deaths_air = 0;
  int64_t blow[kBlowKinds] = {}, blow_air[kBlowKinds] = {};
  int64_t quad_kills = 0, quad_blow[kBlowKinds] = {};
  int64_t jumps = 0, fight_jumps = 0;
  double alive_s = 0, fight_s = 0;  // player-seconds alive, and in a fight
};

inline void add(BlastRead& a, const BlastRead& b) {
  a.bursts += b.bursts;
  a.own += b.own;
  a.wide += b.wide;
  for (int k = 0; k < kBlastKinds; k++) a.by_kind[k] += b.by_kind[k];
  a.knocks += b.knocks;
  a.knock_burst += b.knock_burst;
  a.knock_beam += b.knock_beam;
  a.knock_death += b.knock_death;
  a.deaths += b.deaths;
  a.deaths_air += b.deaths_air;
  for (int k = 0; k < kBlowKinds; k++)
    a.blow[k] += b.blow[k], a.blow_air[k] += b.blow_air[k], a.quad_blow[k] += b.quad_blow[k];
  a.quad_kills += b.quad_kills;
  a.jumps += b.jumps;
  a.fight_jumps += b.fight_jumps;
  a.alive_s += b.alive_s;
  a.fight_s += b.fight_s;
}

// One player's state across frames.
struct BlastTrack {
  bool was_live = false, was_ground = true;
  ge::shooter::Vec3f was_at;
  double was_t = 0;
  int was_frags = 0;
  double burst_t = -1e9;  // the last burst put to him
  double beam_t = -1e9, pellets_t = -1e9;
  bool knocked = false;  // thrown up, not yet down
  double knock_t = 0;
  bool knock_burst = false, knock_beam = false;
};

// A frame's effect as the read wants it.
struct BlastEffect {
  int kind = 0;  // 0 a burst, 1 a beam (its end), 2 pellets
  ge::shooter::Vec3f at;
};

inline float blast_box_dist(ge::shooter::Vec3f p, ge::shooter::Vec3f c) {
  // The box: 16 each way across, 24 under the centre, 32 over it.
  const float dx = std::fmax(std::fabs(p.x - c.x) - 16.f, 0.f);
  const float dy = std::fmax(std::fabs(p.y - c.y) - 16.f, 0.f);
  const float dz = p.z < c.z - 24 ? c.z - 24 - p.z : p.z > c.z + 32 ? p.z - c.z - 32 : 0.f;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// One frame at `t`: `players` and `tracks` by slot, the frame's `effects`.
inline void blast_frame(std::vector<BlastTrack>& tracks, BlastRead& r, double t,
                        const std::vector<BlastPlayer>& players,
                        const std::vector<BlastEffect>& effects) {
  const size_t n = players.size();
  // The effects put to the players.
  for (const BlastEffect& e : effects) {
    int best = -1;
    float near = e.kind == 0 ? kBlastReach : e.kind == 1 ? kBeamNear : kPelletNear;
    for (size_t i = 0; i < n; i++) {
      // the live, and one this very frame's blow killed
      if (!players[i].live && !(players[i].sent && tracks[i].was_live)) continue;
      const float d = blast_box_dist(e.at, players[i].at);
      if (d < near) near = d, best = static_cast<int>(i);
    }
    if (e.kind == 0) r.bursts++;
    if (best < 0) {
      if (e.kind == 0) r.wide++;
      continue;
    }
    const BlastPlayer& p = players[static_cast<size_t>(best)];
    BlastTrack& k = tracks[static_cast<size_t>(best)];
    if (e.kind == 1) {
      k.beam_t = t;
      if (k.knocked) k.knock_beam = true;
      continue;
    }
    if (e.kind == 2) {
      k.pellets_t = t;
      continue;
    }
    const ge::shooter::Vec3f feet{p.at.x, p.at.y, p.at.z - 24};
    const float fx = e.at.x - feet.x, fy = e.at.y - feet.y, fz = e.at.z - feet.z;
    if (t - p.fired <= kBlastOwnS && std::sqrt(fx * fx + fy * fy + fz * fz) < kBlastOwnNear) {
      r.own++;
      continue;
    }
    const int kind = !p.ground                                  ? kBlastAir
                     : near <= kBlastBody && fz > kBlastFloorDz ? kBlastBodyHit
                     : fz <= kBlastFloorDz                      ? kBlastFeet
                                                                : kBlastNear;
    r.by_kind[kind]++;
    if (k.knocked) k.knock_burst = true;
    k.burst_t = t;
  }
  // The killers this frame: their frags rose, holding the quad or not.
  bool quad_killer = false;
  for (size_t j = 0; j < n; j++)
    if (players[j].sent && players[j].frags > tracks[j].was_frags && players[j].quad)
      quad_killer = true;
  for (size_t i = 0; i < n; i++) {
    const BlastPlayer& p = players[i];
    BlastTrack& k = tracks[i];
    if (!p.sent) continue;  // not sent this frame: as he was
    if (k.was_live && !p.live) {
      // A death: its last blow, in the air or not, the killer's quad.
      r.deaths++;
      const bool air = !k.was_ground;
      r.deaths_air += air;
      const int blow = t - k.burst_t <= kBlowS     ? kBlowBlast
                       : t - k.beam_t <= kBlowS    ? kBlowBeam
                       : t - k.pellets_t <= kBlowS ? kBlowPellets
                                                   : kBlowOther;
      r.blow[blow]++;
      r.blow_air[blow] += air;
      if (quad_killer) {
        r.quad_kills++;
        r.quad_blow[blow]++;
      }
      if (k.knocked) {
        r.knock_death++;
        r.knock_burst += k.knock_burst;
        r.knock_beam += k.knock_beam;
      }
      k.knocked = false;
    }
    // A multiview recording holds a player's last state between his updates: his motion is read
    // between his own updates.
    if (p.live && k.was_live && p.at.x == k.was_at.x && p.at.y == k.was_at.y &&
        p.at.z == k.was_at.z) {
      k.was_frags = p.frags;
      continue;
    }
    if (p.live && k.was_live) {
      const double dt = t - k.was_t;
      r.alive_s += dt;
      if (p.fight) r.fight_s += dt;
      const float rise = dt > 0 ? static_cast<float>((p.at.z - k.was_at.z) / dt) : 0.f;
      if (k.was_ground && !p.ground && rise > kKnockRise) {
        if (t - k.burst_t <= kKnockS) {
          r.knocks++;
          k.knocked = true;
          k.knock_t = t;
          k.knock_burst = k.knock_beam = false;
        } else if (rise > kJumpRise) {
          r.jumps++;
          r.fight_jumps += p.fight;
        }
      } else if (!k.was_ground && !p.ground && rise > kKnockRise && t - k.burst_t <= kKnockS &&
                 !k.knocked) {
        // thrown up from the air (a jump met by a burst)
        r.knocks++;
        k.knocked = true;
        k.knock_t = t;
        k.knock_burst = k.knock_beam = false;
      }
      if (k.knocked && (p.ground || t - k.knock_t > kKnockFollowS)) {
        r.knock_burst += k.knock_burst;
        r.knock_beam += k.knock_beam;
        k.knocked = false;
      }
    }
    k.was_live = p.live;
    k.was_ground = p.ground;
    k.was_at = p.at;
    k.was_t = t;
    k.was_frags = p.frags;
  }
}

}  // namespace shooters_edge::labs
