// ge:summary The body's points off his stance, the bones as capsules, a round's path through them
// ge:keywords gunfire body-points body-hit capsule bone pass-distance stance
// ge:skill sim
// ge:test tests/fall_line_gunfire_test.cpp
#include "fall_line/sim/gunfire_body.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "fall_line/sim/weapons_rules.hpp"

namespace fall_line::sim {

namespace {

namespace lm = ge::core::libm;
namespace sk = ge::skier;
using ge::core::hypot3;
using sk::Part;
using P = BodyPoint;

// THE STANCE in the body frame (right, up, forward of the skis' base under the boots, m), stood
// tall and folded right down into a tuck: each point blended between them by how far he tucks.
struct Stance {
  double r, u_tall, u_tuck, f_tall, f_tuck;
};
// Each point's left side (the right is its mirror); the head on the midline.
constexpr std::array<Stance, 7> kStance{{
    {0, 1.68, 1.13, 0.08, 0.53},      // head
    {0.2, 1.45, 0.95, 0.05, 0.4},     // shoulder
    {0.28, 1.15, 0.8, 0.15, 0.45},    // elbow
    {0.3, 0.9, 0.7, 0.35, 0.6},       // hand
    {0.13, 0.95, 0.65, -0.05, -0.1},  // hip
    {0.13, 0.5, 0.4, 0.12, 0.22},     // knee
    {0.12, 0.08, 0.08, 0, 0},         // foot
}};
// DOWN on the snow: laid out face down along his way, his head where his tips were going.
constexpr std::array<Stance, 7> kLying{{
    {0, 0.14, 0.14, 1.6, 1.6},
    {0.2, 0.14, 0.14, 1.3, 1.3},
    {0.35, 0.12, 0.12, 1.0, 1.0},
    {0.4, 0.1, 0.1, 0.75, 0.75},
    {0.13, 0.15, 0.15, 0.6, 0.6},
    {0.13, 0.12, 0.12, 0.15, 0.15},
    {0.12, 0.1, 0.1, -0.3, -0.3},
}};

void put(BodyPoints& b, P p, double x, double y, double z) {
  const size_t i = 3 * static_cast<size_t>(p);
  b.p[i] = x;
  b.p[i + 1] = y;
  b.p[i + 2] = z;
}

Vec3 at(const BodyPoints& b, P p) {
  const size_t i = 3 * static_cast<size_t>(p);
  return {b.p[i], b.p[i + 1], b.p[i + 2]};
}

Vec3 mid(const Vec3& a, const Vec3& c) {
  return {(a.x + c.x) / 2, (a.y + c.y) / 2, (a.z + c.z) / 2};
}

// One bone: its two ends (a point, or one of the made ends below), its radius and how its part is
// named by where along it (`u`, 0..1) the round struck and whether from behind.
enum class End : uint8_t { point, shoulders, hips, neck, ball };
enum class Name : uint8_t {
  head,
  neck,
  trunk,
  pelvis,
  upper_arm,
  forearm,
  hand,
  thigh,
  shin,
  foot
};
struct Bone {
  End ea;
  P a;
  End eb;
  P b;
  double r;
  Name name;
  int8_t side;  // -1 left, 1 right, 0 midline
};

namespace w = weapons;

constexpr std::array<Bone, 16> kBones{{
    {End::point, P::head, End::ball, P::head, w::kHeadR, Name::head, 0},
    {End::shoulders, P::head, End::neck, P::head, w::kNeckR, Name::neck, 0},
    {End::shoulders, P::head, End::hips, P::head, w::kTrunkR, Name::trunk, 0},
    {End::point, P::hip_l, End::point, P::hip_r, w::kPelvisR, Name::pelvis, 0},
    {End::point, P::shoulder_l, End::point, P::elbow_l, w::kUpperArmR, Name::upper_arm, -1},
    {End::point, P::elbow_l, End::point, P::hand_l, w::kForearmR, Name::forearm, -1},
    {End::point, P::hand_l, End::ball, P::hand_l, w::kHandR, Name::hand, -1},
    {End::point, P::hip_l, End::point, P::knee_l, w::kThighR, Name::thigh, -1},
    {End::point, P::knee_l, End::point, P::foot_l, w::kShinR, Name::shin, -1},
    {End::point, P::foot_l, End::ball, P::foot_l, w::kFootR, Name::foot, -1},
    {End::point, P::shoulder_r, End::point, P::elbow_r, w::kUpperArmR, Name::upper_arm, 1},
    {End::point, P::elbow_r, End::point, P::hand_r, w::kForearmR, Name::forearm, 1},
    {End::point, P::hand_r, End::ball, P::hand_r, w::kHandR, Name::hand, 1},
    {End::point, P::hip_r, End::point, P::knee_r, w::kThighR, Name::thigh, 1},
    {End::point, P::knee_r, End::point, P::foot_r, w::kShinR, Name::shin, 1},
    {End::point, P::foot_r, End::ball, P::foot_r, w::kFootR, Name::foot, 1},
}};

Vec3 end_of(const BodyPoints& b, End e, P p) {
  switch (e) {
    case End::point:
    case End::ball:
      return at(b, p);
    case End::shoulders:
      return mid(at(b, P::shoulder_l), at(b, P::shoulder_r));
    case End::hips:
      return mid(at(b, P::hip_l), at(b, P::hip_r));
    case End::neck: {
      // Two thirds of the way from the shoulders to the head: the neck's top.
      const Vec3 s = mid(at(b, P::shoulder_l), at(b, P::shoulder_r));
      const Vec3 h = at(b, P::head);
      return {s.x + (h.x - s.x) * 2 / 3, s.y + (h.y - s.y) * 2 / 3, s.z + (h.z - s.z) * 2 / 3};
    }
  }
  return at(b, p);
}

Part part_of(const Bone& bone, double u, bool back) {
  const auto sided = [&bone](sk::Base base) { return sk::sided(base, bone.side); };
  switch (bone.name) {
    case Name::head:
      return Part::head;
    case Name::neck:
      return Part::neck;
    case Name::trunk:
      return u < 0.55 ? (back ? Part::back : Part::chest) : Part::abdomen;
    case Name::pelvis:
      return Part::pelvis;
    case Name::upper_arm:
      return sided(u < 0.25 ? sk::Base::shoulder : sk::Base::arm);
    case Name::forearm:
      return sided(sk::Base::arm);
    case Name::hand:
      return sided(sk::Base::hand);
    case Name::thigh:
      return sided(u > 0.85 ? sk::Base::knee : sk::Base::thigh);
    case Name::shin:
      return sided(sk::Base::shin);
    case Name::foot:
      return sided(sk::Base::foot);
  }
  return Part::chest;
}

// A segment p -> q against the capsule a -> c of radius r: whether it passes within it, and the
// share along p -> q where it enters (`s`) and along a -> c where it is nearest (`u`).
bool capsule_hit(const Vec3& p, const Vec3& q, const Vec3& a, const Vec3& c, double r, double& s,
                 double& u) {
  const double d1x = q.x - p.x, d1y = q.y - p.y, d1z = q.z - p.z;
  const double d2x = c.x - a.x, d2y = c.y - a.y, d2z = c.z - a.z;
  const double rx = p.x - a.x, ry = p.y - a.y, rz = p.z - a.z;
  const double aa = d1x * d1x + d1y * d1y + d1z * d1z;
  const double ee = d2x * d2x + d2y * d2y + d2z * d2z;
  const double ff = d2x * rx + d2y * ry + d2z * rz;
  const double cc = d1x * rx + d1y * ry + d1z * rz;
  const double bb = d1x * d2x + d1y * d2y + d1z * d2z;
  const auto unit = [](double v) { return v < 0 ? 0.0 : v > 1 ? 1.0 : v; };
  if (ee < 1e-9) {
    s = aa > 1e-12 ? unit(-cc / aa) : 0;
    u = 0;
  } else {
    const double den = aa * ee - bb * bb;
    s = den > 1e-12 ? unit((bb * ff - cc * ee) / den) : 0;
    u = (bb * s + ff) / ee;
    if (u < 0) {
      u = 0;
      s = aa > 1e-12 ? unit(-cc / aa) : 0;
    } else if (u > 1) {
      u = 1;
      s = aa > 1e-12 ? unit((bb - cc) / aa) : 0;
    }
  }
  const double cx = p.x + d1x * s - (a.x + d2x * u);
  const double cy = p.y + d1y * s - (a.y + d2y * u);
  const double cz = p.z + d1z * s - (a.z + d2z * u);
  if (hypot3(cx, cy, cz) > r) return false;
  // Backed off to where the path enters the capsule, near enough.
  const double len = std::sqrt(aa);
  if (len > 1e-9) s = s - r / len < 0 ? 0 : s - r / len;
  return true;
}

// Whether a round going (dx, dy, dz) strikes him from behind: along his front, his right (the
// shoulders, left to right) crossed with his spine (the hips up to the shoulders).
bool from_behind(const BodyPoints& b, double dx, double dy, double dz) {
  const Vec3 l = at(b, P::shoulder_l);
  const Vec3 r = at(b, P::shoulder_r);
  const Vec3 s = mid(l, r);
  const Vec3 h = mid(at(b, P::hip_l), at(b, P::hip_r));
  const double ax = r.x - l.x, ay = r.y - l.y, az = r.z - l.z;
  const double ux = s.x - h.x, uy = s.y - h.y, uz = s.z - h.z;
  const double fx = ay * uz - az * uy;
  const double fy = az * ux - ax * uz;
  const double fz = ax * uy - ay * ux;
  return fx * dx + fy * dy + fz * dz > 0;
}

}  // namespace

BodyPoints body_points(const sk::SkierState& s) {
  const double t = s.tuck < 0 ? 0 : s.tuck > 1 ? 1 : s.tuck;
  const double fx = lm::sin(s.heading);
  const double fz = lm::cos(s.heading);
  const double rx = fz;
  const double rz = -fx;
  const double lean = s.down ? 0 : s.lean;
  const double cl = lm::cos(lean);
  const double sl = lm::sin(lean);
  const std::array<Stance, 7>& rows = s.down ? kLying : kStance;
  BodyPoints b;
  const auto place = [&](P p, const Stance& st, double side) {
    const double r0 = st.r * side;
    const double u0 = st.u_tall + (st.u_tuck - st.u_tall) * t;
    const double f = st.f_tall + (st.f_tuck - st.f_tall) * t;
    // Leaned into the turn about his way: right-side-down positive tips what is over the skis
    // to the right.
    const double r = r0 * cl + u0 * sl;
    const double u = u0 * cl - r0 * sl;
    put(b, p, s.x + rx * r + fx * f, s.y + u, s.z + rz * r + fz * f);
  };
  place(P::head, rows[0], 0);
  place(P::shoulder_l, rows[1], -1);
  place(P::shoulder_r, rows[1], 1);
  place(P::elbow_l, rows[2], -1);
  place(P::elbow_r, rows[2], 1);
  place(P::hand_l, rows[3], -1);
  place(P::hand_r, rows[3], 1);
  place(P::hip_l, rows[4], -1);
  place(P::hip_r, rows[4], 1);
  place(P::knee_l, rows[5], -1);
  place(P::knee_r, rows[5], 1);
  place(P::foot_l, rows[6], -1);
  place(P::foot_r, rows[6], 1);
  return b;
}

Vec3 body_centre(const sk::SkierState& s) {
  const BodyPoints b = body_points(s);
  return mid(mid(at(b, P::shoulder_l), at(b, P::shoulder_r)),
             mid(at(b, P::hip_l), at(b, P::hip_r)));
}

std::optional<BodyHit> body_hit(const BodyPoints& b, const Vec3& p, const Vec3& q) {
  std::optional<BodyHit> best;
  const bool back = from_behind(b, q.x - p.x, q.y - p.y, q.z - p.z);
  for (const Bone& bone : kBones) {
    const Vec3 a = end_of(b, bone.ea, bone.a);
    const Vec3 c = bone.eb == End::ball ? a : end_of(b, bone.eb, bone.b);
    double s = 0;
    double u = 0;
    if (!capsule_hit(p, q, a, c, bone.r, s, u)) continue;
    if (best && s >= best->s) continue;
    best = BodyHit{part_of(bone, u, back), s};
  }
  return best;
}

double pass_distance(const Vec3& p, const Vec3& q, const Vec3& c) {
  const double dx = q.x - p.x, dy = q.y - p.y, dz = q.z - p.z;
  const double l = dx * dx + dy * dy + dz * dz;
  double t = l > 1e-12 ? ((c.x - p.x) * dx + (c.y - p.y) * dy + (c.z - p.z) * dz) / l : 0;
  t = t < 0 ? 0 : t > 1 ? 1 : t;
  return hypot3(p.x + dx * t - c.x, p.y + dy * t - c.y, p.z + dz * t - c.z);
}

}  // namespace fall_line::sim
