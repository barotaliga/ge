// THE RUN-UP AND THE JUMP (pilot_step_parts.hpp): a jump missed and the ones beside it given up,
// the flip's turn round and its lead, the view the keys land under, the heading across a pit at a
// lip and the strafe across it, and the run-up, the run and the jump itself (`approach_jump`).
//
// ge:summary The pilot's run-up and jump: the line, the run, the flip, a lip's jump, a miss
// ge:keywords pilot approach_jump run-up jump flip missed_jump lip_crossing strafe_across
// ge:keywords lip_ahead lipjump jumpfly backstep runupkeys view_when_keys_land shot_view_ready
// ge:skill shooterbots bots
// ge:test tests/shooterbots_pilot_test.cpp
#include "shooterbots/pilot_gaps.hpp"
#include "shooterbots/pilot_step_parts.hpp"

#include "shooterbots/kit.hpp"

namespace ge::shooterbots::pilot_parts {

namespace {

constexpr float kJumpAlong = 0.9f;  // a jump goes moving at least this share of its speed
constexpr float kJumpOver = 1.15f;  // and at most this share (and 16 u/s) of it
constexpr float kJumpAim = 0.95f;   // with the velocity within this cosine of the jump's way
constexpr float kPastStart = 24;    // past a jump's start this far without its speed: run up
constexpr float kFlyFrom = 24;      // `jumpfly`: a jump flown from this far before its start
constexpr float kStillJump = 20;    // a standing jump up into an updraft: slower than this, units/s
constexpr float kStillAt = 12;      // and this near its foot, units
constexpr float kStillKeys =
    160;                        // a velocity this far off the wished one: keys held full, units/s
constexpr float kAcrossOn = 3;  // `strafe_across`: on the crossing within this, degrees
constexpr float kAcrossBeat_s = 0.04f;  // and on it the side keys swapped this often, s
// `lip_crossing`: the headings tried, degrees
constexpr int kCrossFan = 60, kCrossStep = 5;
constexpr float kCrossEvery = 8, kCrossMost = 640;  // and the places along each, units
constexpr float kCrossWay = 2;           // a crossing's cost a degree off the route's way, units
constexpr float kWayAhead = 160;         // the route's way read at its node this far on, units
constexpr double kCrossTurnFloor = 0.5;  // a turn within 60 degrees costs nothing (made in the air)
constexpr float kLipAhead = 4;           // `lipjump`: a lip looked for this many commands ahead
constexpr float kRunOnFrom = 8;  // `backstep`: a run-up this far behind the start or more runs on
constexpr float kGiveUpFromKin = 128;  // (`giveup`) and its start this near
constexpr float kJumpKin = 72;  // a jump given up takes those beside it, start and end this near
constexpr float kSlowing_s = 0.25f;  // a run slows to a jump's speed in about this, s
constexpr float kFacing = 10;        // a run-up goes when the view is this near the jump's way, deg
constexpr float kTurnRate = 400;     // a hand turning round on the spot, degrees a second
constexpr float kFlipFacing = 25;    // a flip goes with the view this near straight behind, deg
constexpr float kFlipSpare_s = 0.05f;  // a flip's turn starts this much earlier than it takes, s
constexpr float kOnLine = 10;          // a jump is run at on its line within this across, units
constexpr float kHarmUnder = 48;       // `over_harm`: lava or slime this near across under the way

// A FLIP on the route ahead past only walks: true, `back` the view's heading at its shot (behind,
// away from its target) and `v` its speed; `d_to_flip` how far along the route its start is.
size_t flip_index(const Pilot& p) {
  const NavGraph& g = *p.g;
  for (size_t j = p.k + 1; j + 1 < p.route.size() && j <= p.k + 12; j++) {
    const NavLink* l = link_of(g, p.route[j], p.route[j + 1]);
    if (!l) return 0;
    if (l->kind == NavKind::rocket && l->back) return j;
    if (l->kind != NavKind::walk) return 0;
  }
  return 0;
}

// `jumpfly`: a jump pressed now lands by `T` (within a gap's landing, pilot_gaps.hpp), and under
// a keyboard pressed a command or two late too, flown by the move from here.
bool jump_flies(const Pilot& p, const PilotSense& s, int msec, Vec3f T) {
  if (s.world == nullptr) return false;
  MoveSettings move = p.g->move;
  move.maxspeed = s.maxspeed;
  if (!gap_lands(*s.world, move, s.origin, s.velocity, T, msec, p.set.gap_past, p.set.gap_strafe))
    return false;
  return !p.set.curve_keys || gap_lands_late(p, s, msec, T);
}

}  // namespace

// A jump missed (landed short, or no nearer its start): the second miss of the same one is not a
// jump this player makes, and it is never planned again.
void missed_jump(Pilot& p, NavExtraLink j) {
  // An updraft's jump is never missed: the draft lifts whoever is in it, and where it sets him down
  // (off the landing node, by the tube's top) is no fault of the jump. Cut, it shut off e3m5's
  // upper floors from below for the rest of the map.
  if (const NavLink* l = link_of(*p.g, j.from, j.to); l != nullptr && l->updraft) return;
  int times = 0;
  for (const NavExtraLink& e : p.short_jumps) times += e.from == j.from && e.to == j.to ? 1 : 0;
  if (p.short_jumps.size() < 256) p.short_jumps.push_back(j);
  if (times != 1) return;
  // It and every jump beside it (the graph lays a jump from each node along an edge): from within
  // kJumpKin of its start to within kJumpKin of its end.
  const NavGraph& g = *p.g;
  const Vec3f fa = node(g, j.from).origin, fb = node(g, j.to).origin;
  for (size_t na = 0; na < g.nodes.size(); na++) {
    const NavNode& n = g.nodes[na];
    // (`giveup`) The starts further along too: a plan again starts the same jump a node or three
    // along its run-up (dm2's flip to the quad, from nodes 96 units apart).
    const float from_kin = p.set.give_up ? kGiveUpFromKin : kJumpKin;
    if (across(n.origin, fa) > from_kin || n.origin.z > fa.z + kJumpKin ||
        n.origin.z < fa.z - kJumpKin)
      continue;
    for (int32_t k = 0; k < n.links; k++) {
      const NavLink& kl = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
      const Vec3f to = node(g, kl.to).origin;
      const NavKind kin = p.set.give_up ? j.kind : NavKind::jump;  // (`giveup`) its own kind
      if (kl.kind != kin || across(to, fb) > kJumpKin || to.z > fb.z + kJumpKin ||
          to.z < fb.z - kJumpKin)
        continue;
      const NavExtraLink e{static_cast<int32_t>(na), kl.to, kl.kind, kl.time_s};
      p.failed.push_back(e);
      p.cut.push_back(e);
    }
  }
}

// The run-up and the jump from S toward T at `v`: line up behind S (the run-up point, `run_up`
// back along the jump's way), run at S with the forward axis held to `v`, and jump at S moving
// at it along the way. True on the command the jump is pressed.
// How long before a flip's start its turn round begins: what the view's hand takes to turn 180
// degrees, its delay and its slew over them (within 0.02 s of the time each profile's hand
// takes to come within 25 degrees), and a little to spare; at least a few commands.
float flip_lead_s(const Pilot& p) {
  const ai::AxisHands& h = p.hands.spec(0);
  const double t = h.delay_s + (h.slew > 0 ? 180 / h.slew : 0) + kFlipSpare_s;
  return t > 0.03 ? static_cast<float>(t) : 0.03f;
}

// The view the hands will show when keys pressed now arrive (the keys' delay), the view wanted
// held meanwhile: worked out on a copy of the hands. A turn strafed with keys for the view as it
// is now presses them for a view already gone.
float view_when_keys_land(const Pilot& p, float want, int msec) {
  const ai::AxisHands& k = p.hands.spec(1);
  // The commands in the keys' delay, to the nearest.
  int steps = 0;
  while ((steps + 0.5) * msec < k.delay_s * 1000) steps++;
  if (steps <= 0) return p.yaw;
  ai::Hands h = p.hands;
  const double wanted[4] = {want, p.wanted[1], p.wanted[2], 0};
  double out[4] = {};
  const size_t n = h.axes() < 4 ? h.axes() : 4;
  for (int i = 0; i < steps; i++)
    h.step(std::span<const double>(wanted, n), std::span<double>(out, n));
  return static_cast<float>(out[0]);
}

bool flip_ahead(const Pilot& p, const PilotSense& s, float& back, float& v) {
  (void)s;
  const size_t j = flip_index(p);
  if (j == 0) return false;
  const NavGraph& g = *p.g;
  const Vec3f n = node(g, p.route[j]).origin, t = node(g, p.route[j + 1]).origin;
  back = heading(t.x - n.x, t.y - n.y) + 180;
  v = link_of(g, p.route[j], p.route[j + 1])->speed;
  return true;
}

float d_to_flip(const Pilot& p, const PilotSense& s) {
  const size_t j = flip_index(p);
  if (j == 0) return 1e30f;
  Vec3f at = s.origin;
  float went = 0;
  for (size_t i = p.k + 1; i <= j; i++) {
    const Vec3f n = node(*p.g, p.route[i]).origin;
    went += across(at, n);
    at = n;
  }
  return went;
}

// A rocket jump's shot may go: none ahead, no hand flying the view, or the hand there.
bool shot_view_ready(const Pilot& p) { return p.shot_pitch == 0 || !p.shot_hand || p.shot_ready; }

// `lipjump`: the heading across the pit ahead of `o` (moving along `vh`) to the floor beyond,
// within `kCrossFan` of the velocity: along each heading of the fan, the first place past a lip
// with a floor within a step under the player's box; the one costing least as distance over the
// share of the speed along it (a turn in the air loses the rest), and `kCrossWay` units a degree
// off `way` (the route's heading on: a crossing off it, onto another floor, is not the way).
// `vh` itself when none is found.
// The route's heading on from `at`: at its first node `kWayAhead` or more away (the last else).
float route_heading(const Pilot& p, Vec3f at, float fallback) {
  for (size_t j = p.k + 1; j < p.route.size(); j++) {
    const Vec3f n = node(*p.g, p.route[j]).origin;
    if (across(at, n) >= kWayAhead || j + 1 == p.route.size()) return yaw_at(at, n);
  }
  return fallback;
}

float lip_crossing(const MoveWorld& world, Vec3f o, float vh, float way) {
  float best = vh, cost = 1e30f;
  for (int a = -kCrossFan; a <= kCrossFan; a += kCrossStep) {
    const float h = vh + static_cast<float>(a);
    const double r = static_cast<double>(h) * (core::kPi / 180);
    const Vec3f dir{static_cast<float>(core::libm::cos(r)), static_cast<float>(core::libm::sin(r)),
                    0};
    const double turn = core::libm::cos(static_cast<double>(a) * (core::kPi / 180));
    bool gap = false;
    // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter): whole steps, exact in float
    for (float d = kCrossEvery; d <= kCrossMost; d += kCrossEvery) {
      const Vec3f at = o + dir * d;
      const HullTrace tr = player_trace(world, at, {at.x, at.y, at.z - kStepHeight - 2});
      if (tr.startsolid || tr.allsolid) break;  // a wall (the far side higher): not this way
      const bool floor = tr.fraction < 1;
      if (!floor) gap = true;
      if (gap && floor) {
        const float off = wrap(h - way);
        const float c = d / static_cast<float>(turn > kCrossTurnFloor ? 1.0 : turn) +
                        kCrossWay * (off < 0 ? -off : off);
        if (c < cost) cost = c, best = h;
        break;
      }
    }
  }
  return best;
}

// A flight strafed across, `off` degrees off the velocity (+ left): the view on the velocity,
// forward let go, the side key that turns the velocity toward it (the right one turns it
// clockwise), no key once within `kAcrossOn` (a key swapped each command under late hands swung
// the heading 30 degrees about the line): the heading brought onto the crossing and held.
// `dt`: the time to add to the side's hold (0 where the caller has).
void strafe_across(Pilot& p, float vh, float off, Want& w, float dt) {
  p.side_s += dt;
  w.yaw = unwrap(vh, p.yaw);
  w.forward = 0;
  w.side = off < -kAcrossOn ? 1.0f : off > kAcrossOn ? -1.0f : 0.0f;
  if (w.side != 0) {
    p.strafe_side = static_cast<int8_t>(w.side > 0 ? 1 : -1);
    p.side_s = 0;
    return;
  }
  // On the line: the view held on it and the side keys swapped every `kAcrossBeat_s`, the air's
  // gain each way.
  w.yaw = unwrap(vh + off, p.yaw);
  if (p.side_s >= kAcrossBeat_s) {
    p.strafe_side = static_cast<int8_t>(p.strafe_side >= 0 ? -1 : 1);
    p.side_s = 0;
  }
  w.side = static_cast<float>(p.strafe_side);
}

// `lipjump`: the player's box `kLipAhead` commands on at `v` (a keyboard's jump key goes down a
// command or two late) has no floor within a step under it (in a wall there is no lip: false).
bool lip_ahead(const MoveWorld& world, Vec3f o, Vec3f v, int msec) {
  const float dt = kLipAhead * static_cast<float>(msec) * 0.001f;
  const Vec3f at{o.x + v.x * dt, o.y + v.y * dt, o.z};
  const HullTrace tr = player_trace(world, at, {at.x, at.y, at.z - kStepHeight - 2});
  return !tr.startsolid && !tr.allsolid && tr.fraction >= 1;
}

// Whether a jump from `S` to `T` flies over lava or slime: a harm node (nav.hpp `nav_harm_near`)
// under a quarter, the half or three quarters of its way, at the higher end's height.
bool over_harm(const NavGraph& g, Vec3f S, Vec3f T) {
  if (g.harm.empty()) return false;
  const float z = S.z > T.z ? S.z : T.z;
  for (const float f : {0.25f, 0.5f, 0.75f})
    if (nav_harm_near(g, {S.x + (T.x - S.x) * f, S.y + (T.y - S.y) * f, z}, kHarmUnder))
      return true;
  return false;
}

// `flip`: a rocket jump played as the flip (nav.hpp `RocketJumpSpec::back`): the view turned round
// into the jump from `rocket_flip_lead_s` before its start, strafing through the turn (the keys a
// hand presses nearest the way, `flip_keys`), and the shot only with the view behind.
bool approach_jump(Pilot& p, const PilotSense& s, Vec3f S, Vec3f T, float v, Want& w, Vec3f& target,
                   const char*& because, bool flip, int msec, bool plain, bool still) {
  Vec3f dir{T.x - S.x, T.y - S.y, 0};
  normalize(dir);
  const Vec3f rel{s.origin.x - S.x, s.origin.y - S.y, 0};
  const float along = dot(rel, dir);
  const float lat = rel.x * dir.y - rel.y * dir.x;
  const float lat_abs = lat < 0 ? -lat : lat;
  const float speed = across({}, s.velocity);
  const float along_v = dot(s.velocity, dir);
  const float cosv = speed > 1 ? along_v / speed : 0;
  const float run_up = p.g->set.run_up;
  if (v <= 0) {
    // A standing jump (proven with no run-up: up a stair's tread): to S, then facing T, forward
    // held to the full and the jump pressed, as the proof flew it.
    target = S;
    p.mode = PilotMode::jump;
    const float d = across(s.origin, S);
    if (d > p.set.arrive && along < 0) {
      w.yaw = unwrap(yaw_at(s.origin, S), p.yaw);
      w.forward = d > 2 * p.set.look ? 1 : d / (2 * p.set.look);
      because = "to the foot of a standing jump";
      return false;
    }
    // A standing flip: turned round on the spot, then the back key with the jump.
    const float up = heading(dir.x, dir.y) + (flip ? 180 : 0);
    w.yaw = unwrap(up, p.yaw);
    w.forward = flip || still ? 0.0f : 1.0f;
    // Up into an updraft: from a stand (a step's way on and the draft takes him by its side, where
    // e3m5's tube top held him under its lip).
    const float to_s = across(s.origin, S);
    if (still && (speed > kStillJump || to_s > kStillAt)) {
      // Held to its foot (the run in carries him on past it): the keys ask for the velocity that
      // closes on it in a quarter second, so the way on is braked rather than circled.
      const float gain = 4;
      float wx = (S.x - s.origin.x) * gain - s.velocity.x;
      float wy = (S.y - s.origin.y) * gain - s.velocity.y;
      const float wl = across({}, {wx, wy, 0});
      const float k = wl > kStillKeys ? 1.0f : wl / kStillKeys;
      if (wl > 0) wx /= wl, wy /= wl;
      const double rs = static_cast<double>(heading(wx, wy) - p.yaw) * (core::kPi / 180);
      w.forward = static_cast<float>(core::libm::cos(rs)) * k;
      w.side = static_cast<float>(-core::libm::sin(rs)) * k;
      because = "stopping under an updraft";
      return false;
    }
    const float off = wrap(p.yaw - up);
    // The shot when the turn is done: the rest of it at about a hand's about-face.
    p.shot_yaw = up;
    p.shot_in_s = ((off < 0 ? -off : off) - kFacing) / kTurnRate;
    if (p.shot_in_s < 0) p.shot_in_s = 0;
    if (s.onground && !p.jump_pressed && off < kFacing && off > -kFacing && shot_view_ready(p)) {
      if (flip) flip_keys(heading(dir.x, dir.y), p.yaw, w.forward, w.side);
      w.jump = true;
      because = flip ? "a standing flip" : "a standing jump up";
      return true;
    }
    because = flip ? "turning round for a standing flip" : "turning to a standing jump";
    return false;
  }
  // The run-up point: as far back as the graph found room to run up (a flip's farther: room for
  // its turn at the speed).
  const float back_up = run_up + (flip ? v * flip_lead_s(p) : 0);
  const Vec3f P = S - dir * back_up;
  if (p.set.lip_jump && !s.onground && !p.airborne && s.waterlevel == 0) {
    // In the air from a jump of the walk's (a hop's, one at a lip): a person never lines up in
    // the air (the back key there threw away a lip jump's flight across a pit, 450 -> 40 u/s);
    // the flight kept, the view on it, no keys, until it is down.
    target = S;
    if (speed > 1) w.yaw = unwrap(heading(s.velocity.x, s.velocity.y), p.yaw);
    w.forward = 0;
    w.side = 0;
    because = "in the air: lining up once down";
    if (p.lip_pressed_s >= 0 && speed > 1) {
      const float vh = heading(s.velocity.x, s.velocity.y);
      strafe_across(p, vh, wrap(p.lip_yaw - vh), w, static_cast<float>(msec) * 0.001f);
      because = "a lip's jump: strafing across the pit";
    }
    return false;
  }
  if (p.mode != PilotMode::run_up && p.mode != PilotMode::jump) p.mode = PilotMode::jump;
  // AT A LIP ON A JUMP'S RUN (`lipjump`): about to run off its floor at a run (past the start, or
  // turned to line up again with the momentum still on), a plain jump's run jumps there at T: a
  // person never runs off a lip without the jump (a beam's end was run off lining up again).
  if (plain && p.set.lip_jump && s.onground && !p.jump_pressed && speed > kLipSpeed &&
      s.world != nullptr && shot_view_ready(p) && lip_ahead(*s.world, s.origin, s.velocity, msec)) {
    p.mode = PilotMode::jump;
    w.jump = true;
    w.forward = 1;
    w.side = 0;
    w.yaw = unwrap(yaw_at(s.origin, T), p.yaw);
    p.stats.lip_jumps++;
    because = "a jump's run at a lip: jump";
    return true;
  }
  // FLOWN FROM HERE (`jumpfly`, a plain jump): near the start and not on its line at its speed, a
  // jump whose flight from here lands by T is taken as it is.
  bool flown = false;
  if (p.mode == PilotMode::jump && plain && p.set.jump_fly && s.onground && !p.jump_pressed &&
      along >= -kFlyFrom && along <= kPastStart && shot_view_ready(p)) {
    const bool strict = along >= -2 && lat_abs <= kOnLine && along_v >= kJumpAlong * v &&
                        along_v <= kJumpOver * v + 16 && cosv >= kJumpAim;
    // Over lava or slime, where the air's keys carry the flight (`air_carries`), only on the
    // proof's own line and speed: a flight guessed from off it (a knock, a fight's keys) that
    // lands short ends in the pit (mintro's ledge over its lava).
    flown =
        !strict && (!air_carries(p.g->move) || !over_harm(*p.g, S, T)) && jump_flies(p, s, msec, T);
  }
  if (p.mode == PilotMode::jump && !flown) {
    // Off the line at the start (the run steers back onto it before), or past it without the
    // speed: run up again.
    const bool off_line = along > -2 && (lat_abs > kOnLine || (speed > kSlow && cosv < kJumpAim));
    const bool overran = along > kPastStart || (along > 2 && (along_v < kJumpAlong * v));
    if (off_line || overran) {
      p.mode = PilotMode::run_up;
      p.stats.run_ups++;
      p.best = 1e30f;
    }
  }
  // THE RUN ON (`backstep`): moving down the jump's line behind its start, it runs on from here.
  if (p.mode == PilotMode::run_up && p.set.backstep && !flip && s.onground && along < -kRunOnFrom &&
      along > -(back_up + p.set.look) && lat_abs <= kOnLine && speed > kSlow && cosv >= kJumpAim &&
      along_v <= kJumpOver * v + 16) {
    p.mode = PilotMode::jump;
    p.best = 1e30f;
  }
  if (p.mode == PilotMode::run_up) {
    target = P;
    const float d = across(s.origin, P);
    p.shot_in_s = 1e9f;                        // lining up: no shot soon
    const float down = heading(dir.x, dir.y);  // the view down the jump
    if (p.set.backstep && !flip && d > p.set.arrive) {
      // STEPPED BACK: the view kept down the jump, the keys toward the point under the view as
      // it will be when they land (`runupkeys`; keys for a view not reached yet walk the player
      // off the wrong way, off a walkway's edge while the view turns).
      w.yaw = unwrap(down, p.yaw);
      const float go = d > 2 * p.set.look ? 1 : d / (2 * p.set.look);
      const Vec3f to{(P.x - s.origin.x) / d, (P.y - s.origin.y) / d, 0};
      const float keys_view = p.set.runup_keys ? view_when_keys_land(p, w.yaw, msec) : down;
      const ViewAxes ax = angle_vectors({0, keys_view, 0});
      w.forward = dot(to, ax.forward) * go;
      w.side = dot(to, ax.right) * go;
    } else if (d > p.set.look * 0.25f) {
      // Slowing into the run-up point, so the run at the start is a run, not a turn.
      w.yaw = unwrap(yaw_at(s.origin, P), p.yaw);
      w.forward = d > 2 * p.set.look ? 1 : d / (2 * p.set.look);
      // Turning before running: the share of the way the view already faces (a run at a point
      // behind sweeps a wide circle round it under late hands).
      w.forward *= facing(w.yaw, p.yaw);
    } else {
      // At the point: stand, turned down the jump (late hands circle a point aimed at).
      w.yaw = unwrap(down, p.yaw);
      w.forward = 0;
    }
    because = "lining up behind the jump";
    const float off = wrap(p.yaw - down);
    if (d <= 2 * p.set.arrive && speed < kSlow && off < kFacing && off > -kFacing) {
      p.mode = PilotMode::jump;
      p.best = 1e30f;
    }
    return false;
  }
  // On the line: run through S at the jump's speed, the view along the way.
  target = S;
  const Vec3f aim = along < -(back_up + p.set.arrive) ? P : S + dir * p.set.look;
  w.yaw = unwrap(yaw_at(s.origin, aim), p.yaw);
  // Down to the jump's speed before the run-up: friction takes about a quarter second.
  w.forward = along > -(run_up + p.set.corner + speed * kSlowing_s) ? v / s.maxspeed : 1;
  bool behind = true;
  if (flip && along > -(v * flip_lead_s(p) + 2)) {
    // The flip: turning round into the jump, strafing through the turn.
    const float way = w.yaw, share = w.forward;
    const float back = heading(dir.x, dir.y) + 180;
    w.yaw = unwrap(back, p.yaw);
    flip_keys(way, view_when_keys_land(p, w.yaw, msec), w.forward, w.side);
    w.forward *= share;
    w.side *= share;
    const float off = wrap(p.yaw - back);
    behind = off < kFlipFacing && off > -kFlipFacing;
  } else if (flip) {
    behind = false;
  }
  // The shot at S: the run there at the speed along the way (at least the jump's own).
  p.shot_yaw = heading(dir.x, dir.y) + (flip ? 180 : 0);
  p.shot_in_s = along >= 0 ? 0 : -along / (along_v > v ? along_v : v);
  const bool fast = along_v >= kJumpAlong * v && along_v <= kJumpOver * v + 16 && cosv >= kJumpAim;
  if (along >= -2 && lat_abs <= kOnLine && fast && behind && s.onground && !p.jump_pressed &&
      shot_view_ready(p)) {
    w.jump = true;
    because = "at the start at the jump's speed: jump";
    return true;
  }
  if (flown) {
    w.jump = true;
    w.yaw = unwrap(yaw_at(s.origin, T), p.yaw);
    because = "near the start, a jump from here lands: jump";
    p.stats.jumps_flown++;
    return true;
  }
  because = flip && behind ? "running at the flip, the view behind"
            : flip         ? "running at the flip, turning round"
                           : "running at the jump";
  return false;
}

}  // namespace ge::shooterbots::pilot_parts
