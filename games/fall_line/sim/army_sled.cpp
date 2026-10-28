// The army's snowmobiles (army_sled.hpp).
//
// ge:summary sled_pad, step_army_sleds and the hash: the call, chase, halt, dismount and home
// ge:keywords army snowmobile sled call set-off chase halt dismount home watch dig right-it
// ge:skill sim snowmobile
// ge:test tests/fall_line_army_sled_test.cpp
#include "fall_line/sim/army_sled.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "fall_line/sim/army.hpp"
#include "fall_line/sim/army_drive.hpp"
#include "fall_line/sim/army_rules.hpp"
#include "fall_line/sim/army_sled_rules.hpp"
#include "fall_line/sim/gunfire.hpp"
#include "fall_line/sim/lift_ride.hpp"
#include "fall_line/sim/sled_ride.hpp"
#include "mountain/mapgen/query.hpp"
#include "snowmobile/pilot.hpp"

namespace fall_line::sim {

namespace {

namespace sk = ge::skier;
namespace sm = ge::snowmobile;
namespace mg = ge::mountain::mapgen;
namespace lm = ge::core::libm;
namespace num = ge::core::num;
using ge::core::angle_diff;
using ge::core::clamp;
using ge::core::hypot;
using ge::core::kPi;
using namespace army_sled;

// Steps between a machine's looks and between its choices of way (each on its own step of them,
// by its id).
constexpr int64_t kLookSteps = static_cast<int64_t>(wanted::kLookEveryS * kHz);
const int64_t kDecideSteps = num::to_int32(num::round(kDecideS * kHz));
// A machine's crew off it are a squad numbered past this.
constexpr int32_t kSledSquad = 20000;

constexpr DriveRead kRead{kLookM,  kStepM, kFan,      kSpread,      kRise,     kDrop,    kTunnelM,
                          kTrunkM, kEdgeM, kTurnCost, kBlockedCost, kTreeCost, kKeepCost};

void say(GameState& state, ArmySledChange c, const ArmySled& m) {
  state.events.push_back(
      GameEvent{EventKind::army_sled, 0, static_cast<int32_t>(c), static_cast<double>(m.id)});
}

// Whether a point has no trunk within `r` m of it.
bool open_at(const GameState& state, double x, double z, double r) {
  bool open = true;
  sk::for_each_tree_near(state.trees, x, z, [&](int32_t i) {
    const mg::TreeDef& t = state.level->trees[static_cast<size_t>(i)];
    if (hypot(t.x - x, t.z - z) < r + t.radius) open = false;
  });
  return open;
}

// How the hunted man moves, m/s: his skis' way, or the free ride's machine's while he rides it.
Spot quarry_way(const GameState& state) {
  if (const FreeSled* m = ridden(state)) return {m->body.vx, 0, m->body.vz};
  const sk::SkierState& c = state.racers[0].skier;
  return {c.vx, 0, c.vz};
}

sm::SledWorld world_of(const GameState& state) { return sled_world(state); }

// ─── the call ────────────────────────────────────────────────────────────────────────────────

// CALL IN what is asked for; send home what is no longer.
void call(GameState& state, int32_t want) {
  ArmyState& a = state.army;
  int32_t live = 0;
  int32_t waiting = 0;
  for (ArmySled& m : a.sleds) {
    if (m.mode != ArmySledMode::due && m.mode != ArmySledMode::drive &&
        m.mode != ArmySledMode::halt) {
      continue;
    }
    if (live >= want) {
      if (m.mode == ArmySledMode::due) {
        m.mode = ArmySledMode::gone;
      } else {
        m.mode = ArmySledMode::leave;
        m.t = 0;
      }
      continue;
    }
    live++;
    waiting += m.mode == ArmySledMode::due ? 1 : 0;
  }
  while (live < want && static_cast<int32_t>(a.sleds.size()) < kMostSleds) {
    const SledPad pad = sled_pad(state, a.sleds_called);
    ArmySled m;
    m.id = a.next_sled++;
    m.mode = ArmySledMode::due;
    m.due = state.t + kRespondS + kStaggerS * waiting;
    m.body = sm::stand_sled(*state.level, pad.x, pad.z, pad.heading);
    m.home_x = pad.x;
    m.home_z = pad.z;
    m.way = pad.heading;
    m.progress.x = pad.x;
    m.progress.z = pad.z;
    m.progress.side = m.id % 2 == 0 ? 1 : -1;
    a.sleds.push_back(m);
    a.sleds_called++;
    waiting++;
    live++;
    say(state, ArmySledChange::called, a.sleds.back());
  }
}

// OUT OF THE VILLAGE: the engine started, the nose turned at him.
void set_off(GameState& state, ArmySled& m, Spot q) {
  sm::SledState& b = m.body;
  m.way = lm::atan2(q.x - b.x, q.z - b.z);
  b = sm::stand_sled(*state.level, b.x, b.z, m.way);
  b.running = true;
  m.mode = ArmySledMode::drive;
  m.t = 0;
  m.progress.x = b.x;
  m.progress.z = b.z;
  m.progress.t = state.t;
  m.next = state.t + kFirstHaltS;
  say(state, ArmySledChange::out, m);
}

// ─── up the runs ─────────────────────────────────────────────────────────────────────────────

// THE WAY TO CLIMB to `to`: of the ways passing within `kWayNearM` of him, the one the machine is
// below him on, cheapest by the distance onto it and the arc up it; -1 when none is, or he is
// within `kDirectM`.
void pick_way(const GameState& state, ArmySled& m, Spot to) {
  m.on_way = -1;
  const sm::SledState& b = m.body;
  if (!state.ways || hypot(to.x - b.x, to.z - b.z) < kDirectM) return;
  double best = 0;
  for (size_t k = 0; k < state.ways->size(); k++) {
    const mg::Track& t = (*state.ways)[k];
    if (t.points.size() < 2) continue;
    const mg::TrackHit hq = mg::nearest_track_point(t, to.x, to.z);
    if (hq.distance > kWayNearM) continue;
    const mg::TrackHit hm = mg::nearest_track_point(t, b.x, b.z);
    if (hm.s < hq.s + kAlongM) continue;
    const double cost = hm.distance + (hm.s - hq.s);
    if (m.on_way < 0 || cost < best) {
      best = cost;
      m.on_way = static_cast<int32_t>(k);
      m.way_end = hq.s;
    }
  }
}

// Where to steer for along its way: onto it, then `kAlongM` up it; false once it is at the arc
// nearest him (it goes straight at him from there).
bool way_aim(const GameState& state, const ArmySled& m, double& x, double& z) {
  if (m.on_way < 0) return false;
  const mg::Track& t = (*state.ways)[static_cast<size_t>(m.on_way)];
  const mg::TrackHit h = mg::nearest_track_point(t, m.body.x, m.body.z);
  if (h.s < m.way_end + kAlongM) return false;
  const mg::TrackPoint p = army_way_at(t, num::max(m.way_end, h.s - kAlongM));
  x = p.x;
  z = p.z;
  return true;
}

// ─── the crew's eyes ─────────────────────────────────────────────────────────────────────────

// Whether the crew see him now: looked every quarter second, each machine on its own step, and
// a sighting kept `kSawS`.
bool looks(GameState& state, ArmySled& m, Spot q, bool on) {
  if ((state.steps + m.id) % kLookSteps == 0) {
    const sm::SledState& b = m.body;
    const Pursuer p{0, PursuerKind::snowmobile, b.x, b.y + kEyeM, b.z};
    if (on && sees_quarry(*state.level, state.trees, p, q, false)) m.saw = state.t;
  }
  return state.t - m.saw < kSawS;
}

// ─── the drive ───────────────────────────────────────────────────────────────────────────────

// ONE STEP OF THE BODY under `ctl`, its crew on it (or not): the snow, the trunks, the edge; and
// stood back on its belt by its crew once it has lain on its side `kRightS`.
void drive(GameState& state, ArmySled& m, const sm::SledControls& ctl, bool crew) {
  sm::SledEvents ev;
  sm::step_sled(m.body, ctl, crew ? kRiderKg : 0, crew, world_of(state), kDt, ev);
  if (crew && m.body.over_for > kRightS) {
    const bool running = m.body.running;
    const double rpm = m.body.rpm;
    m.body = sm::stand_sled(*state.level, m.body.x, m.body.z, m.body.heading);
    m.body.running = running;
    m.body.rpm = rpm;
    m.next = num::max(m.next, state.t + kRestS);
    say(state, ArmySledChange::righted, m);
  }
}

// The hands along the way chosen at `pace` m/s: the pilot's, to an aim well out along it.
sm::SledControls hands_to(const GameState& state, const ArmySled& m, double pace) {
  if (pace < 0.5) return sm::SledControls{0, 1, 0, 0};
  const sm::SledState& b = m.body;
  const sm::SledAim aim{b.x + lm::sin(m.way) * kAimOutM, b.z + lm::cos(m.way) * kAimOutM, pace};
  return sm::sled_pilot(b, world_of(state), aim);
}

// Braked to a stop, the bars turned to bring the nose round to `face`.
sm::SledControls halt_on(const ArmySled& m, double face) {
  const double off = angle_diff(m.body.heading, face);
  return sm::SledControls{m.body.speed < 0.6 && std::fabs(off) > 0.5 ? 0.25 : 0, 1,
                          clamp(off * 2, -1, 1), 0};
}

// The way to steer toward `want`, read off the snow every `kDecideS`, and the pace by the woods.
// With `grade` (the rise to where it is going over the way there) steeper than it climbs and the
// way straight there stopped within `kBlockedM`, it is BLOCKED: he is out of its reach.
void steer(GameState& state, ArmySled& m, double want, double grade) {
  if ((state.steps + m.id) % kDecideSteps == 0) {
    const sm::SledState& b = m.body;
    const WayChoice c = choose_way(*state.level, state.trees, b.x, b.z, m.way, want, kRead);
    m.way = c.way;
    m.pace = c.woods ? kWoods : kCruise;
    m.blocked =
        grade > kRise &&
        way_cost(*state.level, state.trees, b.x, b.z, want, want, m.way, kRead).reach < kBlockedM;
  }
  // Swung off to a side, it holds that side a while.
  if (ge::ai::detouring(m.progress, state.t)) m.way = want + m.progress.side * kDetourRad;
}

// NOT GETTING STUCK: progress checked a window at a time; a swing off to a side, then dug out and
// stood on open snow facing `want`.
void watch(GameState& state, ArmySled& m, double want) {
  sm::SledState& b = m.body;
  if (!ge::ai::caught(m.progress, b.x, b.z, state.t, kWindowS, kLeastM)) return;
  if (m.progress.slow == 1) {
    m.progress.side = -m.progress.side;
    m.progress.until = state.t + kDetourS;
    return;
  }
  // DUG OUT: back along the way it came, on the first open snow found.
  const double back = b.heading + kPi;
  double x = b.x;
  double z = b.z;
  for (int k = 1; k <= 4; k++) {
    const double px = b.x + lm::sin(back) * kDigM * k;
    const double pz = b.z + lm::cos(back) * kDigM * k;
    if (open_at(state, px, pz, kRoomM)) {
      x = px;
      z = pz;
      break;
    }
  }
  b = sm::stand_sled(*state.level, x, z, want);
  b.running = true;
  m.way = want;
  m.dug++;
  m.progress.until = state.t + kDetourS;
  say(state, ArmySledChange::dug, m);
}

// Stood off him on purpose, or halted: not stuck; the watch starts its window again from here.
void rest(const GameState& state, ArmySled& m) {
  ge::ai::restart(m.progress, m.body.x, m.body.z, state.t);
}

// ─── the crew off it ─────────────────────────────────────────────────────────────────────────

// THE CREW OFF TO TAKE HIM IN: the driver out to the left, the gunner to the right, soldiers after
// him on foot from now on; the machine left standing empty.
void crew_off(GameState& state, ArmySled& m, SoldierMode mode, ArmySledChange change) {
  const sm::SledState& b = m.body;
  const double rx = lm::cos(b.heading);
  const double rz = -lm::sin(b.heading);
  for (const double out : {-1.0, 1.0}) {
    if (army::kMostSoldiers <= state.army.soldiers.size()) break;
    enlist_soldier(state, SoldierFrom::sled, kSledSquad + m.id, mode, b.x + rx * out * kStepOffM,
                   b.z + rz * out * kStepOffM, b.heading);
  }
  m.mode = ArmySledMode::empty;
  m.t = 0;
  say(state, change, m);
}

void dismount(GameState& state, ArmySled& m) {
  crew_off(state, m, SoldierMode::chase, ArmySledChange::dismount);
}

// ─── the modes ───────────────────────────────────────────────────────────────────────────────

// THE CHASE: the way to him read off the snow, the pace by the woods and by how near he is, a short
// halt when he is in sight and range; or, where the army does not shoot to kill and he is slow or
// down, the crew off to take him in.
void chase(GameState& state, ArmySled& m, Spot q, FireRule fire) {
  const WantedState& w = state.wanted;
  sm::SledState& b = m.body;
  const bool sees = looks(state, m, q, true);
  const Spot v = quarry_way(state);
  const Spot to = sees || w.seen ? Spot{q.x + v.x * kLeadS, q.y, q.z + v.z * kLeadS}
                                 : Spot{w.last.x, w.last.y, w.last.z};
  const double d = hypot(q.x - b.x, q.z - b.z);
  const double toward = lm::atan2(to.x - b.x, to.z - b.z);
  const sk::SkierState& c = state.racers[0].skier;
  const bool down = c.down;
  if (!w.lethal) {
    const bool slow = down || c.speed < army::kArrestSpeed;
    if (sees && slow && d < kDismountM && b.speed < kDismountSpeed && !on_sled(state, 0) &&
        !on_lift(state, 0)) {
      dismount(state, m);
      return;
    }
  }
  // THE SHORT HALT, never at a man already down; where the army does not shoot to kill only
  // while it is well off him.
  if (sees && !down && fire != FireRule::hold && d < kHaltRangeM && d > kHaltLeastM &&
      state.t >= m.next && (w.lethal || d > kStandArrestM * 3)) {
    m.mode = ArmySledMode::halt;
    m.t = 0;
    say(state, ArmySledChange::halt, m);
    drive(state, m, halt_on(m, toward - kSide), true);
    return;
  }
  // Up the runs while he is far, straight at him near.
  if ((state.steps + m.id) % kDecideSteps == 0) pick_way(state, m, to);
  double ax = 0;
  double az = 0;
  const bool along = way_aim(state, m, ax, az);
  const double heading = along ? lm::atan2(ax - b.x, az - b.z) : toward;
  // The grade to where it is going: up the run to its aim, or to him.
  const double gx = along ? ax : to.x;
  const double gz = along ? az : to.z;
  const double grade = (state.level->ground_at(gx, gz) - state.level->ground_at(b.x, b.z)) /
                       num::max(hypot(gx - b.x, gz - b.z), 1.0);
  steer(state, m, heading, grade);
  const double stand = w.lethal ? kStandLethalM : kStandArrestM;
  // Eased from its pace to a crawl over the last `kSlowM` to where it stands off him, and stopped
  // there while its crew have him in sight.
  const double pace =
      (sees && d < stand) || m.blocked
          ? 0
          : num::min(m.pace, kClose + (m.pace - kClose) * clamp((d - stand) / kSlowM, 0, 1));
  drive(state, m, hands_to(state, m, pace), true);
  if (pace > 2) {
    watch(state, m, heading);
  } else {
    rest(state, m);
  }
}

// THE HALT: stopped side-on to him, the gunner on him and firing a few bursts once settled
// (gunfire.hpp); then on again.
void halt(GameState& state, ArmySled& m, Spot q, FireRule fire) {
  const WantedState& w = state.wanted;
  const bool sees = looks(state, m, q, true);
  const sm::SledState& b = m.body;
  drive(state, m, halt_on(m, lm::atan2(q.x - b.x, q.z - b.z) - kSide), true);
  const bool lost = !sees && state.t - m.saw > kLostS;
  const int32_t gun = kGunBase + m.id;
  if (m.t > kHoldS || lost || m.bursts >= kHaltBursts || w.ended != WantedEnd::running) {
    m.mode = ArmySledMode::drive;
    m.t = 0;
    m.bursts = 0;
    m.next = state.t + kMoveS + kMoveSpreadS * num::mod(m.id * 0.618, 1);
    rest(state, m);
    disarm_gun(state, gun);
    return;
  }
  if (!state.army.fire || m.t < kSettleS || b.speed > kStill || !sees) return;
  const double fx = lm::sin(b.heading);
  const double fz = lm::cos(b.heading);
  const double x = b.x + fz * kMuzzleRightM + fx * kMuzzleAheadM;
  const double y = b.y + kMuzzleUpM;
  const double z = b.z - fx * kMuzzleRightM + fz * kMuzzleAheadM;
  Gun* g = arm_gun(state, gun, WeaponKind::lmg, x, y, z);
  if (g == nullptr) return;
  g->x = x;
  g->y = y;
  g->z = z;
  g->vx = b.vx;
  g->vy = b.vy;
  g->vz = b.vz;
  if (fire_at(state, *g, AimTarget{}, fire)) m.bursts++;
}

// HOME: back to the village it came from, and gone there.
void home(GameState& state, ArmySled& m) {
  const sm::SledState& b = m.body;
  const double d = hypot(m.home_x - b.x, m.home_z - b.z);
  if (d < kGoneM || m.t > kHomeS) {
    m.mode = ArmySledMode::gone;
    say(state, ArmySledChange::gone, m);
    return;
  }
  const double toward = lm::atan2(m.home_x - b.x, m.home_z - b.z);
  steer(state, m, toward, 0);
  drive(state, m, hands_to(state, m, d < kHomeSlowM ? kClose : m.pace), true);
  watch(state, m, toward);
}

// WRITE THE CREWS into the wanted level's pursuers: every machine on the snow with its crew on it.
void write_crews(GameState& state) {
  std::vector<Pursuer>& ps = state.wanted.pursuers;
  std::erase_if(ps,
                [](const Pursuer& p) { return p.id >= kPursuerBase && p.id < army::kPursuerBase; });
  for (const ArmySled& m : state.army.sleds) {
    if (ps.size() >= ps.capacity()) break;
    if (m.mode != ArmySledMode::drive && m.mode != ArmySledMode::halt) continue;
    const sm::SledState& b = m.body;
    ps.push_back(Pursuer{kPursuerBase + m.id % (army::kPursuerBase - kPursuerBase),
                         PursuerKind::snowmobile, b.x, b.y + kEyeM, b.z});
  }
}

}  // namespace

SledPad sled_pad(const GameState& state, int32_t k) {
  const ArmyState& a = state.army;
  const double rx = lm::cos(a.pad_heading);
  const double rz = -lm::sin(a.pad_heading);
  const double side = k % 2 == 0 ? 1 : -1;
  const int32_t rank = 1 + (k % 6) / 2;
  for (int32_t tries = 0; tries < 8; tries++) {
    const double r = side * (kApartM * (rank + tries) + 2);
    const double x = a.pad_x + rx * r;
    const double z = a.pad_z + rz * r;
    if (open_at(state, x, z, kRoomM)) return {x, z, a.pad_heading};
  }
  return {a.pad_x, a.pad_z, a.pad_heading};
}

void knock_off(GameState& state, ArmySled& m) {
  crew_off(state, m, SoldierMode::down, ArmySledChange::knocked);
}

bool crewed(const ArmySled& m) {
  return m.mode == ArmySledMode::due || m.mode == ArmySledMode::drive ||
         m.mode == ArmySledMode::halt || m.mode == ArmySledMode::leave;
}

void step_army_sleds(GameState& state, bool on, Spot q, FireRule fire) {
  ArmyState& a = state.army;
  if (!state.sled.present) return;
  const int32_t want =
      on ? roster_of(state.wanted.stars).snowmobiles + std::min(a.wave_sleds, kWaveSleds) : 0;
  if (a.sleds.empty() && want == 0) return;
  call(state, want);
  for (ArmySled& m : a.sleds) {
    m.t += kDt;
    switch (m.mode) {
      case ArmySledMode::due:
        if (state.t >= m.due) set_off(state, m, q);
        break;
      case ArmySledMode::drive:
        if (!on) {
          m.mode = ArmySledMode::leave;
          m.t = 0;
          home(state, m);
        } else {
          chase(state, m, q, fire);
        }
        break;
      case ArmySledMode::halt:
        halt(state, m, q, fire);
        break;
      case ArmySledMode::leave:
        home(state, m);
        break;
      case ArmySledMode::empty:
        // Left standing on its parking brake, settling into the snow.
        drive(state, m, sm::SledControls{}, false);
        if (!on && hypot(q.x - m.body.x, q.z - m.body.z) > kEmptyGoneM) {
          m.mode = ArmySledMode::gone;
        }
        break;
      case ArmySledMode::gone:
        break;
    }
  }
  // Machines left standing past the most kept: the oldest let go.
  int32_t empty = 0;
  for (const ArmySled& m : a.sleds) empty += m.mode == ArmySledMode::empty ? 1 : 0;
  for (ArmySled& m : a.sleds) {
    if (empty <= kMostEmpty) break;
    if (m.mode != ArmySledMode::empty) continue;
    m.mode = ArmySledMode::gone;
    empty--;
  }
  for (const ArmySled& m : a.sleds) {
    if (m.mode == ArmySledMode::gone) disarm_gun(state, kGunBase + m.id);
  }
  std::erase_if(a.sleds, [](const ArmySled& m) { return m.mode == ArmySledMode::gone; });
  write_crews(state);
}

uint64_t army_sleds_hash(const ArmyState& a, uint64_t h) {
  const auto mix = [&h](double v) {
    uint64_t u;
    std::memcpy(&u, &v, sizeof u);
    for (int b = 0; b < 64; b += 8) {
      h ^= (u >> b) & 0xffu;
      h *= 0x100000001b3ull;
    }
  };
  mix(static_cast<double>(a.sleds.size()));
  for (const ArmySled& m : a.sleds) {
    mix(m.id);
    mix(static_cast<double>(m.mode));
    mix(m.due);
    mix(m.way);
    mix(m.pace);
    mix(m.blocked ? 1 : 0);
    mix(m.on_way);
    mix(m.way_end);
    mix(m.next);
    mix(m.saw);
    mix(m.progress.slow);
    mix(m.dug);
    h = sm::sled_hash(m.body, h);
  }
  mix(a.next_sled);
  mix(a.sleds_called);
  mix(a.wave_sleds);
  return h;
}

const char* army_sled_mode_name(ArmySledMode m) {
  switch (m) {
    case ArmySledMode::due:
      return "due";
    case ArmySledMode::drive:
      return "drive";
    case ArmySledMode::halt:
      return "halt";
    case ArmySledMode::leave:
      return "leave";
    case ArmySledMode::empty:
      return "empty";
    case ArmySledMode::gone:
      return "gone";
  }
  return "?";
}

const char* army_sled_change_name(ArmySledChange c) {
  switch (c) {
    case ArmySledChange::called:
      return "called";
    case ArmySledChange::out:
      return "out";
    case ArmySledChange::halt:
      return "halt";
    case ArmySledChange::dismount:
      return "dismount";
    case ArmySledChange::dug:
      return "dug";
    case ArmySledChange::righted:
      return "righted";
    case ArmySledChange::gone:
      return "gone";
    case ArmySledChange::knocked:
      return "knocked";
    case ArmySledChange::stolen:
      return "stolen";
  }
  return "?";
}

}  // namespace fall_line::sim
