// The pilot at a gap trick (pilot_gaps.hpp).
//
// ge:summary The pilot at a gap trick: the gap in the stretch, the curve and the run to its lip
// ge:keywords pilot gap-trick gap_in gap_curve_near gap_curve gap-circle gap_run gap_lands
// ge:keywords run-to-the-lip ground-strafe rollout
// ge:skill shooter bots
// ge:test tests/shooter_nav_gaps_test.cpp
#include "shooter/pilot_gaps.hpp"

#include <iterator>

#include "shooter/circle_jump.hpp"
#include "shooter/nav_gaps.hpp"
#include "shooter/pilot_parts.hpp"
#include "shooter/pmove.hpp"

namespace ge::shooter::pilot_parts {
namespace {

constexpr float kGapCurveHarm = 96;    // and not over lava or slime this near its middle
constexpr float kGapCurveSpeed = 0;    // from a walk this fast, units/s
constexpr int kGapRunMost = 40;        // a run to the lip looked ahead this many commands
constexpr float kGapRunWindow = 120;   // a jump tried from each place this near the lip, units
constexpr float kGapRunPast = 24;      // and none this far past it along the jump
constexpr size_t kGapCurveLinks = 12;  // the gap within this many links ahead (nodes ~32 apart)

bool over_harm(const Pilot& p, Vec3f S, Vec3f T) {
  return nav_harm_near(*p.g, {(S.x + T.x) * 0.5f, (S.y + T.y) * 0.5f, S.z}, kGapCurveHarm);
}

}  // namespace

bool gap_lands(const MoveWorld& world, const MoveSettings& move, Vec3f at, Vec3f velocity, Vec3f T,
               int msec) {
  const JumpFlight f = fly_jump(world, move, at, velocity, T, msec);
  const float dz = f.end.z - T.z;
  return f.landed && across(f.end, T) <= kGapLand && dz <= kGapUp && dz >= -kGapUp;
}

int32_t gap_in(const Pilot& p, int32_t end, size_t from) {
  for (size_t j = from > p.k ? from : p.k; j + 1 < p.route.size() && j < static_cast<size_t>(end);
       j++) {
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
  now = gap_lands(*s.world, move, s.origin, s.velocity, T, msec);
  const float vh = across({}, s.velocity) > 1 ? heading(s.velocity.x, s.velocity.y) : p.yaw;
  err = wrap(yaw_at(s.origin, T) - vh);
  return true;
}

GapRun gap_run(const Pilot& p, const PilotSense& s, int msec) {
  GapRun out;
  const size_t gk = static_cast<size_t>(p.gap_k);
  if (!p.set.gap_circle || s.world == nullptr || p.gap_k < 0 || gk > p.k + kGapCurveLinks)
    return out;
  const Vec3f S = node(*p.g, p.route[gk]).origin, T = node(*p.g, p.route[gk + 1]).origin;
  if (across(s.origin, S) > kGapCurveFrom || over_harm(p, S, T)) return out;
  Vec3f dir{T.x - S.x, T.y - S.y, 0};
  normalize(dir);
  MoveSettings move;
  move.maxspeed = s.maxspeed;
  const float dt = static_cast<float>(msec) * 0.001f;
  const auto lands = [&](const MoveState& st) {
    return gap_lands(*s.world, move, st.origin, st.velocity, T, msec);
  };
  // Each way on: straight on (side 0), or a curve on the ground to the left or the right, the
  // wish at the best angle for speed (circle_jump.hpp). The first that brings a place a jump
  // from lands across wins; the soonest such place among them.
  struct Way {
    int8_t side;  // the first command's side key (0 straight at the lip)
    int flip;     // the side key swapped every this many commands (0 never: one curve)
  };
  constexpr Way kWays[] = {{0, 0}, {-1, 0}, {1, 0}, {-1, 4}, {1, 4}, {-1, 7}, {1, 7}};
  constexpr int kWayN = static_cast<int>(std::size(kWays));
  for (int k = -1; k < kWayN; k++) {
    // The way under way first (it keeps a tie), then the rest.
    const int w = k < 0 ? p.gap_way : k;
    if (w < 0 || (k >= 0 && w == p.gap_way)) continue;
    const Way& way = kWays[w];
    // The way under way goes on from where it is (its side swaps keep their beat).
    const int from = w == p.gap_way ? p.gap_way_i : 0;
    MoveState st;
    st.origin = s.origin;
    st.velocity = s.velocity;
    st.maxspeed = s.maxspeed;
    st.ground = 0;
    float first_yaw = 0;
    int8_t first_side = 0;
    // The keys and the view go through a copy of the pilot's own hands (their travel and turn
    // take commands: a side key swapped too often never gets there).
    ai::Hands hands = p.hands;
    float last_yaw = p.yaw;
    for (int i = 0; i < kGapRunMost && (!out.found || i + 1 < out.n); i++) {
      const int8_t side = way.flip > 0 && ((i + from) / way.flip) % 2 == 1
                              ? static_cast<int8_t>(-way.side)
                              : way.side;
      const float v = across({}, st.velocity);
      const float vh = v > 1 ? heading(st.velocity.x, st.velocity.y) : p.yaw;
      const float yaw = side == 0
                            ? yaw_at(st.origin, S)
                            : vh - static_cast<float>(side) * (circle_angle(v, dt, move) - 45.f);
      if (i == 0) first_yaw = yaw, first_side = side;
      const double wanted[4] = {unwrap(yaw, last_yaw), 1, static_cast<double>(side), 0};
      double pressed[4];
      hands.step(wanted, pressed);
      last_yaw = static_cast<float>(pressed[0]);
      MoveCmd c;
      c.msec = static_cast<uint8_t>(msec);
      c.angles = {0, wrap(last_yaw), 0};
      c.forward = static_cast<int16_t>(static_cast<float>(pressed[1]) * move.maxspeed);
      c.side = static_cast<int16_t>(static_cast<float>(pressed[2]) * move.maxspeed);
      const float z0 = st.origin.z;
      player_move(st, c, move, *s.world);
      if (st.ground < 0 || st.origin.z < z0 - kStepHeight) break;  // off the floor
      const float along = dot(Vec3f{st.origin.x - S.x, st.origin.y - S.y, 0}, dir);
      if (along > kGapRunPast) break;
      if (across(st.origin, S) <= kGapRunWindow && lands(st)) {
        if (!out.found || i + 1 < out.n) {
          out.found = true;
          out.n = i + 1;
          out.side = first_side;
          out.flip = way.flip;
          out.way = w;
          out.yaw = first_yaw;
        }
        break;
      }
    }
  }
  return out;
}

}  // namespace ge::shooter::pilot_parts
