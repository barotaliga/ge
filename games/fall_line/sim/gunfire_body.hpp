// WHERE A ROUND MEETS HIS BODY (gunfire.hpp): the body as capsules round thirteen points (the
// head, the shoulders, elbows, hands, hips, knees and feet), and a round's path this step a
// segment through them. The first capsule along the path is the part it strikes: the trunk's upper
// half the chest (or the back, struck from behind), its lower the belly; an upper arm near the
// shoulder the shoulder; a thigh near the knee the knee.
//
// THE POINTS come off his stance on the skis (`body_points`): stood over the bindings, folded as
// far as he tucks and leaned into his turn by the body's lean; down, laid out face down along his
// way on the snow. They are built only when a round comes within reach of him, so the rest of a
// run never pays for them.
//
// ge:summary Where a round meets the skier's body: his points, the bones as capsules, the part hit
// ge:keywords gunfire body-points body-hit capsule bone part pass-distance stance lying chest back
// ge:skill sim
// ge:test tests/fall_line_gunfire_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "skier/anatomy.hpp"
#include "skier/skier.hpp"

namespace fall_line::sim {

// The body's points, in the order `BodyPoints::p` keeps them (x y z each).
enum class BodyPoint : uint8_t {
  head,
  shoulder_l,
  shoulder_r,
  elbow_l,
  elbow_r,
  hand_l,
  hand_r,
  hip_l,
  hip_r,
  knee_l,
  knee_r,
  foot_l,
  foot_r,
};
inline constexpr int32_t kBodyPoints = 13;

struct BodyPoints {
  std::array<double, static_cast<size_t>(3) * kBodyPoints> p{};
};

struct Vec3 {
  double x = 0, y = 0, z = 0;
};

// HIS POINTS, world frame, m, off his stance on the skis (or down on the snow).
BodyPoints body_points(const ge::skier::SkierState& s);
// THE MIDDLE a shooter aims at: halfway between his shoulders and his hips.
Vec3 body_centre(const ge::skier::SkierState& s);

struct BodyHit {
  ge::skier::Part part = ge::skier::Part::chest;
  double s = 0;  // how far along the segment, 0..1
};
// THE FIRST PART the segment p -> q strikes of the body at `b`, or none: it misses him.
std::optional<BodyHit> body_hit(const BodyPoints& b, const Vec3& p, const Vec3& q);
// The nearest the segment p -> q passes the point c, m.
double pass_distance(const Vec3& p, const Vec3& q, const Vec3& c);

}  // namespace fall_line::sim
