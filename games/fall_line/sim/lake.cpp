// The free ride's tarn: skimming it, going in, swimming out (lake.hpp).
//
// ge:summary lake_water, enter_lake and step_on_water: the skier on and in the tarn's water
// ge:keywords tarn lake sank skim ashore swim step_on_water enter_lake lake_water
// ge:skill sim
// ge:test tests/fall_line_lake_test.cpp
#include "fall_line/sim/lake.hpp"

#include <vector>

#include "core/heightfield.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "core/quat.hpp"
#include "mountain/air/air.hpp"
#include "mountain/tarn/tarn.hpp"
#include "skier/swim.hpp"
#include "snowmobile/body.hpp"
#include "snowmobile/sled.hpp"
#include "snowmobile/spec.hpp"
#include "water/swim.hpp"

namespace fall_line::sim {

namespace {

namespace mg = ge::mountain::mapgen;
namespace sk = ge::skier;
namespace tn = ge::mountain::tarn;
namespace lm = ge::core::libm;
namespace sm = ge::snowmobile;

double ground(const mg::Level& level, double x, double z) {
  return ge::core::sample_field(level.ground, x, z);
}

double bed_height(const void* context, double x, double z) {
  return ground(*static_cast<const mg::Level*>(context), x, z);
}

const ge::water::SwimSpec& skier_swimmer() {
  static const ge::water::SwimSpec spec = ge::skier::dressed_skier();
  return spec;
}

// Into the water at his skis' place and speed: a `sank` event, the swimmer begun.
void go_in(GameState& state, int32_t i, int32_t k) {
  Racer& r = state.racers[static_cast<size_t>(i)];
  FreeRide& f = r.free;
  const sk::SkierState& s = r.skier;
  f.skimming = false;
  f.swimming = true;
  f.lake = k;
  f.swim = ge::water::SwimState{};
  // He goes under to his chest (or to the bed where it is shallower), so the swimmer's water has
  // him from the first step: feet at the surface read as out of it.
  const mg::MapLake& lake = state.level->lakes[static_cast<size_t>(k)];
  const double y = ge::core::num::max(ground(*state.level, s.x, s.z), lake.level - kPlunge);
  f.swim.p = {s.x, ge::core::num::min(s.y, y), s.z};
  f.swim.v = {s.vx, ge::core::num::min(s.vy, 0.0), s.vz};
  f.swim.heading = s.heading;
  state.events.push_back(GameEvent{EventKind::sank, i, k, lake.ice});
}

// The machine level on the water at its place, heading `heading`, its belt `kSledDraft` under the
// surface, nothing turning it.
void float_sled(sm::SledState& b, double surface, double heading) {
  b.y = surface + sm::spec::kCogHeight - kSledDraft;
  b.vy = 0;
  b.q = ge::core::from_euler(heading, 0, 0);
  b.wx = 0;
  b.wy = 0;
  b.wz = 0;
  b.airborne = false;
  b.air_time = 0;
  b.over_for = 0;
  sm::derive_sled(b);
}

// The machine gone under on lake `k`: on the dug bed where it went in, its engine drowned, asleep.
void sink_sled(const mg::Level& level, FreeSled& m, int32_t k) {
  const double heading = m.body.heading;
  m.body = sm::stand_sled(level, m.body.x, m.body.z, heading);
  m.skimming = false;
  m.sunk = true;
  m.lake = k;
  m.rider = false;
  m.thrown = false;
  m.mode = SledMode::down;
  m.t = 0;
  m.still = 0;
  m.asleep = true;
}

// Racer `i` thrown off machine `m` into lake `k` with the way it had: the machine under, he in.
void sled_goes_under(GameState& state, int32_t i, FreeSled& m, int32_t k) {
  Racer& r = state.racers[static_cast<size_t>(i)];
  const sm::SledState b = m.body;
  const ge::core::Vec3 at = sm::boards_at(b);
  r.skier = sk::stand_skier(state.world(), at.x, at.z, b.heading, 0);
  r.skier.x = at.x;
  r.skier.y = at.y;
  r.skier.z = at.z;
  r.skier.vx = b.vx;
  r.skier.vy = b.vy;
  r.skier.vz = b.vz;
  r.skier.speed = b.speed;
  sink_sled(*state.level, m, k);
  state.events.push_back(
      GameEvent{EventKind::sled, i, static_cast<int32_t>(SledPhase::sunk), b.speed});
  go_in(state, i, k);
}

}  // namespace

std::shared_ptr<const ge::water::World> lake_water(const std::shared_ptr<const mg::Level>& level) {
  bool open = false;
  for (const mg::MapLake& lake : level->lakes) open = open || lake.depth > 0;
  if (!open) return nullptr;
  auto world = std::make_shared<ge::water::World>(ge::mountain::air::field_of(*level));
  for (const mg::MapLake& lake : level->lakes) {
    if (lake.depth <= 0) continue;
    const std::vector<ge::water::Point> shore = tn::shore_of(lake);
    ge::water::add_lake(*world, shore, lake.level, {.temperature = tn::kTarnWater},
                        {bed_height, level.get()});
  }
  return world;
}

int32_t open_lake_at(const GameState& state, double x, double z) {
  const auto& lakes = state.level->lakes;
  for (size_t k = 0; k < lakes.size(); ++k) {
    if (lakes[k].depth > 0 && tn::on_lake(lakes[k], x, z)) return static_cast<int32_t>(k);
  }
  return -1;
}

bool on_water(const GameState& state, int32_t i) {
  const FreeRide& f = state.racers[static_cast<size_t>(i)].free;
  return f.skimming || f.swimming;
}

void enter_lake(GameState& state, int32_t i) {
  Racer& r = state.racers[static_cast<size_t>(i)];
  const sk::SkierState& s = r.skier;
  const int32_t k = open_lake_at(state, s.x, s.z);
  if (k < 0 || s.y > state.level->lakes[static_cast<size_t>(k)].level + kSurface) return;
  if (!s.down && ge::core::hypot(s.vx, s.vz) >= kSkimSpeed) {
    r.free.skimming = true;
    r.free.lake = k;
    state.events.push_back(GameEvent{EventKind::skim, i, k, ge::core::hypot(s.vx, s.vz)});
    return;
  }
  go_in(state, i, k);
}

bool step_on_water(GameState& state, int32_t i, double steer, double dt) {
  Racer& r = state.racers[static_cast<size_t>(i)];
  FreeRide& f = r.free;
  sk::SkierState& s = r.skier;
  const mg::Level& level = *state.level;
  const mg::MapLake& lake = level.lakes[static_cast<size_t>(f.lake)];
  if (f.skimming) {
    // Planing: held on the surface, turned by the steer, slowed by the water.
    const double turn = steer * kSkimTurn * dt, c = lm::cos(turn), sn = lm::sin(turn);
    const double vx = s.vx * c + s.vz * sn, vz = s.vz * c - s.vx * sn;
    const double speed = ge::core::hypot(vx, vz);
    const double keep = 1 / (1 + kSkimDrag * speed * dt);
    s.vx = vx * keep;
    s.vz = vz * keep;
    s.vy = 0;
    s.x += s.vx * dt;
    s.z += s.vz * dt;
    s.y = lake.level;
    s.speed = speed * keep;
    s.heading = lm::atan2(s.vx, s.vz);
    s.on_snow = true;
    if (!tn::on_lake(lake, s.x, s.z)) {
      // Over the far shore at speed: on his skis again on the bank.
      f.skimming = false;
      f.lake = -1;
      s.y = ground(level, s.x, s.z);
      return false;
    }
    if (s.speed < kSinkSpeed) go_in(state, i, f.lake);
    return false;
  }
  // Swimming flat out for the nearest shore: away from the lake's middle.
  ge::water::SwimState& w = f.swim;
  const double ox = w.p.x - lake.x, oz = w.p.z - lake.z;
  const double out = ge::core::hypot(ox, oz);
  ge::water::SwimInput in;
  if (out > 0) {
    in.x = ox / out;
    in.z = oz / out;
  }
  ge::water::step_swimmer(*state.water, skier_swimmer(), in, w, state.t, dt);
  const double bed = ground(level, w.p.x, w.p.z);
  if (w.p.y < bed) {
    w.p.y = bed;
    if (w.v.y < 0) w.v.y = 0;
  }
  s.x = w.p.x;
  s.y = w.p.y;
  s.z = w.p.z;
  s.vx = w.v.x;
  s.vy = w.v.y;
  s.vz = w.v.z;
  s.speed = ge::core::hypot(ge::core::hypot(w.v.x, w.v.z), w.v.y);
  s.on_snow = false;
  s.down = true;
  const double angle = lm::atan2(oz, ox);
  if (lake.level - bed < kWade || out >= tn::reach_of(lake, angle) - kShore) {
    // At the shore, or where the water is shallow enough to stand in: out onto the bank, facing
    // away from the water. (The swimmer's level is no guide: afloat, his column rides high.)
    const double reach = tn::reach_of(lake, angle) + kBank;
    const double heading = lm::atan2(ox, oz);
    const double in_water = w.in_water;
    const int32_t k = f.lake;
    s = sk::stand_skier(state.world(), lake.x + reach * lm::cos(angle),
                        lake.z + reach * lm::sin(angle), heading, 0);
    dry_off(state, i);
    state.events.push_back(GameEvent{EventKind::ashore, i, k, in_water});
    return false;
  }
  return w.drowning || w.failed || w.in_water >= kSwimReset;
}

SledWater sled_on_lake(GameState& state, int32_t i, FreeSled& m, const sm::SledControls& c,
                       double dt) {
  sm::SledState& b = m.body;
  const int32_t k = m.skimming ? m.lake : open_lake_at(state, b.x, b.z);
  if (k < 0) return SledWater::off;
  const mg::MapLake& lake = state.level->lakes[static_cast<size_t>(k)];
  if (!m.skimming) {
    // Onto it: the belt down at the surface (a machine jumped out over it flies on until then).
    if (b.y > lake.level + sm::spec::kCogHeight + kSurface) return SledWater::off;
    const double flat = ge::core::hypot(b.vx, b.vz);
    if (flat < kSledSkim) {
      sled_goes_under(state, i, m, k);
      return SledWater::sunk;
    }
    m.skimming = true;
    m.lake = k;
    state.events.push_back(GameEvent{EventKind::skim, i, k, flat});
  }
  // Planing: turned by the bars, driven by the belt's paddles, slowed by the water and the brake.
  const double turn = c.steer * kSledTurn * dt, co = lm::cos(turn), sn = lm::sin(turn);
  const double vx = b.vx * co + b.vz * sn, vz = b.vz * co - b.vx * sn;
  const double speed = ge::core::hypot(vx, vz);
  const double pull = kSledPaddle * c.throttle - kSledBrake * c.brake;
  const double next = ge::core::num::max(0.0, speed + (pull - kSledDrag * speed * speed) * dt);
  const double scale = speed > 0 ? next / speed : 0;
  b.vx = vx * scale;
  b.vz = vz * scale;
  b.x += b.vx * dt;
  b.z += b.vz * dt;
  b.controls = c;
  float_sled(b, lake.level, next > 0 ? lm::atan2(b.vx, b.vz) : b.heading);
  if (!tn::on_lake(lake, b.x, b.z)) {
    // Over the far shore at speed: on the snow again, its own step from the next.
    m.skimming = false;
    m.lake = -1;
    b.y = ground(*state.level, b.x, b.z) + sm::spec::kCogHeight;
    return SledWater::skims;
  }
  if (next < kSledSink) {
    sled_goes_under(state, i, m, k);
    return SledWater::sunk;
  }
  return SledWater::skims;
}

void sink_empty(const mg::Level& level, FreeSled& m) { sink_sled(level, m, m.lake); }

void haul_out(const mg::Level& level, FreeSled& m) {
  const int32_t k = m.lake;
  m.sunk = false;
  m.skimming = false;
  m.lake = -1;
  m.mode = SledMode::parked;
  m.asleep = false;
  if (k < 0) return;
  const mg::MapLake& lake = level.lakes[static_cast<size_t>(k)];
  const double ox = m.body.x - lake.x, oz = m.body.z - lake.z;
  const double angle = lm::atan2(oz, ox);
  const double reach = tn::reach_of(lake, angle) + 2 * kBank;
  m.body = sm::stand_sled(level, lake.x + reach * lm::cos(angle), lake.z + reach * lm::sin(angle),
                          lm::atan2(ox, oz));
}

void dry_off(GameState& state, int32_t i) {
  FreeRide& f = state.racers[static_cast<size_t>(i)].free;
  f.lake = -1;
  f.skimming = false;
  f.swimming = false;
  f.swim = ge::water::SwimState{};
}

}  // namespace fall_line::sim
