// The aim help, defined: each target's miss at its depth on the two axes, the nearest within the
// leeway, the bent view and a shot's line turned with it.
//
// ge:summary The aim help defined: misses by axis, the target chosen, the bend applied
// ge:keywords aim-help aim_help aim_bend_dir leeway
// ge:skill shooter
// ge:test tests/shooter_aim_help_test.cpp
#include "shooter/aim_help.hpp"

#include <vector>

#include "shooter/server.hpp"
#include "shooter/trace.hpp"

namespace ge::shooter {

namespace {

float absf(float v) { return v < 0 ? -v : v; }

// The shift that brings `v` inside [lo + inset, hi - inset] (the middle when the span is too
// narrow); the miss past [lo, hi] in `miss`.
float shift_into(float v, float lo, float hi, float& miss) {
  miss = v < lo ? lo - v : v > hi ? v - hi : 0;
  const float inset = (hi - lo) * 0.5f < kAimInset ? (hi - lo) * 0.5f : kAimInset;
  if (v < lo + inset) return lo + inset - v;
  if (v > hi - inset) return hi - inset - v;
  return 0;
}

}  // namespace

AimBend aim_help(Vec3f eye, Vec3f forward, float range, std::span<const AimTarget> targets,
                 AimLeeway leeway) {
  AimBend best;
  // Sideways is the plan's right of the view; straight up or down has none.
  Vec3f right{forward.y, -forward.x, 0};
  if (normalize(right) < 1e-4f) return best;
  float best_turn = 0;
  for (const AimTarget& t : targets) {
    const Vec3f centre = (t.absmin + t.absmax) * 0.5f;
    const float depth = dot(centre - eye, forward);
    if (depth <= 0 || depth > range) continue;
    const Vec3f at = mad(eye, depth, forward);  // where the line is at the target's depth
    // Across: the box's half width along `right`, against the line's offset from its centre.
    const Vec3f half = (t.absmax - t.absmin) * 0.5f;
    const float width = absf(right.x) * half.x + absf(right.y) * half.y;
    const float off = dot(at - centre, right);
    float miss_x = 0, miss_y = 0;
    const float dx = shift_into(off, -width, width, miss_x);
    const float dz = shift_into(at.z, t.absmin.z, t.absmax.z, miss_y);
    if (miss_x == 0 && miss_y == 0) {
      best = AimBend{};
      best.direct = true;
      best.target = t.ent;
      return best;
    }
    if (miss_x > leeway.across || miss_y > leeway.updown) continue;
    // Only the axis that misses moves.
    const float sx = miss_x > 0 ? dx : 0, sz = miss_y > 0 ? dz : 0;
    const float turn = (sx * sx + sz * sz) / (depth * depth);
    if (best.bent && turn >= best_turn) continue;
    best_turn = turn;
    best.bent = true;
    best.target = t.ent;
    best.across = sx;
    best.updown = sz;
    best.aim = at + right * sx + Vec3f{0, 0, sz};
  }
  if (best.bent) {
    best.forward = best.aim - eye;
    normalize(best.forward);
  }
  return best;
}

Vec3f aim_bend_dir(Vec3f dir, Vec3f forward, Vec3f bent) {
  Vec3f d = dir + (bent - forward);
  normalize(d);
  return d;
}

namespace {

// The box `lo`..`hi` at time `t` along a mover's own motion.
void mover_at(const AimMover& m, float t, float target_gravity, Vec3f& lo, Vec3f& hi) {
  Vec3f d = m.velocity * t;
  if (m.airborne) d.z -= 0.5f * target_gravity * t * t;
  if (m.absmin.z + d.z < m.floor) d.z = m.floor - m.absmin.z;
  lo = m.absmin + d;
  hi = m.absmax + d;
}

// The missile's place at time `t`.
Vec3f flight_at(const LeadShot& s, float t) {
  Vec3f p = mad(s.origin, t, s.velocity);
  p.z -= 0.5f * s.gravity * t * t;
  return p;
}

}  // namespace

LeadBend aim_lead(const LeadShot& shot, Vec3f view_forward, std::span<const AimMover> targets,
                  AimLeeway leeway, float target_gravity) {
  LeadBend best;
  Vec3f right{view_forward.y, -view_forward.x, 0};
  if (normalize(right) < 1e-4f) return best;
  float best_turn = 0;
  for (const AimMover& m : targets) {
    // Where the missile passes his depth along the view: the first sample past it.
    float prev = 0, t_hit = -1;
    for (float t = 0; t <= shot.max_time; t += kLeadStep) {
      Vec3f lo, hi;
      mover_at(m, t, target_gravity, lo, hi);
      const float depth = dot(flight_at(shot, t) - (lo + hi) * 0.5f, view_forward);
      if (t > 0 && prev < 0 && depth >= 0) {
        t_hit = t;
        break;
      }
      prev = depth;
    }
    if (t_hit < 0) continue;
    Vec3f lo, hi;
    mover_at(m, t_hit, target_gravity, lo, hi);
    const Vec3f centre = (lo + hi) * 0.5f;
    const Vec3f at = flight_at(shot, t_hit);
    const Vec3f half = (hi - lo) * 0.5f;
    const float width = absf(right.x) * half.x + absf(right.y) * half.y;
    float miss_x = 0, miss_y = 0;
    shift_into(dot(at - centre, right), -width, width, miss_x);
    shift_into(at.z, lo.z, hi.z, miss_y);
    if (miss_x == 0 && miss_y == 0) return LeadBend{};  // it meets him as launched
    if (miss_x > leeway.across || miss_y > leeway.updown) continue;
    const float turn = (miss_x * miss_x + miss_y * miss_y) / (t_hit * t_hit);
    if (best.bent && turn >= best_turn) continue;
    best_turn = turn;
    best.bent = true;
    best.target = m.ent;
    best.time = t_hit;
  }
  if (!best.bent) return best;
  const AimMover* m = nullptr;
  for (const AimMover& c : targets)
    if (c.ent == best.target) m = &c;
  Vec3f lo, hi;
  if (shot.gravity == 0) {
    // A straight missile keeps its speed: the meeting time found again from where he will be.
    const float speed = length(shot.velocity);
    float t = best.time;
    for (int k = 0; k < 8; k++) {
      mover_at(*m, t, target_gravity, lo, hi);
      t = length((lo + hi) * 0.5f - shot.origin) / speed;
    }
    mover_at(*m, t, target_gravity, lo, hi);
    best.at = (lo + hi) * 0.5f;
    best.time = t;
    Vec3f d = best.at - shot.origin;
    normalize(d);
    best.velocity = d * speed;
  } else {
    // A thrown one: the arc that lands on his middle after the same time.
    const float t = best.time;
    mover_at(*m, t, target_gravity, lo, hi);
    best.at = (lo + hi) * 0.5f;
    best.velocity = (best.at - shot.origin) * (1 / t);
    best.velocity.z += 0.5f * shot.gravity * t;
  }
  return best;
}

int aim_help_line(Server& sv, int shooter, Vec3f start, Vec3f& end, AimLeeway leeway,
                  uint32_t friends) {
  if (shooter <= 0 || shooter > kMaxClients) return -1;
  if (leeway.across <= 0 && leeway.updown <= 0) return -1;
  const Edict& me = sv.edicts[static_cast<size_t>(shooter)];
  const Vec3f eye = me.origin + me.view_ofs;
  const Vec3f reach = start - eye;
  if (dot(reach, reach) > kShotReach * kShotReach) return -1;
  Vec3f dir = end - start;
  const float len = normalize(dir);
  if (len <= 0) return -1;
  const Vec3f forward = angle_vectors(me.v_angle).forward;
  if (dot(dir, forward) < kShotCone) return -1;
  AimTarget targets[kMaxClients];
  int n = 0;
  const int seats = static_cast<int>(sv.clients.size()) < kMaxClients
                        ? static_cast<int>(sv.clients.size())
                        : kMaxClients;
  for (int slot = 0; slot < seats; slot++) {
    const int e = slot + 1;
    if (e == shooter || ((friends >> slot) & 1) != 0) continue;
    const ClientSlot& c = sv.clients[slot];
    if (!c.active || c.spectator || static_cast<size_t>(e) >= sv.edicts.size()) continue;
    const Edict& d = sv.edicts[static_cast<size_t>(e)];
    if (d.free || d.health <= 0 || d.solid == kSolidNot) continue;
    targets[n++] = AimTarget{e, d.absmin, d.absmax};
  }
  const AimBend b =
      aim_help(eye, forward, len + kShotReach, {targets, static_cast<size_t>(n)}, leeway);
  if (!b.bent) return -1;
  const HullTrace seen = trace_move(sv, eye, {}, {}, b.aim, kTraceNormal, shooter);
  if (seen.fraction < 1 && seen.ent != b.target) return -1;
  end = mad(start, len, aim_bend_dir(dir, forward, b.forward));
  return b.target;
}

int aim_help_missile(Server& sv, int missile, AimLeeway leeway, uint32_t friends) {
  if (leeway.across <= 0 && leeway.updown <= 0) return -1;
  const Edict& mis = sv.edicts[static_cast<size_t>(missile)];
  const int shooter = mis.owner;
  if (shooter <= 0 || shooter > kMaxClients) return -1;
  const bool thrown = mis.movetype == kMovetypeToss || mis.movetype == kMovetypeBounce;
  if (!thrown && mis.movetype != kMovetypeFly && mis.movetype != kMovetypeFlyMissile) return -1;
  const Edict& me = sv.edicts[static_cast<size_t>(shooter)];
  AimMover targets[kMaxClients];
  int n = 0;
  const int seats = static_cast<int>(sv.clients.size()) < kMaxClients
                        ? static_cast<int>(sv.clients.size())
                        : kMaxClients;
  for (int slot = 0; slot < seats; slot++) {
    const int e = slot + 1;
    if (e == shooter || ((friends >> slot) & 1) != 0) continue;
    const ClientSlot& c = sv.clients[slot];
    if (!c.active || c.spectator || static_cast<size_t>(e) >= sv.edicts.size()) continue;
    const Edict& d = sv.edicts[static_cast<size_t>(e)];
    if (d.free || d.health <= 0 || d.solid == kSolidNot) continue;
    AimMover m{e, d.absmin, d.absmax, d.velocity, (d.flags & kFlagOnGround) == 0};
    if (m.airborne) {  // the ground under him: where his fall stops
      const HullTrace down = trace_move(sv, d.origin, d.mins, d.maxs, d.origin + Vec3f{0, 0, -4096},
                                        kTraceNoMonsters, e);
      m.floor = down.endpos.z + d.mins.z;
    }
    targets[n++] = m;
  }
  LeadShot shot;
  shot.origin = mis.origin;
  shot.velocity = mis.velocity;
  shot.gravity = thrown ? sv.set.move.gravity : 0;
  const LeadBend b = aim_lead(shot, angle_vectors(me.v_angle).forward,
                              {targets, static_cast<size_t>(n)}, leeway, sv.set.move.gravity);
  if (!b.bent) return -1;
  if (!thrown) {
    const HullTrace seen = trace_move(sv, mis.origin, {}, {}, b.at, kTraceNormal, shooter);
    if (seen.fraction < 1 && seen.ent != b.target) return -1;
  }
  sv.edicts[static_cast<size_t>(missile)].velocity = b.velocity;
  return b.target;
}

}  // namespace ge::shooter
