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
