// The thrown body: the throw, the step, and the joints, muscles, air and snow it is made of.
//
// ge:summary The thrown body's throw and step: Verlet, the joints, the muscles, the snow, trunks
// ge:keywords ragdoll throw_body throw_of step_ragdoll hold_links hold_joints keep_out calm
// ge:keywords drive pose scrub unturn air-drag friction plough patch spin-drag
// ge:skill skier
// ge:test tests/skier_ragdoll_test.cpp
#include "skier/ragdoll.hpp"

#include <cmath>
#include <cstring>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "skier/tuning.hpp"

namespace ge::skier {

namespace {

namespace rd = tuning::ragdoll;
using mountain::mapgen::Level;
using mountain::mapgen::Vec3;
namespace num = core::num;

constexpr int N = kPoints;

constexpr std::array<double, N> kMass = {
    rd::kMassHip,   rd::kMassHip,  rd::kMassShoulder, rd::kMassShoulder, rd::kMassHead,
    rd::kMassKnee,  rd::kMassKnee, rd::kMassFoot,     rd::kMassFoot,     rd::kMassElbow,
    rd::kMassElbow, rd::kMassHand, rd::kMassHand};

constexpr double total_of() {
  double t = 0;
  for (double m : kMass) t += m;
  return t;
}
constexpr double kTotal = total_of();
constexpr double kTorsoMass = 2 * rd::kMassHip + 2 * rd::kMassShoulder + rd::kMassHead;

double radius_of(int i) {
  if (i < kHead) return rd::kRadiusTorso;
  if (i == kHead) return rd::kRadiusHead;
  return rd::kRadiusLimb;
}

// The torso and the head in the torso's own frame (x right, y up the spine, z out of the chest;
// the origin between the hips).
constexpr std::array<Vec3, 5> kTorsoPoints = {{
    {-rd::kHip, 0, 0},
    {rd::kHip, 0, 0},
    {-rd::kShoulder, rd::kSpine, 0},
    {rd::kShoulder, rd::kSpine, 0},
    {0, rd::kSpine + rd::kNeck, 0.02},
}};

// Every distance the body holds: point, point, length.
struct Link {
  int a, b;
  double len;
};
constexpr int kLinks = 10 + 8;

std::array<Link, kLinks> make_links() {
  std::array<Link, kLinks> out{};
  int k = 0;
  for (int i = 0; i < 5; i++) {
    for (int j = i + 1; j < 5; j++) {
      const Vec3& a = kTorsoPoints[static_cast<size_t>(i)];
      const Vec3& b = kTorsoPoints[static_cast<size_t>(j)];
      out[static_cast<size_t>(k++)] = Link{i, j, core::hypot3(a.x - b.x, a.y - b.y, a.z - b.z)};
    }
  }
  out[static_cast<size_t>(k++)] = Link{kHipL, kKneeL, rd::kThigh};
  out[static_cast<size_t>(k++)] = Link{kHipR, kKneeR, rd::kThigh};
  out[static_cast<size_t>(k++)] = Link{kKneeL, kFootL, rd::kShin};
  out[static_cast<size_t>(k++)] = Link{kKneeR, kFootR, rd::kShin};
  out[static_cast<size_t>(k++)] = Link{kShoulderL, kElbowL, rd::kUpperArm};
  out[static_cast<size_t>(k++)] = Link{kShoulderR, kElbowR, rd::kUpperArm};
  out[static_cast<size_t>(k++)] = Link{kElbowL, kHandL, rd::kForearm};
  out[static_cast<size_t>(k++)] = Link{kElbowR, kHandR, rd::kForearm};
  return out;
}
const std::array<Link, kLinks> kLinkTable = make_links();

// The least distance from the hip to the ankle and from the shoulder to the hand: the knee and
// the elbow folded as tight as they go.
double folded(double a, double b, double least) {
  return std::sqrt(a * a + b * b - 2 * a * b * core::libm::cos(least));
}
const double kKneeFoldSpan = folded(rd::kThigh, rd::kShin, rd::kKneeFold);
const double kElbowFoldSpan = folded(rd::kUpperArm, rd::kForearm, rd::kElbowFold);
// A limb this share of its full reach or longer is straight, and its hinge is left alone.
constexpr double kStraight = 0.97;

double& px(PointArray& p, int i, int a) {
  return p[3 * static_cast<size_t>(i) + static_cast<size_t>(a)];
}
double cx(const PointArray& p, int i, int a) {
  return p[3 * static_cast<size_t>(i) + static_cast<size_t>(a)];
}

Vec3 unit(double x, double y, double z) {
  const double l = core::hypot3(x, y, z);
  const double s = l > 0 ? 1 / l : 1;
  return Vec3{x * s, y * s, z * s};
}

double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// The pelvis's frame: across (hip to hip), up (the spine), the chest out of it (x cross y = z).
struct Frame {
  Vec3 across{1, 0, 0};
  Vec3 up{0, 1, 0};
  Vec3 chest{0, 0, 1};
};

Frame frame_of(const PointArray& p) {
  Frame f;
  f.across = unit(cx(p, kHipR, 0) - cx(p, kHipL, 0), cx(p, kHipR, 1) - cx(p, kHipL, 1),
                  cx(p, kHipR, 2) - cx(p, kHipL, 2));
  f.up =
      unit((cx(p, kShoulderL, 0) + cx(p, kShoulderR, 0) - cx(p, kHipL, 0) - cx(p, kHipR, 0)) / 2,
           (cx(p, kShoulderL, 1) + cx(p, kShoulderR, 1) - cx(p, kHipL, 1) - cx(p, kHipR, 1)) / 2,
           (cx(p, kShoulderL, 2) + cx(p, kShoulderR, 2) - cx(p, kHipL, 2) - cx(p, kHipR, 2)) / 2);
  const Vec3& a = f.across;
  const Vec3& u = f.up;
  f.chest = unit(a.y * u.z - a.z * u.y, a.z * u.x - a.x * u.z, a.x * u.y - a.y * u.x);
  return f;
}

Vec3 spine_of(const PointArray& p) {
  return unit(cx(p, kShoulderL, 0) + cx(p, kShoulderR, 0) - cx(p, kHipL, 0) - cx(p, kHipR, 0),
              cx(p, kShoulderL, 1) + cx(p, kShoulderR, 1) - cx(p, kHipL, 1) - cx(p, kHipR, 1),
              cx(p, kShoulderL, 2) + cx(p, kShoulderR, 2) - cx(p, kHipL, 2) - cx(p, kHipR, 2));
}

// The centre of mass's velocity, m a step.
Vec3 velocity_of(const PointArray& p, const PointArray& l) {
  double x = 0, y = 0, z = 0;
  for (int i = 0; i < N; i++) {
    const double m = kMass[static_cast<size_t>(i)];
    x += m * (cx(p, i, 0) - cx(l, i, 0));
    y += m * (cx(p, i, 1) - cx(l, i, 1));
    z += m * (cx(p, i, 2) - cx(l, i, 2));
  }
  return Vec3{x / kTotal, y / kTotal, z / kTotal};
}

// The body's kinetic energy about its centre of mass, in steps (only compared with itself).
double internal(const PointArray& p, const PointArray& l) {
  const Vec3 vc = velocity_of(p, l);
  double e = 0;
  for (int i = 0; i < N; i++) {
    const double dx = cx(p, i, 0) - cx(l, i, 0) - vc.x;
    const double dy = cx(p, i, 1) - cx(l, i, 1) - vc.y;
    const double dz = cx(p, i, 2) - cx(l, i, 2) - vc.z;
    e += kMass[static_cast<size_t>(i)] * (dx * dx + dy * dy + dz * dz);
  }
  return e / 2;
}

// THE JOINTS GIVE THE BODY NO ENERGY: whatever a pass of them added to the body's motion about its
// centre of mass, `before` of it going in, is taken back off that motion; its way is left alone.
void calm(const PointArray& p, PointArray& l, double before) {
  const double after = internal(p, l);
  if (after <= before || after < 1e-12) return;
  const double s = std::sqrt(before / after);
  const Vec3 vc = velocity_of(p, l);
  for (int i = 0; i < N; i++) {
    px(l, i, 0) = cx(p, i, 0) - (vc.x + (cx(p, i, 0) - cx(l, i, 0) - vc.x) * s);
    px(l, i, 1) = cx(p, i, 1) - (vc.y + (cx(p, i, 1) - cx(l, i, 1) - vc.y) * s);
    px(l, i, 2) = cx(p, i, 2) - (vc.z + (cx(p, i, 2) - cx(l, i, 2) - vc.z) * s);
  }
}

// Every distance the body holds, each pulled back to its length, the lighter end moved the more.
void hold_links(PointArray& p) {
  for (const Link& k : kLinkTable) {
    const double dx = cx(p, k.b, 0) - cx(p, k.a, 0);
    const double dy = cx(p, k.b, 1) - cx(p, k.a, 1);
    const double dz = cx(p, k.b, 2) - cx(p, k.a, 2);
    double d = core::hypot3(dx, dy, dz);
    if (d == 0) d = 1e-9;
    const double wi = 1 / kMass[static_cast<size_t>(k.a)];
    const double wj = 1 / kMass[static_cast<size_t>(k.b)];
    const double s = (d - k.len) / d / (wi + wj);
    px(p, k.a, 0) += dx * s * wi;
    px(p, k.a, 1) += dy * s * wi;
    px(p, k.a, 2) += dz * s * wi;
    px(p, k.b, 0) -= dx * s * wj;
    px(p, k.b, 1) -= dy * s * wj;
    px(p, k.b, 2) -= dz * s * wj;
  }
}

// Move point `i` by `by` m along `d` against point `j` (and `k`, unless -1): the push shared by
// mass, and the relative velocity across the joint that runs into the limit taken away the same
// way, so a limit is an inelastic stop and the correction itself carries no velocity.
void shift(PointArray& p, PointArray& l, int i, int j, int k, const Vec3& d, double by) {
  const double wi = 1 / kMass[static_cast<size_t>(i)];
  const double wj = k < 0 ? 1 / kMass[static_cast<size_t>(j)]
                          : 1 / (kMass[static_cast<size_t>(j)] + kMass[static_cast<size_t>(k)]);
  const double share = wi / (wi + wj);
  const int t = k < 0 ? j : k;
  auto along = [&](int q) {
    return (cx(p, q, 0) - cx(l, q, 0)) * d.x + (cx(p, q, 1) - cx(l, q, 1)) * d.y +
           (cx(p, q, 2) - cx(l, q, 2)) * d.z;
  };
  const double vi = along(i);
  const double vj = (along(j) + along(t)) / 2;
  const double rel = vi - vj;
  const double into = by > 0 ? num::min(0.0, rel) : num::max(0.0, rel);
  const double a = by * share;
  const double b = by - a;
  const double va = into * share;
  const double vb = into - va;
  const int who[3] = {i, j, k};
  const double move[3] = {a, -b, -b};
  const double vel[3] = {va, -vb, -vb};
  for (int n = 0; n < 3; n++) {
    const int q = who[n];
    if (q < 0) continue;
    px(p, q, 0) += d.x * move[n];
    px(p, q, 1) += d.y * move[n];
    px(p, q, 2) += d.z * move[n];
    px(l, q, 0) += d.x * (move[n] + vel[n]);
    px(l, q, 1) += d.y * (move[n] + vel[n]);
    px(l, q, 2) += d.z * (move[n] + vel[n]);
  }
}

// Move point `i` by `by` m along `d`, its last position by `last` m.
void move(PointArray& p, PointArray& l, int i, const Vec3& d, double by, double last) {
  px(p, i, 0) += d.x * by;
  px(p, i, 1) += d.y * by;
  px(p, i, 2) += d.z * by;
  px(l, i, 0) += d.x * last;
  px(l, i, 1) += d.y * last;
  px(l, i, 2) += d.z * last;
}

// No limb through the chest: every point of the arms and the legs kept `kTorso` off the spine's
// line, pushed out square to it, the trunk pushed back as one piece by mass, the closing velocity
// between them taken away.
void keep_out(PointArray& p, PointArray& l) {
  const double ax = (cx(p, kHipL, 0) + cx(p, kHipR, 0)) / 2;
  const double ay = (cx(p, kHipL, 1) + cx(p, kHipR, 1)) / 2;
  const double az = (cx(p, kHipL, 2) + cx(p, kHipR, 2)) / 2;
  const double sx = (cx(p, kShoulderL, 0) + cx(p, kShoulderR, 0)) / 2 - ax;
  const double sy = (cx(p, kShoulderL, 1) + cx(p, kShoulderR, 1)) / 2 - ay;
  const double sz = (cx(p, kShoulderL, 2) + cx(p, kShoulderR, 2)) / 2 - az;
  double ss = sx * sx + sy * sy + sz * sz;
  if (ss == 0) ss = 1e-9;
  for (int i = kKneeL; i < N; i++) {
    const double t = core::clamp(
        ((cx(p, i, 0) - ax) * sx + (cx(p, i, 1) - ay) * sy + (cx(p, i, 2) - az) * sz) / ss, 0, 1);
    const double dx = cx(p, i, 0) - (ax + sx * t);
    const double dy = cx(p, i, 1) - (ay + sy * t);
    const double dz = cx(p, i, 2) - (az + sz * t);
    const double d = core::hypot3(dx, dy, dz);
    if (d >= rd::kTorso || d < 1e-6) continue;
    const Vec3 fold = unit(dx, dy, dz);
    const double by = rd::kTorso - d;
    const double mi = kMass[static_cast<size_t>(i)];
    const double limb = (by * kTorsoMass) / (mi + kTorsoMass);
    const double trunk = limb - by;
    double vt = 0;
    for (int k = 0; k < 5; k++) {
      vt += (cx(p, k, 0) - cx(l, k, 0)) * fold.x + (cx(p, k, 1) - cx(l, k, 1)) * fold.y +
            (cx(p, k, 2) - cx(l, k, 2)) * fold.z;
    }
    const double rel = (cx(p, i, 0) - cx(l, i, 0)) * fold.x + (cx(p, i, 1) - cx(l, i, 1)) * fold.y +
                       (cx(p, i, 2) - cx(l, i, 2)) * fold.z - vt / 5;
    const double into = num::min(0.0, rel);
    const double vl = (into * kTorsoMass) / (mi + kTorsoMass);
    move(p, l, i, fold, limb, limb + vl);
    for (int k = 0; k < 5; k++) move(p, l, k, fold, trunk, trunk - into + vl);
  }
}

// What the joints will not do, each a position put right that carries no velocity.
void hold_joints(PointArray& p, PointArray& l) {
  const Frame f = frame_of(p);
  for (int s = 0; s < 2; s++) {
    const double side = s == 0 ? -1 : 1;
    const int hip = kHipL + s;
    const int knee = kKneeL + s;
    const int foot = kFootL + s;
    // The knee bends forward only, once the leg is bent at all.
    const double lx = cx(p, foot, 0) - cx(p, hip, 0);
    const double ly = cx(p, foot, 1) - cx(p, hip, 1);
    const double lz = cx(p, foot, 2) - cx(p, hip, 2);
    double ll = lx * lx + ly * ly + lz * lz;
    if (ll == 0) ll = 1e-9;
    const double cl = (f.chest.x * lx + f.chest.y * ly + f.chest.z * lz) / ll;
    const Vec3 bend = unit(f.chest.x - cl * lx, f.chest.y - cl * ly, f.chest.z - cl * lz);
    const double back = (cx(p, knee, 0) - (cx(p, hip, 0) + cx(p, foot, 0)) / 2) * bend.x +
                        (cx(p, knee, 1) - (cx(p, hip, 1) + cx(p, foot, 1)) / 2) * bend.y +
                        (cx(p, knee, 2) - (cx(p, hip, 2) + cx(p, foot, 2)) / 2) * bend.z;
    const double straight = kStraight * (rd::kThigh + rd::kShin);
    if (back < 0 && ll < straight * straight) shift(p, l, knee, hip, foot, bend, -back);
    // The thigh: not far behind the hip, not far above it, not splayed past the splits nor
    // crossed far over the other leg.
    const double tx = cx(p, knee, 0) - cx(p, hip, 0);
    const double ty = cx(p, knee, 1) - cx(p, hip, 1);
    const double tz = cx(p, knee, 2) - cx(p, hip, 2);
    const double fore = tx * f.chest.x + ty * f.chest.y + tz * f.chest.z;
    if (fore < -rd::kHipBack * rd::kThigh) {
      shift(p, l, knee, hip, -1, f.chest, -rd::kHipBack * rd::kThigh - fore);
    }
    const double rise = tx * f.up.x + ty * f.up.y + tz * f.up.z;
    if (rise > rd::kHipUp * rd::kThigh) {
      shift(p, l, knee, hip, -1, f.up, rd::kHipUp * rd::kThigh - rise);
    }
    const double out = side * (tx * f.across.x + ty * f.across.y + tz * f.across.z);
    if (out > 0.75 * rd::kThigh) {
      shift(p, l, knee, hip, -1, f.across, side * (0.75 * rd::kThigh - out));
    }
    if (out < -0.25 * rd::kThigh) {
      shift(p, l, knee, hip, -1, f.across, side * (-0.25 * rd::kThigh - out));
    }
    // The knee folded no tighter than the calf against the thigh.
    const double span = std::sqrt(ll);
    if (span < kKneeFoldSpan) shift(p, l, foot, hip, -1, unit(lx, ly, lz), kKneeFoldSpan - span);
    // The hand never folded into its shoulder, nor the forearm through the upper arm.
    const int shoulder = kShoulderL + s;
    const int elbow = kElbowL + s;
    const int hand = kHandL + s;
    const double hx = cx(p, hand, 0) - cx(p, shoulder, 0);
    const double hy = cx(p, hand, 1) - cx(p, shoulder, 1);
    const double hz = cx(p, hand, 2) - cx(p, shoulder, 2);
    double reach = core::hypot3(hx, hy, hz);
    if (reach == 0) reach = 1e-9;
    const double least = num::max(rd::kFoldArm, kElbowFoldSpan);
    if (reach < least) shift(p, l, hand, shoulder, -1, unit(hx, hy, hz), least - reach);
    // The elbow never points out ahead of the chest off the line from the shoulder to the hand,
    // measured where that line leaves the chest a direction to be square in, taken in as it firms.
    const double ch = (f.chest.x * hx + f.chest.y * hy + f.chest.z * hz) / (reach * reach);
    const double sx = f.chest.x - ch * hx;
    const double sy = f.chest.y - ch * hy;
    const double sz = f.chest.z - ch * hz;
    const double firm = core::clamp((sx * sx + sy * sy + sz * sz - 0.09) / 0.16, 0, 1);
    if (firm > 0) {
      const Vec3 b = unit(sx, sy, sz);
      const double ahead = (cx(p, elbow, 0) - (cx(p, shoulder, 0) + cx(p, hand, 0)) / 2) * b.x +
                           (cx(p, elbow, 1) - (cx(p, shoulder, 1) + cx(p, hand, 1)) / 2) * b.y +
                           (cx(p, elbow, 2) - (cx(p, shoulder, 2) + cx(p, hand, 2)) / 2) * b.z;
      if (ahead > 0 && reach < kStraight * (rd::kUpperArm + rd::kForearm)) {
        shift(p, l, elbow, shoulder, hand, b, -ahead * firm * firm);
      }
    }
  }
  keep_out(p, l);
}

// The turn, rad a step, that carries angular momentum h about `com`: the body's inertia about
// its centre of mass inverted.
Vec3 unturn(const PointArray& p, const Vec3& com, double hx, double hy, double hz) {
  double xx = 0, yy = 0, zz = 0, xy = 0, xz = 0, yz = 0;
  for (int i = 0; i < N; i++) {
    const double rx = cx(p, i, 0) - com.x;
    const double ry = cx(p, i, 1) - com.y;
    const double rz = cx(p, i, 2) - com.z;
    const double m = kMass[static_cast<size_t>(i)];
    xx += m * (ry * ry + rz * rz);
    yy += m * (rx * rx + rz * rz);
    zz += m * (rx * rx + ry * ry);
    xy -= m * rx * ry;
    xz -= m * rx * rz;
    yz -= m * ry * rz;
  }
  const double c00 = yy * zz - yz * yz;
  const double c01 = xz * yz - xy * zz;
  const double c02 = xy * yz - xz * yy;
  const double det = xx * c00 + xy * c01 + xz * c02;
  if ((det < 0 ? -det : det) < 1e-12) return Vec3{0, 0, 0};
  const double c11 = xx * zz - xz * xz;
  const double c12 = xy * xz - xx * yz;
  const double c22 = xx * yy - xy * xy;
  return Vec3{(c00 * hx + c01 * hy + c02 * hz) / det, (c01 * hx + c11 * hy + c12 * hz) / det,
              (c02 * hx + c12 * hy + c22 * hz) / det};
}

// Take `share` of the body's turning about its centre of mass away: a rigid turn removed from
// every point's velocity, its way left alone.
void scrub(const PointArray& p, PointArray& l, double share) {
  const Vec3 com = centre_of(p);
  double hx = 0, hy = 0, hz = 0;
  for (int i = 0; i < N; i++) {
    const double rx = cx(p, i, 0) - com.x;
    const double ry = cx(p, i, 1) - com.y;
    const double rz = cx(p, i, 2) - com.z;
    const double vx = cx(p, i, 0) - cx(l, i, 0);
    const double vy = cx(p, i, 1) - cx(l, i, 1);
    const double vz = cx(p, i, 2) - cx(l, i, 2);
    const double m = kMass[static_cast<size_t>(i)];
    hx += m * (ry * vz - rz * vy);
    hy += m * (rz * vx - rx * vz);
    hz += m * (rx * vy - ry * vx);
  }
  const Vec3 w = unturn(p, com, hx * share, hy * share, hz * share);
  for (int i = 0; i < N; i++) {
    const double rx = cx(p, i, 0) - com.x;
    const double ry = cx(p, i, 1) - com.y;
    const double rz = cx(p, i, 2) - com.z;
    px(l, i, 0) += w.y * rz - w.z * ry;
    px(l, i, 1) += w.z * rx - w.x * rz;
    px(l, i, 2) += w.x * ry - w.y * rx;
  }
}

// The limbs the muscles drive, each off the trunk point it hangs from.
constexpr int kLimb[8] = {kKneeL, kKneeR, kFootL, kFootR, kElbowL, kElbowR, kHandL, kHandR};
constexpr int kRoot[8] = {kHipL,      kHipR,      kHipL,      kHipR,
                          kShoulderL, kShoulderR, kShoulderL, kShoulderR};

// The pose the muscles drive toward (per limb, off its root, in the trunk's frame: x across to
// his right, y up the spine, z out of the chest), `brace` of the way from the slack pose a body
// lies in to the protective reflex. Held in HIS frame, never chasing the world's down: hands
// that kept reaching for the snow as he turned would windmill him.
std::array<Vec3, 8> pose_of(double brace) {
  std::array<Vec3, 8> pose{};
  const double arm = rd::kUpperArm + rd::kForearm;
  for (int s = 0; s < 2; s++) {
    const double side = s == 0 ? -1 : 1;
    // LEGS: spread and a little bent in the brace, never tucked; the slack all but straight.
    const double hip = 0.15 + 0.15 * brace;
    const double knee = 0.3 + 0.25 * brace;
    const double out = 0.12 + 0.1 * brace;
    const double kx = side * out * rd::kThigh;
    const double ky = -core::libm::cos(hip) * rd::kThigh;
    const double kz = core::libm::sin(hip) * rd::kThigh;
    pose[static_cast<size_t>(s)] = Vec3{kx, ky, kz};
    pose[static_cast<size_t>(2) + static_cast<size_t>(s)] =
        Vec3{kx + side * out * rd::kShin, ky - core::libm::cos(hip - knee) * rd::kShin,
             kz + core::libm::sin(hip - knee) * rd::kShin};
    // ARMS: the slack ones out at his sides; the brace's thrown out ahead of him, wide and a
    // little down, the hands put out to meet the snow.
    const double slack = 1 - brace;
    const Vec3 aim = unit(side * (0.55 * slack + 0.5 * brace), -0.8 * slack - 0.3 * brace,
                          0.05 * slack + 0.8 * brace);
    const double h = (slack * 0.92 + brace * rd::kReach) * arm;
    pose[static_cast<size_t>(6) + static_cast<size_t>(s)] = Vec3{aim.x * h, aim.y * h, aim.z * h};
    // The elbow where the two bones put it, bowed back and out off the line to the hand.
    const double u = rd::kUpperArm;
    const double along = (u * u - rd::kForearm * rd::kForearm + h * h) / (2 * h);
    const double off = std::sqrt(num::max(0.0, u * u - along * along));
    const double bx = side * 0.4;
    const double bz = -1;
    const double ba = bx * aim.x + bz * aim.z;
    const Vec3 bow = unit(bx - ba * aim.x, -ba * aim.y, bz - ba * aim.z);
    pose[static_cast<size_t>(4) + static_cast<size_t>(s)] =
        Vec3{aim.x * along + bow.x * off, aim.y * along + bow.y * off, aim.z * along + bow.z * off};
  }
  return pose;
}

// THE MUSCLES: every limb pulled toward its pose by a damped spring, as a change of velocity,
// with the push and the twist that change would put on the body as a whole taken back off every
// point: the muscles turn the limbs against the trunk, never the man.
void drive(PointArray& p, PointArray& l, const Ragdoll& b, double dt) {
  const double brace = b.down < 0 ? 1 : core::libm::exp(-rd::kRelax * b.down);
  const double w = rd::kLie + (rd::kBrace - rd::kLie) * brace;
  const double zeta = rd::kLieDamp + (rd::kBraceDamp - rd::kLieDamp) * brace;
  const double spring = num::min(0.5, (w * dt) * (w * dt));
  const double damp = num::min(0.9, 2 * zeta * w * dt);
  const Frame f = frame_of(p);
  const std::array<Vec3, 8> pose = pose_of(brace);
  std::array<Vec3, 8> dv{};
  double sx = 0, sy = 0, sz = 0;
  for (int k = 0; k < 8; k++) {
    const int i = kLimb[k];
    const int r = kRoot[k];
    // A limb planted in the snow yields: driven against it, it is a lever he vaults himself over.
    if (b.planted & (1u << i)) continue;
    const Vec3& q = pose[static_cast<size_t>(k)];
    const double tgt[3] = {
        cx(p, r, 0) + f.across.x * q.x + f.up.x * q.y + f.chest.x * q.z,
        cx(p, r, 1) + f.across.y * q.x + f.up.y * q.y + f.chest.y * q.z,
        cx(p, r, 2) + f.across.z * q.x + f.up.z * q.y + f.chest.z * q.z,
    };
    double d[3];
    for (int a = 0; a < 3; a++) {
      const double rel = cx(p, i, a) - cx(l, i, a) - (cx(p, r, a) - cx(l, r, a));
      d[a] = spring * (tgt[a] - cx(p, i, a)) - damp * rel;
    }
    dv[static_cast<size_t>(k)] = Vec3{d[0], d[1], d[2]};
    const double m = kMass[static_cast<size_t>(i)];
    sx += m * d[0];
    sy += m * d[1];
    sz += m * d[2];
  }
  const Vec3 com = centre_of(p);
  double hx = 0, hy = 0, hz = 0;
  for (int k = 0; k < 8; k++) {
    const int i = kLimb[k];
    const Vec3& d = dv[static_cast<size_t>(k)];
    const double m = kMass[static_cast<size_t>(i)];
    const double rx = cx(p, i, 0) - com.x;
    const double ry = cx(p, i, 1) - com.y;
    const double rz = cx(p, i, 2) - com.z;
    hx += m * (ry * d.z - rz * d.y);
    hy += m * (rz * d.x - rx * d.z);
    hz += m * (rx * d.y - ry * d.x);
    px(l, i, 0) -= d.x;
    px(l, i, 1) -= d.y;
    px(l, i, 2) -= d.z;
  }
  const Vec3 spin = unturn(p, com, hx, hy, hz);
  for (int i = 0; i < N; i++) {
    const double rx = cx(p, i, 0) - com.x;
    const double ry = cx(p, i, 1) - com.y;
    const double rz = cx(p, i, 2) - com.z;
    px(l, i, 0) += sx / kTotal + spin.y * rz - spin.z * ry;
    px(l, i, 1) += sy / kTotal + spin.z * rx - spin.x * rz;
    px(l, i, 2) += sz / kTotal + spin.x * ry - spin.y * rx;
  }
}

// THE AIR: the quadratic slowing of the centre of mass's way, taken off every point alike so it
// turns the body none, integrated exactly over the step (v <- v / (1 + k |v| dt)).
void air_drag(const PointArray& p, PointArray& l, double dt) {
  const Vec3 v = velocity_of(p, l);
  const double speed = core::hypot3(v.x, v.y, v.z) / dt;
  const double k = 0.5 * tuning::kAirDensity * rd::kDragArea / kTotal;
  const double lose = 1 - 1 / (1 + k * speed * dt);
  if (lose <= 0) return;
  for (int i = 0; i < N; i++) {
    px(l, i, 0) += v.x * lose;
    px(l, i, 1) += v.y * lose;
    px(l, i, 2) += v.z * lose;
  }
}

// The body as it stands on the skis, in the skier's frame (x right, y up, z forward, the origin
// at the centre of gravity): the hips over the boots, the trunk pitched forward over bent knees,
// the hands out ahead on the poles and the feet in the bindings.
std::array<Vec3, N> standing() {
  constexpr double kPitch = 0.4;
  const Vec3 hips{0, -0.05, -0.06};
  const double c = core::libm::cos(kPitch);
  const double s = core::libm::sin(kPitch);
  std::array<Vec3, N> out{};
  for (int i = 0; i < 5; i++) {
    const Vec3& q = kTorsoPoints[static_cast<size_t>(i)];
    out[static_cast<size_t>(i)] =
        Vec3{hips.x + q.x, hips.y + q.y * c - q.z * s, hips.z + q.y * s + q.z * c};
  }
  for (int k = 0; k < 2; k++) {
    const double side = k == 0 ? -1 : 1;
    out[static_cast<size_t>(kKneeL) + static_cast<size_t>(k)] = Vec3{side * 0.16, -0.5, 0.12};
    out[static_cast<size_t>(kFootL) + static_cast<size_t>(k)] = Vec3{side * 0.15, -0.95, 0};
    out[static_cast<size_t>(kElbowL) + static_cast<size_t>(k)] = Vec3{side * 0.32, 0.22, 0.18};
    out[static_cast<size_t>(kHandL) + static_cast<size_t>(k)] = Vec3{side * 0.3, 0.05, 0.45};
  }
  return out;
}

void fold_hash(uint64_t& h, double v) {
  uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  for (int b = 0; b < 64; b += 8) {
    h ^= (u >> b) & 0xffu;
    h *= 0x100000001b3ull;
  }
}

}  // namespace

const char* point_name(int32_t point) {
  static const char* const kNames[N] = {"hip-l",   "hip-r",  "shoulder-l", "shoulder-r", "head",
                                        "knee-l",  "knee-r", "foot-l",     "foot-r",     "elbow-l",
                                        "elbow-r", "hand-l", "hand-r"};
  return point >= 0 && point < N ? kNames[point] : "?";
}

double point_mass(int32_t point) { return kMass[static_cast<size_t>(point)]; }

double point_radius(int32_t point) { return radius_of(point); }

Vec3 centre_of(const PointArray& p) {
  double x = 0, y = 0, z = 0;
  for (int i = 0; i < N; i++) {
    const double m = kMass[static_cast<size_t>(i)];
    x += cx(p, i, 0) * m;
    y += cx(p, i, 1) * m;
    z += cx(p, i, 2) * m;
  }
  return Vec3{x / kTotal, y / kTotal, z / kTotal};
}

Ragdoll throw_body(double x, double y, double z, double heading, double lean, double vx, double vy,
                   double vz, double wx, double wy, double wz, double dt) {
  // The skier's frame in the world: his right rolled down by `lean` about his forward.
  const double sh = core::libm::sin(heading);
  const double ch = core::libm::cos(heading);
  const double sl = core::libm::sin(lean);
  const double cl = core::libm::cos(lean);
  const Vec3 fwd{sh, 0, ch};
  const Vec3 right{ch * cl, -sl, -sh * cl};
  const Vec3 up{ch * sl, cl, -sh * sl};
  const double oy = y + rd::kStand;
  Ragdoll b;
  b.active = true;
  const std::array<Vec3, N> stand = standing();
  for (int i = 0; i < N; i++) {
    const Vec3& q = stand[static_cast<size_t>(i)];
    px(b.p, i, 0) = x + right.x * q.x + up.x * q.y + fwd.x * q.z;
    px(b.p, i, 1) = oy + right.y * q.x + up.y * q.y + fwd.y * q.z;
    px(b.p, i, 2) = z + right.z * q.x + up.z * q.y + fwd.z * q.z;
  }
  // Settle the joints to their lengths before anything moves.
  for (int k = 0; k < 16; k++) hold_links(b.p);
  const Vec3 com = centre_of(b.p);
  // Each point's last position: where the body's way and turn put it a step ago.
  const double step = dt;
  for (int i = 0; i < N; i++) {
    const double rx = cx(b.p, i, 0) - com.x;
    const double ry = cx(b.p, i, 1) - com.y;
    const double rz = cx(b.p, i, 2) - com.z;
    px(b.last, i, 0) = cx(b.p, i, 0) - (vx + wy * rz - wz * ry) * step;
    px(b.last, i, 1) = cx(b.p, i, 1) - (vy + wz * rx - wx * rz) * step;
    px(b.last, i, 2) = cx(b.p, i, 2) - (vz + wx * ry - wy * rx) * step;
  }
  b.x = com.x;
  b.y = com.y;
  b.z = com.z;
  b.vx = vx;
  b.vy = vy;
  b.vz = vz;
  return b;
}

Ragdoll throw_of(double x, double y, double z, double heading, double lean, double vx, double vy,
                 double vz, double yaw_rate, double side, const Over& how, double dt) {
  const double flat = core::hypot(vx, vz);
  const double spin = num::min(rd::kMaxSpin, flat * rd::kKeep / rd::kTumbleRadius);
  const double pitch = how.pitch * spin;
  const double roll = -side * how.side * num::max(rd::kTopple, spin);
  const double turn = core::hypot(pitch, roll);
  const double cap = turn > rd::kMaxSpin ? rd::kMaxSpin / turn : 1;
  const double fx = core::libm::sin(heading);
  const double fz = core::libm::cos(heading);
  const double wx = (fz * pitch + fx * roll) * cap;
  const double wy = yaw_rate * rd::kCarry;
  const double wz = (-fx * pitch + fz * roll) * cap;
  const double up = num::max(0.0, vy) * rd::kKeep + how.up;
  return throw_body(x, y, z, heading, lean, vx * rd::kKeep, up, vz * rd::kKeep, wx, wy, wz, dt);
}

void step_ragdoll(Ragdoll& b, const Level& level, const TreeIndex* trees, double dt) {
  PointArray& P = b.p;
  PointArray& L = b.last;
  b.t += dt;
  const Vec3 spine0 = spine_of(P);
  drive(P, L, b, dt);
  air_drag(P, L, dt);
  const double g = tuning::kGravity * dt * dt;
  for (size_t i = 0; i < P.size(); i++) {
    const double v = P[i] - L[i];
    L[i] = P[i];
    P[i] += v;
  }
  for (int i = 0; i < N; i++) px(P, i, 1) -= g;
  // The snow under each point, read once a step: it moves a hand's width.
  std::array<double, N> hold{}, soft{}, packed{}, pushed{}, arrive{};
  std::array<Vec3, N> normal{};
  for (int i = 0; i < N; i++) {
    const double x = cx(P, i, 0);
    const double z = cx(P, i, 2);
    const double pk = core::clamp(level.packed_at(x, z), 0, 1);
    packed[static_cast<size_t>(i)] = pk;
    soft[static_cast<size_t>(i)] = 1 - pk;
    const double ground = level.ground_at(x, z);
    const double r = radius_of(i);
    const double floor = ground - rd::kSink * (1 - pk) + r;
    // A point already under that floor (thrown from skis sunk in powder) is held where it is: the
    // loose snow under it is pressed, and nothing lifts it out of the hole in one step. Only the
    // settled base under the loose snow is never gone through.
    const double base = ground - tuning::snow::kPowderSink * (1 - pk) + r;
    hold[static_cast<size_t>(i)] =
        num::min(floor, num::max(num::max(cx(P, i, 1), cx(L, i, 1)), base));
    level.normal_at(x, z, normal[static_cast<size_t>(i)]);
    const Vec3& n = normal[static_cast<size_t>(i)];
    arrive[static_cast<size_t>(i)] =
        num::max(0.0, -((cx(P, i, 0) - cx(L, i, 0)) * n.x + (cx(P, i, 1) - cx(L, i, 1)) * n.y +
                        (cx(P, i, 2) - cx(L, i, 2)) * n.z) /
                          dt);
  }
  b.impact.fill(0);
  b.struck.fill(0);
  const double lo = tuning::wipeout::kEdgeMargin;
  const double hi = level.size - tuning::wipeout::kEdgeMargin;
  const Vec3 com0 = centre_of(P);
  for (int k = 0; k < rd::kIterations; k++) {
    // The joints' limits first and the bones' lengths last, and the pair giving the body no
    // energy.
    const double before = internal(P, L);
    hold_joints(P, L);
    hold_links(P);
    calm(P, L, before);
    for (int i = 0; i < N; i++) {
      px(P, i, 0) = core::clamp(cx(P, i, 0), lo, hi);
      px(P, i, 2) = core::clamp(cx(P, i, 2), lo, hi);
      if (trees) {
        for_each_tree_near(*trees, com0.x, com0.z, [&](int32_t t) {
          const auto& tree = level.trees[static_cast<size_t>(t)];
          if (cx(P, i, 1) > tree.y + tree.height) return;
          const double dx = cx(P, i, 0) - tree.x;
          const double dz = cx(P, i, 2) - tree.z;
          double d = core::hypot(dx, dz);
          if (d == 0) d = 1e-6;
          const double reach = radius_of(i) + tree.radius;
          if (d >= reach) return;
          const double closing =
              -((cx(P, i, 0) - cx(L, i, 0)) * dx + (cx(P, i, 2) - cx(L, i, 2)) * dz) / (d * dt);
          if (closing > b.struck[static_cast<size_t>(i)])
            b.struck[static_cast<size_t>(i)] = closing;
          px(P, i, 0) = tree.x + dx / d * reach;
          px(P, i, 2) = tree.z + dz / d * reach;
        });
      }
      const double h = hold[static_cast<size_t>(i)];
      if (cx(P, i, 1) < h) {
        pushed[static_cast<size_t>(i)] += h - cx(P, i, 1);
        px(P, i, 1) = h;
      }
    }
  }
  // Along the snow: what every point on it loses of its way; `patch` of it off the body as a whole.
  bool touching = false;
  uint16_t planted = 0;
  double lx = 0, ly = 0, lz = 0;
  for (int i = 0; i < N; i++) {
    const size_t s = static_cast<size_t>(i);
    if (pushed[s] <= 0) continue;
    touching = true;
    planted = static_cast<uint16_t>(planted | (1u << i));
    // A fresh contact is a blow; a point already lying on the snow is not.
    if (!(b.planted & (1u << i))) b.impact[s] = arrive[s];
    const Vec3& n = normal[s];
    double vx = (cx(P, i, 0) - cx(L, i, 0)) / dt;
    double vy = (cx(P, i, 1) - cx(L, i, 1)) / dt;
    double vz = (cx(P, i, 2) - cx(L, i, 2)) / dt;
    // The way into the snow gone, and the way out of it no faster than `kPushOut`: a point put
    // back on the surface is a position corrected, not a launch.
    const double un = vx * n.x + vy * n.y + vz * n.z;
    const double out = core::clamp(un, 0, rd::kPushOut) - un;
    vx += out * n.x;
    vy += out * n.y;
    vz += out * n.z;
    const double along = vx * n.x + vy * n.y + vz * n.z;
    const double tx = vx - along * n.x;
    const double ty = vy - along * n.y;
    const double tz = vz - along * n.z;
    const double slide = core::hypot3(tx, ty, tz);
    if (slide > 1e-9) {
      const double mu = rd::kFrictionPacked * packed[s] + rd::kFrictionPowder * (1 - packed[s]);
      const double coulomb = num::min(slide, mu * num::max(tuning::kGravity * n.y * dt, arrive[s]));
      const double plough = (slide - coulomb) * num::min(1.0, rd::kPlough * soft[s] * dt);
      const double share = (coulomb + plough) / slide;
      const double own = share * (1 - rd::kPatch);
      const double m = kMass[s] * share * rd::kPatch;
      vx -= tx * own;
      vy -= ty * own;
      vz -= tz * own;
      lx += tx * m;
      ly += ty * m;
      lz += tz * m;
    }
    px(L, i, 0) = cx(P, i, 0) - vx * dt;
    px(L, i, 1) = cx(P, i, 1) - vy * dt;
    px(L, i, 2) = cx(P, i, 2) - vz * dt;
  }
  double fastest = 0;
  for (int i = 0; i < N; i++) {
    px(L, i, 0) += lx / kTotal * dt;
    px(L, i, 1) += ly / kTotal * dt;
    px(L, i, 2) += lz / kTotal * dt;
    fastest = num::max(fastest, core::hypot3(cx(P, i, 0) - cx(L, i, 0), cx(P, i, 1) - cx(L, i, 1),
                                             cx(P, i, 2) - cx(L, i, 2)) /
                                    dt);
  }
  // The body on the snow scrubs off his turning, the more of him lies on it and the deeper the
  // powder he lies in.
  double lying = 0;
  double buried = 0;
  for (int i = 0; i < N; i++) {
    if (pushed[static_cast<size_t>(i)] <= 0) continue;
    lying += kMass[static_cast<size_t>(i)];
    buried += kMass[static_cast<size_t>(i)] * soft[static_cast<size_t>(i)];
  }
  if (lying > 0) {
    const double deep = buried / lying;
    lying = num::min(1.0, lying / kTorsoMass);
    scrub(P, L, num::min(1.0, rd::kSpinDrag * lying * (1 + deep) * dt));
  }
  const Vec3 com = centre_of(P);
  const Vec3 was = centre_of(L);
  b.x = com.x;
  b.y = com.y;
  b.z = com.z;
  b.vx = (com.x - was.x) / dt;
  b.vy = (com.y - was.y) / dt;
  b.vz = (com.z - was.z) / dt;
  const Vec3 spine1 = spine_of(P);
  b.tumble += core::libm::acos(core::clamp(dot(spine0, spine1), -1, 1));
  b.touching = touching;
  b.planted = planted;
  // Down: the brace gives way from the step his trunk, or a trunk's weight of him, meets the snow.
  const double trunk_on =
      pushed[kHipL] + pushed[kHipR] + pushed[kShoulderL] + pushed[kShoulderR] + pushed[kHead];
  if (b.down >= 0) {
    b.down += dt;
  } else if (lying >= 0.5 || trunk_on > 0) {
    b.down = 0;
  }
  b.still = touching && fastest < rd::kRestSpeed ? b.still + dt : 0;
  b.aloft = touching ? 0 : b.aloft + dt;
  if (b.aloft > rd::kFlightGap) {
    b.ground = 0;
  } else if (touching) {
    b.ground += dt;
  }
}

uint64_t ragdoll_hash(const Ragdoll& b, uint64_t h) {
  if (!b.active) return h;
  for (double v : b.p) fold_hash(h, v);
  for (double v : b.last) fold_hash(h, v);
  const double nums[] = {b.t, b.tumble, b.down, b.still, b.aloft, b.ground};
  for (double v : nums) fold_hash(h, v);
  h ^= b.planted;
  h *= 0x100000001b3ull;
  return h;
}

}  // namespace ge::skier
