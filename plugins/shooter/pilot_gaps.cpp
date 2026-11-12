// The pilot at a gap trick (pilot_gaps.hpp).
//
// ge:summary The pilot at a gap trick: the gap in the stretch, the curve and the run to its lip
// ge:keywords pilot gap-trick gap_in gap_curve_near gap_curve gap-circle gap_run gap_lands
// ge:keywords run-to-the-lip ground-strafe rollout gap_takeoff gap_off gap_lands_late
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
constexpr float kHopShort =
    64;                         // `lipjump`: a hop "this side" of a gap comes down this short of it
constexpr int kGapKeyLate = 2;  // a keyboard's jump key goes down up to this many commands late
// The takeoff: the next commands' places tried, a takeoff no further than this past its start (the
// lip), the ground's friction a second; a gap given up stays off the routes this long.
constexpr int kGapRunOn = 4;
constexpr float kGapPastStart = 24;
constexpr float kGroundFriction = 4;
constexpr double kGapOff_s = 10;
constexpr int kCastMost = 160;        // `hop_cast`: a flight forecast this many commands at most
constexpr float kKeyAxis = 400;       // a keyboard's key: the classic client's axis a key, units/s
constexpr float kKeyPressMin = 0.3f;  // a wish under this share of a run presses no key in the air

bool over_harm(const Pilot& p, Vec3f S, Vec3f T) {
  return nav_harm_near(*p.g, {(S.x + T.x) * 0.5f, (S.y + T.y) * 0.5f, S.z}, kGapCurveHarm);
}

// The side key a keyboard holds now: the last command's curve's, else the side axis's sign.
int8_t held_side(const Pilot& p) {
  if (p.curve_keys_last != kNoCurveKeys) return p.curve_keys_last;
  return static_cast<int8_t>(p.pressed[2] > 0.5 ? 1 : p.pressed[2] < -0.5 ? -1 : 0);
}

}  // namespace

bool gap_lands_late(const Pilot& p, const PilotSense& s, int msec, Vec3f T) {
  if (s.world == nullptr) return false;
  MoveSettings move;
  move.maxspeed = s.maxspeed;
  MoveState st;
  st.origin = s.origin;
  st.velocity = s.velocity;
  st.maxspeed = s.maxspeed;
  st.ground = 0;
  MoveCmd c;
  c.msec = static_cast<uint8_t>(msec);
  c.angles = {0, wrap(p.yaw), 0};
  c.forward = static_cast<int16_t>(move.maxspeed);
  c.side = static_cast<int16_t>(static_cast<float>(held_side(p)) * move.maxspeed);
  for (int k = 1; k <= kGapKeyLate; k++) {
    const float z0 = st.origin.z;
    player_move(st, c, move, *s.world);
    if (st.ground < 0 || st.origin.z < z0 - kStepHeight) return false;  // off the floor
    if (!gap_lands(*s.world, move, st.origin, st.velocity, T, msec, p.set.gap_past,
                   p.set.gap_strafe))
      return false;
  }
  return true;
}

bool gap_lands(const MoveWorld& world, const MoveSettings& move, Vec3f at, Vec3f velocity, Vec3f T,
               int msec, float past, bool strafe) {
  const JumpFlight f = fly_jump(world, move, at, velocity, T, msec, 1.5f, strafe);
  return f.landed && lands_by(at, f.end, T, past);
}

bool lands_by(Vec3f at, Vec3f end, Vec3f T, float past) {
  const float dz = end.z - T.z;
  if (dz > kGapUp || dz < -kGapUp) return false;
  if (across(end, T) <= kGapLand) return true;
  if (past <= 0) return false;
  // On past T along the line from where it jumped: how far past, and how far off the line.
  Vec3f dir{T.x - at.x, T.y - at.y, 0};
  normalize(dir);
  const Vec3f rel{end.x - T.x, end.y - T.y, 0};
  const float on = dot(rel, dir);
  const float off = rel.x * dir.y - rel.y * dir.x;
  return on >= -kGapLand && on <= past && off <= past && off >= -past;
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
  now = gap_lands(*s.world, move, s.origin, s.velocity, T, msec, p.set.gap_past, p.set.gap_strafe);
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
    return gap_lands(*s.world, move, st.origin, st.velocity, T, msec, p.set.gap_past,
                     p.set.gap_strafe);
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
    // Under a keyboard (`curve_keys`) the keys go down whole, a command late: the side key the
    // last command meant (the way under way's own, else the keys the last command held).
    int8_t key_side = held_side(p);
    bool prev_lands = false;  // a jump asked at the last place landed across
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
      if (p.set.curve_keys) {
        c.forward = static_cast<int16_t>(move.maxspeed);
        c.side = static_cast<int16_t>(static_cast<float>(key_side) * move.maxspeed);
        key_side = side;
      }
      const float z0 = st.origin.z;
      player_move(st, c, move, *s.world);
      if (st.ground < 0 || st.origin.z < z0 - kStepHeight) break;  // off the floor
      const float along = dot(Vec3f{st.origin.x - S.x, st.origin.y - S.y, 0}, dir);
      if (along > kGapRunPast) break;
      // Under a keyboard the jump's key goes down a command after it is asked: the place a jump
      // is asked from and the next both land across.
      const bool here = across(st.origin, S) <= kGapRunWindow && lands(st);
      const bool late_ok = !p.set.curve_keys || prev_lands;
      prev_lands = here;
      if (here && late_ok) {
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

JumpFlight hop_cast(const Pilot& p, const PilotSense& s, int msec) {
  JumpFlight out;
  if (s.world == nullptr || p.forecasting) return out;
  Pilot c = p;
  c.forecasting = true;
  MoveSettings move;
  move.maxspeed = s.maxspeed;
  MoveState st;
  st.origin = s.origin;
  st.velocity = s.velocity;
  st.maxspeed = s.maxspeed;
  st.waterlevel = s.waterlevel;
  st.ground = s.onground ? 0 : -1;
  PilotSense cs = s;
  cs.grenade_ready = false;
  cs.rocket_ready = false;
  const float dt = static_cast<float>(msec) * 0.001f;
  bool aloft = false;
  for (int i = 0; i < kCastMost; i++) {
    cs.origin = st.origin;
    cs.velocity = st.velocity;
    cs.onground = st.ground >= 0;
    cs.waterlevel = st.waterlevel;
    MoveCmd cmd = pilot_step(c, cs, msec);
    if (p.set.curve_keys) {
      // A keyboard: the curve's keys held whole, else the wish's nearest of the eight key sets.
      const float fw = cmd.forward, sd = cmd.side;
      const float m = root_of(fw * fw + sd * sd) / move.maxspeed;
      int set = -1;
      if (c.curve_keys != kNoCurveKeys && cs.onground)
        set = c.curve_keys > 0 ? 1 : c.curve_keys < 0 ? 7 : 0;
      else if (m >= kKeyPressMin) {
        float ang = heading(fw, sd);  // 0 forward, clockwise to the right
        if (ang < 0) ang += 360;
        set = static_cast<int>(ang / 45 + 0.5f) % 8;
      }
      constexpr int8_t kF[8] = {1, 1, 0, -1, -1, -1, 0, 1}, kS[8] = {0, 1, 1, 1, 0, -1, -1, -1};
      cmd.forward = static_cast<int16_t>(set < 0 ? 0 : kF[set] * kKeyAxis);
      cmd.side = static_cast<int16_t>(set < 0 ? 0 : kS[set] * kKeyAxis);
    }
    player_move(st, cmd, move, *s.world);
    out.time_s += dt;
    if (st.waterlevel > 0) return out;  // into a liquid: no landing
    if (st.ground < 0) {
      aloft = true;
    } else if (aloft) {
      out.landed = true;
      out.end = st.origin;
      return out;
    } else if (i > 0) {
      return out;  // never left the ground
    }
  }
  return out;
}

// A gap trick given up: off every route this player plans for `kGapOff_s` (a goal chosen again
// clears the pilot's cut, and the same spot would plan it again at once).
void gap_off(Pilot& p, NavExtraLink l) {
  for (size_t i = 0; i < p.gaps_off.size(); i++)
    if (p.gaps_off[i].from == l.from && p.gaps_off[i].to == l.to) {
      p.gaps_off_s[i] = p.clock_s + kGapOff_s;
      return;
    }
  p.gaps_off.push_back(l);
  p.gaps_off_s.push_back(p.clock_s + kGapOff_s);
}

// THE GAP'S TAKEOFF, on a hop's landing with a gap trick in its stretch (nav_gaps.hpp): the move
// flies a jump pressed now, and from each of the next few commands' places (a run on at the
// friction's loss). A jump now that lands across, by the gap's landing: the gap jumped. One a
// little further on that does: run on, the brink guard stood aside (`brink_free`: the run is
// heading at the lip on purpose). Else a hop now that comes down short and safe on this side: an
// ordinary hop. Else the gap is given up (cut until the next goal; `replanned`, and the caller
// plans the goal round it), the brink guard braking the run at the lip.
bool gap_takeoff(Pilot& p, const PilotSense& s, int msec, const char*& because, bool& replanned) {
  if (p.forecasting) {
    // A copy flying `hop_cast`'s forecast: the hop's jump it was asked about.
    p.safe_flight = true;
    because = "a forecast: the hop's jump";
    return true;
  }
  const NavGraph& g = *p.g;
  const size_t gk = static_cast<size_t>(p.gap_k);
  const NavLink* gl = link_of(g, p.route[gk], p.route[gk + 1]);
  const Vec3f S = node(g, p.route[gk]).origin, T = node(g, p.route[gk + 1]).origin;
  MoveSettings move;
  move.maxspeed = s.maxspeed;
  Vec3f dir{T.x - S.x, T.y - S.y, 0};
  normalize(dir);
  if (s.world != nullptr && gl != nullptr) {
    const float dt = static_cast<float>(msec) * 0.001f;
    const float keep = 1 - kGroundFriction * dt;  // a command's run on the ground keeps this
    Vec3f at = s.origin, v = s.velocity;
    for (int k = 0; k <= kGapRunOn; k++) {
      if (k > 0) {
        at = at + v * dt;
        v = v * keep;
      }
      const float along = dot(Vec3f{at.x - S.x, at.y - S.y, 0}, dir);
      if (along > kGapPastStart) break;  // past the lip
      if (!gap_lands(*s.world, move, at, v, T, msec, p.set.gap_past, p.set.gap_strafe)) continue;
      // Under a keyboard the jump's key goes down late: asked only where it lands either way.
      if (k == 0 && (!p.set.curve_keys || gap_lands_late(p, s, msec, T))) {
        p.gap_flight = true;
        p.gap_aloft = false;
        p.gap_pressed_s = p.clock_s;
        p.gap_link = {p.route[gk], p.route[gk + 1], NavKind::jump, gl->time_s};
        p.safe_flight = true;
        p.brink_free = true;
        p.stats.gap_jumps++;
        p.stats.gap_speed += across({}, s.velocity);
        because = "a gap trick: the jump from here lands across";
        return true;
      }
      p.brink_free = true;
      because = "a gap trick: running on to where its jump lands across";
      return false;
    }
    // `pilotcast`: the hop flown by a copy of the pilot as it will fly it (its strafe's gain,
    // its hands' lag), not forward held at the landing.
    const JumpFlight hop = p.set.pilot_cast
                               ? hop_cast(p, s, msec)
                               : fly_jump(*s.world, move, s.origin, s.velocity, T, msec);
    if (p.set.pilot_cast && hop.landed && lands_by(s.origin, hop.end, T, p.set.gap_past)) {
      // The hop itself comes down across: the gap hopped, strafing as a hop does.
      p.safe_flight = true;
      p.brink_free = true;
      p.stats.gap_jumps++;
      p.stats.gap_speed += across({}, s.velocity);
      because = "a gap trick: the hop's jump, flown as the pilot flies it, lands across";
      return true;
    }
    // `lipjump`: short of the lip by a margin, the hop's strafe in the air gaining on the flight
    // forecast here (forward at the landing); none when the copy flew it.
    const float short_by = p.set.lip_jump && !p.set.pilot_cast ? kHopShort : 0;
    if (hop.landed && hop.end.z >= S.z - kStepHeight &&
        dot(Vec3f{hop.end.x - S.x, hop.end.y - S.y, 0}, dir) <= -short_by) {
      p.safe_flight = true;
      p.brink_free = true;
      because = "a gap trick ahead: a hop that lands this side";
      return true;
    }
  }
  if (p.set.gap_circle && across(s.origin, S) > kGapCurveFrom) {
    // Short of the lip and too far out to curve into it yet: run on along the ground to where
    // the curve begins (a hop from here would come down past it).
    because = "a gap trick ahead: running on to its lip";
    return false;
  }
  p.stats.gap_refused++;
  if (gl != nullptr) gap_off(p, {p.route[gk], p.route[gk + 1], NavKind::jump, gl->time_s});
  replanned = true;  // the caller plans again
  because = "a gap trick too fast to reach: planned round it";
  return false;
}

}  // namespace ge::shooter::pilot_parts
