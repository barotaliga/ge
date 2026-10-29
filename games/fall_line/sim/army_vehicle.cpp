// The ground waves' vehicles (army_vehicle.hpp).
//
// ge:summary call_vehicles, step_army_vehicles and the hash: the call, the drive, the drop, home
// ge:keywords army vehicle carrier truck tank call drive move unload park stand-off ram watch
// ge:keywords road-in hitch trailer attitude pursuers
// ge:skill sim
// ge:test tests/fall_line_army_vehicle_test.cpp
#include "fall_line/sim/army_vehicle.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "fall_line/sim/army.hpp"
#include "fall_line/sim/army_drive.hpp"
#include "fall_line/sim/army_rules.hpp"
#include "fall_line/sim/village.hpp"
#include "mountain/mapgen/query.hpp"
#include "mountain/village/streets.hpp"

namespace fall_line::sim {

namespace {

namespace sk = ge::skier;
namespace mg = ge::mountain::mapgen;
namespace vg = ge::mountain::village;
namespace lm = ge::core::libm;
namespace num = ge::core::num;
using ge::core::angle_diff;
using ge::core::clamp;
using ge::core::hypot;
using ge::core::kPi;
using namespace army_vehicle;

constexpr int64_t kLookSteps = static_cast<int64_t>(wanted::kLookEveryS * kHz);
const int64_t kDecideSteps = num::to_int32(num::round(kDecideS * kHz));

constexpr DriveRead read_of(const Spec& s) {
  return DriveRead{kLookM,  kStepM, kFan,      kSpread,      s.climb,   s.cliff,  kTunnelM,
                   kTrunkM, kEdgeM, kTurnCost, kBlockedCost, kTreeCost, kKeepCost};
}
constexpr DriveRead kCarrierRead = read_of(kCarrier);
constexpr DriveRead kTruckRead = read_of(kTruck);
constexpr DriveRead kTankRead = read_of(kTank);

const DriveRead& read_for(ArmyVehicleKind k) {
  switch (k) {
    case ArmyVehicleKind::carrier:
      return kCarrierRead;
    case ArmyVehicleKind::truck:
      return kTruckRead;
    case ArmyVehicleKind::tank:
      return kTankRead;
  }
  return kCarrierRead;
}

void say(GameState& state, ArmyVehicleChange c, const ArmyVehicle& v) {
  state.events.push_back(
      GameEvent{EventKind::army_vehicle, 0, static_cast<int32_t>(c), static_cast<double>(v.id)});
}

// Whether a trunk thicker than a sapling stands within `r` m of (x, z) (the village's felled trees
// are out of `state.trees`).
bool trunk_near(const GameState& state, double x, double z, double r) {
  bool hit = false;
  sk::for_each_tree_near(state.trees, x, z, [&](int32_t i) {
    const mg::TreeDef& t = state.level->trees[static_cast<size_t>(i)];
    if (t.radius < kFlattenM || hypot(t.x - x, t.z - z) >= r + t.radius) return;
    hit = true;
  });
  return hit;
}

// ─── the call ────────────────────────────────────────────────────────────────────────────────

// THE VILLAGE'S ROAD OUT, from the map's edge in (the first street that runs off the map, a point
// every `kStepM` along it; empty where the map has no village), read once a run.
const std::vector<VehiclePoint>& road_in(GameState& state) {
  ArmyState& a = state.army;
  if (!a.road) {
    auto road = std::make_shared<ArmyRoad>();
    if (state.village) {
      for (const vg::Street& st : state.village->streets.streets) {
        if (!st.exit || st.points.size() < 2) continue;
        // From its end at the edge back along it, a point every `kStepM` (the street's own
        // points lie further apart than its bends allow a straight line between).
        VehiclePoint at{st.points.back().x, st.points.back().z};
        road->points.push_back(at);
        double owed = kStepM;
        for (auto i = static_cast<int64_t>(st.points.size()) - 2; i >= 0; i--) {
          const VehiclePoint to{st.points[static_cast<size_t>(i)].x,
                                st.points[static_cast<size_t>(i)].z};
          double left = hypot(to.x - at.x, to.z - at.z);
          while (left >= owed) {
            const double k = owed / left;
            at = {at.x + (to.x - at.x) * k, at.z + (to.z - at.z) * k};
            road->points.push_back(at);
            left -= owed;
            owed = kStepM;
          }
          owed -= left;
          at = to;
        }
        if (owed < kStepM) road->points.push_back(at);
        break;
      }
    }
    a.road = std::move(road);
  }
  return a.road->points;
}

// Its way up `way` from arc `s0` to `s1` (s falls up the mountain), a point every `kStepM`.
void lay_way(const GameState& state, ArmyVehicle& v, int32_t way, double s0, double s1) {
  const mg::Track& t = (*state.ways)[static_cast<size_t>(way)];
  double s = s0;
  while (s > s1) {
    const mg::TrackPoint p = army_way_at(t, s);
    v.path.push_back({p.x, p.z});
    s -= kStepM;
  }
  const mg::TrackPoint p = army_way_at(t, s1);
  v.path.push_back({p.x, p.z});
}

}  // namespace

const Spec& spec_of(ArmyVehicleKind kind) {
  switch (kind) {
    case ArmyVehicleKind::carrier:
      return kCarrier;
    case ArmyVehicleKind::truck:
      return kTruck;
    case ArmyVehicleKind::tank:
      return kTank;
  }
  return kCarrier;
}

PlanSpot rear_of(const ArmyVehicle& v) {
  const double hx = v.x - lm::sin(v.heading) * kHitchM;
  const double hz = v.z - lm::cos(v.heading) * kHitchM;
  return {hx - lm::sin(v.rear) * kBackM, hz - lm::cos(v.rear) * kBackM};
}

bool on_snow(const ArmyVehicle& v) {
  return v.mode != ArmyVehicleMode::due && v.mode != ArmyVehicleMode::gone;
}

void call_vehicles(GameState& state, const WaveMix& mix, int32_t wave, int32_t way, double s_head,
                   double s_end) {
  ArmyState& a = state.army;
  if (!a.send_vehicles || !state.ways || way < 0) return;
  // Carriers, trucks and tanks taken in turn, so a wave short of room brings some of each.
  std::array<ArmyVehicleKind, kMostVehicles> kinds{};
  size_t n = 0;
  const int32_t most = std::max({mix.tracked, mix.trucks, mix.tanks});
  for (int32_t i = 0; i < most && n < kinds.size(); i++) {
    if (i < mix.tracked && n < kinds.size()) kinds[n++] = ArmyVehicleKind::carrier;
    if (i < mix.trucks && n < kinds.size()) kinds[n++] = ArmyVehicleKind::truck;
    if (i < mix.tanks && n < kinds.size()) kinds[n++] = ArmyVehicleKind::tank;
  }
  if (n == 0) return;
  int32_t live = 0;
  int32_t waiting = 0;
  for (const ArmyVehicle& v : a.vehicles) {
    live += v.mode != ArmyVehicleMode::gone ? 1 : 0;
    waiting += v.mode == ArmyVehicleMode::due ? 1 : 0;
  }
  const int32_t stars = std::clamp(state.wanted.stars, 0, wanted::kMaxStars);
  const auto room = static_cast<size_t>(std::max(
      0, std::min(kMost[static_cast<size_t>(stars)] - live,
                  static_cast<int32_t>(kMostVehicles) - static_cast<int32_t>(a.vehicles.size()))));
  // Where they set off: `kSetOffBackM` below where the column forms up, or the way's foot.
  const mg::Track& t = (*state.ways)[static_cast<size_t>(way)];
  const double s_start = num::min(s_head + kSetOffBackM, t.points.back().s);
  const std::vector<VehiclePoint>& road = road_in(state);
  for (size_t k = 0; k < std::min(n, room); k++) {
    ArmyVehicle v;
    v.id = a.next_vehicle++;
    v.kind = kinds[k];
    v.wave = wave;
    v.mode = ArmyVehicleMode::due;
    v.due = state.t + kStaggerS * waiting;
    v.on_way = way;
    v.way_end = s_end;
    double from = s_start;
    if (v.kind == ArmyVehicleKind::truck && !road.empty()) {
      // In along the road, and onto the way where it passes nearest the road's end.
      v.path = road;
      v.road = static_cast<int32_t>(road.size());
      const mg::TrackHit h = mg::nearest_track_point(t, road.back().x, road.back().z);
      from = num::max(h.s, s_end + kStepM);
    }
    lay_way(state, v, way, from, s_end);
    const VehiclePoint p0 = v.path[0];
    const VehiclePoint p1 = v.path[std::min<size_t>(1, v.path.size() - 1)];
    v.heading = lm::atan2(p1.x - p0.x, p1.z - p0.z);
    v.x = p0.x;
    v.z = p0.z;
    v.y = state.level->ground_at(p0.x, p0.z);
    v.rear = v.heading;
    v.way = v.heading;
    v.leg = std::min<int32_t>(1, static_cast<int32_t>(v.path.size()) - 1);
    v.aboard = spec_of(v.kind).seats;
    v.progress.x = v.x;
    v.progress.z = v.z;
    v.progress.t = state.t;
    v.progress.side = v.id % 2 == 0 ? 1 : -1;
    a.vehicles.push_back(std::move(v));
    a.vehicles_called++;
    waiting++;
    say(state, ArmyVehicleChange::called, a.vehicles.back());
  }
}

namespace {

// OFF UP THE WAY, once its turn comes and its spot is clear.
void set_off(GameState& state, ArmyVehicle& v) {
  for (const ArmyVehicle& o : state.army.vehicles) {
    if (&o == &v || !on_snow(o)) continue;
    if (hypot(o.x - v.x, o.z - v.z) < kRoomM) {
      v.due = state.t + 1;
      return;
    }
  }
  v.mode = ArmyVehicleMode::drive;
  v.t = 0;
  v.leg_t = state.t;
  ge::ai::restart(v.progress, v.x, v.z, state.t);
  say(state, ArmyVehicleChange::out, v);
}

// ─── the crew's eyes ─────────────────────────────────────────────────────────────────────────

// Whether the crew see him now: looked every quarter second, each vehicle on its own step, and a
// sighting kept `kSawS`.
bool looks(GameState& state, ArmyVehicle& v, Spot q) {
  if ((state.steps + v.id) % kLookSteps == 0) {
    const Pursuer p{0, PursuerKind::vehicle, v.x, v.y + spec_of(v.kind).eyes, v.z};
    if (sees_quarry(*state.level, state.trees, p, q, false)) v.saw = state.t;
  }
  return state.t - v.saw < kSawS;
}

// ─── the move ────────────────────────────────────────────────────────────────────────────────

// WHETHER SOMETHING STANDS IN ITS WAY at its front (or its back, backing): a trunk within its
// half width, a wind tunnel's tube (unless it is already in one), or another vehicle ahead.
bool blocked(const GameState& state, const ArmyVehicle& v) {
  const Spec& S = spec_of(v.kind);
  const double dir = v.speed >= 0 ? 1 : -1;
  const double fx = lm::sin(v.heading) * dir;
  const double fz = lm::cos(v.heading) * dir;
  const double px = v.x + fx * (S.half_length + 0.6);
  const double pz = v.z + fz * (S.half_length + 0.6);
  if (trunk_near(state, px, pz, S.half_width * 0.8)) return true;
  if (tunnel_within(*state.level, px, pz, S.half_width * 0.8) &&
      !tunnel_within(*state.level, v.x, v.z, 0)) {
    return true;
  }
  for (const ArmyVehicle& o : state.army.vehicles) {
    if (&o == &v || !on_snow(o)) continue;
    // Right of way to the one called first: a later one on the move is not waited for, so two
    // never wait on each other.
    const bool moving = o.mode == ArmyVehicleMode::drive || o.mode == ArmyVehicleMode::leave;
    if (moving && o.id > v.id) continue;
    const double dx = o.x - v.x;
    const double dz = o.z - v.z;
    const double along = dx * fx + dz * fz;
    const Spec& O = spec_of(o.kind);
    if (along <= 0 || along > S.half_length + O.half_length + 2) continue;
    if (std::fabs(dx * fz - dz * fx) < S.half_width + O.half_width + 0.5) return true;
  }
  return false;
}

// ONE STEP OF THE MOVE toward `pace` m/s on the way `way`: the pace gathered or shed, slowed up a
// rise and stopped at one too steep or by what stands in its way, the heading turned, the middle
// set on the snow, the rear unit trailed and the attitude read off the ground.
void move(const GameState& state, ArmyVehicle& v, double pace, double way) {
  const Spec& S = spec_of(v.kind);
  const mg::Level& level = *state.level;
  const double L = S.half_length;
  double fx = lm::sin(v.heading);
  double fz = lm::cos(v.heading);
  const double rise =
      (level.ground_at(v.x + fx * L, v.z + fz * L) - level.ground_at(v.x - fx * L, v.z - fz * L)) /
      (2 * L);
  const double up = pace >= 0 ? rise : -rise;
  double want = pace;
  if (up > S.climb) {
    want = 0;
  } else if (up > 0) {
    want *= 1 - (1 - S.creep) * (up / S.climb);
  }
  const double dv = want - v.speed;
  v.speed += clamp(dv, -S.brake * kDt, (dv > 0 && v.speed >= 0 ? S.accel : S.brake) * kDt);
  if (std::fabs(v.speed) > 0.05 && blocked(state, v)) v.speed = 0;
  // The turn: tracks at any pace, a truck's wheels no tighter than their radius (backing, its
  // wheels turned the other way, so its front still swings toward `way`).
  const double most = S.radius > 0 ? num::min(S.turn, std::fabs(v.speed) / S.radius) : S.turn;
  const double off = angle_diff(v.heading, way);
  v.heading = num::mod(v.heading + clamp(off, -most * kDt, most * kDt) + kPi, 2 * kPi) - kPi;
  fx = lm::sin(v.heading);
  fz = lm::cos(v.heading);
  v.x += fx * v.speed * kDt;
  v.z += fz * v.speed * kDt;
  v.y = level.ground_at(v.x, v.z);
  if (v.kind == ArmyVehicleKind::carrier) {
    const double r = v.speed / (kHitchM + kBackM) * lm::sin(angle_diff(v.rear, v.heading));
    v.rear += r * kDt;
    v.rear = v.heading - clamp(angle_diff(v.rear, v.heading), -kFoldRad, kFoldRad);
  } else {
    v.rear = v.heading;
  }
  const double W = S.half_width;
  const double front = level.ground_at(v.x + fx * L, v.z + fz * L);
  const double back = level.ground_at(v.x - fx * L, v.z - fz * L);
  const double right = level.ground_at(v.x + fz * W, v.z - fx * W);
  const double left = level.ground_at(v.x - fz * W, v.z + fx * W);
  const double k = num::min(1.0, kDt * 8);
  v.pitch += (lm::atan2(front - back, 2 * L) - v.pitch) * k;
  v.roll += (lm::atan2(left - right, 2 * W) - v.roll) * k;
}

// Stood on purpose: not stuck; the watch starts its window again from here.
void rest(const GameState& state, ArmyVehicle& v) {
  ge::ai::restart(v.progress, v.x, v.z, state.t);
}

// Braked to a stand where it is.
void stand(GameState& state, ArmyVehicle& v) {
  move(state, v, 0, v.heading);
  rest(state, v);
}

// NOT GETTING STUCK: progress checked a window at a time; a swing off to a side, then backed down
// its own track, then put back on its way at the point it was making for.
void watch(GameState& state, ArmyVehicle& v) {
  if (!ge::ai::caught(v.progress, v.x, v.z, state.t, kWindowS, kLeastM)) return;

  if (v.progress.slow == 1) {
    v.progress.side = -v.progress.side;
    v.progress.until = state.t + kDetourS;
    return;
  }
  if (v.progress.slow == 2) {
    // BACKED OUT down the way it came, onto the first open snow.
    const double back = v.heading + kPi;
    const double room = spec_of(v.kind).half_width + 1;
    for (int k = 1; k <= 3; k++) {
      const double x = v.x + lm::sin(back) * kDigM * k;
      const double z = v.z + lm::cos(back) * kDigM * k;
      if (!trunk_near(state, x, z, room)) {
        v.x = x;
        v.z = z;
        break;
      }
    }
    v.speed = 0;
    v.y = state.level->ground_at(v.x, v.z);
    v.dug++;
    v.progress.side = -v.progress.side;
    v.progress.until = state.t + kDetourS;
    say(state, ArmyVehicleChange::dug, v);
    return;
  }
  // HELD A THIRD TIME: put back on its way at the point it was making for, facing on along it.
  const auto last = static_cast<int32_t>(v.path.size()) - 1;
  const int32_t i = std::min(v.leg, last);
  const VehiclePoint p = v.path[static_cast<size_t>(i)];
  const VehiclePoint nx = v.path[static_cast<size_t>(std::min(i + 1, last))];
  v.x = p.x;
  v.z = p.z;
  v.y = state.level->ground_at(p.x, p.z);
  if (i < last) v.heading = lm::atan2(nx.x - p.x, nx.z - p.z);
  v.rear = v.heading;
  v.way = v.heading;
  v.speed = 0;
  v.leg = std::min(i + 1, last + 1);
  v.leg_t = state.t;
  v.progress.slow = 0;
  v.stuck++;
  state.army.vehicles_stuck++;
  say(state, ArmyVehicleChange::stuck, v);
}

// The next point of its way, taken once within `kReachM` of it or past it; null once the way is
// driven.
const VehiclePoint* next_point(ArmyVehicle& v, double t) {
  const auto n = static_cast<int32_t>(v.path.size());
  while (v.leg < n) {
    const VehiclePoint& p = v.path[static_cast<size_t>(v.leg)];
    const VehiclePoint& b = v.path[static_cast<size_t>(std::max(0, v.leg - 1))];
    const double len = num::max(0.1, hypot(p.x - b.x, p.z - b.z));
    const double past = ((v.x - p.x) * (p.x - b.x) + (v.z - p.z) * (p.z - b.z)) / len;
    if (hypot(p.x - v.x, p.z - v.z) > kReachM && past < 0) return &p;
    v.leg++;
    v.leg_t = t;
  }
  return nullptr;
}

// STEERED AND MOVED toward (tx, tz) at up to `pace` m/s: the way read off the snow on its turn, a
// side held while swung off, and the watch kept.
void drive_to(GameState& state, ArmyVehicle& v, double tx, double tz, double pace) {
  const double toward = lm::atan2(tx - v.x, tz - v.z);
  const Spec& S = spec_of(v.kind);
  if ((state.steps + int64_t{v.id} * 7) % kDecideSteps == 0) {
    const WayChoice c =
        choose_way(*state.level, state.trees, v.x, v.z, v.way, toward, read_for(v.kind));
    v.way = c.way;
    v.pace = c.woods ? S.woods : S.cruise;
  }
  if (ge::ai::detouring(v.progress, state.t)) v.way = toward + v.progress.side * kDetourRad;
  double go = num::min(pace, v.pace);
  // A WHEELED TURN: held short with its way well off its nose, it cannot turn where it stands, so
  // it backs with its wheels turned until its nose is round.
  if (S.radius > 0 && go > 1) {
    const double off = std::fabs(angle_diff(v.heading, v.way));
    const bool backing = v.speed < 0;
    if (backing ? off > kBackDoneRad : v.speed < 1 && off > kBackRad && blocked(state, v)) {
      go = -kBackPace;
    }
  }
  move(state, v, go, v.way);
  if (std::fabs(go) > 1) {
    watch(state, v);
  } else {
    rest(state, v);
  }
}

// The pace closing on him: stopped `kStopM` short, crawling in over the `kCrawlM` before.
double closing_pace(double d) {
  return d < kStopM + kCrawlM ? clamp((d - kStopM) / kCrawlM, 0, 1) * kCrawl : 1e9;
}

// ─── the men out ─────────────────────────────────────────────────────────────────────────────

// INTO THE HALT TO PUT THEM DOWN, as many as the waves' bound leaves room for.
void start_drop(GameState& state, ArmyVehicle& v) {
  v.mode = ArmyVehicleMode::drop;
  v.t = 0;
  int32_t live = 0;
  for (const Soldier& s : state.army.soldiers) {
    live += s.from == SoldierFrom::wave && s.mode != SoldierMode::gone ? 1 : 0;
  }
  const int32_t stars = std::clamp(state.wanted.stars, 0, wanted::kMaxStars);
  v.aboard = std::max(0, std::min(v.aboard, army::kWaveMost[static_cast<size_t>(stars)] - live));
  say(state, ArmyVehicleChange::drop, v);
}

// HALTED AND EMPTIED: a man out of the back every `kOutEveryS` (a carrier's after him at once, a
// truck's far from him climbing on with the wave), and on `kLingerS` after the last.
void unload(GameState& state, ArmyVehicle& v, Spot q) {
  stand(state, v);
  ArmyState& a = state.army;
  if (v.aboard > 0 && v.t >= kOutEveryS) {
    v.t = 0;
    if (a.soldiers.size() >= army::kMostSoldiers) {
      v.aboard = 0;
      return;
    }
    const Spec& S = spec_of(v.kind);
    const bool carrier = v.kind == ArmyVehicleKind::carrier;
    const double back = carrier ? v.rear : v.heading;
    // Out of the back: a carrier's rear unit's, a truck's tailboard.
    const double tail = S.half_length;
    const double lane = (v.aboard % 3 - 1) * kFileM;
    const double bx = lm::sin(back);
    const double bz = lm::cos(back);
    const PlanSpot from = carrier ? rear_of(v) : PlanSpot{v.x, v.z};
    const double x = from.x - bx * (tail + kOutM) + bz * lane;
    const double z = from.z - bz * (tail + kOutM) - bx * lane;
    const bool far = hypot(q.x - v.x, q.z - v.z) > kChaseM;
    const bool climb = v.kind == ArmyVehicleKind::truck && far && v.leg >= v.road &&
                       v.leg < static_cast<int32_t>(v.path.size());
    Soldier& s = enlist_soldier(state, SoldierFrom::wave, v.wave,
                                climb ? SoldierMode::climb : SoldierMode::chase, x, z,
                                climb ? v.heading : back + kPi);
    if (climb) {
      const mg::Track& t = (*state.ways)[static_cast<size_t>(v.on_way)];
      s.way = v.on_way;
      s.s = mg::nearest_track_point(t, x, z).s;
      s.s_end = v.way_end;
      s.lane = lane;
    }
    v.aboard--;
    a.unloaded++;
    return;
  }
  if (v.aboard == 0 && v.t >= kLingerS) {
    v.mode = v.kind == ArmyVehicleKind::carrier && state.wanted.ended == WantedEnd::running
                 ? ArmyVehicleMode::park
                 : ArmyVehicleMode::leave;
    v.t = 0;
  }
}

// ─── each kind's part ────────────────────────────────────────────────────────────────────────

// Whether the snow rises ahead of it past the grade it climbs.
bool steep(const GameState& state, const ArmyVehicle& v) {
  const Spec& S = spec_of(v.kind);
  const double L = S.half_length;
  const double fx = lm::sin(v.heading);
  const double fz = lm::cos(v.heading);
  const double ahead = state.level->ground_at(v.x + fx * L * 2, v.z + fz * L * 2);
  return (ahead - state.level->ground_at(v.x, v.z)) / (2 * L) > S.climb * 0.9;
}

// Where it goes once its way is driven: at him while he is seen, else where he was last seen.
Spot quarry_or_last(const GameState& state, Spot q) {
  const WantedState& w = state.wanted;
  return w.seen ? q : Spot{w.last.x, w.last.y, w.last.z};
}

// A CARRIER OR A TRUCK on its way: up the way, then at him; halted to put its men down where its
// kind does.
void carry(GameState& state, ArmyVehicle& v, Spot q) {
  const bool sees = looks(state, v, q);
  const double d = hypot(q.x - v.x, q.z - v.z);
  const VehiclePoint* p = next_point(v, state.t);
  const bool there = v.kind == ArmyVehicleKind::carrier
                         ? (sees && d < kDropCarrierM) || d < kDropBlindM || v.dug >= 2 ||
                               (p == nullptr && d < kDropCarrierM)
                         : (sees && d < kDropTruckM) || d < kDropBlindM || v.dug >= 3 ||
                               (v.progress.slow >= 1 && steep(state, v)) ||
                               (p != nullptr && state.t - v.leg_t > kHeldS);
  if (v.aboard > 0 && there) {
    start_drop(state, v);
    return;
  }
  const Spot to = p != nullptr ? Spot{p->x, 0, p->z} : quarry_or_last(state, q);
  drive_to(state, v, to.x, to.z, closing_pace(d));
}

// The turret turned toward (x, z), or back over the nose with nothing to lay on, at its traverse.
void traverse(ArmyVehicle& v, const Spot* at) {
  const double want =
      at != nullptr ? angle_diff(v.heading, lm::atan2(at->x - v.x, at->z - v.z)) : 0;
  const double step = kTraverse * kDt;
  v.turret += clamp(angle_diff(v.turret, want), -step, step);
}

// THE TANK on its way: up the way, then at him to its stand-off, or, in reach and where the army
// shoots to kill, over him.
void hunt(GameState& state, ArmyVehicle& v, Spot q) {
  const WantedState& w = state.wanted;
  const bool sees = looks(state, v, q);
  const double d = hypot(q.x - v.x, q.z - v.z);
  traverse(v, sees || w.seen ? &q : nullptr);
  const VehiclePoint* p = next_point(v, state.t);
  const bool ram = w.lethal && sees && d < kRamM;
  const Spot to = ram || p == nullptr ? quarry_or_last(state, q) : Spot{p->x, 0, p->z};
  const double off = w.lethal ? kTankLethalM : kTankArrestM;
  if (!ram && p == nullptr && sees && d < off) {
    stand(state, v);
    return;
  }
  drive_to(state, v, to.x, to.z, ram ? 1e9 : closing_pace(d));
}

// HOME: back down the way it came, and gone at its foot or after `kLeaveS`.
void go_home(GameState& state, ArmyVehicle& v) {
  traverse(v, nullptr);
  if (v.t > kLeaveS || v.leg <= 0) {
    v.mode = ArmyVehicleMode::gone;
    say(state, ArmyVehicleChange::gone, v);
    return;
  }
  const VehiclePoint& p =
      v.path[static_cast<size_t>(std::min(v.leg, static_cast<int32_t>(v.path.size())) - 1)];
  if (hypot(p.x - v.x, p.z - v.z) < kReachM) v.leg--;
  drive_to(state, v, p.x, p.z, 1e9);
}

// WRITE THE CREWS into the wanted level's pursuers: every vehicle on the snow with its crew in it.
void write_crews(GameState& state) {
  std::vector<Pursuer>& ps = state.wanted.pursuers;
  std::erase_if(ps, [](const Pursuer& p) {
    return p.id >= kPursuerBase && p.id < kPursuerBase + kPursuerSpan;
  });
  for (const ArmyVehicle& v : state.army.vehicles) {
    if (ps.size() >= ps.capacity()) break;
    if (v.mode == ArmyVehicleMode::due || v.mode == ArmyVehicleMode::leave ||
        v.mode == ArmyVehicleMode::gone) {
      continue;
    }
    ps.push_back(Pursuer{kPursuerBase + v.id % kPursuerSpan, PursuerKind::vehicle, v.x,
                         v.y + spec_of(v.kind).eyes, v.z});
  }
}

}  // namespace

void step_army_vehicles(GameState& state, bool on, Spot q) {
  ArmyState& a = state.army;
  if (a.vehicles.empty()) return;
  const WantedState& w = state.wanted;
  for (ArmyVehicle& v : a.vehicles) {
    v.t += kDt;
    const double far = on ? hypot(v.x - w.last.x, v.z - w.last.z) : hypot(v.x - q.x, v.z - q.z);
    if (on_snow(v) && far > (on ? kBehindM : kHomeM)) {
      v.mode = ArmyVehicleMode::gone;
      say(state, ArmyVehicleChange::gone, v);
    }
    if (!on && (v.mode == ArmyVehicleMode::drive || v.mode == ArmyVehicleMode::park)) {
      v.mode = ArmyVehicleMode::leave;
      v.t = 0;
    }
    switch (v.mode) {
      case ArmyVehicleMode::due:
        if (!on) {
          v.mode = ArmyVehicleMode::gone;
        } else if (state.t >= v.due) {
          set_off(state, v);
        }
        break;
      case ArmyVehicleMode::drive:
        if (v.kind == ArmyVehicleKind::tank) {
          hunt(state, v, q);
        } else {
          carry(state, v, q);
        }
        break;
      case ArmyVehicleMode::drop:
        unload(state, v, q);
        break;
      case ArmyVehicleMode::park:
        looks(state, v, q);
        stand(state, v);
        break;
      case ArmyVehicleMode::leave:
        go_home(state, v);
        break;
      case ArmyVehicleMode::gone:
        break;
    }
  }
  std::erase_if(a.vehicles, [](const ArmyVehicle& v) { return v.mode == ArmyVehicleMode::gone; });
  write_crews(state);
}

uint64_t army_vehicles_hash(const ArmyState& a, uint64_t h) {
  if (a.vehicles_called == 0) return h;
  const auto mix = [&h](double v) {
    uint64_t u;
    std::memcpy(&u, &v, sizeof u);
    for (int b = 0; b < 64; b += 8) {
      h ^= (u >> b) & 0xffu;
      h *= 0x100000001b3ull;
    }
  };
  mix(static_cast<double>(a.vehicles.size()));
  for (const ArmyVehicle& v : a.vehicles) {
    mix(v.id);
    mix(static_cast<double>(v.kind));
    mix(static_cast<double>(v.mode));
    mix(v.due);
    mix(v.t);
    mix(v.x);
    mix(v.y);
    mix(v.z);
    mix(v.heading);
    mix(v.pitch);
    mix(v.roll);
    mix(v.speed);
    mix(v.rear);
    mix(v.way);
    mix(v.pace);
    mix(v.turret);
    mix(v.leg);
    mix(v.leg_t);
    mix(v.aboard);
    mix(v.saw);
    mix(v.progress.slow);
    mix(v.dug);
    mix(v.stuck);
  }
  mix(a.next_vehicle);
  mix(a.vehicles_called);
  mix(a.unloaded);
  mix(a.vehicles_stuck);
  return h;
}

const char* army_vehicle_kind_name(ArmyVehicleKind k) {
  switch (k) {
    case ArmyVehicleKind::carrier:
      return "carrier";
    case ArmyVehicleKind::truck:
      return "truck";
    case ArmyVehicleKind::tank:
      return "tank";
  }
  return "?";
}

const char* army_vehicle_mode_name(ArmyVehicleMode m) {
  switch (m) {
    case ArmyVehicleMode::due:
      return "due";
    case ArmyVehicleMode::drive:
      return "drive";
    case ArmyVehicleMode::drop:
      return "drop";
    case ArmyVehicleMode::park:
      return "park";
    case ArmyVehicleMode::leave:
      return "leave";
    case ArmyVehicleMode::gone:
      return "gone";
  }
  return "?";
}

const char* army_vehicle_change_name(ArmyVehicleChange c) {
  switch (c) {
    case ArmyVehicleChange::called:
      return "called";
    case ArmyVehicleChange::out:
      return "out";
    case ArmyVehicleChange::drop:
      return "drop";
    case ArmyVehicleChange::dug:
      return "dug";
    case ArmyVehicleChange::stuck:
      return "stuck";
    case ArmyVehicleChange::strike:
      return "strike";
    case ArmyVehicleChange::gone:
      return "gone";
  }
  return "?";
}

}  // namespace fall_line::sim
