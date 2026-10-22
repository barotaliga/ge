// The skier's rig posed from a SkierState, and the model's locals made from the pose.
//
// ge:summary The skier poser: lags, the skis on their edge, IK legs and arms, poles, model locals
// ge:keywords skier rig pose ik lean edge pole model locals
// ge:skill skier assets
// ge:test tests/skier_rig_pose_test.cpp
#include "skier/app/rig_pose.hpp"

#include <cmath>

#include "assets/pose.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "skier/app/figure.hpp"

namespace ge::skier {

namespace {

using core::Quat;
using core::Vec3;
namespace libm = core::libm;

Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 scale(Vec3 a, double k) { return {a.x * k, a.y * k, a.z * k}; }
double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double length(Vec3 a) { return std::sqrt(dot(a, a)); }
Vec3 cross(Vec3 a, Vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vec3 unit(Vec3 a, Vec3 fallback = {0, -1, 0}) {
  const double n = length(a);
  return n > 1e-12 ? scale(a, 1 / n) : fallback;
}
Quat conj(Quat q) { return {-q.x, -q.y, -q.z, q.w}; }
Vec3 rot(Quat q, Vec3 v) { return core::rotate(q, v); }

// The rotation whose columns are the given right, up and forward axes.
Quat from_basis(Vec3 r, Vec3 u, Vec3 f) {
  const double t = r.x + u.y + f.z;
  Quat q;
  if (t > 0) {
    const double s = std::sqrt(t + 1) * 2;
    q = {(u.z - f.y) / s, (f.x - r.z) / s, (r.y - u.x) / s, s / 4};
  } else if (r.x > u.y && r.x > f.z) {
    const double s = std::sqrt(1 + r.x - u.y - f.z) * 2;
    q = {s / 4, (u.x + r.y) / s, (f.x + r.z) / s, (u.z - f.y) / s};
  } else if (u.y > f.z) {
    const double s = std::sqrt(1 + u.y - r.x - f.z) * 2;
    q = {(u.x + r.y) / s, s / 4, (f.y + u.z) / s, (f.x - r.z) / s};
  } else {
    const double s = std::sqrt(1 + f.z - r.x - u.y) * 2;
    q = {(f.x + r.z) / s, (f.y + u.z) / s, s / 4, (r.y - u.x) / s};
  }
  return core::normalize(q);
}

// The frame with `d` as its y axis and `ref` (made square to it) as its x.
Quat aim(Vec3 d, Vec3 ref) {
  const Vec3 y = unit(d);
  Vec3 x = sub(ref, scale(y, dot(ref, y)));
  if (length(x) < 1e-6) x = cross(y, Vec3{0, 0, 1});
  x = unit(x, Vec3{1, 0, 0});
  return from_basis(x, y, cross(x, y));
}

// The turn that carries a bone from its bind direction onto `posed`, twisted by `ref`.
Quat swing(Vec3 bind_dir, Vec3 posed, Vec3 ref) {
  return core::multiply(aim(posed, ref), conj(aim(bind_dir, Vec3{1, 0, 0})));
}

Quat roll(double right_down) { return core::from_axis_angle(0, 0, 1, -right_down); }
Quat pitch_fwd(double a) { return core::from_axis_angle(1, 0, 0, a); }

// A reach eased toward its most: itself up to `knee` of `most`, then bending smoothly over
// onto `most` (tanh, so its first and second derivatives are continuous there: a hard clamp's
// kink is a snap in the joints it moves).
double soft_reach(double d, double most, double knee = 0.9) {
  const double from = knee * most;
  if (d <= from) return d;
  const double room = most - from;
  return from + room * libm::tanh((d - from) / room);
}

// The middle joint of a two-bone chain from `a` reaching for `b`, bent toward `pole`. With
// `ease`, `b` comes back moved onto the eased reach when it lies far out (the hands: their place
// is a wish); without, `b` is where it must be (the feet: the pelvis is already lowered so a leg
// reaches) and only a reach past straight is held to it.
Vec3 two_bone(Vec3 a, Vec3& b, double upper, double lower, Vec3 pole, bool ease) {
  Vec3 d = sub(b, a);
  const double least = std::fabs(upper - lower) + 1e-3;
  const Vec3 dir = unit(d);
  const double most = (upper + lower) * 0.999;
  const double dist =
      std::fmax(ease ? soft_reach(length(d), most) : std::fmin(length(d), most), least);
  b = add(a, scale(dir, dist));
  const double along = (upper * upper - lower * lower + dist * dist) / (2 * dist);
  const double out = std::sqrt(std::fmax(upper * upper - along * along, 0.0));
  const Vec3 bend = unit(sub(pole, scale(dir, dot(pole, dir))), Vec3{0, 0, 1});
  return add(add(a, scale(dir, along)), scale(bend, out));
}

const RigJoint& J(int i) { return rig_joints()[static_cast<size_t>(i)]; }
double bone(int a, int b) { return length(sub(J(b).at, J(a).at)); }
Vec3 bind_dir(int a, int b) { return unit(sub(J(b).at, J(a).at)); }

double lag(double value, double target, double dt, double tau) {
  return value + (target - value) * (1 - libm::exp(-dt / tau));
}

struct Ids {
  int root = rig_joint("root"), pelvis = rig_joint("pelvis"), spine = rig_joint("spine"),
      chest = rig_joint("chest"), neck = rig_joint("neck"), head = rig_joint("head");
  int shoulder[2] = {rig_joint("shoulder_l"), rig_joint("shoulder_r")};
  int elbow[2] = {rig_joint("elbow_l"), rig_joint("elbow_r")};
  int wrist[2] = {rig_joint("wrist_l"), rig_joint("wrist_r")};
  int pole[2] = {rig_joint("pole_l"), rig_joint("pole_r")};
  int tip[2] = {rig_joint("pole_tip_l"), rig_joint("pole_tip_r")};
  int hip[2] = {rig_joint("hip_l"), rig_joint("hip_r")};
  int knee[2] = {rig_joint("knee_l"), rig_joint("knee_r")};
  int ankle[2] = {rig_joint("ankle_l"), rig_joint("ankle_r")};
  int ski_tip[2] = {rig_joint("ski_tip_l"), rig_joint("ski_tip_r")};
  int ski_tail[2] = {rig_joint("ski_tail_l"), rig_joint("ski_tail_r")};
};

const Ids& ids() {
  static const Ids k;
  return k;
}

// The ski's widest half-width, m: a ski on its edge stands on its shovel and tail (the sidecut
// bends it onto the snow along the whole edge), so it is rolled about the widest line, and the
// waist rises a little off the snow rather than the shovel cutting into it.
double widest() {
  static const double k = [] {
    const double tail = J(rig_joint("ski_tail_l")).at.z, tip = J(rig_joint("ski_tip_l")).at.z;
    double most = 0;
    for (int i = 0; i <= 200; i++) {
      most = std::fmax(most, ski_half_width(kReferenceSkis, tail + (tip - tail) * i / 200.0));
    }
    return most;
  }();
  return k;
}

constexpr double kPoleTipHeight = 0.06;  // m over the snow the tips trail at
constexpr double kDownRoll = 1.45;       // rad: lying on the side

}  // namespace

void RigPoser::pose(const SkierState& s, double dt, RigPose& out) {
  const Ids& k = ids();
  const double lean_t = s.down ? kDownRoll : s.lean;
  const double edge_t = s.down ? kDownRoll : s.edge;
  const double tuck_t = core::clamp(s.tuck, 0.0, 1.0);
  if (!primed_) {
    lean1_ = lean2_ = lean_t;
    edge_ = edge_t;
    tuck_ = tuck_t;
    primed_ = true;
  } else {
    lean1_ = lag(lean1_, lean_t, dt, kLeanLag);
    lean2_ = lag(lean2_, lean1_, dt, kLeanLag);
    edge_ = lag(edge_, edge_t, dt, kEdgeLag);
    tuck_ = lag(tuck_, tuck_t, dt, kTuckLag);
  }
  const double lean = lean2_, edge = edge_, tuck = tuck_;
  for (size_t i = 0; i < kRigJoints; i++) {
    out.at[i] = J(static_cast<int>(i)).at;
    out.turn[i] = core::identity();
  }
  const auto set = [&](int j, Vec3 at, Quat turn) {
    out.at[static_cast<size_t>(j)] = at;
    out.turn[static_cast<size_t>(j)] = turn;
  };

  // The skis, each rolled about its edge on the snow; the boots ride them.
  const Quat ski_roll = roll(edge);
  const double edge_side = edge >= 0 ? 1 : -1;
  const double half = widest();
  Vec3 ankle[2];
  for (int m = 0; m < 2; m++) {
    const Vec3 a = J(k.ankle[m]).at;
    const Vec3 pivot{a.x + edge_side * half, 0, 0};
    const auto on_ski = [&](Vec3 p) { return add(pivot, rot(ski_roll, sub(p, pivot))); };
    ankle[m] = on_ski(a);
    set(k.ankle[m], ankle[m], ski_roll);
    set(k.ski_tip[m], on_ski(J(k.ski_tip[m]).at), ski_roll);
    set(k.ski_tail[m], on_ski(J(k.ski_tail[m]).at), ski_roll);
  }

  // The pelvis, inclined about the skis' base, then lowered so the longer leg's reach is eased
  // (soft_reach) short of straight. The longer leg is a smooth maximum of the two, so the pelvis
  // does not kink as the legs trade places.
  const double thigh = bone(k.hip[0], k.knee[0]), shin = bone(k.knee[0], k.ankle[0]);
  const Quat pelvis_turn = core::multiply(roll(0.85 * lean), pitch_fwd(0.25 * tuck));
  Vec3 pelvis = rot(roll(lean), Vec3{0, figure_hip_height(tuck), -0.05 - 0.08 * tuck});
  Vec3 hip[2];
  const auto longer = [&]() {
    double d[2];
    for (int m = 0; m < 2; m++) {
      hip[m] = add(pelvis, rot(pelvis_turn, sub(J(k.hip[m]).at, J(k.pelvis).at)));
      d[m] = length(sub(hip[m], ankle[m]));
    }
    const double hi = std::fmax(d[0], d[1]);
    constexpr double kSharp = 60;  // 1/m: how closely the smooth maximum hugs the true one
    return hi +
           libm::log(libm::exp(kSharp * (d[0] - hi)) + libm::exp(kSharp * (d[1] - hi))) / kSharp;
  };
  const double want = soft_reach(longer(), 0.985 * (thigh + shin), 0.85);
  for (int pass = 0; pass < 6; pass++) pelvis.y -= longer() - want;
  longer();
  set(k.pelvis, pelvis, pelvis_turn);
  const Vec3 fwd = rot(pelvis_turn, Vec3{0, 0, 1});
  const Vec3 right = rot(pelvis_turn, Vec3{1, 0, 0});
  for (int m = 0; m < 2; m++) {
    Vec3 foot = ankle[m];
    const Vec3 knee = two_bone(hip[m], foot, thigh, shin, fwd, false);
    set(k.hip[m], hip[m], swing(bind_dir(k.hip[m], k.knee[m]), sub(knee, hip[m]), right));
    set(k.knee[m], knee, swing(bind_dir(k.knee[m], k.ankle[m]), sub(foot, knee), right));
  }

  // The torso folds forward with the tuck; each joint up it leans into the turn less.
  const double bend = 0.25 + 0.85 * tuck;
  const Quat spine_turn = core::multiply(roll(0.7 * lean), pitch_fwd(0.5 * bend));
  const Quat chest_turn = core::multiply(roll(0.5 * lean), pitch_fwd(bend));
  const Quat neck_turn = core::multiply(roll(0.35 * lean), pitch_fwd(0.45 * bend));
  const Quat head_turn = core::multiply(roll(0.25 * lean), pitch_fwd(0.15 * bend));
  const Vec3 spine = add(pelvis, rot(pelvis_turn, sub(J(k.spine).at, J(k.pelvis).at)));
  const Vec3 chest = add(spine, rot(spine_turn, sub(J(k.chest).at, J(k.spine).at)));
  const Vec3 neck = add(chest, rot(chest_turn, sub(J(k.neck).at, J(k.chest).at)));
  const Vec3 head = add(neck, rot(neck_turn, sub(J(k.head).at, J(k.neck).at)));
  set(k.spine, spine, spine_turn);
  set(k.chest, chest, chest_turn);
  set(k.neck, neck, neck_turn);
  set(k.head, head, head_turn);

  // The arms reach for the hands' places in front of the hips; the poles trail from the fists.
  const Quat arms_roll = roll(0.6 * lean);
  const Vec3 chest_x = rot(chest_turn, Vec3{1, 0, 0});
  const double upper = bone(k.shoulder[0], k.elbow[0]), fore = bone(k.elbow[0], k.wrist[0]);
  const double grip = bone(k.wrist[0], k.pole[0]);
  for (int m = 0; m < 2; m++) {
    const double side = m == 0 ? -1 : 1;
    const Vec3 shoulder = add(chest, rot(chest_turn, sub(J(k.shoulder[m]).at, J(k.chest).at)));
    Vec3 wrist = rot(arms_roll, Vec3{side * 0.27, figure_hip_height(tuck) + 0.12 - 0.05 * tuck,
                                     0.27 + 0.04 * tuck});
    const Vec3 elbow = two_bone(shoulder, wrist, upper, fore, Vec3{side * 0.6, -0.2, -0.75}, true);
    const Vec3 forearm = unit(sub(wrist, elbow));
    set(k.shoulder[m], shoulder,
        swing(bind_dir(k.shoulder[m], k.elbow[m]), sub(elbow, shoulder), chest_x));
    const Quat fore_turn = swing(bind_dir(k.elbow[m], k.wrist[m]), forearm, chest_x);
    set(k.elbow[m], elbow, fore_turn);
    set(k.wrist[m], wrist, fore_turn);
    const Vec3 grip_at = add(wrist, scale(forearm, grip));
    // The tip at its height over the snow, trailing back and out from the fist.
    const double drop = core::clamp((kPoleTipHeight - grip_at.y) / kPoleLength, -1.0, 1.0);
    const Vec3 flat = unit(Vec3{side * 0.12, 0, -0.30 - 0.35 * tuck}, Vec3{0, 0, -1});
    const double reach = std::sqrt(std::fmax(1 - drop * drop, 0.0));
    const Vec3 along{flat.x * reach, drop, flat.z * reach};
    set(k.pole[m], grip_at, swing(bind_dir(k.pole[m], k.tip[m]), along, chest_x));
    set(k.tip[m], add(grip_at, scale(along, kPoleLength)),
        out.turn[static_cast<size_t>(k.pole[m])]);
  }
}

RigBinding bind_rig(const assets::Model& model) {
  RigBinding b;
  const size_t n = model.joints.size();
  b.rig_of.assign(n, -1);
  std::vector<assets::Trs> local(n);
  b.bind.resize(n);
  assets::rest_pose(model, local);
  assets::pose_globals(model, local, b.bind);
  int found = 0;
  for (size_t j = 0; j < n; j++) {
    b.rig_of[j] = rig_joint(model.joints[j].name);
    if (b.rig_of[j] >= 0) found++;
  }
  b.complete = found == kRigJoints;
  return b;
}

void rig_locals(const assets::Model& model, const RigBinding& binding, const RigPose& pose,
                std::span<assets::Trs> local) {
  // Globals as a rotation (columns right, up, forward) and a translation, in double.
  struct G {
    Vec3 c[3];
    Vec3 t;
  };
  const auto of_affine = [](const assets::Affine& a) {
    G g;
    for (size_t col = 0; col < 3; col++) {
      g.c[col] = Vec3{a.m[col], a.m[4 + col], a.m[8 + col]};
    }
    g.t = Vec3{a.m[3], a.m[7], a.m[11]};
    return g;
  };
  std::vector<G> global(model.joints.size());
  for (size_t j = 0; j < model.joints.size(); j++) {
    const int r = binding.rig_of[j];
    const int parent = model.joints[j].parent;
    G g = of_affine(binding.bind[j]);
    if (r >= 0) {
      const Quat turn = pose.turn[static_cast<size_t>(r)];
      for (Vec3& c : g.c) c = rot(turn, c);
      g.t = pose.at[static_cast<size_t>(r)];
    } else if (parent >= 0) {
      // Not the rig's: carried by its parent as at rest.
      const G& p = global[static_cast<size_t>(parent)];
      const G pb = of_affine(binding.bind[static_cast<size_t>(parent)]);
      const G rest = g;
      for (size_t col = 0; col < 3; col++) {
        const Vec3 in{dot(rest.c[col], pb.c[0]), dot(rest.c[col], pb.c[1]),
                      dot(rest.c[col], pb.c[2])};
        g.c[col] = add(add(scale(p.c[0], in.x), scale(p.c[1], in.y)), scale(p.c[2], in.z));
      }
      const Vec3 d = sub(rest.t, pb.t);
      const Vec3 in{dot(d, pb.c[0]), dot(d, pb.c[1]), dot(d, pb.c[2])};
      g.t = add(p.t, add(add(scale(p.c[0], in.x), scale(p.c[1], in.y)), scale(p.c[2], in.z)));
    }
    global[j] = g;
    // The local: the parent's global inverted (its rotation transposed), times this one.
    G l = g;
    if (parent >= 0) {
      const G& p = global[static_cast<size_t>(parent)];
      for (size_t col = 0; col < 3; col++) {
        l.c[col] = Vec3{dot(g.c[col], p.c[0]), dot(g.c[col], p.c[1]), dot(g.c[col], p.c[2])};
      }
      const Vec3 d = sub(g.t, p.t);
      l.t = Vec3{dot(d, p.c[0]), dot(d, p.c[1]), dot(d, p.c[2])};
    }
    // A bone's scale stays 1: its columns are unit, and the quaternion is taken from them.
    const Quat q = from_basis(unit(l.c[0]), unit(l.c[1]), unit(l.c[2]));
    assets::Trs& out = local[j];
    out.t[0] = static_cast<float>(l.t.x);
    out.t[1] = static_cast<float>(l.t.y);
    out.t[2] = static_cast<float>(l.t.z);
    out.r[0] = static_cast<float>(q.x);
    out.r[1] = static_cast<float>(q.y);
    out.r[2] = static_cast<float>(q.z);
    out.r[3] = static_cast<float>(q.w);
    out.s[0] = out.s[1] = out.s[2] = 1;
  }
}

}  // namespace ge::skier
