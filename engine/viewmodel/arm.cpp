// The arm's angles from its points and the hand's directions, by the engine's libm, and the
// faults against the ranges.
//
// ge:summary arm_angles, arm_faults and arm_excess: the arm's joints measured and judged
// ge:keywords viewmodel arm angles faults excess shoulder elbow wrist twist
// ge:skill render held-weapons
// ge:test tests/viewmodel_arm_test.cpp
#include "viewmodel/arm.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::viewmodel {

namespace {

namespace lm = core::libm;

struct V {
  double x = 0, y = 0, z = 0;
};
V of(const double p[3]) { return {p[0], p[1], p[2]}; }
V operator-(V a, V b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V operator*(V a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V cross(V a, V b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
double len(V a) { return std::sqrt(dot(a, a)); }
V unit(V a) {
  const double l = len(a);
  return l > 0 ? a * (1.0 / l) : V{};
}
// `a` with its part along unit `n` taken out.
V across(V a, V n) { return a - n * dot(a, n); }
double deg(double rad) { return rad * 180.0 / core::kPi; }

}  // namespace

ArmAngles arm_angles(const ArmPose& p) {
  ArmAngles out;
  const V s = of(p.shoulder), e = of(p.elbow), w = of(p.wrist);
  const V right = unit(of(p.right)), up = unit(of(p.up)), fwd = unit(of(p.forward));
  const V out_dir = right * static_cast<double>(p.side);
  const V u = unit(e - s), f = unit(w - e);

  out.shoulder_elevation = deg(lm::asin(core::clamp(dot(u, up), -1.0, 1.0)));
  if (out.shoulder_elevation > -60) out.shoulder_yaw = deg(lm::atan2(dot(u, out_dir), dot(u, fwd)));

  out.elbow_flex = deg(lm::acos(core::clamp(dot(u, f), -1.0, 1.0)));
  const bool bent = out.elbow_flex >= 10;
  if (bent) {
    const V line = unit(w - s);
    const V perp = unit(across(e - s, line));
    out.elbow_roll = deg(lm::atan2(dot(perp, out_dir), -dot(perp, up)));
  }

  // The wrist in the forearm's frame: its line, the radial side the hand carries round with it,
  // and the back of the hand, which is radial x distal for a right hand in a frame whose right,
  // up and forward follow one another (right x up = forward), and the other way for a left.
  const V radial = unit(across(of(p.radial), f));
  const V back = cross(radial, f) * static_cast<double>(p.side);
  const V hand = unit(of(p.distal));
  out.wrist_extension = deg(lm::atan2(dot(hand, back), dot(hand, f)));
  out.wrist_radial = deg(lm::atan2(dot(hand, radial), dot(hand, f)));
  if (bent) {
    // Thumb-up is the radial side toward the elbow's crease (the inside of its bend).
    const V crease = unit(across(u * -1.0, f));
    const V side = cross(f, crease);
    out.forearm_twist = deg(lm::atan2(dot(radial, side), dot(radial, crease)));
  }
  return out;
}

std::vector<std::string> arm_faults(const ArmAngles& a, const ArmLimits& l) {
  std::vector<std::string> out;
  auto check = [&](const char* what, double v, double lo, double hi) {
    if (v >= lo && v <= hi) return;
    char b[160];
    std::snprintf(b, sizeof b, "%s is %.0f deg (range %.0f..%.0f)", what, v, lo, hi);
    out.emplace_back(b);
  };
  check("the shoulder's elevation", a.shoulder_elevation, -90, l.shoulder_elevation_max);
  check("the shoulder's swing", a.shoulder_yaw, l.shoulder_yaw_min, l.shoulder_yaw_max);
  check("the elbow's bend", a.elbow_flex, 0, l.elbow_flex_max);
  check("the elbow's point (0 down, 90 out, 180 up)", a.elbow_roll, l.elbow_roll_min,
        l.elbow_roll_max);
  check("the wrist's extension (+ back, - palm)", a.wrist_extension, -l.wrist_flexion_max,
        l.wrist_extension_max);
  check("the wrist's deviation (+ thumb, - little finger)", a.wrist_radial, -l.wrist_ulnar_max,
        l.wrist_radial_max);
  check("the forearm's twist", a.forearm_twist, -l.forearm_twist_max, l.forearm_twist_max);
  return out;
}

double arm_excess(const ArmAngles& a, const ArmLimits& l) {
  auto over = [](double v, double lo, double hi) {
    return v < lo ? lo - v : v > hi ? v - hi : 0.0;
  };
  return over(a.shoulder_elevation, -90, l.shoulder_elevation_max) +
         over(a.shoulder_yaw, l.shoulder_yaw_min, l.shoulder_yaw_max) +
         over(a.elbow_flex, 0, l.elbow_flex_max) +
         over(a.elbow_roll, l.elbow_roll_min, l.elbow_roll_max) +
         over(a.wrist_extension, -l.wrist_flexion_max, l.wrist_extension_max) +
         over(a.wrist_radial, -l.wrist_ulnar_max, l.wrist_radial_max) +
         over(a.forearm_twist, -l.forearm_twist_max, l.forearm_twist_max);
}

namespace {

// The joint whose name ends with `suffix`, or -1.
int ending(const assets::Model& m, std::string_view suffix) {
  for (size_t j = 0; j < m.joints.size(); ++j) {
    const std::string& n = m.joints[j].name;
    if (n.size() >= suffix.size() &&
        n.compare(n.size() - suffix.size(), suffix.size(), suffix) == 0)
      return static_cast<int>(j);
  }
  return -1;
}

void origin(const assets::Affine& a, double out[3]) {
  out[0] = a.m[3], out[1] = a.m[7], out[2] = a.m[11];
}
void column(const assets::Affine& a, size_t c, double sign, double out[3]) {
  for (size_t k = 0; k < 3; ++k) out[k] = sign * a.m[k * 4 + c];
}
void turn(const assets::Affine& a, const double in[3], double out[3]) {
  for (size_t k = 0; k < 3; ++k)
    out[k] = a.m[k * 4] * in[0] + a.m[k * 4 + 1] * in[1] + a.m[k * 4 + 2] * in[2];
}

}  // namespace

int arm_poses(const assets::Model& model, std::span<const assets::Affine> global,
              const double right[3], const double up[3], const double forward[3], ArmPose out[2]) {
  int n = 0;
  const int torso = ending(model, ".torso");
  for (const char* s : {"r", "l"}) {
    const std::string side(s);
    const int upper = ending(model, ".hand_" + side + "_upper");
    const int fore = ending(model, ".hand_" + side + "_fore");
    const int hand = ending(model, ".hand_" + side);
    if (upper < 0 || fore < 0 || hand < 0 || static_cast<size_t>(hand) >= global.size()) continue;
    ArmPose& p = out[n++];
    p.side = side == "r" ? 1 : -1;
    origin(global[size_t(upper)], p.shoulder);
    origin(global[size_t(fore)], p.elbow);
    origin(global[size_t(hand)], p.wrist);
    // The builder's hand node has +y distal and +x radial; through glTF's y-up and the loader's
    // mirror those arrive as the node's -z and -x.
    column(global[size_t(hand)], 2, -1.0, p.distal);
    column(global[size_t(hand)], 0, -1.0, p.radial);
    if (torso >= 0 && static_cast<size_t>(torso) < global.size()) {
      turn(global[size_t(torso)], right, p.right);
      turn(global[size_t(torso)], up, p.up);
      turn(global[size_t(torso)], forward, p.forward);
    } else {
      for (int k = 0; k < 3; ++k) p.right[k] = right[k], p.up[k] = up[k], p.forward[k] = forward[k];
    }
  }
  return n;
}

}  // namespace ge::viewmodel
