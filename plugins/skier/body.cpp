// The skier's body (body.hpp).
//
// ge:summary The skier's body: the blows of a trunk and a landing, the ladders judged, death
// ge:keywords body step_body judge_body trunk_on_skis landing ladder risk hash mortal
// ge:skill skier
// ge:test tests/skier_body_test.cpp
#include "skier/body.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/noise.hpp"
#include "core/num.hpp"
#include "skier/tuning.hpp"

namespace ge::skier {

namespace {

namespace I = injury;
namespace lm = core::libm;
namespace num = core::num;

// The risk draws' own salts: no other draw reads these hashes.
constexpr uint32_t kSalt = 0x6b0d1e5u;
constexpr uint32_t kSideSalt = 0x51de0e6u;
constexpr uint32_t kFrailSalt = 0x7a11u;

constexpr size_t idx(Part p, Mech m) {
  return static_cast<size_t>(p) * kMechs + static_cast<size_t>(m);
}

// EVERY PART'S LADDER, worst first (and within a rank the dearest dose first): kinds.
constexpr int32_t kRungs = 24;
struct Ladders {
  std::array<std::array<uint8_t, kRungs>, kParts> kind{};
  std::array<int32_t, kParts> count{};
};

Ladders build_ladders() {
  Ladders l;
  const auto cat = injuries_catalog();
  for (int32_t p = 0; p < kParts; p++) {
    const Base base = base_of(static_cast<Part>(p));
    auto& rungs = l.kind[static_cast<size_t>(p)];
    int32_t& n = l.count[static_cast<size_t>(p)];
    for (size_t k = 0; k < cat.size(); k++) {
      if (cat[k].base != base) continue;
      if (n >= kRungs) break;  // the catalog's longest ladder is far shorter
      rungs[static_cast<size_t>(n++)] = static_cast<uint8_t>(k);
    }
    std::stable_sort(rungs.begin(), rungs.begin() + n, [&](uint8_t a, uint8_t b) {
      const InjuryDef& da = cat[a];
      const InjuryDef& db = cat[b];
      return da.ais != db.ais ? da.ais > db.ais : da.at > db.at;
    });
  }
  return l;
}

const Ladders& ladders() {
  static const Ladders l = build_ladders();
  return l;
}

constexpr bool central(Part p) {
  return static_cast<int32_t>(p) <= static_cast<int32_t>(Part::shoulder_r);
}

// A blow met at `v` m/s on `part` against a solid of give `against` m (the helmet's liner
// added on the head), `share` of it reaching the part.
double blow_on(Part part, double v, double against, double share = 1) {
  const Base base = base_of(part);
  const double helmet = base == Base::head ? I::kHelmet : 0;
  return share * blow_of(v, I::kGive[static_cast<size_t>(base)] + against + helmet);
}

// A TRUNK MET ON THE SKIS at `v` m/s closing, standing at (tx, tz) of `radius` m; `wiped` when
// it stopped him.
void trunk_on_skis(Doses& d, const SkierState& s, const SkiSpec& spec, double v, double tx,
                   double tz, double radius, bool wiped) {
  const double dx = tx - s.x;
  const double dz = tz - s.z;
  const double fx = lm::sin(s.heading);
  const double fz = lm::cos(s.heading);
  const double across = dx * fz - dz * fx;
  const double along = dx * fx + dz * fz;
  const bool ahead = along > 0.4 * (spec.length / 2) && std::fabs(across) < radius + I::kLane;
  const int32_t side = across >= 0 ? 1 : -1;
  double top = 0;
  Part top_part = Part::shin_l;
  const auto hit = [&](Part part, double share, Facing face) {
    const double g = blow_on(part, v, I::kTrunk, share);
    d.strike(part, g, face);
    if (g > top && central(part)) {
      top = g;
      top_part = part;
    }
  };
  if (ahead) {
    // The tips meet it and the skis stop: the shins levered over the boots, the knees wrenched.
    for (int32_t s2 : {-1, 1}) {
      d.charge(sided(Base::shin, s2), Mech::bend, v);
      d.charge(sided(Base::knee, s2), Mech::twist, v * I::kNoseTwist);
    }
    top = v * v / (2 * I::kStop * tuning::kGravity);
    top_part = Part::pelvis;
    if (wiped) {
      // ...and he pitches on into it, on the side of his line it stands: a trunk is narrower
      // than he is, so it meets one shoulder, and the hip under it after.
      namespace F = I::front;
      hit(sided(Base::shoulder, side), F::kShoulder, Facing::front);
      hit(Part::chest, F::kChest, Facing::front);
      hit(Part::head, F::kHead, Facing::front);
      hit(Part::neck, F::kHead * I::kNeckShare, Facing::none);
      hit(Part::pelvis, F::kPelvis, Facing::front);
      hit(Part::abdomen, F::kAbdomen, Facing::front);
    }
  } else {
    namespace S = I::side;
    const Facing face = side < 0 ? Facing::left : Facing::right;
    hit(sided(Base::shoulder, side), S::kShoulder, face);
    hit(sided(Base::arm, side), S::kArm, face);
    hit(Part::chest, S::kChest, face);
    hit(Part::abdomen, S::kAbdomen, face);
    hit(Part::pelvis, S::kPelvis, face);
    hit(sided(Base::thigh, side), S::kThigh, face);
    hit(Part::head, S::kHead, face);
  }
  d.offer(top, top_part, BlowFrom::tree);
}

// A LANDING of load `g` after `flight` s, the skis `off` rad off his way; `folded` when the
// legs gave under it.
void landing(Doses& d, const SkiSpec& spec, double g, double flight, double off, bool folded) {
  const double crooked = core::clamp(off / I::kRolled, 0.0, 1.0);
  const double drawer = g * (I::kSquare + I::kCrooked * crooked);
  d.charge(Part::knee_l, Mech::drawer, drawer);
  d.charge(Part::knee_r, Mech::drawer, drawer);
  // UP THE LEGS: each carries half his weight's stop, a heavier skier harder for the same g.
  const double leg = g * (spec.mass / kReferenceSkis.mass);
  for (int32_t s : {-1, 1}) {
    d.charge(sided(Base::foot, s), Mech::load, leg);
    d.charge(sided(Base::shin, s), Mech::load, leg);
    d.charge(sided(Base::knee, s), Mech::load, leg);
    d.charge(sided(Base::thigh, s), Mech::load, leg);
  }
  d.charge(Part::pelvis, Mech::load, leg);
  if (folded) d.cap = kParts;
  if (flight >= I::kLandingAir) d.offer(g, Part::back, BlowFrom::landing);
  // THE SPINE, THE ORGANS AND THE NECK stopped with him.
  d.charge(Part::back, Mech::load, folded ? g * I::kFolded : g);
  d.charge(Part::chest, Mech::load, g);
  d.charge(Part::abdomen, Mech::load, g);
  d.strike(Part::neck, (g - 1) * I::kNeck);
}

// A LANDING ACROSS HIS WAY: the ski on the side he slides toward catches, its knee twisted at
// the slide's speed and the other's at half of it.
void caught(Doses& d, const SkierState& s) {
  const double plan = core::hypot(s.vx, s.vz);
  double off = core::angle_diff(s.heading, lm::atan2(s.vx, s.vz));
  if (off > core::kPi / 2) off -= core::kPi;  // landed switch: the tails lead
  if (off < -core::kPi / 2) off += core::kPi;
  const double slide = plan * std::fabs(lm::sin(off));
  const int32_t side = off > 0 ? 1 : -1;
  d.charge(sided(Base::knee, side), Mech::twist, slide);
  d.charge(sided(Base::knee, -side), Mech::twist, slide * 0.5);
}

// Whether the blow on part `p` came from a side the injury `def` is done from.
bool struck_on(const InjuryDef& def, const Doses& d, int32_t p) {
  if (def.face == Facing::none && def.on == 0) return true;
  const Facing f = d.faced[static_cast<size_t>(p)];
  if (f == Facing::none) return false;
  if (def.face != Facing::none && f != def.face) return false;
  if (def.on == 0) return true;
  const uint8_t bit = static_cast<uint8_t>(1u << static_cast<uint32_t>(f));
  const Facing out = side_of(static_cast<Part>(p)) < 0 ? Facing::left : Facing::right;
  return (def.on & bit) != 0 || ((def.on & kOnOutside) != 0 && f == out);
}

bool has_injury(const BodyState& b, uint8_t kind, Part part) {
  for (int32_t k = 0; k < std::min(b.count, kMostInjuries); k++) {
    const Injury& h = b.injuries[static_cast<size_t>(k)];
    if (h.kind == kind && h.part == part) return true;
  }
  return false;
}

// WHICH SIDE a paired organ the injury names is hurt on: the side the blow came from, or (from
// the front, behind or up through the legs) one drawn off a hash of the step.
int8_t organ_side(const InjuryDef& def, Facing face, const BodyClock& c, double k) {
  if ((def.organs & kPairedOrgans) == 0) return 0;
  if (face == Facing::left) return -1;
  if (face == Facing::right) return 1;
  return core::hash2(static_cast<double>(c.tick), k, static_cast<double>(c.seed ^ kSideSalt)) < 0.5
             ? -1
             : 1;
}

struct Found {
  int32_t p;
  uint8_t kind;
  uint8_t ais;
  double energy;
  int8_t side;
};

}  // namespace

const char* blow_from_name(BlowFrom f) {
  switch (f) {
    case BlowFrom::none:
      return "none";
    case BlowFrom::landing:
      return "landing";
    case BlowFrom::snow:
      return "snow";
    case BlowFrom::tree:
      return "tree";
  }
  return "none";
}

void Doses::strike(Part part, double g, Facing face) {
  const size_t k = idx(part, Mech::blunt);
  if (g <= dose[k]) return;
  dose[k] = g;
  faced[static_cast<size_t>(part)] = face;
}

void Doses::charge(Part part, Mech mech, double d) {
  const size_t k = idx(part, mech);
  if (d > dose[k]) dose[k] = d;
}

void Doses::offer(double g, Part part, BlowFrom from) {
  if (g <= bill_g) return;
  bill_g = g;
  bill_part = part;
  bill_from = from;
}

double blow_of(double v, double give) { return I::kPeak * v * v / (2 * give * tuning::kGravity); }

double risk_of(double d, double at) {
  if (d < I::kFloor * at) return 0;
  return 1 / (1 + lm::pow(at / d, I::kSteep));
}

double energy_over(Mech mech, double d, double at) {
  const double r = d / at;
  return mech == Mech::twist || mech == Mech::bend ? r * r : r;
}

int32_t severity_of(const BodyState& b) {
  // The regions: the head and neck, the chest and back, the abdomen, the limbs and the pelvis.
  std::array<int32_t, 4> top{};
  for (int32_t p = 0; p < kParts; p++) {
    const int32_t r = p <= 1 ? 0 : p <= 3 ? 1 : p == 4 ? 2 : 3;
    auto& t = top[static_cast<size_t>(r)];
    t = std::max(t, static_cast<int32_t>(b.worst[static_cast<size_t>(p)]));
  }
  std::sort(top.begin(), top.end(), [](int32_t a, int32_t c) { return a > c; });
  return top[0] * top[0] + top[1] * top[1] + top[2] * top[2];
}

bool dies_of(uint32_t seed, int32_t iss) {
  if (iss < I::kLeast) return false;
  const double chance = 1 / (1 + lm::exp((I::kHalf - iss) / I::kSpread));
  return core::hash2(static_cast<double>(seed), kFrailSalt, static_cast<double>(kSalt + 3)) <
         chance;
}

void mend_body(BodyState& b) {
  b.worst.fill(0);
  b.count = 0;
  b.mortal = -1;
  b.dead = -1;
}

void judge_body(BodyState& b, const Doses& d, bool down, const BodyClock& clock, BodyEvents& out) {
  out.first = std::min(b.count, kMostInjuries);
  out.taken = 0;
  out.mortal = false;
  out.died = false;
  if (d.bill_g >= (d.bill_from == BlowFrom::landing ? I::kLandingShown : I::kShown)) {
    // A blow he went down on takes the meter from one he rode out, and one he rode out never
    // takes it from a fall's.
    const Impact& cur = b.impact;
    const bool live = cur.id > 0 && cur.age < I::kHold;
    const double left = live ? cur.g * (1 - cur.age / I::kHold) : 0;
    if (!live || (down && !cur.fall) || (down == cur.fall && d.bill_g >= left)) {
      b.blows += 1;
      b.impact = Impact{d.bill_g, d.bill_part, d.bill_from, 0, b.blows, down};
    }
    b.peak = num::max(b.peak, d.bill_g);
    if (down) b.fall_peak = num::max(b.fall_peak, d.bill_g);
  }
  const auto cat = injuries_catalog();
  // A FRACTURE STRUCK AGAIN breaks worse: each already on the body takes the energy of this
  // step's dose on its part, if harder.
  for (int32_t k = 0; k < std::min(b.count, kMostInjuries); k++) {
    Injury& h = b.injuries[static_cast<size_t>(k)];
    const InjuryDef& def = cat[h.kind];
    if (def.fracture == Fracture::none) continue;
    const auto p = static_cast<int32_t>(h.part);
    const double dose = d.at(h.part, def.mech);
    if (dose <= 0 || !struck_on(def, d, p)) continue;
    const double at = def.at * (1 - I::kWeaken * b.worst[static_cast<size_t>(p)]);
    h.energy = num::max(h.energy, energy_over(def.mech, dose, at));
  }
  // Every part's worst injury drawn this step, then the worst `cap` of them taken.
  std::array<Found, kParts> found{};
  int32_t n = 0;
  const Ladders& lad = ladders();
  for (int32_t p = 0; p < kParts; p++) {
    const auto part = static_cast<Part>(p);
    const uint8_t worst = b.worst[static_cast<size_t>(p)];
    for (int32_t r = 0; r < lad.count[static_cast<size_t>(p)]; r++) {
      const uint8_t kind = lad.kind[static_cast<size_t>(p)][static_cast<size_t>(r)];
      const InjuryDef& def = cat[kind];
      if (def.ais < worst) break;
      const double dose = d.at(part, def.mech);
      if (dose <= 0 || !struck_on(def, d, p) || has_injury(b, kind, part)) continue;
      const double at = def.at * (1 - I::kWeaken * worst);
      const double key = p * 128.0 + kind;
      const double u = core::hash2(static_cast<double>(clock.tick), key,
                                   static_cast<double>(clock.seed ^ kSalt));
      if (u >= risk_of(dose, at)) continue;
      found[static_cast<size_t>(n++)] =
          Found{p, kind, def.ais, energy_over(def.mech, dose, at),
                organ_side(def, d.faced[static_cast<size_t>(p)], clock, key)};
      break;
    }
  }
  std::sort(found.begin(), found.begin() + n, [](const Found& a, const Found& c) {
    return a.ais != c.ais ? a.ais > c.ais : a.p < c.p;
  });
  for (int32_t k = 0; k < std::min(n, d.cap); k++) {
    const Found& f = found[static_cast<size_t>(k)];
    if (b.count < kMostInjuries) {
      b.injuries[static_cast<size_t>(b.count)] =
          Injury{static_cast<Part>(f.p), f.kind, f.ais, f.side, clock.t, f.energy};
      out.taken += 1;
    }
    b.count += 1;
    auto& w = b.worst[static_cast<size_t>(f.p)];
    w = std::max(w, f.ais);
  }
  if (out.taken == 0 || b.mortal >= 0) return;
  const int32_t iss = severity_of(b);
  bool mortal = iss >= I::kMortalIss || dies_of(clock.seed, iss);
  for (int32_t p = 0; p < kParts && !mortal; p++) {
    mortal = b.worst[static_cast<size_t>(p)] >= I::kMortalAis;
  }
  if (!mortal) return;
  b.mortal = clock.t;
  out.mortal = true;
}

void step_body(BodyState& b, SkierState& s, const SkierEvents& ev, const World& world,
               const SkiSpec& spec, const BodyClock& clock, BodyEvents& out) {
  if (b.impact.id > 0) b.impact.age += clock.dt;
  Doses d;
  if (b.dead < 0) {
    if (ev.tree >= 0 && ev.closing > 0) {
      const auto& t = world.level->trees[static_cast<size_t>(ev.tree)];
      const bool wiped = ev.wiped_out && ev.cause == Wipeout::tree;
      trunk_on_skis(d, s, spec, ev.closing, t.x, t.z, t.radius, wiped);
    }
    if (ev.landed) {
      const bool folded = ev.wiped_out && ev.cause == Wipeout::landing;
      landing(d, spec, ev.landing_g, ev.flight_s, ev.landing_off, folded);
    }
    if (ev.wiped_out && ev.cause == Wipeout::across) caught(d, s);
  }
  judge_body(b, d, s.down, clock, out);
  if (b.mortal < 0 || b.dead >= 0) return;
  // Mortally hurt on his skis, he goes down; he dies once he has lain still, or at the latest.
  if (!s.down) put_down(s, Wipeout::hurt);
  if ((s.down_time >= I::kStill && clock.t > b.mortal) || clock.t - b.mortal >= I::kLast) {
    b.dead = clock.t;
    out.died = true;
  }
}

uint64_t body_hash(const BodyState& b, uint64_t h) {
  const auto mix = [&h](uint64_t u, int bytes) {
    for (int k = 0; k < bytes * 8; k += 8) {
      h ^= (u >> k) & 0xffu;
      h *= 0x100000001b3ull;
    }
  };
  const auto mixd = [&mix](double v) {
    uint64_t u;
    std::memcpy(&u, &v, sizeof u);
    mix(u, 8);
  };
  for (uint8_t w : b.worst) mix(w, 1);
  for (int32_t k = 0; k < std::min(b.count, kMostInjuries); k++) {
    const Injury& j = b.injuries[static_cast<size_t>(k)];
    mix(static_cast<uint64_t>(j.part), 1);
    mix(j.kind, 1);
    mix(static_cast<uint8_t>(j.side), 1);
    mixd(j.t);
    mixd(j.energy);
  }
  mix(static_cast<uint32_t>(b.count), 4);
  mixd(b.peak);
  mixd(b.fall_peak);
  mix(static_cast<uint32_t>(b.blows), 4);
  mixd(b.mortal);
  mixd(b.dead);
  return h;
}

}  // namespace ge::skier
