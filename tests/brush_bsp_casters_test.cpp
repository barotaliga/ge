// plugins/brush/app/casters.hpp's choices for the moving casters: the lights that light one of
// them most get the casters' cubes (strength over distance, the casters near the eye first, none
// out of reach or without a world cube, at most kCasterLights), and the casters nearest the eye
// become the capsules, each its box's vertical capsule.
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
