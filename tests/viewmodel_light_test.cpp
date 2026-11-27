// The held weapon's light (engine/viewmodel/light.hpp): the fall-off is the renderer's (inverse
// square softened by the emitter, ending at the range); the strongest lights are kept in a fixed
// order; a light is turned into the view's frame; a face toward a light is brighter than one
// away; and the floor holds a gun in the dark above black.
//
// ge:summary Tests: the held weapon's light fall-off, ranking, view frame, shading and floor
// ge:keywords test viewmodel light probe point-light falloff floor shade_held light_held
// ge:skill render testing
// ge:test tests/viewmodel_light_test.cpp
#include <cmath>
#include <vector>

#include "check.hpp"
#include "viewmodel/light.hpp"

namespace vm = ge::viewmodel;

TEST(the_falloff_is_the_renderers_law) {
  CHECK(std::fabs(vm::light_falloff(2, 100, 0.05f) - 0.25f) < 1e-3f);
  CHECK(vm::light_falloff(10, 10, 0.05f) == 0);
  CHECK(vm::light_falloff(0.01f, 10, 0.1f) > vm::light_falloff(0.05f, 10, 0.1f));
  CHECK(std::fabs(vm::light_falloff(0.0f, 10, 0.1f) - 100.0f) <
        1e-3f);  // the emitter's size caps it
}

TEST(the_strongest_lights_are_kept_in_order) {
  std::vector<vm::PointLight> lights;
  for (int i = 0; i < 20; ++i) {
    vm::PointLight l;
    l.position[2] = 1 + i * 0.5;  // ahead, further and further
    l.range_m = 50;
    lights.push_back(l);
  }
  ge::camera::Pose view;
  const float at[3] = {0, 0, 0};
  const vm::Lighting l = vm::light_held(vm::Probe{}, lights, view, at);
  CHECK(l.count == vm::kHeldLights);
  for (int i = 0; i < l.count; ++i)
    CHECK(std::fabs(l.lights[i].at[2] - (1 + float(i) * 0.5f)) < 1e-5f);
}

TEST(a_light_on_the_right_lights_the_right_face) {
  vm::PointLight lamp;
  ge::camera::Pose view;
  view.yaw = 1.0;  // looking off to the right of +z
  // A lamp one metre to the view's right: the view's right is (cos yaw, 0, -sin yaw).
  lamp.position[0] = std::cos(1.0);
  lamp.position[2] = -std::sin(1.0);
  vm::Probe probe;
  probe.ambient[0] = probe.ambient[1] = probe.ambient[2] = 0;
  const float at[3] = {0, 0, 0};
  const vm::Lighting l = vm::light_held(probe, {&lamp, 1}, view, at, 0.0f);
  CHECK(l.count == 1);
  CHECK(std::fabs(l.lights[0].at[0] - 1) < 1e-5f && std::fabs(l.lights[0].at[2]) < 1e-5f);
  const float p[3] = {0, 0, 0};
  const float right[3] = {1, 0, 0}, left[3] = {-1, 0, 0};
  float dr[3], sr[3], dl[3], sl[3];
  vm::shade_held(l, p, right, dr, sr);
  vm::shade_held(l, p, left, dl, sl);
  CHECK(dr[0] > 0.5f);
  CHECK(dl[0] == 0);
}

TEST(the_floor_holds_a_gun_in_the_dark) {
  vm::Probe probe;
  probe.ambient[0] = probe.ambient[1] = probe.ambient[2] = 0;
  ge::camera::Pose view;
  const float at[3] = {0, 0, 0};
  const vm::Lighting l = vm::light_held(probe, {}, view, at, 0.08f);
  const float p[3] = {0.1f, -0.1f, 0.3f}, n[3] = {0, 0, -1};
  float d[3], s[3];
  vm::shade_held(l, p, n, d, s);
  for (int k = 0; k < 3; ++k) CHECK(d[k] == 0.08f);
}

TEST(a_mirrored_gun_is_lit_from_the_side_it_is_seen_on) {
  // A lamp on the view's left: the mirrored gun, lit at its grip mirrored and then flipped, has
  // the lamp on its right in its own frame, so the face the flipped picture turns left is lit.
  vm::PointLight lamp;
  lamp.position[0] = -1;
  ge::camera::Pose view;
  vm::Probe probe;
  probe.ambient[0] = probe.ambient[1] = probe.ambient[2] = 0;
  const float grip[3] = {-0.2f, -0.2f, 0.4f};  // a right-hand grip at 0.2 m, mirrored
  vm::Lighting l = vm::light_held(probe, {&lamp, 1}, view, grip, 0.0f);
  l.toward[0] = 0.5f;
  vm::mirror_lighting(l);
  CHECK(l.count == 1 && std::fabs(l.lights[0].at[0] - 1) < 1e-5f && l.toward[0] == -0.5f);
  // The world mirrored about the view's upright middle: a point and a way swap sides.
  view.yaw = 1.0;
  view.eye = {2, 1, 3};
  double at[3] = {2 + std::cos(1.0), 1.5, 3 - std::sin(1.0)};  // a metre to the right
  vm::mirror_world_point(view, at);
  CHECK(std::fabs(at[0] - (2 - std::cos(1.0))) < 1e-12 && std::fabs(at[1] - 1.5) < 1e-12 &&
        std::fabs(at[2] - (3 + std::sin(1.0))) < 1e-12);
  double way[3] = {std::sin(1.0), 0, std::cos(1.0)};  // straight ahead stays
  vm::mirror_world_dir(view, way);
  CHECK(std::fabs(way[0] - std::sin(1.0)) < 1e-12 && std::fabs(way[2] - std::cos(1.0)) < 1e-12);
}

TEST(a_lamp_beside_the_gun_is_rolled_off_its_colour_kept) {
  // Light past the knee eases toward `most` and never reaches it; the hue (the channels' ratios)
  // and the highlight's share stay; under the knee, or with no roll-off set, nothing changes.
  vm::Lighting l;
  l.knee = 0.8f;
  l.most = 1.6f;
  float d[3] = {8.0f, 4.0f, 2.0f}, s[3] = {1.0f, 0.5f, 0.25f};
  vm::tone_held(l, d, s);
  CHECK(d[0] > l.knee && d[0] < l.most);
  CHECK(std::fabs(d[1] / d[0] - 0.5f) < 1e-6f && std::fabs(d[2] / d[0] - 0.25f) < 1e-6f);
  CHECK(std::fabs(s[0] / d[0] - 0.125f) < 1e-6f);
  const float e = 8.0f - 0.8f;
  CHECK(std::fabs(d[0] - (0.8f + e / (1 + e / 0.8f))) < 1e-5f);
  float dim[3] = {0.5f, 0.6f, 0.7f}, none[3] = {0, 0, 0};
  vm::tone_held(l, dim, none);
  CHECK(dim[0] == 0.5f && dim[1] == 0.6f && dim[2] == 0.7f);
  vm::Lighting off;  // knee and most 0: the light as it adds up
  float bright[3] = {9, 9, 9};
  vm::tone_held(off, bright, none);
  CHECK(bright[0] == 9 && bright[1] == 9 && bright[2] == 9);
}
