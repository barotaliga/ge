// ge:summary The pilot's step: the link's mode, waits, rides, swims, strafes, hops, the air
// ge:keywords pilot pilot_step pilot_go run-up strafe-jump wait ride stuck replan
// ge:skill shooter bots
// ge:test tests/shooter_pilot_test.cpp
#include "shooter/pilot.hpp"

#include <algorithm>
#include <utility>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/escape.hpp"
#include "shooter/look_ahead.hpp"
#include "shooter/nav_gaps.hpp"
#include "shooter/nav_script.hpp"
#include "shooter/pilot_gaps.hpp"
#include "shooter/pilot_movers.hpp"
#include "shooter/pilot_parts.hpp"
#include "shooter/pilot_recipes.hpp"
#include "shooter/pilot_step_parts.hpp"
#include "shooter/server.hpp"
#include "shooter/swim.hpp"
#include "shooter/usercmd.hpp"

namespace ge::shooter {

namespace {

// (`liftbrake`) The run's stop on the ground: ~v/4 under friction, the hands' lag on top, s; plus
// units.
constexpr float kLiftStop_s = 0.35f, kLiftStopPad = 16;

using namespace pilot_parts;

// (`hopland`) THE HOP'S SIDE SWAPPED AT THE LANDING, the maintainer's way: the strafe key and the
// mouse turn one way the whole flight; just before the landing (`hop_swap_s`), once the heading
// has come round past the aim, they swap with the jump. The turn is paced so the flight lands
// past the aim by as much as the last swap was (an arc mirrored about the way), less the faster it
// flies (the maintainer: speed built, turn less and keep it): the view's lead
// into the turn (bot.cpp) set from the turn the rest of the flight needs against the turn now.
constexpr float kHopGravity = 800;      // the time to the landing reckoned under this, units/s/s
constexpr float kHopAccel = 10;         // the air's accelerate (the server's sv_accelerate)
constexpr float kHopHandLag_s = 0.05f;  // the hand's view behind its want at a turn, s
constexpr float kHopRateGain = 0.02f;   // the lead's correction a degree a second behind the pace
constexpr float kDegToRad = static_cast<float>(core::kPi / 180);
constexpr float kHopLeadLo = -6, kHopLeadHi = 14;  // the lead's reach, degrees
constexpr float kHopRateFilter_s = 0.08f;          // the turn now, smoothed over this, s

void hop_land_side(Pilot& p, const PilotSense& s, float err, float vh, float speed, float dt) {
  const float jump_v = 270;
  float t_land = 2 * jump_v / kHopGravity;  // on the ground: a whole flight ahead
  if (s.onground) {
    p.hop_floor_z = s.origin.z;
  } else {
    const float vz = s.velocity.z, h = s.origin.z - p.hop_floor_z;
    const float d = vz * vz + 2 * kHopGravity * (h > 0 ? h : 0);
    t_land = (vz + std::sqrt(d)) / kHopGravity;
  }
  if (p.strafe_side == 0) {
    p.strafe_side = err > 0 ? -1 : 1;  // the side the aim lies on (the right turns clockwise)
    p.side_s = 0;
  }
  const float side = static_cast<float>(p.strafe_side);
  const float ahead = err * -side;  // the aim ahead in the turn's direction, degrees
  if ((s.onground || t_land <= p.set.hop_swap_s) && ahead < 0 && p.side_s >= p.set.hop_side_s) {
    // (The faster, the less: the arc's most falls from `hop_arc_hi` at `hop_arc_fast_lo` units/s
    // to `hop_arc_lo` at `hop_arc_fast_hi`: speed built, the hop keeps it.)
    const float f =
        (speed - p.set.hop_arc_fast_lo) / (p.set.hop_arc_fast_hi - p.set.hop_arc_fast_lo);
    const float most = p.set.hop_arc_hi + (p.set.hop_arc_lo - p.set.hop_arc_hi) * (f < 0   ? 0
                                                                                   : f > 1 ? 1
                                                                                           : f);
    const float past = -ahead;
    p.hop_arc_end = past < p.set.hop_arc_lo ? p.set.hop_arc_lo : past > most ? most : past;
    p.strafe_side = static_cast<int8_t>(-p.strafe_side);
    p.side_s = 0;
    p.side_why = "land";
    p.hop_rate = 0;
    p.hop_vh_was = vh;
    return;
  }
  // The turn now (toward the side), smoothed, and the turn the rest of the flight needs.
  if (dt > 0 && speed > kSlow) {
    const float now = wrap(vh - p.hop_vh_was) * -side / dt;
    const float k = dt < kHopRateFilter_s ? dt / kHopRateFilter_s : 1;
    p.hop_rate += (now - p.hop_rate) * k;
  }
  p.hop_vh_was = vh;
  const float t = t_land > 0.05f ? t_land : 0.05f;
  const float need = (ahead + p.hop_arc_end) / t;
  // The view's offset into the turn that gives it, by the air move (its add capped at 30 along
  // the wish, `accelerate` x 320 x dt a command): the wish's share along the velocity `c` leaves
  // `30 - c` across, so the turn a command needs sets c, and the side key's wish lies 90 degrees
  // off the view: the view `-asin(c / v)` into the turn. Past the arc's end the add is nothing
  // (c = 30: neither gained nor lost). Led on by the hand's lag at the turn needed.
  const float v = speed > 30 ? speed : 30;
  float across = v * (need * kDegToRad) * dt;
  const float most = kHopAccel * 320 * dt;
  across = across < 0 ? 0 : across > most ? most : across;
  const float c = 30 - across;
  float lead = -static_cast<float>(core::libm::asin(static_cast<double>(c / v))) / kDegToRad +
               (need > 0 ? need : 0) * kHopHandLag_s + (need - p.hop_rate) * kHopRateGain;
  lead = lead < kHopLeadLo ? kHopLeadLo : lead > kHopLeadHi ? kHopLeadHi : lead;
  p.hop_lead = lead;
  p.hop_lead_on = !s.onground;
}

// Out of a pit (escape.hpp): the takeoff reached within this across, units; the rise to the
// surface from under it reckoned this long for the hand's stroke down, s; given up after this
// long no nearer the takeoff or never fired, s.
constexpr float kEscapeAt = 16;
constexpr float kEscapeDepth = 4;  // the swim to the takeoff rises to this under its height
constexpr float kEscapeRise_s = 0.3f;
constexpr float kEscapeGiveUp_s = 3;
constexpr float kSwimPitch = 70;  // a swimmer's view pitched at most this far, degrees
constexpr float kSwimRise = 16;   // the next node this much higher: jump held to rise, units
// A script's keys are pressed in full only with the view within ~14 degrees of the way (the cos).
constexpr double kScriptFacing = 0.97;
constexpr float kScriptFell = 40;  // standing this far under a script's lowest step: fallen off
constexpr float kOffFloor = 12;    // a pool's floor swum over this far above its standing height
constexpr double kLipLiftOff_s = 0.05;  // a lip's jump is off the ground within this      //
                                        // `lipjump`: a lip's landing looked for this many links on
constexpr float kRunUpFallen = 40;  // `underway`: a run-up this far under its jump's start fell off
constexpr float kStrafeHold = 2;    // the strafe's side holds until the target is this far off, deg
// The last jump of a hop's stretch (`headroom`): one whose flight lands before the stretch ends
// (a climb, a mover, a lip, a jump of the route's), not on it: a flight's reach at the speed held.
constexpr float kLastJump_s = 0.55f;
constexpr float kHeadroom_s = 0.8f;  // a hop's jump's flight checked this far, or to its landing
constexpr float kSimpleSide = 3;     // the simple hop takes the side the route is this far off, deg
constexpr float kStrafeSpeed = 0.8f;  // a strafe starts moving at least this share of maxspeed
// `hop_chain`: a landing within this of a hop's air (s) chains into a hop at this speed or more.
constexpr double kChainLand_s = 0.05;
constexpr float kChainSpeed = 250;
// A gap trick (nav_gaps.hpp): a jump lands across when within this of its landing node; a gap
// flight come down within this of it landed.
constexpr float kGapLanded = 96;
constexpr double kLeapLiftOff_s = 0.15;  // a jump's lift-off waited for this long (`liftoff`)
constexpr double kGapLiftOff_s = 0.3;    // a gap's jump not off the ground this long after: none
constexpr float kMashStartSpeed = 370;   // (`mashstart`) a mashing walk hops from this fast, u/s
constexpr double kMashStart_s = 0.4;     // or once it has mashed this long
constexpr float kAimGround = 360;        // (`aimground`) an aimed hop jumps from this fast, u/s
// The hop's aim judged for `hop_tight` this far on in flight, s.
constexpr float kTightLook_s[] = {0.4f, 0.7f, 1.0f};
constexpr float kStraightWay =
    10;  // (`straightfly`) a way bending less than this ahead is straight, deg
constexpr float kStraightErr = 8;  // and the aim this near the run, deg
constexpr float kStraightTop =
    480;                        // and the run under this (faster, the weave is the air's gain), u/s
constexpr float kTryKin = 72;   // (`giveup`) a jump's run-ups counted together, ends this near
constexpr int kRunUpTries = 4;  // a jump lined up for this many times and never made is given up
constexpr float kClear = 4;     // a player waits this far clear of a mover's box, units
constexpr float kBrake = 0.25f;  // the least share of maxspeed walked into the goal
// In the air (`air`): the aim kAirPast past the target toward the node after it within kAirCorner
// of it, units; slower
// than kAirSlow across, or the aim more than kAirGiveUp off the velocity, forward at it (the
// proof's way); within kAirHold of it, no key; the side key full from kAirFull off, degrees.
constexpr float kAirCorner = 64, kAirPast = 24, kAirSlow = 100;
constexpr float kAirGiveUp = 100, kAirHold = 2, kAirFull = 12;
constexpr float kGrenadeStill = 30;  // a grenade jump fires only this slow, units/s
constexpr float kGrenadeAim = 4;     // and with the view this near the target's way, deg
constexpr float kGrenadeNear = 32;   // and within this of its start, units
constexpr float kGrenadeSlow = 160;  // slowing into the start from this far, units
constexpr float kGrenadeStep = 4;    // the step to its stand: this times the way off, units/s

bool is_barred(const Pilot& p, int32_t m) {
  for (const int32_t b : p.barred)
    if (b == m) return true;
  return false;
}

// `liplook`: the walk's look grows with the run within this of a harm node lower down, units.
constexpr float kLipNear = 192;

}  // namespace

const char* pilot_mode_name(PilotMode m) {
  switch (m) {
    case PilotMode::idle:
      return "idle";
    case PilotMode::walk:
      return "walk";
    case PilotMode::run_up:
      return "run_up";
    case PilotMode::jump:
      return "jump";
    case PilotMode::air:
      return "air";
    case PilotMode::wait:
      return "wait";
    case PilotMode::ride:
      return "ride";
    case PilotMode::strafe:
      return "strafe";
    case PilotMode::arrived:
      return "arrived";
    case PilotMode::hop:
      return "hop";
    case PilotMode::swim:
      return "swim";
    case PilotMode::grenade:
      return "grenade";
  }
  return "?";
}

std::vector<ai::AxisHands> pilot_hands(PilotHands profile) {
  std::vector<ai::AxisHands> h(4);
  for (ai::AxisHands& a : h) a.pass = true;
  h[0].lo = -1e9;
  h[0].hi = 1e9;
  if (profile == PilotHands::human) {
    // The view: a mouse's turn, a reaction late, settling with a little overshoot.
    h[0] = {false, 0.15, 0.8, 0.4, 0.08, 1, 900, -1e9, 1e9, 0};
    // A stick's thumb: late, settling over a tenth of a second, across its travel in a quarter.
    h[1] = {false, 0.18, 0, 0.5, 0.1, 1, 8, -1, 1, 0};
    h[2] = {false, 0.18, 0, 0.5, 0.1, 1, 8, -1, 1, 0};
  } else if (profile == PilotHands::practised) {
    // A movement drilled to habit: no reaction to wait for (the hand runs ahead of the eye, as a
    // recorded pro's view stays on his velocity while it turns 200 degrees a second), a little
    // wander, the view's turn and the keys' travel still a hand's.
    h[0] = {false, 0, 0.3, 0.4, 0, 1, 1800, -1e9, 1e9, 0};
    h[1] = {false, 0, 0, 0.5, 0, 1, 30, -1, 1, 0};
    h[2] = {false, 0, 0, 0.5, 0, 1, 30, -1, 1, 0};
  } else if (profile == PilotHands::steady) {
    // A movement known but not drilled: a little late (steered from that far ahead), a
    // wandering view, no spring (a spring this quick rings at the command rate).
    h[0] = {false, 0.03, 1.5, 0.6, 0, 1, 2400, -1e9, 1e9, 0};
    h[1] = {false, 0.03, 0, 0.5, 0, 1, 20, -1, 1, 0};
    h[2] = {false, 0.03, 0, 0.5, 0, 1, 20, -1, 1, 0};
  }
  return h;
}

float pilot_lead_s(PilotHands profile) {
  return profile == PilotHands::human ? 0.15f : profile == PilotHands::steady ? 0.03f : 0.0f;
}

void pilot_init(Pilot& p, const NavGraph& g, const PilotSettings& set,
                std::span<const ai::AxisHands> hands, double hz, uint32_t seed) {
  Pilot fresh;
  p = std::move(fresh);
  p.g = &g;
  p.set = set;
  p.hands = ai::Hands(hands, hz, seed);
  p.lane_rng = core::make_rng(seed * 2246822519u + 3266489917u);
  p.search.reset(g.nodes.size());
}

void pilot_face(Pilot& p, float yaw) {
  p.yaw = yaw;
  const double at[4] = {yaw, 0, 0, 0};
  p.hands.reset(std::span<const double>(at, p.hands.axes() < 4 ? p.hands.axes() : 4));
}

// TOO TIGHT FOR THE MOVE (recorded only; it decides nothing): the hop's aim along the route at
// `kTightLook_s` of flight (`hop_aim`, the line it steers by), each against the tightest circle the
// air turns the run on at this speed (the most it turns a command, `hop_turn_rate`: a radius of
// about v * v * dt / 30, 88 units at 450, 156 at 600). `hop_tight` is the worst point's radius
// allowed over the radius it needs (past 1: the way turns tighter than the flight can follow, so
// it runs on wide of it), recorded as the `turn` decision and a trace's `tight` column.
static void hop_turn_need(Pilot& p, const PilotSense& s, float vh, float speed, float dt,
                          ai::Why* why) {
  p.hop_tight = 0;
  if (speed < kSlow || dt <= 0 || p.strafe_end_node < 0) return;
  const float most = hop_turn_rate(speed, dt);  // degrees a second
  if (most <= 0) return;
  const float r_min = speed / (most * kDegToRad);
  for (const float look_s : kTightLook_s) {
    const float look = speed * look_s;
    const Vec3f at = hop_aim(p, s.origin, look, p.strafe_end_node);
    const float d = across(s.origin, at);
    if (d < 1) continue;
    float off = wrap(yaw_at(s.origin, at) - vh);
    if (off < 0) off = -off;
    // A point `off` degrees and `d` units away lies on a circle from the run of radius
    // d / (2 sin off); past a quarter turn no circle of the run reaches it short of a loop.
    const float sine = off >= 90 ? 1.f : static_cast<float>(core::libm::sin(off * kDegToRad));
    if (sine <= 0) continue;
    const float ratio = r_min * 2 * sine / d;
    if (ratio > p.hop_tight) p.hop_tight = ratio;
  }
  if (why) {
    why->decide("turn");
    why->fact("speed", speed);
    why->fact("min_radius", r_min);
    why->fact("tight", p.hop_tight);
    why->choose(p.hop_tight > 1 ? "too-tight" : "ok",
                p.hop_tight > 1 ? "the way ahead turns tighter than the air turns at this speed"
                                : "the way ahead within the air's turn");
  }
}

MoveCmd pilot_step(Pilot& p, const PilotSense& s, int msec, ai::Why* why) {
  const float dt = static_cast<float>(msec) * 0.001f;
  const float t = static_cast<float>(p.stats.time_s);
  p.stats.time_s += static_cast<double>(dt);
  p.clock_s += static_cast<double>(dt);
  p.circling_now = false;
  // A hop's landing this command (`hop_chain`): the hop was in the air on the last one.
  const bool chain =
      p.set.hop_chain && s.onground && p.hop_air_s >= 0 && p.clock_s - p.hop_air_s <= kChainLand_s;
  // (Carried through the whole flight: a hop's stretch that ended in the air still lands one.)
  if (!s.onground &&
      (p.mode == PilotMode::hop || (p.hop_air_s >= 0 && p.clock_s - p.hop_air_s <= kChainLand_s)))
    p.hop_air_s = p.clock_s;
  see_movers(p, s);
  const NavGraph& g = *p.g;
  Want w;
  w.yaw = p.yaw;
  const char* because = "no route";
  p.shot_pitch = 0;
  p.shot_in_s = 0;
  p.brink_free = false;
  p.walk_air = false;
  p.curve_keys_last = p.curve_keys;
  p.curve_keys = kNoCurveKeys;
  if (p.gap_flight && !s.onground) p.gap_aloft = true;
  if (p.lip_pressed_s >= 0 && s.onground && p.clock_s - p.lip_pressed_s > kLipLiftOff_s)
    p.lip_pressed_s = -1;  // a lip's jump come down (or never off)
  if (p.gap_flight && s.onground && !p.gap_aloft && p.clock_s - p.gap_pressed_s > kGapLiftOff_s) {
    p.gap_flight = false;  // the jump never left the ground (a key held off): no flight to judge
  } else if (p.gap_flight && s.onground && p.gap_aloft) {
    // A gap trick's jump come down (whatever the mode now: the hop's stretch may end in its
    // flight): by its landing, or short (a miss: twice, never again).
    p.gap_flight = false;
    const Vec3f T = node(g, p.gap_link.to).origin;
    const float dz = s.origin.z - T.z;
    if (across(s.origin, T) <= kGapLanded && dz <= kGapUp && dz >= -kGapUp)
      p.stats.gap_landed++;
    else
      missed_jump(p, p.gap_link);
    // The gap is behind: the next in the hop's stretch, if any, is the one ahead now.
    p.gap_k = -1;
    for (size_t j = p.k; p.mode == PilotMode::hop && j + 1 < p.route.size(); j++) {
      if (p.route[j] != p.gap_link.from || p.route[j + 1] != p.gap_link.to) continue;
      p.gap_k = gap_in(p, p.strafe_end_node, j + 1);
      break;
    }
  }
  if (p.mode != PilotMode::hop) {
    p.gap_k = -1;
    p.recipe = -1;  // a start under way belongs to a hop (planned again: off)
    p.safe_flight = false;
  } else if (!s.onground && p.safe_flight) {
    p.brink_free = true;  // a flight the move proved lands safe: nothing brakes it
  }
  p.shot_now = false;
  p.escaping = false;
  const float speed = across({}, s.velocity);
  via_check(p, s, speed);
  // Where the player will be when the hands have caught up: the pilot steers from there.
  const Vec3f ahead = s.origin + s.velocity * p.set.lead_s;

  // Through every node already passed (several in a command near a corner).
  for (int guard = 0; p.mode != PilotMode::idle && p.mode != PilotMode::arrived && guard < 8;
       guard++) {
    if (p.k + 1 >= p.route.size()) {
      p.mode = PilotMode::arrived;
      break;
    }
    if (p.mode == PilotMode::strafe || p.mode == PilotMode::hop) break;
    if (passed(p, s, p.k) || nearer_on(p, s)) {
      const NavLink* done = link_of(*p.g, p.route[p.k], p.route[p.k + 1]);
      const bool escaped = done != nullptr && done->escape >= 0 && p.airborne;
      if (escaped) p.stats.escapes_out++;
      advance(p, s);
      if (escaped || (done != nullptr && done->escape >= 0)) p.escape_on = -1;
      if (escaped && p.plan_on_landing) {
        p.plan_on_landing = false;
        replan(p, s);
      }
      continue;
    }
    break;
  }
  // The guard can end the loop on the node it just passed: the route's last is the goal (else
  // its next node is read past the route's end).
  if (p.mode != PilotMode::idle && p.mode != PilotMode::arrived && p.k + 1 >= p.route.size())
    p.mode = PilotMode::arrived;
  if (p.mode == PilotMode::arrived) because = "at the goal";

  bool replanned = false;
  if (p.mode != PilotMode::idle && p.mode != PilotMode::arrived) {
    const int32_t a = p.route[p.k], b = p.route[p.k + 1];
    const NavLink* lp = link_of(g, a, b);
    const NavLink l = lp ? *lp : NavLink{b, NavKind::walk, 0, -1, 0};
    const Vec3f A = node(g, a).origin, B = node(g, b).origin;
    const NavLink* l2 = p.k + 2 < p.route.size() ? link_of(g, b, p.route[p.k + 2]) : nullptr;
    // A teleporter's link: into its trigger, where the link starts (B is where it sends).
    Vec3f target = l.teleport ? A : B;

    // The hop's stretch (an expert's), entered on the ground on a walk moving near full speed.
    // Near a gap trick over water (`gap_circle`), slower: the hop's curve begun out from the lip,
    // as people curve into it from a walk.
    const bool near_gap = p.set.gap_circle && gap_curve_near(p, s);
    // People's hop start (`recipes`): slower than a run, a hop's stretch ahead with no gap in it.
    p.recipe_note = p.recipe >= 0                                           ? "playing"
                    : p.set.recipes.entries.empty() || !p.set.hop           ? "-"
                    : p.mode != PilotMode::walk                             ? "not walking"
                    : !s.onground                                           ? "in the air"
                    : speed >= kStrafeSpeed * s.maxspeed                    ? "running"
                    : p.clock_s < p.recipe_retry_s                          ? "waiting"
                    : chain                                                 ? "chaining"
                    : !(l.kind == NavKind::walk || l.kind == NavKind::drop) ? "not a walk"
                    : l.mover >= 0 || near_gap || (l2 && leaps(l2->kind))   ? "a mover, gap or jump"
                                                                            : "looked";
    if (p.recipe_note[0] == 'l' && s.world != nullptr) {
      const int32_t j = hop_stretch(p, s.origin);
      p.recipe_note = j < 0 ? "no hop stretch" : gap_in(p, j) >= 0 ? "a gap in it" : "none proven";
      if (j >= 0 && gap_in(p, j) < 0 &&
          recipe_choose(p, s, yaw_at(s.origin, hop_aim(p, s.origin, kRecipeLook, j)), j)) {
        p.recipe_note = "begun";
        p.mode = PilotMode::hop;
        p.strafe_end_node = j;
        p.gap_k = -1;
        p.strafe_side = 0;
        p.circle_s = -1;
        p.circle_side = 0;
        p.best = 1e30f;
        p.best_s = t;
        p.stats.hops++;
      }
    }
    // (`hopjump`) A hop given a new route in the air (off its way, a new goal) hops on along it
    // from the flight, not from a walk's start once down.
    const bool rejoin = p.set.hop_jump_held && !s.onground && s.waterlevel == 0 && !p.airborne &&
                        p.hop_air_s >= 0 && p.clock_s - p.hop_air_s <= kChainLand_s &&
                        speed >= p.set.hop_slow * s.maxspeed;
    // (`mashstart`) A walk mashing its strafe keys builds its run before the hop's first jump:
    // held off until the run reaches `kMashStartSpeed`, or `kMashStart_s` of mashing has passed.
    const bool mash_building = p.set.mash_start && p.mode == PilotMode::walk && s.onground &&
                               !chain && !rejoin && !near_gap &&
                               p.curve_keys_last != kNoCurveKeys && speed < kMashStartSpeed &&
                               p.clock_s - p.mash_from < kMashStart_s;
    if (mash_building) p.stats.mash_starts++;
    if (p.set.hop && p.mode == PilotMode::walk && (s.onground || rejoin) && !mash_building &&
        (l.kind == NavKind::walk || l.kind == NavKind::drop || (l.gap && hops_gaps(p))) &&
        l.mover < 0 &&
        (speed >= kStrafeSpeed * s.maxspeed || near_gap || (chain && speed >= kChainSpeed) ||
         rejoin) &&
        !(l2 && leaps(l2->kind) && !(l2->gap && hops_gaps(p)))) {
      const int32_t j = hop_stretch(p, s.origin);
      if (j >= 0 && (speed >= kStrafeSpeed * s.maxspeed || gap_in(p, j) >= 0 ||
                     (chain && speed >= kChainSpeed) || rejoin)) {
        p.mode = PilotMode::hop;
        p.strafe_end_node = j;
        p.gap_k = gap_in(p, j);
        p.strafe_side = 0;
        // (A hop's landing chained: jumped again at once, no curve on the ground first.)
        p.circle_s = p.set.hop_circle && !chain && !rejoin ? 0.f : -1.f;
        p.circle_side = 0;
        p.best = 1e30f;
        p.best_s = t;
        p.stats.hops++;
      }
    }
    if (p.mode == PilotMode::walk && p.recipe_note[0] == 'r' && p.recipe_note[1] == 'u') {
      // Running and no hop begun: which of the hop's conditions held it off (for a trace).
      p.recipe_note =
          !s.onground ? "running: in the air"
          : !(l.kind == NavKind::walk || l.kind == NavKind::drop || (l.gap && hops_gaps(p)))
              ? "running: not a walk"
          : l.mover >= 0                                          ? "running: a mover"
          : (l2 && leaps(l2->kind) && !(l2->gap && hops_gaps(p))) ? "running: a leap next"
          : hop_stretch(p, s.origin) < 0                          ? hop_stretch_why(p, s.origin)
                                                                  : "running: not hopped";
    }
    float hop_left = 0;
    if (p.mode == PilotMode::hop && !p.set.hop) {
      // The hop turned off mid-stretch (a bot that took the view to aim): walk on from here.
      p.hop_end_why = "off";
      p.hop_end_at = "";
      restart(p, s);
      p.mode = PilotMode::walk;
    }
    if (p.mode == PilotMode::hop) {
      hop_left = hop_follow(p, s.origin, p.strafe_end_node);
      target = node(g, p.route[static_cast<size_t>(p.strafe_end_node)]).origin;
      const float off =
          off_segment(s.origin, node(g, p.route[p.k]).origin, node(g, p.route[p.k + 1]).origin);
      const bool at_end = hop_left <= p.set.hop_end && !p.gap_flight;
      const bool slow =
          s.onground && speed < p.set.hop_slow * s.maxspeed && !p.jump_pressed && p.recipe < 0;
      if (at_end || slow ||
          (p.set.hop_headroom && s.onground && !p.jump_pressed && p.gap_k < 0 &&
           hop_left <= speed * kLastJump_s)) {
        p.hop_end_why = at_end ? "end" : slow ? "slow" : "room";
        p.hop_end_at = at_end || !slow ? hop_stretch_end(p) : "";
        restart(p, s);
        p.mode = PilotMode::walk;
      } else if (off > p.set.lost && !(p.lip_pressed_s >= 0 && !s.onground)) {
        // (A lip's jump flies the crossing, off the route's line: planned again once down, not
        // from over the pit, where no way starts.)
        p.stats.lost++;
        replanned = true;
        replan(p, s);
        because = "hopped off the way: plan again";
      } else if (hop_left < p.best - 0.5f) {
        p.best = hop_left;
        p.best_s = t;
      } else if (t - p.best_s > p.set.stuck_s) {
        p.stats.stuck++;
        // No nearer on the way to a jump (its run-up walled in): a miss of that jump.
        if (leaps(l.kind))
          missed_jump(p, {a, b, l.kind, l.time_s});
        else if (l2 && leaps(l2->kind))
          missed_jump(p, {b, p.route[p.k + 2], l2->kind, l2->time_s});
        replanned = true;
        replan(p, s);
        because = "the hop is no nearer: plan again";
      }
    }

    // The strafe stretch, entered on a level walk moving near full speed.
    if (!p.set.hop && p.set.strafe && p.mode == PilotMode::walk && s.onground &&
        l.kind == NavKind::walk && l.mover < 0 && speed >= kStrafeSpeed * s.maxspeed) {
      const int32_t j = strafe_stretch(p, s.origin);
      if (j >= 0) {
        p.mode = PilotMode::strafe;
        p.strafe_end_node = j;
        p.stats.strafes++;
      }
    }
    if (p.mode == PilotMode::strafe) {
      target = node(g, p.route[static_cast<size_t>(p.strafe_end_node)]).origin;
      if (across(s.origin, target) <= p.set.strafe_end) {
        p.k = static_cast<size_t>(p.strafe_end_node) - 1;
        restart(p, s);
        p.mode = PilotMode::walk;
      }
    }

    // A gap trick reached on foot, not out of a hop (nav_gaps.hpp): no run makes it. Round it
    // until the next goal.
    if (!replanned && p.mode != PilotMode::hop && !p.airborne && s.onground &&
        ((leaps(l.kind) && l.gap) || (l2 && l2->gap && leaps(l2->kind)))) {
      p.stats.gap_refused++;
      gap_off(p, l.gap ? NavExtraLink{a, b, l.kind, l.time_s}
                       : NavExtraLink{b, p.route[p.k + 2], l2->kind, l2->time_s});
      replanned = true;
      replan(p, s);
      because = "a gap trick not hopped into: planned round it";
    }

    if (replanned) {
      // Planned again off the hop: this command coasts; the next one starts on the new route.
      w.yaw = p.yaw;
    } else if (p.mode == PilotMode::hop) {
      const float vh = speed > kSlow ? heading(s.velocity.x, s.velocity.y) : p.yaw;
      const float look = speed * p.set.hop_look_s > 64 ? speed * p.set.hop_look_s : 64;
      const Vec3f aim =
          via_aim(p, s, s.origin, look, hop_aim(p, s.origin, look, p.strafe_end_node));
      const float aim_yaw = yaw_at(s.origin, aim);
      p.hop_aim_yaw = aim_yaw;
      const float err = wrap(aim_yaw - vh);
      hop_turn_need(p, s, vh, speed, dt, why);
      p.arc_mine = -1;
      p.arc_other = -1;
      // (`aimland`) A fight's view: its air turns only with the mouse, so a flight straight on that
      // meets a wall is not jumped into.
      const bool aim_walled =
          p.set.aim_land_s > 0 && s.aimed && s.world != nullptr && !p.gap_flight && speed > kSlow &&
          arc_clear(*s.world, s.origin, s.velocity, 0, dt, p.set.aim_land_s) < p.set.aim_land_s;
      // The side the aim lies on (the right side turns the velocity clockwise), held until the
      // aim lies `hop_hold` the other way (`hop_air_hold` in the air): arcs that weave round the
      // route.
      p.side_s += dt;
      const float hold = s.onground ? p.set.hop_hold : p.set.hop_air_hold;
      const int8_t want = err > hold ? -1 : err < -hold ? 1 : 0;
      const float swing = err < 0 ? -err : err;
      p.hop_lead_on = false;
      // (`hoprhythm`) A swap in the air kept a moment before anything swaps it back.
      if (s.onground) p.hop_air_swapped = false;
      const bool held = p.hop_air_swapped && p.side_s < p.set.hop_air_keep_s;
      if (!p.set.hop_simple && p.set.hop_land) {
        hop_land_side(p, s, err, vh, speed, dt);
      } else if (!p.set.hop_simple &&
                 (p.strafe_side == 0 ||
                  (want != 0 && want != p.strafe_side && !(held && !s.onground) &&
                   (p.side_s >= p.set.hop_side_s || swing > p.set.hop_swing)))) {
        if (p.strafe_side != 0 && !s.onground) p.hop_air_swapped = true;
        if (p.strafe_side != 0) p.side_why = p.side_s >= p.set.hop_side_s ? "aim" : "swing";
        p.strafe_side = want != 0 ? want : 1;
        p.side_s = 0;
      }
      if (p.set.hop_simple && (s.onground || p.strafe_side == 0)) {
        // The simple hop: the side the route turns to, chosen at each jump and held through
        // the air (a swap of side swings the view 90 degrees, so it is not done in flight).
        p.strafe_side = err > kSimpleSide ? -1 : err < -kSimpleSide ? 1 : p.strafe_side;
        if (p.strafe_side == 0) p.strafe_side = 1;
      }
      // A wall in the arc: the other side at once, when its arc is the clearer (`hoprhythm`: or
      // the side held and flown straight, when straight on is clearer than both).
      if ((!p.set.hop_simple || p.set.hop_open) && s.world != nullptr &&
          (!s.onground || p.set.hop_wall_ground)) {
        const float mine =
            arc_clear(*s.world, s.origin, s.velocity, p.strafe_side, dt, p.set.hop_see_s);
        p.arc_mine = mine;  // (recorded: a trace's `arcm`, `arco`)
        if (mine < p.set.hop_see_s) {
          const float other =
              arc_clear(*s.world, s.origin, s.velocity, -p.strafe_side, dt, p.set.hop_see_s);
          p.arc_other = other;
          const float straight =
              p.set.hop_straight ? arc_clear(*s.world, s.origin, s.velocity, 0, dt, p.set.hop_see_s)
                                 : 0;
          if (straight > (mine > other ? mine : other)) {
            // Straight on is the clearest: the side held, the view set where the side key's wish
            // adds nothing across (bot.cpp's lead, `-asin(30 / v)` into the turn).
            const float v = speed > 30 ? speed : 30;
            p.hop_lead =
                -static_cast<float>(core::libm::asin(static_cast<double>(30 / v))) / kDegToRad;
            p.hop_lead_on = true;
          } else if (other > mine && !(held && other < p.set.hop_see_s)) {
            p.hop_air_swapped = !s.onground;
            p.strafe_side = static_cast<int8_t>(-p.strafe_side);
            p.side_s = 0;
            p.side_why = s.onground ? "wall-jump" : "wall";
          }
        }
      }
      // (`straightfly`) A straight way ahead (the hop's aim at half a second and at a second of
      // flight within `kStraightWay` of each other) with the aim near the run: flown straight on,
      // the side held and the view where its wish adds nothing across, as people fly a corridor
      // (dm2's west corridor: their view 9 degrees off the run, turning 25 degrees a second, walls
      // 16 %; the bots' weave 71 degrees a second, walls 79 %).
      if (p.set.straight_fly && !s.onground && !p.hop_lead_on && p.strafe_end_node >= 0 &&
          speed > kSlow && speed < kStraightTop && err < kStraightErr && err > -kStraightErr) {
        const float near_yaw =
            yaw_at(s.origin, hop_aim(p, s.origin, speed * 0.5f, p.strafe_end_node));
        const float far_yaw = yaw_at(s.origin, hop_aim(p, s.origin, speed, p.strafe_end_node));
        const float bend = wrap(far_yaw - near_yaw);
        if (bend < kStraightWay && bend > -kStraightWay) {
          p.hop_lead =
              -static_cast<float>(core::libm::asin(static_cast<double>(30 / speed))) / kDegToRad;
          p.hop_lead_on = true;
          p.stats.straight_flies++;
        }
      }
      // The view on the velocity, wanted where it will have turned to when late hands get there:
      // the most turn the move gives at this speed, the way the side held turns it.
      const float rate = -static_cast<float>(p.strafe_side) * hop_turn_rate(speed, dt);
      float lead = rate * p.set.lead_s;  // (the simple hop leads by its own turn)
      if (lead > 45) lead = 45;
      if (lead < -45) lead = -45;
      w.side = static_cast<float>(p.strafe_side);
      if (p.set.hop_simple) {
        // Forward and the side key: their wish lies 45 degrees off the view toward the side. Its
        // angle off the velocity steers: at acos(30 / v) the air adds nothing, at 90 it adds 30
        // across (the most turn); between, the turn the route asks for (its error made good in
        // `hop_look_s`), and only as much speed as that turn brings: it keeps its speed on a
        // straight and gains in the curves (a gentle weave on a straight lost more to walls and
        // late hands than it gained, measured on the se-hop lab's trips).
        const double side = static_cast<double>(p.strafe_side);
        const double toward = -side * static_cast<double>(err) * (core::kPi / 180);
        double add = static_cast<double>(speed) * toward * static_cast<double>(dt) /
                     static_cast<double>(p.set.hop_look_s);
        add = add < 0 ? 0 : add > 30 ? 30 : add;
        double c = speed > 30 ? (30 - add) / static_cast<double>(speed) : 0;
        c = c < 0 ? 0 : c > 1 ? 1 : c;
        const double across = core::libm::acos(c) * (180 / core::kPi);
        const float rate_simple = static_cast<float>(add) / (speed > 1 ? speed : 1) / dt *
                                  static_cast<float>(180 / core::kPi);
        w.yaw = unwrap(vh - static_cast<float>(side * (across - 45)) -
                           static_cast<float>(side) * rate_simple * p.set.lead_s,
                       p.yaw);
        w.forward = 1;
      } else {
        w.yaw = unwrap(vh + lead, p.yaw);
        const float err_abs = err < 0 ? -err : err;
        if (err_abs > p.set.hop_brake && speed > s.maxspeed) w.forward = -1;
      }
      GapRun run;
      RecipeWant rw;
      // A gap's jump asked under a keyboard (`curve_keys`), its key not down yet: the keys and the
      // view held as it was asked with (a run re-planned meanwhile turned the takeoff away).
      const bool asked = p.set.curve_keys && s.onground && p.gap_flight && !p.gap_aloft;
      if (!asked && s.onground && p.gap_k >= 0 && static_cast<size_t>(p.gap_k) >= p.k &&
          !p.jump_pressed) {
        // Near a gap's lip with no jump from here landing across: run on to where one does,
        // straight or curving on the ground for speed (people's last steps into a gap).
        float e = 0;
        bool now = false;
        if (gap_curve(p, s, msec, e, now) && !now) run = gap_run(p, s, msec);
      }
      if (!run.found) p.gap_way = -1;
      if (asked) {
        w.forward = static_cast<float>(p.wanted[1]);
        w.side = static_cast<float>(p.wanted[2]);
        w.yaw = p.yaw;
        w.jump = true;
        p.brink_free = true;
        p.curve_keys = p.curve_keys_last;
        because = "a gap trick: its jump asked, the key on the way";
      } else if (run.found) {
        p.gap_way_i = run.way == p.gap_way ? p.gap_way_i + 1 : 1;
        p.gap_way = run.way;
        p.circle_s = -1;
        w.forward = 1;
        w.side = static_cast<float>(run.side);
        w.yaw = unwrap(run.yaw, p.yaw);
        w.jump = false;
        p.brink_free = true;
        p.curve_keys = run.side;
        because = run.side == 0  ? "a gap trick: running at its lip"
                  : run.flip > 0 ? "a gap trick: strafing side to side to its lip"
                                 : "a gap trick: curving on the ground to its lip";
      } else if (p.recipe >= 0 && recipe_play(p, s, dt, rw)) {
        w.yaw = unwrap(rw.yaw, p.yaw);
        w.forward = rw.forward;
        w.side = rw.side;
        w.jump = rw.jump;
        p.curve_keys = rw.curve;
        p.strafe_side = static_cast<int8_t>(rw.side > 0 ? 1 : rw.side < 0 ? -1 : p.strafe_side);
        because = rw.jump ? "a hop start copied: its jump" : "a hop start copied: its run";
      } else if (s.onground && circling(p, s, vh, speed, err, dt, msec, w)) {
        because = "a circle jump: curving into the hop";
      } else if (s.onground) {
        // The landing's own command: jump (before friction) and strafe on; with a gap trick
        // ahead, only a jump the move says lands safe (gap_takeoff).
        w.jump = !p.jump_pressed;
        because = "a landing: jump on it";
        p.safe_flight = false;
        if (w.jump && p.gap_k >= 0 && static_cast<size_t>(p.gap_k) >= p.k) {
          p.gap_back = false;
          w.jump = gap_takeoff(p, s, msec, because, replanned);
          if (replanned) replan(p, s);  // the gap given up: the goal planned round it
          if (w.jump && p.gap_flight && p.set.curve_keys && p.curve_keys_last != kNoCurveKeys) {
            // Asked out of a curve under a keyboard: its keys and view held (`gap_lands_late`
            // judged the jump on them).
            w.forward = 1;
            w.side = static_cast<float>(p.curve_keys_last);
            w.yaw = p.yaw;
            p.curve_keys = p.curve_keys_last;
          }
          if (!w.jump && !replanned) {
            // Running on to the lip: a run's keys along the route, not the air's side key.
            w.forward = 1;
            w.side = 0;
            w.yaw = unwrap(aim_yaw, p.yaw);
            // (`gapturn`) Backing off to the gap's curve: the run there, turned under the view.
            if (p.gap_back) w.yaw = unwrap(yaw_at(s.origin, p.gap_back_at), p.yaw);
          }
        }
        if (w.jump && p.set.hop_headroom && s.world != nullptr &&
            jump_blocked(p, *s.world, s.origin, speed, kHeadroom_s)) {
          // A ceiling in the jump's way: run on under it along the route, jump later.
          w.jump = false;
          w.forward = 1;
          w.side = 0;
          w.yaw = unwrap(aim_yaw, p.yaw);
          because = "the jump's way is low: run on";
        }
        if (w.jump && p.set.hop_land_wall_s > 0 && s.world != nullptr && !p.gap_flight &&
            land_walled(*s.world, s.origin, s.velocity, dt, p.set.hop_land_wall_s)) {
          // A wall in the jump's way on every curve the air gives: run on along the route (the
          // ground slides along a wall at a run's speed; a flight into it lands at ~100), jump
          // once clear.
          w.jump = false;
          w.forward = 1;
          w.side = 0;
          w.yaw = unwrap(aim_yaw, p.yaw);
          because = "a wall in the jump's way: run on";
          p.stats.land_walls++;
        }
        if (w.jump && aim_walled) {
          // (`aimland`) A fight's view: the flight straight on meets a wall. Run on, as above.
          w.jump = false;
          w.forward = 1;
          w.side = 0;
          w.yaw = unwrap(aim_yaw, p.yaw);
          because = "a fight's view, a wall in the jump's way: run on";
          p.stats.aim_lands++;
        }
        if (w.jump && p.set.aim_ground && s.aimed && !p.gap_flight && speed < kAimGround) {
          // (`aimground`) A fight's view and a run under a hop's speed: the fight stays on the
          // ground (such flights lost ~48 units/s each, 42 % into a wall; people seldom jump slow).
          w.jump = false;
          because = "a fight's view, the run slow: no hop";
          p.stats.aim_grounds++;
        }
        if (w.jump) p.stats.hop_jumps++;
      } else if (p.gap_flight) {
        // A gap trick's jump in the air: flown as its takeoff's look flew it (nav_gaps.hpp
        // `fly_jump`), forward at the landing, so it comes down where that said.
        w.jump = false;
        w.forward = 1;
        w.side = 0;
        w.yaw = unwrap(yaw_at(s.origin, node(g, p.gap_link.to).origin), p.yaw);
        because = "a gap trick: flying at its landing";
        if (p.set.gap_strafe && speed > 1) {
          // `airgap`: strafed across as its forecast flew it (the view on the velocity, the side
          // key toward the landing).
          const float off_land = wrap(yaw_at(s.origin, node(g, p.gap_link.to).origin) - vh);
          w.yaw = unwrap(vh, p.yaw);
          w.forward = 0;
          w.side = off_land < -1  ? 1.0f
                   : off_land > 1 ? -1.0f
                                  : (p.strafe_side >= 0 ? -1.0f : 1.0f);
          p.strafe_side = static_cast<int8_t>(w.side > 0 ? 1 : -1);
          because = "a gap trick: strafing across to its landing";
        }
      } else if (p.lip_pressed_s >= 0 && speed > kSlow) {
        // A lip's jump: flown across at the far side, not woven.
        w.jump = false;
        strafe_across(p, vh, wrap(p.lip_yaw - vh), w, 0);
        because = "a lip's jump: strafing across the pit";
      } else {
        w.jump = false;  // let go in the air, so the landing's press is a fresh one
        because = "hopping: the side key, the view on the velocity";
        // (`hopjump`) Held on the way down, so it goes off on the landing frame; not for a landing
        // too slow to hop on (a run gains faster there), into a gap trick's lip (its takeoff is
        // judged on the ground) or under a ceiling over the way.
        if (p.set.hop_jump_held && s.velocity.z < 0 && speed >= p.set.hop_slow * s.maxspeed &&
            (!(p.gap_k >= 0 && static_cast<size_t>(p.gap_k) >= p.k) ||
             (p.set.gap_hold && gap_hold_jumps(p, s, msec))) &&
            !(p.set.hop_headroom && s.world != nullptr &&
              jump_blocked(p, *s.world, s.origin, speed, kHeadroom_s)) &&
            !aim_walled) {
          w.jump = true;
          because = "hopping: the jump held for the landing";
        }
      }
      if (p.set.hop_side_only && !p.gap_flight && p.lip_pressed_s < 0 && !run.found &&
          p.recipe < 0 && !asked && !p.circling_now && (!s.onground || w.jump))
        w.forward = 0;  // (`hopside`) the strafe key alone
    } else if (p.mode == PilotMode::strafe) {
      const float tb = yaw_at(s.origin, target);
      if (s.onground) {
        w.yaw = unwrap(tb, p.yaw);
        w.forward = 1;
        w.jump = !p.jump_pressed;
        because = "a landing: jump again";
      } else {
        const float vh = heading(s.velocity.x, s.velocity.y);
        const float err = wrap(tb - vh);
        // The right side turns the velocity clockwise: the side the target is on, held until
        // the target is kStrafeHold off the other way.
        if (err > kStrafeHold)
          p.strafe_side = -1;
        else if (err < -kStrafeHold || p.strafe_side == 0)
          p.strafe_side = 1;
        w.yaw = unwrap(vh, p.yaw);
        w.side = static_cast<float>(p.strafe_side);
        w.jump = !p.jump_pressed;  // pressed afresh every other command: a landing catches one
        because = "in the air: the wish across the velocity";
      }
    } else if (l.escape >= 0 && static_cast<size_t>(l.escape) < g.escapes.size() && p.airborne &&
               s.onground) {
      // Out of a pit, landed off the ledge's node (passed() takes it on it): out of the liquid,
      // or fallen back into it (missed: twice, never planned again).
      const bool out = landed_out(p, s);
      if (out) {
        p.stats.escapes_out++;
      } else {
        missed_jump(p, {a, b, l.kind, l.time_s});
      }
      p.escape_on = -1;
      p.plan_on_landing = false;
      replanned = true;
      replan(p, s);
      because =
          out ? "out of the pit: landed beside the ledge's node" : "out of the pit: fell back";
    } else if (l.escape >= 0 && static_cast<size_t>(l.escape) < g.escapes.size() && p.airborne &&
               !s.onground) {
      // Out of a pit, in the flight (still in the liquid too, until the blast): the view held
      // away from the ledge and the back key, as the proof flew.
      const EscapeKeys k = escape_flight_keys(g.escapes[static_cast<size_t>(l.escape)]);
      p.escaping = true;
      p.mode = PilotMode::air;
      w.yaw = unwrap(k.yaw, p.yaw);
      w.forward = k.forward;
      because = "out of the pit: thrown back onto the ledge";
    } else if (l.escape >= 0 && static_cast<size_t>(l.escape) < g.escapes.size() && !p.airborne) {
      // OUT OF A PIT BY ROCKET (escape.hpp): swum to the takeoff under the ledge, then backed
      // against the wall with the view away from the ledge, swimming up, and the shot down as the
      // eyes come out (the launcher in hand and the view's hand there); its flight is `air`'s.
      const NavEscape& e = g.escapes[static_cast<size_t>(l.escape)];
      if (p.escape_on != l.escape) {
        p.escape_on = l.escape;
        p.escape_back_s = -1;
        p.best = 1e30f;
        p.best_s = t;
      }
      p.mode = PilotMode::swim;
      if (p.escape_back_s < 0) {
        // The view turned away from the ledge at once (the shot's heading), the takeoff swum to
        // by the keys in the view's frame, rising all the way: the liquid burns while he turns.
        const float d = across(s.origin, e.from);
        w.yaw = unwrap(e.yaw, p.yaw);
        const ViewAxes ax = angle_vectors({0, p.yaw, 0});
        const Vec3f to{e.from.x - s.origin.x, e.from.y - s.origin.y, 0};
        const float k = d > 1 ? (d < p.set.look ? d / p.set.look : 1) / d : 0;
        w.forward = dot(to, ax.forward) * k;
        w.side = dot(to, ax.right) * k;
        // At the takeoff's depth, eyes under (out early, the swimmer bobs and sinks back: the shot
        // must go on the rise from there, as its proof's).
        w.jump = s.origin.z < e.from.z - kEscapeDepth;
        because = "out of the pit: to the takeoff under the ledge";
        if (d < p.best - 0.5f) {
          p.best = d;
          p.best_s = t;
        }
        if (d <= kEscapeAt) {
          p.escape_back_s = 0;
          p.best_s = t;
        }
      }
      if (p.escape_back_s >= 0) {
        p.escaping = true;
        // The depth as the move works it out (the proof's), where the pilot sees the world.
        const int depth = s.world != nullptr ? water_level(*s.world, s.origin) : s.waterlevel;
        const EscapeKeys k = escape_back_keys(e, depth, s.velocity, p.escape_back_s);
        p.escape_back_s += dt;
        w.yaw = unwrap(k.yaw, p.yaw);
        w.pitch = k.pitch;
        w.forward = k.forward;
        w.jump = k.jump;
        p.shot_pitch = e.pitch;
        p.shot_yaw = e.yaw;
        p.shot_in_s =
            k.fire
                ? 0
                : (depth >= 3 ? kEscapeRise_s : 0) +
                      (p.escape_back_s < kEscapeBackMin_s ? kEscapeBackMin_s - p.escape_back_s : 0);
        because = "out of the pit: backing against the wall, swimming up";
        if (k.fire && s.rocket_ready && shot_view_ready(p)) {
          w.pitch = k.pitch;
          w.attack = true;
          p.shot_now = true;
          p.shot_in_s = 0;
          p.airborne = true;
          p.mode = PilotMode::air;
          p.stats.rocket_jumps++;
          p.stats.escapes++;
          because = "out of the pit: the rocket down as the eyes come out";
        } else if (k.fire) {
          because = s.rocket_ready ? "out of the pit: the view down for the shot"
                                   : "out of the pit: the launcher";
        }
      }
      // Never fired (the takeoff out of reach, the launcher never in hand): planned round.
      if (!p.airborne && t - p.best_s > kEscapeGiveUp_s) {
        missed_jump(p, {a, b, l.kind, l.time_s});
        missed_jump(p, {a, b, l.kind, l.time_s});
        p.escape_on = -1;
        replanned = true;
        replan(p, s);
        because = "out of the pit: never fired, planned round";
      }
    } else if (l.kind == NavKind::script && l.script >= 0 &&
               static_cast<size_t>(l.script) < g.scripts.size() && !mover_in_way(p, s, l, a, b)) {
      // A SCRIPTED WAY (nav_script.hpp): the script's own keys, as its proof flew them.
      const bool stale =
          p.set.script_fresh && p.clock_s - p.script_at > 2 * static_cast<double>(dt);
      p.script_at = p.clock_s;
      if (p.script_from != a || p.script_to != b || stale) {
        p.script_from = a;
        p.script_to = b;
        p.script_step = 0;
        p.script_seen = 0;
        p.script_done = false;
        p.best_s = t;
      }
      const NavScript& sc = g.scripts[static_cast<size_t>(l.script)];
      const ScriptKeys k = script_keys(sc, p.script_step, s.origin, s.velocity, s.onground,
                                       p.set.arrive, p.set.script_entry);
      p.mode = s.onground ? PilotMode::walk : PilotMode::air;
      if (p.script_step > p.script_seen) {
        p.script_seen = p.script_step;
        p.best_s = t;
      }
      float lowest = A.z;
      for (const ScriptStep& st : sc.steps) lowest = st.at.z < lowest ? st.at.z : lowest;
      if (k.done) {
        p.script_done = true;
        because = "a scripted way: its last step";
      } else if ((s.onground && s.origin.z < lowest - kScriptFell) ||
                 t - p.best_s > 2 * p.set.stuck_s) {
        // Fallen off it, or no step further for long: missed (twice: not taken again).
        int times = 0;
        for (const NavExtraLink& e : p.short_jumps) times += e.from == a && e.to == b ? 1 : 0;
        const NavExtraLink e{a, b, l.kind, l.time_s};
        missed_jump(p, e);
        if (times >= 1) {
          p.failed.push_back(e);
          p.cut.push_back(e);
        }
        p.stats.lost++;
        p.script_from = -1;
        replanned = true;
        replan(p, s);
        because = "a scripted way missed: plan again";
      } else {
        // As the proof keys it, but forward only as far as the view already faces the way and
        // the hop only facing it (the hands turn at a person's pace; the stairs are narrow).
        w.yaw = unwrap(k.yaw, p.yaw);
        const double c = core::libm::cos(static_cast<double>(w.yaw - p.yaw) * (core::kPi / 180));
        const float faces = c >= kScriptFacing ? 1.0f
                            : c > 0.5 ? static_cast<float>((c - 0.5) / (kScriptFacing - 0.5))
                                      : 0.0f;
        w.forward = k.forward * faces;
        w.jump = k.jump && c >= kScriptFacing && !p.jump_pressed;
        if (w.jump) p.stats.jumps++;
        because = k.jump ? "a scripted way: a hop up" : "a scripted way";
      }
    } else if (l.kind == NavKind::swim || node(g, b).swim ||
               (s.waterlevel >= 2 && node(g, a).swim) ||
               (p.set.floor_swim && s.waterlevel >= 2 && !s.onground && l.mover < 0 &&
                pool_floor(node(g, a)) && pool_floor(node(g, b)))) {
      // Swimming: the view pitched at the next node, forward, and jump held to rise; out of the
      // water onto a dry node by the climb the nav proved; into it off a dry edge, walked. A
      // pool's floor swum over (`floor_swim`): level, off the floor (where its friction slows a
      // swimmer) and no more than `floor_swim_up` over it, while the way to the next node is
      // clear at that height; else down at the node (under a lintel at the floor's height).
      const Vec3f me = s.origin;
      const bool floor = pool_floor(node(g, b)) && l.kind != NavKind::swim;
      float aim_z = B.z;
      if (floor && p.set.floor_swim && s.world != nullptr) {
        const float z = std::clamp(me.z, B.z + kOffFloor, B.z + p.set.floor_swim_up);
        const HullTrace tr = player_trace(*s.world, me, {B.x, B.y, z});
        if (!tr.startsolid && tr.fraction >= 1) aim_z = z;
      }
      const float flat = floor ? std::max(across(me, B), p.set.look) : across(me, B);
      const float dz = aim_z - me.z;
      w.yaw = unwrap(yaw_at(me, B), p.yaw);
      w.forward = facing(w.yaw, p.yaw);
      if (!node(g, b).swim && !(floor && p.set.floor_swim) && s.waterlevel >= 1 &&
          dz > -kSwimRise) {
        MoveState ms;
        ms.origin = me;
        ms.waterlevel = s.waterlevel;
        const MoveCmd out = swim_out(ms, B, msec);
        w.pitch = out.angles.x;
        w.jump = (out.buttons & kButtonJump) != 0;
        w.forward = 1;
        because = "climbing out of the water";
      } else if (s.waterlevel >= 2 || (!node(g, b).swim && s.waterlevel >= 1)) {
        // Through the water at the next node: a cell's centre, or a pool's floor under it.
        const double up = core::libm::atan2(static_cast<double>(dz), static_cast<double>(flat));
        float pitch = static_cast<float>(-up * (180.0 / core::kPi));
        pitch = pitch > kSwimPitch ? kSwimPitch : pitch < -kSwimPitch ? -kSwimPitch : pitch;
        w.pitch = pitch;
        w.jump = dz > kSwimRise;
        because = "swimming the route";
      } else {
        because = "into the water";
      }
      if (p.mode != PilotMode::swim) {
        p.mode = PilotMode::swim;
        p.best = 1e30f;
        p.best_s = t;
      }
      const Vec3f d = me - B;
      const float d3 = length(d);
      if (d3 < p.best - 0.5f) {
        p.best = d3;
        p.best_s = t;
      } else if (t - p.best_s > 2 * p.set.stuck_s) {
        p.stats.stuck++;
        replanned = true;
        replan(p, s);
        because = "no nearer in the water: plan again";
      }
    } else if (l.kind == NavKind::ride && !mover_in_way(p, s, l, a, b)) {
      if (p.mode != PilotMode::ride) p.stats.rides++;
      p.mode = PilotMode::ride;
      because = "carried up";
      // Over to the top node while carried: the links off the top start there, and a player who
      // boarded at an edge (late hands) walks off the far side of it. On a moving platform the
      // spot is the top node's place on its top wherever it is now (he stands still on it).
      Vec3f spot = B;
      bool platform = false;
      if (const NavMover* rm = mover_of(g.movers, l.mover);
          rm != nullptr && moving_platform(*rm) && l.stop >= 0)
        if (const NavMover* nm = mover_of(s.movers, l.mover); nm != nullptr) {
          spot = B - rm->stops[static_cast<size_t>(l.stop)] + platform_at(*rm, *nm);
          platform = true;
        }
      const float d = across(s.origin, spot);
      // Come onto a moving platform fast (off a hop): keys let go until friction has the run
      // down, as its top is no wider than a few steps (dm2's: 94 units; at 375 he ran off it).
      if (platform && s.onground && across({}, s.velocity) > kPlatformBrake) {
        w.forward = 0;
        w.side = 0;
        because = "on a moving platform too fast: braking";
      } else if (d > p.set.arrive) {
        w.yaw = unwrap(yaw_at(s.origin, spot), p.yaw);
        w.forward = facing(w.yaw, p.yaw) * (d < p.set.look * kBrake ? kBrake
                                            : d < p.set.look        ? d / p.set.look
                                                                    : 1);
        because = "carried up, stepping to the middle";
      }
      // A plat in the water (`floor_swim`): a swimmer over its top swims down onto it, as a plat
      // goes up only for a player standing on it.
      const NavMover* now = mover_of(s.movers, l.mover);
      if (p.set.floor_swim && now != nullptr && s.waterlevel >= 2 && !s.onground &&
          s.origin.z > now->absmax.z - kPlayerMins.z + kStandUp) {
        w.yaw = unwrap(yaw_at(s.origin, B), p.yaw);
        w.pitch = kSwimPitch;
        w.forward = 1;
        because = "swimming down onto the plat";
      }
    } else if (l.mover >= 0 && node(g, a).mover == l.mover && p.mode != PilotMode::air &&
               under_raised(p, s, l.mover) && plan_from(p, s) != a) {
      // A link across a raised plat's top with the player under it (carried past the node he
      // waited at): planned again from off it, never walked into its legs again and again.
      replanned = true;
      replan(p, s);
      because = "under a raised plat: plan again from off it";
    } else if (mover_in_way(p, s, l, a, b) && p.mode != PilotMode::air) {
      if (p.mode != PilotMode::wait) {
        p.mode = PilotMode::wait;
        p.wait_s = 0;
        p.stats.waits++;
      }
      p.wait_s += dt;
      w.yaw = unwrap(yaw_at(s.origin, B), p.yaw);
      because = "a mover in the way";
      // Waited for clear of the mover's box: a plat coming down on a player stops on him.
      const NavMover* in = mover_of(g.movers, l.mover);
      if (in != nullptr && !moving_platform(*in) &&
          boxes_meet(s.origin + kPlayerMins - Vec3f{kClear, kClear, 0},
                     s.origin + kPlayerMaxs + Vec3f{kClear, kClear, 0}, in->absmin,
                     Vec3f{in->absmax.x, in->absmax.y, in->absmax.z + in->travel.z})) {
        const Vec3f mid{(in->absmin.x + in->absmax.x) * 0.5f, (in->absmin.y + in->absmax.y) * 0.5f,
                        s.origin.z};
        Vec3f away{s.origin.x - mid.x, s.origin.y - mid.y, 0};
        // Straight out of the box's nearer side.
        const float hx = (in->absmax.x - in->absmin.x) * 0.5f,
                    hy = (in->absmax.y - in->absmin.y) * 0.5f;
        const float ox = (away.x < 0 ? -away.x : away.x) - hx,
                    oy = (away.y < 0 ? -away.y : away.y) - hy;
        if (ox > oy)
          away = {away.x < 0 ? -1.0f : 1.0f, 0, 0};
        else
          away = {0, away.y < 0 ? -1.0f : 1.0f, 0};
        if (p.set.floor_swim && s.world != nullptr) {
          // Out of the side nearest the player that is open (a plat in a shaft is walled on
          // some sides): the four, by how far each is from his box's edge to the mover's.
          const Vec3f sides[4] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
          const float gaps[4] = {in->absmax.x - (s.origin.x + kPlayerMins.x),
                                 (s.origin.x + kPlayerMaxs.x) - in->absmin.x,
                                 in->absmax.y - (s.origin.y + kPlayerMins.y),
                                 (s.origin.y + kPlayerMaxs.y) - in->absmin.y};
          float best = 1e30f;
          for (int i = 0; i < 4; i++) {
            const float go = gaps[i] + kClear;
            if (go >= best) continue;
            const HullTrace tr = player_trace(*s.world, s.origin, s.origin + sides[i] * go);
            if (!tr.startsolid && tr.fraction >= 1) {
              best = go;
              away = sides[i];
            }
          }
        }
        w.yaw = unwrap(heading(away.x, away.y), p.yaw);
        w.forward = 0.5f;
        because = "a mover in the way: stepping clear of it";
      }
      const NavMover* rest = mover_of(g.movers, l.mover);
      const bool door = rest == nullptr || !ridden(*rest);
      // A door away from its rest is open and still in the way: its leaf stands across this
      // link (a sliding door's half beside the doorway's middle). Every link through it its leaf
      // meets now is cut, and the way planned through the rest of the doorway.
      const NavMover* now_m = mover_of(s.movers, l.mover);
      const bool open = door && rest != nullptr && now_m != nullptr &&
                        (now_m->absmin.x != rest->absmin.x || now_m->absmin.y != rest->absmin.y ||
                         now_m->absmin.z != rest->absmin.z);
      if (open && p.wait_s > p.set.wait_s) {
        const NavGraph& gg = *p.g;
        for (size_t na = 0; na < gg.nodes.size(); na++) {
          const NavNode& n = gg.nodes[na];
          for (int32_t k = 0; k < n.links; k++) {
            const NavLink& kl =
                gg.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
            if (kl.mover != l.mover) continue;
            const Vec3f o = gg.nodes[static_cast<size_t>(kl.to)].origin;
            Vec3f lo = n.origin, hi = n.origin;
            for (int i = 0; i < 3; i++) {
              if (o[i] < lo[i]) lo[i] = o[i];
              if (o[i] > hi[i]) hi[i] = o[i];
            }
            if (boxes_meet(lo + kPlayerMins, hi + kPlayerMaxs, now_m->absmin, now_m->absmax))
              p.cut.push_back({static_cast<int32_t>(na), kl.to, kl.kind, kl.time_s});
          }
        }
        p.stats.barred++;
        replanned = true;
        replan(p, s);
        because = "waited too long at an open door: round its leaf";
      } else if (door && p.wait_s > p.set.wait_s && !is_barred(p, l.mover)) {
        for (int32_t& slot : p.barred)
          if (slot < 0) {
            slot = l.mover;
            break;
          }
        p.stats.barred++;
        replanned = true;
        replan(p, s);
        because = "waited too long: barred";
      }
    } else if (l.kind == NavKind::grenade && !p.airborne) {
      // The grenade jump's start (pilot.hpp `grenade`).
      if (p.mode != PilotMode::grenade) {
        p.mode = PilotMode::grenade;
        p.best_s = t;
      }
      const GrenadeJumpSpec gj;
      Vec3f dir{B.x - A.x, B.y - A.y, 0};
      normalize(dir);
      w.yaw = unwrap(yaw_at(s.origin, B), p.yaw);
      if (p.grenade_s < 0) {
        const float d = across(s.origin, A);
        const float v = root_of(s.velocity.x * s.velocity.x + s.velocity.y * s.velocity.y);
        // Slowing into the start from `kGrenadeSlow` off, and still anywhere within `kGrenadeNear`
        // of it (late hands coming in at a run overshoot a point and circle it); the stand is
        // taken from where the grenade really lies.
        if (d > kGrenadeNear) {
          w.yaw = unwrap(yaw_at(s.origin, A), p.yaw);
          w.forward = facing(w.yaw, p.yaw) * (d < kGrenadeSlow ? d / kGrenadeSlow : 1);
          because = "to a grenade jump's start";
        } else {
          w.pitch = l.pitch > 0 ? l.pitch : gj.pitch;
          const float off = wrap(yaw_at(s.origin, B) - p.yaw);
          if (s.onground && v < kGrenadeStill && (off < 0 ? -off : off) < kGrenadeAim &&
              s.grenade_ready) {
            w.attack = true;
            p.grenade_s = 0;
            const Vec3f at = s.world ? grenade_at(*s.world, s.origin, {w.pitch, wrap(p.yaw), 0},
                                                  gj.fuse, gj, msec)
                                     : s.origin;
            p.grenade_stand = at + dir * l.lead;
            p.grenade_yaw = wrap(yaw_at(s.origin, B));
            p.stats.grenade_jumps++;
            because = "a grenade jump: the grenade at the feet";
          } else {
            because = s.grenade_ready ? "lining up a grenade jump" : "a grenade jump: the launcher";
          }
        }
        // Never fired (walled off its start, the launcher never in hand): planned round.
        if (t - p.best_s > 2 * p.set.stuck_s) {
          missed_jump(p, {a, b, l.kind, l.time_s});
          missed_jump(p, {a, b, l.kind, l.time_s});
          replanned = true;
          replan(p, s);
          because = "a grenade jump never fired: planned round";
        }
      } else {
        p.grenade_s += dt;
        if (p.grenade_s < gj.fuse - gj.jump_before_s) {
          // The view kept on the target, stepped to the stand with the keys in its frame (a
          // person does not turn to walk a few units; late hands turning to a point circle it).
          w.yaw = unwrap(p.grenade_yaw, p.yaw);
          const ViewAxes ax = angle_vectors({0, p.yaw, 0});
          const Vec3f d{p.grenade_stand.x - s.origin.x, p.grenade_stand.y - s.origin.y, 0};
          const auto key = [&](float along) {
            const float k = along * kGrenadeStep / s.maxspeed;
            return k > 1 ? 1.f : k < -1 ? -1.f : k;
          };
          w.forward = key(dot(d, ax.forward));
          w.side = key(dot(d, ax.right));
          because = "to the stand past the grenade, the fuse burning";
        } else {
          w.forward = 1;
          w.jump = true;
          p.airborne = true;
          p.leap_s = p.clock_s;
          p.stats.jumps++;
          because = "the grenade jump: jump as the fuse runs out";
        }
      }
    } else if (flies(l.kind) && p.airborne) {
      if (!s.onground) {
        p.mode = PilotMode::air;
        // STEERED IN THE AIR (pilot.hpp `air`): the aim the target, or near it the node after it
        // when the route walks on from there (the momentum carried round the corner it turns).
        Vec3f aim = B;
        if (l2 && (l2->kind == NavKind::walk || l2->kind == NavKind::drop) &&
            across(s.origin, B) < kAirCorner) {
          // A point a little past the target toward the next node: the landing stays on the
          // target, the velocity already turning the route's way.
          Vec3f on = node(g, p.route[p.k + 2]).origin - B;
          on.z = 0;
          const float len = root_of(on.x * on.x + on.y * on.y);
          if (len > 1) aim = B + on * (kAirPast / len);
        }
        const float vh = heading(s.velocity.x, s.velocity.y);
        const float view_off = wrap(p.yaw - vh);
        const float err = wrap(yaw_at(s.origin, aim) - vh);
        const float err_abs = err < 0 ? -err : err;
        if (l.kind == NavKind::rocket && l.back && (view_off > 90 || view_off < -90)) {
          // The flip's view turning back to the target, strafing round the other way.
          w.yaw = unwrap(yaw_at(s.origin, B), p.yaw);
          flip_keys(w.yaw, view_when_keys_land(p, w.yaw, msec), w.forward, w.side);
          because = "in the flip: the view back to the target";
        } else if (!p.set.air_steer || speed < kAirSlow || err_abs > kAirGiveUp) {
          // Slow (straight up off a standing jump), or the aim behind (passed over it): the view
          // at the aim and forward, as the proof flew.
          w.yaw = unwrap(yaw_at(s.origin, aim), p.yaw);
          w.forward = 1;
          because = "in the air: toward the target";
        } else if (err_abs <= kAirHold) {
          // On the line: the view along the velocity, no key (nothing taken off the speed).
          w.yaw = unwrap(vh, p.yaw);
          because = "in the air: on the line";
        } else {
          // Off the line: the side key toward the turn (the right side turns the velocity
          // clockwise), pressed by how far off, the view led on the velocity by the turn the
          // move gives (late hands get there as it has turned).
          const float side = err > 0 ? -1.f : 1.f;
          const float share = err_abs < kAirFull ? err_abs / kAirFull : 1.f;
          float lead = -side * hop_turn_rate(speed, dt) * p.set.lead_s;
          lead = lead > 45 ? 45 : lead < -45 ? -45 : lead;
          w.yaw = unwrap(vh + lead, p.yaw);
          w.side = side * share;
          because = aim.x == B.x && aim.y == B.y ? "in the air: strafing onto the target"
                                                 : "in the air: strafing round the corner";
        }
      } else if (p.set.lift_off && p.clock_s - p.leap_s <= kLeapLiftOff_s) {
        // (`liftoff`) Not off the ground yet: the jump's key still on its way down. The jump
        // asked again, the run held on the way (a flip's keys under the view behind).
        p.mode = PilotMode::air;
        w.jump = true;
        w.yaw = p.yaw;
        if (l.kind == NavKind::rocket && l.back)
          flip_keys(yaw_at(s.origin, B), p.yaw, w.forward, w.side);
        else
          w.forward = 1;
        because = "the jump not off the ground yet";
      } else {
        // Landed, and not on the target (passed() would have moved on): plan again. A jump
        // landed short twice is not one this player makes: never planned again.
        p.stats.lost++;
        missed_jump(p, {a, b, l.kind, l.time_s});
        replanned = true;
        replan(p, s);
        because = "landed short";
      }
    } else if (leaps(l.kind) || (l2 && leaps(l2->kind) && l2->escape < 0 && l.mover < 0 &&
                                 l.kind != NavKind::drop)) {
      // A jump: from this link's start, or (a walk into a jump's start) from the next one's.
      const bool on_it = leaps(l.kind);
      const Vec3f S = on_it ? A : B;
      const Vec3f T = on_it ? B : node(g, p.route[p.k + 2]).origin;
      const float v = on_it ? l.speed : l2->speed;
      const NavExtraLink jl = on_it ? NavExtraLink{a, b, l.kind, l.time_s}
                                    : NavExtraLink{b, p.route[p.k + 2], l2->kind, l2->time_s};
      // FALLEN OFF THE RUN-UP (`underway`): well under the jump's start, or in water, it is not
      // made from here: plan again (the jump itself is not to blame).
      const bool dry_start = !node(g, on_it ? a : b).wet;
      const bool fallen =
          p.set.under_way && (p.mode == PilotMode::run_up || p.mode == PilotMode::jump) &&
          ((dry_start && s.waterlevel >= 2) || (s.onground && s.origin.z < S.z - kRunUpFallen));
      if (fallen) {
        p.stats.lost++;
        replanned = true;
        replan(p, s);
        because = "fallen off the run-up: planned again";
      } else {
        // (`giveup`) The same jump from the node beside (a plan again starts it there): its end
        // and kind, not its exact link.
        const bool same_try =
            p.set.give_up && p.try_to >= 0 && p.try_kind == jl.kind &&
            across(node(g, jl.to).origin, node(g, p.try_to).origin) <= kTryKin &&
            std::fabs(node(g, jl.to).origin.z - node(g, p.try_to).origin.z) <= kTryKin;
        if (!same_try && (jl.from != p.try_from || jl.to != p.try_to)) {
          p.try_kind = jl.kind;
          p.try_from = jl.from;
          p.try_to = jl.to;
          p.tries = 0;
        }
        const int32_t run_ups = p.stats.run_ups;
        const NavLink& jumped = on_it ? l : *l2;
        const bool flip = jumped.kind == NavKind::rocket && jumped.back;
        const float shot_pitch = jumped.pitch > 0 ? jumped.pitch
                                 : flip           ? p.set.rocket_back_pitch
                                                  : p.set.rocket_pitch;
        if (jumped.kind == NavKind::rocket) p.shot_pitch = shot_pitch;
        bool jumped_now = approach_jump(p, s, S, T, v, w, target, because, flip, msec,
                                        jumped.kind == NavKind::jump);
        if (jumped_now && jumped.kind == NavKind::rocket && p.set.rocket_wait && !s.rocket_ready) {
          // (`rjready`) The launcher not up (or not ready to fire): no jump; run on, line up again.
          jumped_now = false;
          w.jump = false;
          because = "a rocket jump: the launcher not ready";
        }
        if (jumped_now) {
          // A rocket jump: the shot at the feet with the jump (its blast does the rest); the flip's
          // behind, down at the floor it runs off.
          if (jumped.kind == NavKind::rocket) {
            w.pitch = shot_pitch;
            w.attack = true;
            p.shot_now = true;
            p.shot_in_s = 0;
            because = flip ? "a flip: the shot behind with the jump"
                           : "a rocket jump: the shot at the feet with the jump";
            p.stats.rocket_jumps++;
            if (flip) p.stats.flips++;
          }
          p.stats.jumps++;
          if (!on_it) advance(p, s);
          p.airborne = true;
          p.leap_s = p.clock_s;
          p.mode = PilotMode::air;
        } else if (p.stats.run_ups != run_ups && ++p.tries >= kRunUpTries) {
          // Lined up again and again and never off (the run-up walled in, the line never held):
          // not a jump this player makes.
          missed_jump(p, jl);
          missed_jump(p, jl);
          p.tries = 0;
          replanned = true;
          replan(p, s);
          because = "a jump never made: planned round";
        }
      }
    } else {
      if (p.mode == PilotMode::wait || p.mode == PilotMode::run_up || p.mode == PilotMode::jump ||
          p.mode == PilotMode::swim)
        p.mode = PilotMode::walk;
      // (`liplook`) Near lava or slime the look runs `lip_look_s` ahead at the run's speed: a
      // corner by the edge turned early, inside it, not overshot over the edge.
      float look = p.set.look;
      if (p.set.lip_look_s > 0 && nav_harm_near(*p.g, s.origin, kLipNear)) {
        const float run = across({}, s.velocity) * p.set.lip_look_s;
        if (run > look) look = run;
      }
      const Vec3f aim = via_aim(p, s, ahead, look, pursue(p, ahead, look));
      target = lane_target(p, dt, t, aim);
      const bool foreseeing = p.set.ahead && s.world != nullptr && s.onground;
      if (foreseeing) target = aim_clear(p, *s.world, s.origin, ahead, aim, target);
      // A player standing on the way (`ahead_bodies`): round him, or wait where there is no room.
      Vec3f round;
      const bool body = foreseeing && p.set.ahead_bodies &&
                        body_on_way(*s.world, s.origin, target, s.others, round);
      const bool wait_body = body && across(round, s.origin) < 1;
      if (body && !wait_body) target = round;
      if (across(ahead, target) > 1) w.yaw = unwrap(yaw_at(ahead, target), p.yaw);
      w.forward = wait_body ? 0.f : facing(w.yaw, p.yaw);
      bool fall_seen = false;
      const WalkWay seen =
          foreseeing ? foresee(p, s, target, B, w, why, fall_seen) : WalkWay::pursue;
      // Slowing into the goal (late hands run past a small top and off it, or orbit it).
      const float d = across(ahead, node(*p.g, p.route.back()).origin);
      if (d < p.set.look) w.forward *= d < p.set.look * kBrake ? kBrake : d / p.set.look;
      // (`liftbrake`) The next link boards a lift not there (away from its rest, or on its way):
      // the keys let go within the run's stop of its edge, so the pilot waits there, not in its
      // empty shaft (dm2's RA lift: run into at 390 units/s, ~4 % of the bots' time waited there,
      // in the shaft under it as it came down onto them and went back up).
      bool lift_brake = false;
      if (p.set.lift_brake && s.onground && l2 != nullptr && l2->mover >= 0 &&
          mover_in_way(p, s, *l2, b, p.route[p.k + 2])) {
        const float v = root_of(s.velocity.x * s.velocity.x + s.velocity.y * s.velocity.y);
        if (across(s.origin, B) < v * kLiftStop_s + kLiftStopPad) {
          w.forward = 0;
          w.side = 0;
          lift_brake = true;
        }
      }
      if (!s.onground && s.waterlevel == 0 && !p.airborne) {
        p.walk_air = true;
        p.walk_air_yaw = yaw_at(s.origin, target);
      }
      because = lift_brake                ? "a lift not there ahead: braking short of it"
                : wait_body               ? "a player standing in the way: waiting for him"
                : body                    ? "a player standing in the way: round him"
                : seen != WalkWay::pursue ? walk_way_because(seen, fall_seen)
                : l.kind == NavKind::drop ? "walking off the edge"
                                          : "walking the route";
      // (`walkmash`) A full run on dry ground, nothing ahead to brake for: forward and a strafe
      // key swapped every `mash_s`, the wish 45 degrees off the run on either side, so the ground
      // builds the speed past the run's (people spend a quarter of their time on the ground at
      // 320-360 units/s, where forward alone holds 320).
      if (p.set.walk_mash && s.onground && s.waterlevel == 0 && w.forward >= 0.99f &&
          seen == WalkWay::pursue && !body && !lift_brake && l.kind != NavKind::drop &&
          !nav_harm_near(*p.g, s.origin, kLipNear)) {
        if (p.clock_s - p.mash_at >= p.set.mash_s) {
          p.mash_side = static_cast<int8_t>(-p.mash_side);
          p.mash_at = p.clock_s;
        }
        // The keys, not the wish: a keyboard presses W and this side whole while the view lies
        // along the run (bots/keyboard.cpp); turned under a view off it, the wish would be one
        // strafe key alone.
        p.curve_keys = p.mash_side;
        if (p.curve_keys_last == kNoCurveKeys) p.mash_from = p.clock_s;  // (`mashstart`)
        p.stats.mashed++;
      }
      // A flip ahead within its turn: turning round into it already, strafing along the route.
      float back = 0;
      float flip_v = 0;
      if (flip_ahead(p, s, back, flip_v) && d_to_flip(p, s) <= flip_v * flip_lead_s(p) + 2) {
        const float way = w.yaw, share = w.forward;
        w.yaw = unwrap(back, p.yaw);
        flip_keys(way, view_when_keys_land(p, w.yaw, msec), w.forward, w.side);
        w.forward *= share;
        w.side *= share;
        because = "turning round into a flip";
      }
    }

    // STUCK and LOST, on the ground while moving under its own legs (or swimming a walk).
    const bool swimming_walk =
        p.set.floor_swim && s.waterlevel >= 2 && !s.onground && p.mode == PilotMode::walk;
    const bool fallen_walk = p.set.under_way && s.onground && p.mode == PilotMode::walk;
    if (!replanned && (swimming_walk || fallen_walk) && p.k + 1 < p.route.size() &&
        std::min(node(g, p.route[p.k]).origin.z, node(g, p.route[p.k + 1]).origin.z) - s.origin.z >
            p.set.lost_under) {
      p.stats.lost++;
      replanned = true;
      replan(p, s);
      because = swimming_walk ? "in the water under the way: plan again"
                              : "fallen under the way: plan again";
    }
    if (!replanned && (s.onground || swimming_walk) &&
        (p.mode == PilotMode::walk || p.mode == PilotMode::jump || p.mode == PilotMode::run_up)) {
      const float d = across(s.origin, target);
      if (d < p.best - 0.5f) {
        p.best = d;
        p.best_s = t;
      } else if (t - p.best_s > p.set.stuck_s) {
        p.stats.stuck++;
        // No nearer on the way to a jump (its run-up walled in): a miss of that jump.
        if (leaps(l.kind))
          missed_jump(p, {a, b, l.kind, l.time_s});
        else if (l2 && leaps(l2->kind))
          missed_jump(p, {b, p.route[p.k + 2], l2->kind, l2->time_s});
        replan(p, s);
        because = "no nearer: plan again";
      } else if (p.k + 1 < p.route.size() &&
                 off_segment(s.origin, node(g, p.route[p.k]).origin,
                             node(g, p.route[p.k + 1]).origin) > p.set.lost) {
        p.stats.lost++;
        replan(p, s);
        because = "off the way: plan again";
      }
    }
  }

  if (why) {
    why->decide("link");
    why->fact("k", static_cast<double>(p.k));
    why->fact("speed", static_cast<double>(speed));
    why->fact("onground", s.onground ? 1 : 0);
    if (p.k + 1 < p.route.size()) {
      const NavLink* l = link_of(g, p.route[p.k], p.route[p.k + 1]);
      why->fact("kind", l ? static_cast<double>(l->kind) : -1);
      why->fact("to", static_cast<double>(p.route[p.k + 1]));
      why->fact("dist", static_cast<double>(across(s.origin, node(g, p.route[p.k + 1]).origin)));
    }
    why->choose(pilot_mode_name(p.mode), because);
  }
  // A moving platform's link on the route's next steps: its lip is where the platform stops
  // (boarded at a step, waited for), never jumped off (dm2's high mega's ledge: a hop's lip jump
  // flew off it into the lava with the platform away).
  bool platform_ahead = false;
  for (size_t q = p.k; q + 1 < p.route.size() && q < p.k + 8 && !platform_ahead; q++) {
    const NavLink* pl = link_of(g, p.route[q], p.route[q + 1]);
    platform_ahead = pl != nullptr && pl->stop >= 0;
  }
  // AT A LIP (`lipjump`): a hop on the ground about to run off its floor jumps there: a person
  // never runs off a lip without the jump (it carries him furthest; on a trick map, across).
  if (platform_ahead && s.world != nullptr && p.mode == PilotMode::hop && s.onground && w.jump &&
      lip_ahead(*s.world, s.origin, s.velocity, msec)) {
    w.jump = false;
    because = "a moving platform's lip ahead: no jump off it";
  } else if (p.set.lip_jump && p.mode == PilotMode::hop && s.onground && !w.jump &&
             !p.jump_pressed && !platform_ahead && s.world != nullptr && speed > kLipSpeed &&
             lip_ahead(*s.world, s.origin, s.velocity, msec)) {
    w.jump = true;
    because = "at a lip: jump";
    p.stats.lip_jumps++;
    p.lip_pressed_s = p.clock_s;
    p.lip_yaw = lip_crossing(*s.world, s.origin, heading(s.velocity.x, s.velocity.y),
                             route_heading(p, s.origin, heading(s.velocity.x, s.velocity.y)));
  } else if (p.set.lip_jump && p.mode == PilotMode::hop && s.onground && w.jump &&
             !p.jump_pressed && !p.gap_flight && !p.forecasting && s.world != nullptr &&
             speed > kSlow) {
    // A HOP THAT FALLS: a hop's jump whose flight (forward along the run) comes down under the
    // floor it leaves, into a pit beside the way, is not pressed: run on (unless the lip is here).
    MoveSettings move = p.g->move;
    move.maxspeed = s.maxspeed;
    const Vec3f aim{s.origin.x + s.velocity.x, s.origin.y + s.velocity.y, s.origin.z};
    // `pilotcast`: flown by a copy of the pilot (its strafe, its view, its hands), not forward.
    const JumpFlight f = p.set.pilot_cast
                             ? hop_cast(p, s, msec)
                             : fly_jump(*s.world, move, s.origin, s.velocity, aim, msec);
    const bool lip = lip_ahead(*s.world, s.origin, s.velocity, msec);
    if ((!f.landed || f.end.z < s.origin.z - kStepHeight - 8) && !lip) {
      w.jump = false;
      because = "a hop that falls in a pit: run on";
    } else if (lip && speed > kLipSpeed) {
      // A hop's own jump at a lip: flown across the pit as a lip's jump is.
      p.lip_pressed_s = p.clock_s;
      p.lip_yaw = lip_crossing(*s.world, s.origin, heading(s.velocity.x, s.velocity.y),
                               route_heading(p, s.origin, heading(s.velocity.x, s.velocity.y)));
    }
  }
  p.because = because;

  // Through the hands.
  const double wanted[4] = {w.yaw, w.forward, w.side, w.jump ? 1.0 : 0.0};
  double pressed[4];
  static constexpr const char* kNames[3] = {"yaw", "forward", "side"};
  ai::record_intent(why, kNames, std::span<const double>(wanted, 3));
  p.hands.step(wanted, pressed);
  for (int i = 0; i < 3; i++) {
    p.wanted[i] = wanted[i];
    p.pressed[i] = pressed[i];
  }
  p.yaw = static_cast<float>(pressed[0]);
  MoveCmd c;
  c.msec = static_cast<uint8_t>(msec);
  c.angles = {quantize_angle(w.pitch), quantize_angle(wrap(p.yaw)), 0};
  c.forward = quantize_axis(static_cast<float>(pressed[1]) * s.maxspeed);
  c.side = quantize_axis(static_cast<float>(pressed[2]) * s.maxspeed);
  p.jump_pressed = pressed[3] > 0.5;
  c.buttons =
      static_cast<uint8_t>((p.jump_pressed ? kButtonJump : 0) | (w.attack ? kButtonAttack : 0));
  return c;
}

std::vector<Vec3f> pilot_path(const Pilot& p, Vec3f at, std::vector<uint8_t>* breaks) {
  std::vector<Vec3f> out{at};
  if (breaks) breaks->assign(1, 0);
  if (p.g == nullptr || p.k + 1 >= p.route.size()) return out;
  const NavGraph& g = *p.g;
  bool lane = p.offset != 0;
  for (size_t k = p.k; k + 1 < p.route.size(); k++) {
    const Vec3f a = node(g, p.route[k]).origin, b = node(g, p.route[k + 1]).origin;
    const NavLink* l = link_of(g, p.route[k], p.route[k + 1]);
    lane =
        lane && l != nullptr && through(*l) && level(g, p.route[k], p.route[k + 1], p.set.slopes);
    // A teleporter's link: far across and next to no time.
    if (breaks)
      breaks->push_back(l != nullptr &&
                                (l->teleport || (l->kind == NavKind::walk && l->time_s < 0.2f &&
                                                 across(a, b) > 4 * g.set.spacing))
                            ? 1
                            : 0);
    Vec3f side{-(b.y - a.y), b.x - a.x, 0};
    if (!lane || normalize(side) <= 0) {
      out.push_back(b);
      continue;
    }
    out.push_back(b + side * p.offset);
  }
  return out;
}

}  // namespace ge::shooter
