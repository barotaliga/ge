// plugins/brush/app/casters.hpp's choices for the moving casters: the lights that light one of
// them most get the casters' cubes (strength over distance, the casters near the eye first, none
// out of reach or without a world cube, at most kCasterLights), and the casters nearest the eye
// become the capsules, each its box's vertical capsule; each cube's light sees its casters as
// cones that hold every one of them.
//
// ge:summary Tests for the moving casters' light pick and capsules
// ge:keywords test brush bsp model-shadows caster_lights shadow_capsules capsule-shadow
// ge:skill brush testing
#include <cmath>
#include <cstddef>
#include <vector>

#include "brush/app/casters.hpp"
#include "check.hpp"

namespace {

namespace ba = ge::brush::app;

ba::GpuLight light_at(double x, double y, double z, double reach, double strength, int cube) {
  ba::GpuLight l{};
  l.pos[0] = float(x);
  l.pos[1] = float(y);
  l.pos[2] = float(z);
  l.pos[3] = float(reach);
  l.colour[0] = l.colour[1] = l.colour[2] = float(strength);
  l.info[0] = float(cube);
  l.info[1] = 0.04f;
  return l;
}

// A figure 0.6 m wide and 1.8 m tall standing at (x, 0, z).
ba::CasterBox figure_at(float x, float z) {
  return {{x - 0.3f, 0, z - 0.3f}, {x + 0.3f, 1.8f, z + 0.3f}};
}

const double kEye[3] = {0, 1.6, 0};

bool near(double got, double want) { return std::abs(got - want) < 1e-6; }

}  // namespace

TEST(the_light_nearest_a_caster_gets_the_first_cube) {
  const std::vector<ba::GpuLight> lights{light_at(0, 2, 6, 10, 1, 0), light_at(0, 2, 1, 10, 1, 6),
                                         light_at(0, 2, 3, 10, 1, 12)};
  const std::vector<size_t> shadowed{0, 1, 2};
  const std::vector<ba::CasterBox> boxes{figure_at(0, 0)};
  const std::vector<size_t> got = ba::caster_lights(lights, shadowed, boxes, kEye);
  CHECK(got.size() == 3);
  CHECK(got[0] == 1);
  CHECK(got[1] == 2);
  CHECK(got[2] == 0);
}

TEST(a_stronger_light_further_off_can_rank_first) {
  // 1 m off at strength 1 against 3 m off at strength 20: 20 / 9 > 1 / 1.
  const std::vector<ba::GpuLight> lights{light_at(0, 1, 1.3, 10, 1, 0),
                                         light_at(0, 1, 3.3, 10, 20, 6)};
  const std::vector<size_t> shadowed{0, 1};
  const std::vector<ba::CasterBox> boxes{figure_at(0, 0)};
  const std::vector<size_t> got = ba::caster_lights(lights, shadowed, boxes, kEye);
  CHECK(got.size() == 2);
  CHECK(got[0] == 1);
}

TEST(a_light_out_of_reach_or_without_a_world_cube_gets_none) {
  const std::vector<ba::GpuLight> lights{light_at(0, 1, 5, 2, 1, 0),    // reach 2, 4.7 m off
                                         light_at(0, 1, 1, 10, 1, -1),  // no world cube
                                         light_at(0, 1, 1, 10, 1, 6)};
  const std::vector<size_t> shadowed{0, 1, 2};
  const std::vector<ba::CasterBox> boxes{figure_at(0, 0)};
  const std::vector<size_t> got = ba::caster_lights(lights, shadowed, boxes, kEye);
  CHECK(got.size() == 1);
  CHECK(got[0] == 2);
  CHECK(ba::caster_lights(lights, shadowed, {}, kEye).empty());
}

TEST(a_caster_near_the_eye_wins_its_light_over_one_far_off) {
  // Two lights the same, each 1 m from a figure: the figure by the eye's light first.
  const std::vector<ba::GpuLight> lights{light_at(20, 1, 1, 10, 1, 0), light_at(0, 1, 1, 10, 1, 6)};
  const std::vector<size_t> shadowed{0, 1};
  const std::vector<ba::CasterBox> boxes{figure_at(20, 0), figure_at(0, 0)};
  const std::vector<size_t> got = ba::caster_lights(lights, shadowed, boxes, kEye);
  CHECK(got.size() == 2);
  CHECK(got[0] == 1);
}

TEST(at_most_kcasterlights_lights_get_a_cube) {
  std::vector<ba::GpuLight> lights;
  std::vector<size_t> shadowed;
  for (int i = 0; i < 12; ++i) {
    lights.push_back(light_at(0, 1, 1 + 0.5 * i, 20, 1, 6 * i));
    shadowed.push_back(size_t(i));
  }
  const std::vector<ba::CasterBox> boxes{figure_at(0, 0)};
  const std::vector<size_t> got = ba::caster_lights(lights, shadowed, boxes, kEye);
  CHECK(got.size() == size_t(ba::kCasterLights));
  for (size_t i = 0; i < got.size(); ++i) CHECK(got[i] == i);  // the nearest in order
}

TEST(the_capsules_are_the_casters_nearest_the_eye_each_its_box_upright) {
  std::vector<ba::CasterBox> boxes;
  for (int i = 5; i >= 0; --i) boxes.push_back(figure_at(0, float(i * 2 + 1)));
  const std::vector<ba::Capsule> caps = ba::shadow_capsules(boxes, kEye);
  CHECK(caps.size() == size_t(ba::kShadowCapsules));
  for (size_t c = 0; c < caps.size(); ++c) {
    CHECK(near(caps[c].a[2], double(c * 2 + 1)));  // nearest first
    CHECK(near(caps[c].r, 0.15));                  // a quarter of 0.6 m
    CHECK(near(caps[c].a[0], 0.0));
    CHECK(near(caps[c].b[0], 0.0));
    CHECK(near(caps[c].a[1], 0.15));  // a radius over the floor
    CHECK(near(caps[c].b[1], 1.65));  // a radius under the top
  }
  CHECK(ba::shadow_capsules({}, kEye).empty());
}

TEST(a_lights_caster_cones_hold_each_caster_in_reach_and_no_other_way) {
  const ba::GpuLight l = light_at(0, 1, 0, 10, 1, 0);
  const std::vector<ba::CasterBox> boxes{figure_at(0, 4),
                                         figure_at(0, 30)};  // the second past reach
  const ba::CasterCones c = ba::caster_cones(l, boxes, 0);
  // One cone toward +z, its half-angle the box's sphere's seen from the light.
  CHECK(std::abs(c.cone[0][2] - 0.99) < 0.02);
  const double r = std::sqrt(0.3 * 0.3 * 2 + 0.9 * 0.9), d = std::sqrt(16 + 0.1 * 0.1);
  CHECK(std::abs(double(c.cone[0][3]) - std::cos(std::asin(r / d))) < 1e-4);
  for (int k = 1; k < ba::kCasterCones; ++k) CHECK(c.cone[k][3] == 2.0f);  // no other way
  CHECK(std::abs(double(c.nearest) - (d - r)) < 1e-4);
  // None in reach: no way inside, the nearest the reach.
  const ba::CasterCones none = ba::caster_cones(l, std::vector<ba::CasterBox>{figure_at(0, 30)}, 0);
  for (int k = 0; k < ba::kCasterCones; ++k) CHECK(none.cone[k][3] == 2.0f);
  CHECK(none.nearest == 10.0f);
  // The light inside a caster's sphere: every way.
  const ba::CasterCones in = ba::caster_cones(l, std::vector<ba::CasterBox>{figure_at(0, 0.1f)}, 0);
  CHECK(in.cone[0][3] == -2.0f);
  CHECK(in.nearest == 0.0f);
}

TEST(past_kcastercones_casters_merge_into_cones_that_still_hold_every_one) {
  const ba::GpuLight l = light_at(0, 1, 0, 20, 1, 0);
  std::vector<ba::CasterBox> boxes;
  for (int i = 0; i < 9; ++i) {
    const double a = i * 0.7;
    boxes.push_back(figure_at(float(5 * std::sin(a)), float(5 * std::cos(a))));
  }
  const ba::CasterCones c = ba::caster_cones(l, boxes, 0.1);
  // Every corner of every box lies inside some cone.
  for (const ba::CasterBox& b : boxes)
    for (int corner = 0; corner < 8; ++corner) {
      const double p[3] = {corner & 1 ? b.hi[0] : b.lo[0], corner & 2 ? b.hi[1] : b.lo[1],
                           corner & 4 ? b.hi[2] : b.lo[2]};
      double q[3], len = 0;
      for (int k = 0; k < 3; ++k) {
        q[k] = p[k] - l.pos[k];
        len += q[k] * q[k];
      }
      len = std::sqrt(len);
      bool inside = false;
      for (int k = 0; k < ba::kCasterCones; ++k) {
        const double dot = (q[0] * c.cone[k][0] + q[1] * c.cone[k][1] + q[2] * c.cone[k][2]) / len;
        inside = inside || dot >= double(c.cone[k][3]);
      }
      CHECK(inside);
    }
}

TEST(a_box_is_in_every_cube_face_any_of_it_reaches_within_the_reach) {
  // Boxes all round a light: any point of a box inside a face's quarter of the ways and within
  // the reach puts the box in that face; a box wholly past the reach is in none.
  const float pos[3] = {0, 1, 0};
  const float reach = 6;
  uint32_t seed = 12345;
  const auto next = [&] {
    seed = seed * 1664525u + 1013904223u;
    return float(seed >> 8) / float(1u << 24);
  };
  int in_some = 0, culled = 0;
  for (int t = 0; t < 400; ++t) {
    ba::CasterBox b{};
    for (int k = 0; k < 3; ++k) {
      const float c = pos[k] + (next() * 2 - 1) * 8, h = 0.1f + next() * 1.2f;
      b.lo[k] = c - h;
      b.hi[k] = c + h;
    }
    bool want[6] = {};
    for (int s = 0; s < 1000; ++s) {
      double q[3], d2 = 0;
      for (int k = 0; k < 3; ++k) {
        q[k] = b.lo[k] + (b.hi[k] - b.lo[k]) * next() - pos[k];
        d2 += q[k] * q[k];
      }
      if (d2 >= double(reach) * reach) continue;
      for (int f = 0; f < 6; ++f) {
        const int i = f / 2;
        const double along = f % 2 == 0 ? q[i] : -q[i];
        if (along >= std::abs(q[(i + 1) % 3]) && along >= std::abs(q[(i + 2) % 3])) want[f] = true;
      }
    }
    for (int f = 0; f < 6; ++f) {
      const bool got = ba::box_in_face(b, pos, reach, f);
      if (want[f]) CHECK(got);
      in_some += got ? 1 : 0;
      culled += got ? 0 : 1;
    }
  }
  CHECK(culled > 0);  // the test leaves faces out, not only keeps them
  CHECK(in_some > 0);
  // Wholly past the reach: in no face.
  const ba::CasterBox far{{10, 0, 10}, {11, 2, 11}};
  for (int f = 0; f < 6; ++f) CHECK(!ba::box_in_face(far, pos, reach, f));
}
