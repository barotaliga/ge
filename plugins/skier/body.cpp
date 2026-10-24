// The skier's body (body.hpp).
//
// ge:summary The skier's body: the blows of a trunk and a landing, the ladders judged, death
// ge:keywords body step_body judge_body trunk_on_skis landing ladder risk hash mortal
// ge:keywords ragdoll_blows point_blow snow_give torso_of facing_of axial_of pay_owed
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

// A TRUNK MET ON THE SKIS at `v` m/s closing, standing at (tx, tz) of `radius` m.
void trunk_on_skis(Doses& d, const SkierState& s, const SkiSpec& spec, double v, double tx,
                   double tz, double radius) {
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
    // The tips meet it and the skis stop: the shins levered over the boots, the knees wrenched;
    // the body goes on into it thrown (`ragdoll_blows`), or (slow enough) is jarred on its skis.
    for (int32_t s2 : {-1, 1}) {
      d.charge(sided(Base::shin, s2), Mech::bend, v);
      d.charge(sided(Base::knee, s2), Mech::twist, v * I::kNoseTwist);
    }
    top = v * v / (2 * I::kStop * tuning::kGravity);
    top_part = Part::pelvis;
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

// A LANDING'S LOAD ON THE TRUNK, the neck and the organs in it.
void charge_trunk(Doses& d, const Owed& o) {
  d.charge(Part::back, Mech::load, o.back);
  d.charge(Part::chest, Mech::load, o.chest);
  d.charge(Part::abdomen, Mech::load, o.abdomen);
  d.strike(Part::neck, o.neck);
}

// A LANDING of load `g` after `flight` s, the skis `off` rad off his way; `folded` when the
// legs gave under it.
void landing(Doses& d, Owed& owed, const SkiSpec& spec, double g, double flight, double off,
             bool folded, double t) {
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
  // THE SPINE, THE ORGANS AND THE NECK stopped with him: owed, when the legs folded under it,
  // until his trunk is in the snow.
  const Owed load{true, t, folded ? g * I::kFolded : g, g, g, (g - 1) * I::kNeck};
  if (folded) {
    owed = load;
  } else {
    charge_trunk(d, load);
  }
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

// THE SNOW'S GIVE at (x, z), m: the groomer's and loose snow's by the packed share there.
double snow_give(const mountain::mapgen::Level& level, double x, double z) {
  const double p = core::clamp(level.packed_at(x, z), 0.0, 1.0);
  return p * I::kSnowPacked + (1 - p) * I::kSnowLoose;
}

// THE THROWN BODY'S FRAME: up its spine, across its hips to the right, and out of its chest.
struct Torso {
  double sx, sy, sz;  // spine
  double rx, ry, rz;  // right
  double cx, cy, cz;  // chest
};

double pt(const PointArray& p, int32_t i, int32_t a) {
  return p[3 * static_cast<size_t>(i) + static_cast<size_t>(a)];
}

Torso torso_of(const PointArray& p) {
  Torso t{};
  double ux = (pt(p, kShoulderL, 0) + pt(p, kShoulderR, 0) - pt(p, kHipL, 0) - pt(p, kHipR, 0)) / 2;
  double uy = (pt(p, kShoulderL, 1) + pt(p, kShoulderR, 1) - pt(p, kHipL, 1) - pt(p, kHipR, 1)) / 2;
  double uz = (pt(p, kShoulderL, 2) + pt(p, kShoulderR, 2) - pt(p, kHipL, 2) - pt(p, kHipR, 2)) / 2;
  double l = core::hypot3(ux, uy, uz);
  if (l == 0) l = 1;
  t.sx = ux / l;
  t.sy = uy / l;
  t.sz = uz / l;
  const double hx = pt(p, kHipR, 0) - pt(p, kHipL, 0);
  const double hy = pt(p, kHipR, 1) - pt(p, kHipL, 1);
  const double hz = pt(p, kHipR, 2) - pt(p, kHipL, 2);
  l = core::hypot3(hx, hy, hz);
  if (l == 0) l = 1;
  t.rx = hx / l;
  t.ry = hy / l;
  t.rz = hz / l;
  // Out of the chest: right x up.
  const double ox = t.ry * t.sz - t.rz * t.sy;
  const double oy = t.rz * t.sx - t.rx * t.sz;
  const double oz = t.rx * t.sy - t.ry * t.sx;
  l = core::hypot3(ox, oy, oz);
  if (l == 0) l = 1;
  t.cx = ox / l;
  t.cy = oy / l;
  t.cz = oz / l;
  return t;
}

// WHICH WAY HE MET a solid whose way out toward him is n: his front, his back or a side.
Facing facing_of(const Torso& t, double nx, double ny, double nz) {
  const double d = t.cx * nx + t.cy * ny + t.cz * nz;
  if (d < -0.5) return Facing::front;
  if (d > 0.5) return Facing::back;
  return t.rx * nx + t.ry * ny + t.rz * nz > 0 ? Facing::left : Facing::right;
}

// How far a blow along n runs along his spine, 0..1: from the head down it (`head`, a dive) or up
// it from the seat.
double axial_of(const Torso& t, double nx, double ny, double nz, bool head) {
  const double d = t.sx * nx + t.sy * ny + t.sz * nz;
  return core::clamp(head ? -d : d, 0.0, 1.0);
}

// How far a blow along n on a foot runs up its leg, 0..1: the line from the foot to its hip.
double leg_axial_of(const PointArray& p, int32_t foot, double nx, double ny, double nz) {
  const int32_t hip = foot == kFootL ? kHipL : kHipR;
  const double ux = pt(p, hip, 0) - pt(p, foot, 0);
  const double uy = pt(p, hip, 1) - pt(p, foot, 1);
  const double uz = pt(p, hip, 2) - pt(p, foot, 2);
  double l = core::hypot3(ux, uy, uz);
  if (l == 0) l = 1;
  return core::clamp((ux * nx + uy * ny + uz * nz) / l, 0.0, 1.0);
}

// The outer side of a part on `side`: his left of a left one.
constexpr Facing outside(int32_t side) { return side < 0 ? Facing::left : Facing::right; }

// THE TRUNK'S SHARE of a blow on a shoulder or a hip, by which way he faces what he met: met flat
// on a side, the flank itself meets it and more reaches the organs.
void trunk_share(Doses& d, bool hip, double v, double against, Facing face) {
  namespace S = I::share;
  const double s = face == Facing::left || face == Facing::right ? S::kFlank : S::kTrunk;
  if (hip) {
    if (face == Facing::back) d.strike(Part::back, blow_on(Part::back, v, against, s), face);
    d.strike(Part::abdomen, blow_on(Part::abdomen, v, against, s), face);
  } else {
    const Part part = face == Facing::back ? Part::back : Part::chest;
    d.strike(part, blow_on(part, v, against, s), face);
  }
}

// ONE POINT OF THE THROWN BODY meeting the snow or a trunk at `v` m/s, against a give of
// `against` m, the solid's way out toward him n.
void point_blow(Doses& d, const PointArray& p, const Torso& t, int32_t i, double v, double against,
                double nx, double ny, double nz, BlowFrom from) {
  namespace S = I::share;
  const Facing face = facing_of(t, nx, ny, nz);
  const bool left =
      i == kHipL || i == kShoulderL || i == kKneeL || i == kFootL || i == kElbowL || i == kHandL;
  const int32_t side = left ? -1 : 1;
  Part part;
  if (i == kHead) {
    part = Part::head;
    // The neck whipped by any blow on the head, and loaded by one down the spine (a dive): the
    // head's stop, in its helmet, is what it is handed.
    const double axial = axial_of(t, nx, ny, nz, true);
    d.strike(Part::neck, blow_on(Part::head, v, against, I::kNeckShare + S::kNeckAxial * axial),
             face);
  } else if (i == kShoulderL || i == kShoulderR) {
    part = sided(Base::shoulder, side);
    trunk_share(d, false, v, against, face);
  } else if (i == kHipL || i == kHipR) {
    part = Part::pelvis;
    trunk_share(d, true, v, against, face);
    // Sat down on it, the trunk upright over the seat: the blow runs up the spine as a load.
    const double axial = axial_of(t, nx, ny, nz, false);
    if (axial > 0) {
      d.charge(Part::back, Mech::load, blow_on(Part::pelvis, v, against) * I::kSeat * axial);
    }
  } else if (i == kKneeL || i == kKneeR) {
    part = sided(Base::knee, side);
    const Part thigh = sided(Base::thigh, side);
    const Part shin = sided(Base::shin, side);
    d.strike(thigh, blow_on(thigh, v, against, S::kKneeThigh));
    d.strike(shin, blow_on(shin, v, against, S::kKneeShin));
  } else if (i == kFootL || i == kFootR) {
    part = sided(Base::foot, side);
    const Part shin = sided(Base::shin, side);
    d.strike(shin, blow_on(shin, v, against, S::kFootShin));
    // Met sole first along a straight leg, the blow runs up it as a landing's does.
    const double axial = leg_axial_of(p, i, nx, ny, nz);
    if (axial > 0) {
      const double leg = blow_on(part, v, against) * S::kLegAxial * axial;
      for (Base b : {Base::foot, Base::shin, Base::knee, Base::thigh}) {
        d.charge(sided(b, side), Mech::load, leg);
      }
      d.charge(Part::pelvis, Mech::load, leg);
    }
  } else if (i == kElbowL || i == kElbowR) {
    part = sided(Base::arm, side);
    // ...up the arm into the point of the shoulder.
    const Part shoulder = sided(Base::shoulder, side);
    d.strike(shoulder, blow_on(shoulder, v, against, S::kElbowShoulder), outside(side));
  } else {
    part = sided(Base::hand, side);
    const Part arm = sided(Base::arm, side);
    const Part shoulder = sided(Base::shoulder, side);
    d.strike(arm, blow_on(arm, v, against, S::kHandArm));
    d.strike(shoulder, blow_on(shoulder, v, against, S::kHandShoulder), outside(side));
  }
  const double g = blow_on(part, v, against);
  d.strike(part, g, face);
  // The meter bills the body's own blows (the head and the trunk), not a limb's whip.
  if (i <= kHead) d.offer(g, part, from);
}

// THROWN: every point of his body that met the snow or a trunk this step.
void ragdoll_blows(Doses& d, const Ragdoll& b, const World& world) {
  const Torso t = torso_of(b.p);
  const auto& level = *world.level;
  for (int32_t i = 0; i < kPoints; i++) {
    const double x = pt(b.p, i, 0);
    const double z = pt(b.p, i, 2);
    const double v = b.impact[static_cast<size_t>(i)];
    if (v > I::kTouch) {
      mountain::mapgen::Vec3 n{};
      level.normal_at(x, z, n);
      point_blow(d, b.p, t, i, v, snow_give(level, x, z), n.x, n.y, n.z, BlowFrom::snow);
    }
    const double c = b.struck[static_cast<size_t>(i)];
    const int32_t k = b.struck_by[static_cast<size_t>(i)];
    if (c > I::kTouch && k >= 0) {
      // The trunk's way out toward him, from its centre.
      const auto& tree = level.trees[static_cast<size_t>(k)];
      const double dx = x - tree.x;
      const double dz = z - tree.z;
      double l = core::hypot(dx, dz);
      if (l == 0) l = 1;
      point_blow(d, b.p, t, i, c, I::kTrunk, dx / l, 0, dz / l, BlowFrom::tree);
    }
  }
}

// A LANDING'S LOAD OWED TO THE TRUNK paid the step his hips, shoulders or head meet the snow, or
// `kOwedMost` s on wherever he is, with every part's injuries open to it as a landing's are.
void pay_owed(Doses& d, Owed& o, const Ragdoll* b, double t) {
  if (!o.owed) return;
  bool met = t - o.t >= I::kOwedMost || b == nullptr;
  for (int32_t i = 0; i <= kHead && !met; i++) met = b->impact[static_cast<size_t>(i)] > I::kTouch;
  if (!met) return;
  charge_trunk(d, o);
  d.cap = kParts;
  o.owed = false;
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
  b.owed = Owed{};
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
      trunk_on_skis(d, s, spec, ev.closing, t.x, t.z, t.radius);
    }
    if (ev.landed) {
      const bool folded = ev.wiped_out && ev.cause == Wipeout::landing;
      landing(d, b.owed, spec, ev.landing_g, ev.flight_s, ev.landing_off, folded, clock.t);
    }
    if (ev.wiped_out && ev.cause == Wipeout::across) caught(d, s);
    // Thrown, every point of him that met the snow or a trunk; stood back up with a load still
    // owed (put on his skis from outside), it is paid now.
    const bool thrown = s.down && s.thrown.active;
    if (thrown) ragdoll_blows(d, s.thrown, world);
    if (!ev.wiped_out) pay_owed(d, b.owed, thrown ? &s.thrown : nullptr, clock.t);
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
  if (b.owed.owed) mixd(b.owed.t);
  return h;
}

}  // namespace ge::skier
