// The placeholder figure's pieces and its pose from a SkierState.
//
// ge:summary The placeholder figure's pieces and pose
// ge:keywords skier figure pose pieces tuck lean edge
// ge:skill skier render
// ge:test tests/skier_figure_test.cpp
#include "skier/app/figure.hpp"

#include <cmath>

#include "core/libm/libm.hpp"

namespace ge::skier {

namespace {

using core::Quat;
using core::Vec3;
namespace libm = core::libm;

constexpr double kStance = 0.11;  // each foot's way off the centre line, m
constexpr double kAnkle = 0.17;   // the ankle over the skis' base, m
constexpr double kShin = 0.45, kThigh = 0.45, kTorso = 0.55;
constexpr double kHipBack = -0.05;  // the hip behind the ankles, m
constexpr double kAirPitch = 0.6;   // the most the skis pitch to the flight's path, rad

Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 scale(Vec3 a, double k) { return {a.x * k, a.y * k, a.z * k}; }
double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vec3 unit(Vec3 a) {
  const double n = std::sqrt(dot(a, a));
  return n > 0 ? scale(a, 1 / n) : Vec3{0, 1, 0};
}

// The rotation whose columns are the body's right, up and forward in the world.
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

// The rotation carrying the body's up (+y) onto the unit direction d.
Quat up_onto(Vec3 d) {
  if (1 + d.y < 1e-9) return Quat{1, 0, 0, 0};
  Quat q{d.z, 0, -d.x, 1 + d.y};
  return core::normalize(q);
}

struct Frame {
  Vec3 base;  // the skis' base under the boots
  Quat rot;
  Vec3 at(Vec3 local) const { return add(base, core::rotate(rot, local)); }
};

void place(PiecePose& out, const Frame& f, Vec3 centre, Quat local, double sx, double sy,
           double sz) {
  out.centre = f.at(centre);
  out.rotation = core::multiply(f.rot, local);
  out.size[0] = sx;
  out.size[1] = sy;
  out.size[2] = sz;
}

// A limb from a to b (body frame) of radius r.
void segment(PiecePose& out, const Frame& f, Vec3 a, Vec3 b, double r) {
  const Vec3 d = sub(b, a);
  const double len = std::sqrt(dot(d, d));
  place(out, f, scale(add(a, b), 0.5), up_onto(unit(d)), 2 * r, len, 2 * r);
}

constexpr std::array<FigurePiece, kFigurePieces> kPieces = {{
    {FigureShape::box, FigurePart::ski},
    {FigureShape::box, FigurePart::ski},
    {FigureShape::box, FigurePart::boot},
    {FigureShape::box, FigurePart::boot},
    {FigureShape::cylinder, FigurePart::legs},
    {FigureShape::cylinder, FigurePart::legs},
    {FigureShape::cylinder, FigurePart::legs},
    {FigureShape::cylinder, FigurePart::legs},
    {FigureShape::box, FigurePart::legs},
    {FigureShape::box, FigurePart::jacket},
    {FigureShape::cylinder, FigurePart::jacket},
    {FigureShape::cylinder, FigurePart::jacket},
    {FigureShape::cylinder, FigurePart::jacket},
    {FigureShape::cylinder, FigurePart::jacket},
    {FigureShape::ball, FigurePart::helmet},
    {FigureShape::cylinder, FigurePart::pole},
    {FigureShape::cylinder, FigurePart::pole},
}};

constexpr std::array<FigureJoint, kFigureJoints> kJoints = {{
    {"waist", -1, 0},    {"hip_l", 0, -1},       {"hip_r", 0, 1},       {"knee_l", 1, -1},
    {"knee_r", 2, 1},    {"ankle_l", 3, -1},     {"ankle_r", 4, 1},     {"ski_tip_l", 5, -1},
    {"ski_tip_r", 6, 1}, {"ski_tail_l", 5, -1},  {"ski_tail_r", 6, 1},  {"neck", 0, 0},
    {"head", 11, 0},     {"shoulder_l", 11, -1}, {"shoulder_r", 11, 1}, {"elbow_l", 13, -1},
    {"elbow_r", 14, 1},  {"hand_l", 15, -1},     {"hand_r", 16, 1},     {"grip_l", 17, -1},
    {"grip_r", 18, 1},   {"pole_tip_l", 19, -1}, {"pole_tip_r", 20, 1},
}};

struct Legs {
  Vec3 knee, hip;  // the right leg's; the left mirrors x
};

Legs legs_for(double tuck) {
  const double a = 0.35 + 0.55 * tuck;  // the shin forward of upright, rad
  Legs l;
  l.knee = {kStance, kAnkle + kShin * libm::cos(a), kShin * libm::sin(a)};
  const double dz = kHipBack - l.knee.z;
  const double dy = std::sqrt(std::fmax(kThigh * kThigh - dz * dz, 0.0));
  l.hip = {kStance, l.knee.y + dy, kHipBack};
  return l;
}

}  // namespace

const std::array<FigurePiece, kFigurePieces>& figure_pieces() { return kPieces; }

double figure_hip_height(double tuck) { return legs_for(tuck).hip.y; }

FigureFrame figure_frame(const SkierState& s, const World& world) {
  const Vec3 level_f{libm::sin(s.heading), 0, libm::cos(s.heading)};
  Vec3 up{0, 1, 0}, fwd = level_f;
  if (s.on_snow && !s.down && world.level) {
    mountain::mapgen::Vec3 n;
    world.level->normal_at(s.x, s.z, n);
    up = unit(Vec3{n.x, n.y, n.z});
    fwd = unit(sub(level_f, scale(up, dot(level_f, up))));
  } else if (!s.down) {
    const double flat = std::sqrt(s.vx * s.vx + s.vz * s.vz);
    const double p =
        core::num::max(-kAirPitch, core::num::min(kAirPitch, libm::atan2(s.vy, flat + 1e-6)));
    fwd = {level_f.x * libm::cos(p), libm::sin(p), level_f.z * libm::cos(p)};
    up = unit(cross(fwd, cross(Vec3{0, 1, 0}, fwd)));
  }
  return FigureFrame{Vec3{s.x, s.y, s.z}, from_basis(unit(cross(up, fwd)), up, fwd)};
}

void pose_figure(const SkierState& s, const World& world,
                 std::array<PiecePose, kFigurePieces>& out) {
  const double tuck = core::num::max(0.0, core::num::min(1.0, s.tuck));
  const Quat base = figure_frame(s, world).rot;
  // Right side down is a negative turn about the forward axis (engine/core/quat.hpp).
  const double body_roll = s.down ? 1.45 : s.lean;
  const double ski_roll = s.down ? 1.45 : s.edge;
  const Vec3 at{s.x, s.y, s.z};
  const Frame skis{at, core::multiply(base, core::from_axis_angle(0, 0, 1, -ski_roll))};
  const Frame body{at, core::multiply(base, core::from_axis_angle(0, 0, 1, -body_roll))};
  const Quat none = core::identity();

  for (int side = 0; side < 2; ++side) {
    const double x = side == 0 ? -kStance : kStance;
    place(out[static_cast<size_t>(side)], skis, {x, 0.012, 0.08}, none, 0.075, 0.024, 1.75);
    place(out[2 + static_cast<size_t>(side)], skis, {x, 0.10, 0.0}, none, 0.12, 0.2, 0.31);
  }
  const Legs l = legs_for(tuck);
  for (int side = 0; side < 2; ++side) {
    const double m = side == 0 ? -1 : 1;
    const Vec3 ankle{m * kStance, kAnkle, 0};
    const Vec3 knee{m * l.knee.x, l.knee.y, l.knee.z};
    const Vec3 hip{m * l.hip.x, l.hip.y, l.hip.z};
    segment(out[4 + static_cast<size_t>(side)], body, ankle, knee, 0.06);
    segment(out[6 + static_cast<size_t>(side)], body, knee, hip, 0.075);
  }
  const Vec3 hip{0, l.hip.y, kHipBack};
  place(out[8], body, hip, none, 0.34, 0.2, 0.22);
  const double c = 0.25 + 0.85 * tuck;  // the torso forward of upright, rad
  const Vec3 dir{0, libm::cos(c), libm::sin(c)};
  const Vec3 waist = add(hip, Vec3{0, 0.05, 0});
  const Vec3 shoulders = add(waist, scale(dir, kTorso));
  segment(out[9], body, waist, shoulders, 0.17);
  out[9].size[2] = 0.24;
  place(out[14], body, add(shoulders, Vec3{0, 0.17, 0.05}), none, 0.25, 0.27, 0.27);
  for (int side = 0; side < 2; ++side) {
    const double m = side == 0 ? -1 : 1;
    const Vec3 shoulder = add(shoulders, Vec3{m * 0.2, -0.03, 0});
    const Vec3 hand{m * 0.27, l.hip.y + 0.12 - 0.05 * tuck, kHipBack + 0.32 + 0.12 * tuck};
    const Vec3 elbow =
        add(scale(add(shoulder, hand), 0.5), Vec3{m * 0.06, -0.08, -0.04 - 0.04 * tuck});
    segment(out[10 + static_cast<size_t>(side)], body, shoulder, elbow, 0.045);
    segment(out[12 + static_cast<size_t>(side)], body, elbow, hand, 0.04);
    const Vec3 tip{m * 0.38, 0.0, kHipBack - 0.25 - 0.3 * tuck};
    const Vec3 grip = add(hand, Vec3{0, 0.08, 0});
    const Vec3 along = unit(sub(tip, grip));
    segment(out[15 + static_cast<size_t>(side)], body, grip, add(grip, scale(along, 1.15)), 0.009);
  }
}

const std::array<FigureJoint, kFigureJoints>& figure_joints() { return kJoints; }

void figure_joint_positions(const std::array<PiecePose, kFigurePieces>& pieces,
                            const FigureFrame& frame, std::array<Vec3, kFigureJoints>& out) {
  // A piece's two ends along its length (a limb's y, a ski's z), in the figure's frame.
  const auto end = [&](size_t piece, double sign, bool along_z) {
    const PiecePose& p = pieces[piece];
    const Vec3 half = along_z ? Vec3{0, 0, sign * p.size[2] / 2} : Vec3{0, sign * p.size[1] / 2, 0};
    return core::unrotate(frame.rot, sub(add(p.centre, core::rotate(p.rotation, half)), frame.at));
  };
  out[0] = end(9, -1, false);                                             // waist
  out[11] = end(9, 1, false);                                             // neck
  out[12] = core::unrotate(frame.rot, sub(pieces[14].centre, frame.at));  // head
  for (size_t side = 0; side < 2; ++side) {
    out[1 + side] = end(6 + side, 1, false);     // hip
    out[3 + side] = end(4 + side, 1, false);     // knee
    out[5 + side] = end(4 + side, -1, false);    // ankle
    out[7 + side] = end(side, 1, true);          // ski tip
    out[9 + side] = end(side, -1, true);         // ski tail
    out[13 + side] = end(10 + side, -1, false);  // shoulder
    out[15 + side] = end(10 + side, 1, false);   // elbow
    out[17 + side] = end(12 + side, 1, false);   // hand
    out[19 + side] = end(15 + side, -1, false);  // grip
    out[21 + side] = end(15 + side, 1, false);   // pole tip
  }
}

}  // namespace ge::skier
