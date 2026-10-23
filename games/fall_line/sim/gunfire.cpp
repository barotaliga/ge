// ge:summary The gunfire's bodies: the aim, the burst, the flight, the strike and the wound
// ge:keywords gunfire aim lead drop burst round fly strike wound judge_body gauss stream
// ge:skill sim
// ge:test tests/fall_line_gunfire_test.cpp
#include "fall_line/sim/gunfire.hpp"

#include <cmath>
#include <cstring>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "fall_line/sim/gunfire_body.hpp"

namespace fall_line::sim {

namespace {

namespace lm = ge::core::libm;
namespace num = ge::core::num;
namespace sk = ge::skier;
namespace w = weapons;
using ge::core::hypot;
using ge::core::hypot3;

constexpr double kG = 9.81;
constexpr double kTau = 6.283185307179586;
// The gunfire's stream is the map's seed salted with this.
constexpr uint32_t kSalt = 0x6a7f1eu;

// A standard normal draw (Box-Muller) off the stream.
double gauss(ge::core::Rng& r) {
  const double u = num::max(1e-12, ge::core::next(r));
  const double v = ge::core::next(r);
  return std::sqrt(-2 * lm::log(u)) * lm::cos(kTau * v);
}

int32_t stars_of(const GameState& state) {
  const int32_t s = state.wanted.active ? state.wanted.stars : 0;
  return s < 0 ? 0 : s > 5 ? 5 : s;
}

const sk::SkierState& quarry(const GameState& state) { return state.racers.front().skier; }

// Whether a round may find his body: not while a machine's cabin or its seat holds him.
bool exposed(const GameState& state) { return !state.sled.rider && !state.heli.rider; }

struct Aim {
  double x, y, z, vx, vy, vz;
};

// Where the target is and how it moves: his body's middle, or a point.
Aim target_of(const GameState& state, const AimTarget& t) {
  if (!t.quarry) return {t.x, t.y, t.z, t.vx, t.vy, t.vz};
  const sk::SkierState& s = quarry(state);
  const Vec3 c = body_centre(s);
  return {c.x, c.y, c.z, s.vx, s.vy, s.vz};
}

// THE AIM POINT for a round from (mx, my, mz) at a target going its way: led by `lead` of its way
// over the time of flight (three passes), and held over by the drop over it, g t^2 / 2.
Vec3 aim_point(const GunDef& def, double mx, double my, double mz, const Aim& tg, double lead) {
  double t = flight_time(def, hypot3(tg.x - mx, tg.y - my, tg.z - mz));
  double x = tg.x;
  double y = tg.y;
  double z = tg.z;
  for (int32_t i = 0; i < 3; i++) {
    x = tg.x + tg.vx * t * lead;
    y = tg.y + tg.vy * t * lead;
    z = tg.z + tg.vz * t * lead;
    t = flight_time(def, hypot3(x - mx, y - my, z - mz));
  }
  return {x, y + 0.5 * kG * t * t, z};
}

void say(GameState& state, GunfirePhase phase, sk::Part part, double value) {
  const int32_t gate = static_cast<int32_t>(phase) | (static_cast<int32_t>(part) << 8);
  state.events.push_back(GameEvent{EventKind::gunfire, 0, gate, value});
}

// ONE ROUND OUT OF `gun`'s muzzle: aimed, spread and sent.
void fire_round(GameState& state, GunfireState& g, Gun& gun) {
  const GunDef& def = weapons::gun_def(gun.weapon);
  Aim tg = target_of(state, gun.target);
  if (gun.spot) {
    tg.x += gun.ox;
    tg.z += gun.oz;
    tg.y = state.level->ground_at(tg.x, tg.z);
  }
  // The round leaves with the platform's way (a door gun in a machine circling at 30 m/s), so the
  // lead is taken on the target's way RELATIVE to the gun's: what an aerial gunner is taught to
  // allow for his own speed. A shooter standing still leads on the target's alone.
  tg.vx -= gun.vx;
  tg.vy -= gun.vy;
  tg.vz -= gun.vz;
  const Vec3 aim = aim_point(def, gun.x, gun.y, gun.z, tg, gun.el);
  double dx = aim.x - gun.x;
  double dy = aim.y - gun.y;
  double dz = aim.z - gun.z;
  double d = hypot3(dx, dy, dz);
  if (d <= 0) d = 1;
  dx /= d;
  dy /= d;
  dz /= d;
  // The errors, mil: the burst's hold, the round's own spread, the climb.
  const double climb = def.climb * gun.shot;
  const double across =
      (gun.ex + gauss(g.stream) * def.dispersion + climb * 0.35 * gauss(g.stream)) / 1000;
  const double up = (gun.ey + gauss(g.stream) * def.dispersion + climb) / 1000;
  // Right = way x up, levelled; up' = right x way.
  double rx = -dz;
  double rz = dx;
  double rl = hypot(rx, rz);
  if (rl <= 0) rl = 1;
  rx /= rl;
  rz /= rl;
  const double ux = -rz * dy;
  const double uy = rz * dx - rx * dz;
  const double uz = rx * dy;
  double wx = dx + rx * across + ux * up;
  double wy = dy + uy * up;
  double wz = dz + rz * across + uz * up;
  double wl = hypot3(wx, wy, wz);
  if (wl <= 0) wl = 1;
  wx /= wl;
  wy /= wl;
  wz /= wl;
  const bool tracer = def.tracer > 0 && gun.fired % def.tracer == def.tracer - 1;
  gun.shot += 1;
  gun.fired += 1;
  g.fired += 1;
  if (static_cast<int32_t>(g.rounds.size()) >= w::kMostRounds) g.rounds.erase(g.rounds.begin());
  Round r;
  r.id = g.next++;
  r.gun = gun.id;
  r.weapon = gun.weapon;
  r.x = gun.x;
  r.y = gun.y;
  r.z = gun.z;
  r.vx = gun.vx + wx * def.muzzle;
  r.vy = gun.vy + wy * def.muzzle;
  r.vz = gun.vz + wz * def.muzzle;
  r.tracer = tracer;
  r.warn = gun.rules != FireRule::aim;
  g.rounds.push_back(r);
  say(state, GunfirePhase::shot, sk::Part::head, static_cast<double>(gun.weapon));
}

enum class Struck : uint8_t { none, snow, trunk, body };

// ONE STEP OF ONE ROUND: flown and struck, a wound charged to `doses`. False once it is spent.
bool fly_round(GameState& state, GunfireState& g, Round& r, sk::Doses& doses, int32_t& wounds) {
  const GunDef& def = weapons::gun_def(r.weapon);
  r.age += kDt;
  if (r.age > w::kLifeS || r.flown > def.reach) return false;
  const double v = hypot3(r.vx, r.vy, r.vz);
  // The air's drag, k v^2 against the way.
  const double a = -def.drag * v;
  r.vx += r.vx * a * kDt;
  r.vy += r.vy * a * kDt - kG * kDt;
  r.vz += r.vz * a * kDt;
  const Vec3 p0{r.x, r.y, r.z};
  const Vec3 p1{r.x + r.vx * kDt, r.y + r.vy * kDt, r.z + r.vz * kDt};
  const double step = hypot3(p1.x - p0.x, p1.y - p0.y, p1.z - p0.z);
  double s = 2;
  Struck what = Struck::none;
  sk::Part part = sk::Part::chest;
  const auto& level = *state.level;
  // THE SNOW: the first crossing under it, found by halving.
  if (p1.y < level.ground_at(p1.x, p1.z)) {
    double lo = 0;
    double hi = 1;
    for (int32_t k = 0; k < 10; k++) {
      const double m = (lo + hi) / 2;
      const double y = p0.y + (p1.y - p0.y) * m;
      if (y < level.ground_at(p0.x + (p1.x - p0.x) * m, p0.z + (p1.z - p0.z) * m)) {
        hi = m;
      } else {
        lo = m;
      }
    }
    s = hi;
    what = Struck::snow;
  }
  // THE TRUNKS: upright cylinders.
  const double sx = p1.x - p0.x;
  const double sz = p1.z - p0.z;
  const double ll = sx * sx + sz * sz;
  sk::for_each_tree_near(state.trees, (p0.x + p1.x) / 2, (p0.z + p1.z) / 2, [&](int32_t i) {
    const auto& t = level.trees[static_cast<size_t>(i)];
    double k = ll > 0 ? ((t.x - p0.x) * sx + (t.z - p0.z) * sz) / ll : 0;
    k = k < 0 ? 0 : k > 1 ? 1 : k;
    if (k >= s) return;
    const double cx = p0.x + sx * k - t.x;
    const double cz = p0.z + sz * k - t.z;
    if (cx * cx + cz * cz > t.radius * t.radius) return;
    const double y = p0.y + (p1.y - p0.y) * k;
    if (y < t.y || y > t.y + t.height) return;
    s = k;
    what = Struck::trunk;
  });
  // HIS BODY: never warning fire's, never where they do not shoot to kill, never in a machine,
  // and only within reach of the step.
  const sk::SkierState& sq = quarry(state);
  const Vec3 c = body_centre(sq);
  const double pass = pass_distance(p0, p1, c);
  if (!r.warn && state.wanted.lethal && pass < w::kBodyReachM && exposed(state)) {
    const auto hit = body_hit(body_points(sq), p0, p1);
    if (hit && hit->s < s) {
      s = hit->s;
      what = Struck::body;
      part = hit->part;
    }
  }
  // THE CRACK of a round going by close.
  if (!r.snapped && what != Struck::body && pass < w::kSnapM && v > w::kSoundMs) {
    r.snapped = true;
    g.snaps += 1;
    say(state, GunfirePhase::snap, sk::Part::head, 0);
  }
  if (what == Struck::none) {
    r.x = p1.x;
    r.y = p1.y;
    r.z = p1.z;
    r.flown += step;
    return true;
  }
  r.flown += step * s;
  const double j = 0.5 * def.mass * v * v;
  switch (what) {
    case Struck::snow:
      g.snow += 1;
      say(state, GunfirePhase::snow, sk::Part::head, j);
      break;
    case Struck::trunk:
      g.trunks += 1;
      say(state, GunfirePhase::trunk, sk::Part::head, j);
      break;
    case Struck::body:
      // A ROUND IN HIM: a wound on `part` of `j` J.
      g.hits += 1;
      g.energy += j;
      g.by_part[static_cast<size_t>(part)] += 1;
      doses.charge(part, sk::Mech::pierce, j);
      wounds += 1;
      say(state, GunfirePhase::hit, part, j);
      break;
    case Struck::none:
      break;
  }
  return false;
}

}  // namespace

Gun* arm_gun(GameState& state, int32_t id, WeaponKind weapon, double x, double y, double z) {
  GunfireState& g = state.gunfire;
  if (!g.active) {
    g.active = true;
    g.guns.reserve(static_cast<size_t>(w::kMostGuns));
    g.rounds.reserve(static_cast<size_t>(w::kMostRounds));
    g.stream = ge::core::make_rng(num::to_uint32(state.level->seed) ^ kSalt);
  }
  for (Gun& k : g.guns) {
    if (k.id != id) continue;
    k.weapon = weapon;
    return &k;
  }
  if (static_cast<int32_t>(g.guns.size()) >= w::kMostGuns) return nullptr;
  Gun gun;
  gun.id = id;
  gun.weapon = weapon;
  gun.x = x;
  gun.y = y;
  gun.z = z;
  g.guns.push_back(gun);
  return &g.guns.back();
}

void disarm_gun(GameState& state, int32_t id) {
  std::erase_if(state.gunfire.guns, [id](const Gun& k) { return k.id == id; });
}

bool gun_ready(const Gun& gun) { return gun.left == 0 && gun.wait <= 0; }

FireRule rules_of(const GameState& state, FireRule asked) {
  return asked == FireRule::aim && !state.wanted.lethal ? FireRule::warn : asked;
}

bool fire_at(GameState& state, Gun& gun, const AimTarget& target, FireRule asked) {
  GunfireState& g = state.gunfire;
  if (!g.active || !gun_ready(gun)) return false;
  const FireRule rules = rules_of(state, asked);
  if (rules == FireRule::hold) return false;
  const GunDef& def = weapons::gun_def(gun.weapon);
  const Aim tg = target_of(state, target);
  if (hypot3(tg.x - gun.x, tg.y - gun.y, tg.z - gun.z) > def.reach) return false;
  gun.rules = rules;
  gun.target = target;
  gun.left = ge::core::int_in(g.stream, def.burst_least, def.burst_most);
  gun.shot = 0;
  gun.wait = 0;
  // THE HOLD: his own aim, wider early in a hunt.
  const double hold = def.hold * w::kHoldByStars[static_cast<size_t>(stars_of(state))];
  gun.ex = gauss(g.stream) * hold;
  gun.ey = gauss(g.stream) * hold;
  gun.el = 1 + gauss(g.stream) * def.lead;
  // WARNING FIRE: a spot in the snow to a side of him, and ahead along his way.
  gun.spot = rules == FireRule::warn;
  gun.ox = 0;
  gun.oz = 0;
  if (gun.spot) {
    const double a = ge::core::next(g.stream) * kTau;
    const double r = w::kWarnNearM + (w::kWarnFarM - w::kWarnNearM) * ge::core::next(g.stream);
    const double sp = num::max(1, hypot(tg.vx, tg.vz));
    gun.ox = lm::sin(a) * r + tg.vx * (w::kWarnAheadM / sp);
    gun.oz = lm::cos(a) * r + tg.vz * (w::kWarnAheadM / sp);
  }
  return true;
}

void step_gunfire(GameState& state) {
  GunfireState& g = state.gunfire;
  if (!g.active) return;
  const double pace = w::kPaceByStars[static_cast<size_t>(stars_of(state))];
  for (Gun& gun : g.guns) {
    const GunDef& def = weapons::gun_def(gun.weapon);
    if (gun.left > 0) {
      gun.wait -= kDt;
      while (gun.wait <= 0 && gun.left > 0) {
        fire_round(state, g, gun);
        gun.left -= 1;
        gun.wait += 60 / def.rpm;
        // Between bursts the pause its row says, drawn out early in a hunt.
        if (gun.left == 0) {
          gun.wait = num::max(gun.wait, 0) +
                     ge::core::range(g.stream, def.pause_least, def.pause_most) * pace;
        }
      }
    } else if (gun.wait > 0) {
      gun.wait -= kDt;
    }
  }
  sk::Doses doses;
  int32_t wounds = 0;
  size_t keep = 0;
  for (size_t i = 0; i < g.rounds.size(); i++) {
    if (!fly_round(state, g, g.rounds[i], doses, wounds)) continue;
    if (keep != i) g.rounds[keep] = g.rounds[i];
    keep++;
  }
  g.rounds.resize(keep);
  Racer& me = state.racers.front();
  sk::BodyState& body = me.body;
  const sk::BodyClock clock{state.t, kDt, state.steps, num::to_uint32(state.level->seed)};
  if (wounds > 0 && body.dead < 0) {
    doses.cap = wounds;
    sk::BodyEvents out;
    sk::judge_body(body, doses, me.skier.down, clock, out);
  }
}

double flight_time(const GunDef& def, double d) {
  const double k = def.drag;
  return k > 0 ? lm::expm1(k * d) / (k * def.muzzle) : d / def.muzzle;
}

double speed_at(const GunDef& def, double d) { return def.muzzle * lm::exp(-def.drag * d); }

double energy_at(const GunDef& def, double d) {
  const double v = speed_at(def, d);
  return 0.5 * def.mass * v * v;
}

GunfirePhase gunfire_phase(const GameEvent& e) { return static_cast<GunfirePhase>(e.gate & 0xff); }

sk::Part gunfire_part(const GameEvent& e) { return static_cast<sk::Part>((e.gate >> 8) & 0xff); }

const char* gunfire_phase_name(GunfirePhase p) {
  switch (p) {
    case GunfirePhase::shot:
      return "shot";
    case GunfirePhase::hit:
      return "hit";
    case GunfirePhase::snow:
      return "snow";
    case GunfirePhase::trunk:
      return "trunk";
    case GunfirePhase::snap:
      return "snap";
  }
  return "?";
}

const char* weapon_name(WeaponKind k) {
  switch (k) {
    case WeaponKind::rifle:
      return "rifle";
    case WeaponKind::lmg:
      return "lmg";
    case WeaponKind::door_gun:
      return "door-gun";
  }
  return "?";
}

uint64_t gunfire_hash(const GunfireState& g, uint64_t h) {
  if (!g.active) return h;
  const auto mix = [&h](double v) {
    uint64_t u;
    std::memcpy(&u, &v, sizeof u);
    for (int b = 0; b < 64; b += 8) {
      h ^= (u >> b) & 0xffu;
      h *= 0x100000001b3ull;
    }
  };
  mix(static_cast<double>(g.guns.size()));
  for (const Gun& k : g.guns) {
    mix(k.id);
    mix(static_cast<double>(k.weapon));
    mix(k.x);
    mix(k.y);
    mix(k.z);
    mix(k.left);
    mix(k.wait);
    mix(k.fired);
    mix(static_cast<double>(k.rules));
    mix(k.ex);
    mix(k.ey);
    mix(k.el);
  }
  mix(static_cast<double>(g.rounds.size()));
  for (const Round& r : g.rounds) {
    mix(r.id);
    mix(r.x);
    mix(r.y);
    mix(r.z);
    mix(r.vx);
    mix(r.vy);
    mix(r.vz);
  }
  mix(static_cast<double>(g.stream.a));
  mix(g.fired);
  mix(g.hits);
  mix(g.energy);
  mix(g.snow);
  mix(g.trunks);
  mix(g.snaps);
  for (int32_t n : g.by_part) mix(n);
  return h;
}

}  // namespace fall_line::sim
