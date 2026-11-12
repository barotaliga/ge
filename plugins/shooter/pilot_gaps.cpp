// The pilot at a gap trick (pilot_gaps.hpp).
//
// ge:summary The pilot at a gap trick: the gap in the stretch, the curve into one over water
// ge:keywords pilot gap-trick gap_in gap_curve_near gap_curve gap-circle
// ge:skill shooter bots
// ge:test tests/shooter_pilot_test.cpp
#include "shooter/pilot_gaps.hpp"

#include "shooter/nav_gaps.hpp"
#include "shooter/pilot_parts.hpp"

namespace ge::shooter::pilot_parts {
namespace {

constexpr float kGapCurveFrom = 160;   // a curve into a gap begun this near its start, units
constexpr float kGapCurveHarm = 96;    // and not over lava or slime this near its middle
constexpr float kGapCurveSpeed = 150;  // from a walk this fast, units/s
constexpr size_t kGapCurveLinks = 3;   // the gap within this many links ahead

bool over_harm(const Pilot& p, Vec3f S, Vec3f T) {
  return nav_harm_near(*p.g, {(S.x + T.x) * 0.5f, (S.y + T.y) * 0.5f, S.z}, kGapCurveHarm);
}

}  // namespace

int32_t gap_in(const Pilot& p, int32_t end) {
  for (size_t j = p.k; j + 1 < p.route.size() && j < static_cast<size_t>(end); j++) {
    const NavLink* l = link_of(*p.g, p.route[j], p.route[j + 1]);
    if (l != nullptr && l->gap) return static_cast<int32_t>(j);
  }
  return -1;
}

bool gap_curve_near(const Pilot& p, const PilotSense& s) {
  if (across({}, s.velocity) < kGapCurveSpeed) return false;
  for (size_t j = p.k; j + 1 < p.route.size() && j <= p.k + kGapCurveLinks; j++) {
    const NavLink* l = link_of(*p.g, p.route[j], p.route[j + 1]);
    if (l == nullptr || !l->gap) continue;
    const Vec3f S = node(*p.g, p.route[j]).origin, T = node(*p.g, p.route[j + 1]).origin;
    return across(s.origin, S) <= kGapCurveFrom && !over_harm(p, S, T);
  }
  return false;
}

bool gap_curve(const Pilot& p, const PilotSense& s, int msec, float& err, bool& now) {
  const size_t gk = static_cast<size_t>(p.gap_k);
  if (!p.set.gap_circle || s.world == nullptr || gk > p.k + kGapCurveLinks) return false;
  const Vec3f S = node(*p.g, p.route[gk]).origin, T = node(*p.g, p.route[gk + 1]).origin;
  if (across(s.origin, S) > kGapCurveFrom || over_harm(p, S, T)) return false;
  MoveSettings move;
  move.maxspeed = s.maxspeed;
  const JumpFlight f = fly_jump(*s.world, move, s.origin, s.velocity, T, msec);
  const float dz = f.end.z - T.z;
  now = f.landed && across(f.end, T) <= kGapLand && dz <= kGapUp && dz >= -kGapUp;
  const float vh = across({}, s.velocity) > 1 ? heading(s.velocity.x, s.velocity.y) : p.yaw;
  err = wrap(yaw_at(s.origin, T) - vh);
  return true;
}

}  // namespace ge::shooter::pilot_parts
